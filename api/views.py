from django.http import JsonResponse
from django.views.decorators.csrf import csrf_exempt
from django.views.decorators.http import require_http_methods
from django.utils import timezone
from outlets.models import Outlet, SensorData, Alert, PendingCommand, MainBreakerReading, CentralControlUnit, EventLog
from channels.layers import get_channel_layer
from asgiref.sync import async_to_sync
import json
import requests as http_requests
from datetime import timedelta

# How often to persist sensor data to the database.
# Data is always pushed to WebSocket for real-time UI updates.
DB_LOG_INTERVAL = timedelta(seconds=60)

# How recent the CCU must have been seen to attempt direct HTTP
DIRECT_CMD_TIMEOUT = timedelta(seconds=30)
DIRECT_CMD_HTTP_TIMEOUT = 3  # seconds


def _get_client_ip(request):
    """Extract the real client IP from the request."""
    x_forwarded = request.META.get('HTTP_X_FORWARDED_FOR')
    if x_forwarded:
        return x_forwarded.split(',')[0].strip()
    return request.META.get('REMOTE_ADDR')


def _update_ccu_ip(ccu_obj, ip):
    """Update a CCU's IP address and last_seen timestamp."""
    if ccu_obj:
        ccu_obj.ip_address = ip
        ccu_obj.last_seen = timezone.now()
        ccu_obj.save(update_fields=['ip_address', 'last_seen'])


def _send_direct_to_esp32(ip, device_id, command, socket='', value=None):
    """
    Send a command directly to the ESP32's local web server.
    Returns (success: bool, response_data: dict or None).
    """
    try:
        url = f'http://{ip}/api/ext/relay'
        payload = {
            'device_id': device_id,
            'command': command,
            'socket': socket,
        }
        if value is not None:
            payload['value'] = str(value)

        resp = http_requests.post(url, data=payload, timeout=DIRECT_CMD_HTTP_TIMEOUT)
        if resp.status_code == 200:
            return True, resp.json()
        return False, None
    except Exception:
        return False, None

@csrf_exempt
@require_http_methods(["POST"])
def receive_sensor_data(request):
    """
    API endpoint for CCU (ESP32) to send sensor data.
    URL: POST /api/data/
    
    Hybrid approach:
      - ALWAYS broadcasts via WebSocket for real-time UI
      - Only saves to DB every 30 seconds (DB_LOG_INTERVAL)
      - Alerts and relay state updates always happen immediately
    
    Expected JSON payload from CCU firmware:
    {
        "device_id": "FE",
        "current_a": 1250,
        "current_b": 800,
        "relay_a": true,
        "relay_b": false,
        "is_overload": false
    }
    """
    try:
        data = json.loads(request.body)
        
        # Validate required fields
        required_fields = ['device_id', 'current_a', 'current_b']
        if not all(field in data for field in required_fields):
            return JsonResponse({
                'success': False,
                'message': f'Missing required fields. Required: {required_fields}'
            }, status=400)
        
        # Find the outlet by device_id
        try:
            outlet = Outlet.objects.get(device_id=data['device_id'])
        except Outlet.DoesNotExist:
            return JsonResponse({
                'success': False,
                'message': f'Outlet with device_id {data["device_id"]} not found'
            }, status=404)

        # Auto-capture ESP32 IP and link outlet to CCU
        client_ip = _get_client_ip(request)
        # Try to find which CCU is sending — use breaker data's ccu_id or first user CCU
        if not outlet.ccu:
            # Auto-link: find a CCU owned by the same user
            ccu_obj = CentralControlUnit.objects.filter(user=outlet.user).first()
            if ccu_obj:
                outlet.ccu = ccu_obj
                outlet.save(update_fields=['ccu'])
                _update_ccu_ip(ccu_obj, client_ip)
        else:
            _update_ccu_ip(outlet.ccu, client_ip)
        
        # Parse values
        is_overload = data.get('is_overload', False)
        current_a = max(0, int(data['current_a']))
        current_b = max(0, int(data['current_b']))
        
        if current_a == 65535 or current_b == 65535:
            is_overload = True
        
        # Noise floor filter: PIC current sensors output ~49-98mA baseline
        # noise even with no load. Clamp 0-100mA to 0 (skip overload sentinel).
        NOISE_FLOOR_MA = 100
        if 0 < current_a <= NOISE_FLOOR_MA:
            current_a = 0
        if 0 < current_b <= NOISE_FLOOR_MA:
            current_b = 0
        
        now = timezone.now()
        
        # NOTE: Relay state (relay_a/relay_b) from sensor data is NOT used here.
        # The ESP32 OutletDevice only knows relay state from PIC ACK packets,
        # which are unreliable. Relay state is managed exclusively through
        # queue_command() when the user toggles from the UI.
        
        # Alerts always fire immediately (critical events)
        # Format current values — replace 0xFFFF sentinel with "OVERLOAD" label
        display_a = 'OVERLOAD' if current_a == 65535 else f'{current_a}mA'
        display_b = 'OVERLOAD' if current_b == 65535 else f'{current_b}mA'
        
        if is_overload:
            Alert.objects.create(
                outlet=outlet,
                alert_type='overload',
                message=f'Overload trip detected! Socket A: {display_a}, Socket B: {display_b}'
            )
            EventLog.objects.create(
                user=outlet.user,
                source='PIC_HARDWARE',
                action_type='OVERLOAD_TRIPPED',
                target_device=f'0x{outlet.device_id}',
                details=f'Overload trip on {outlet.name}! Socket A: {display_a}, Socket B: {display_b}. Relay auto-cutoff triggered.'
            )
        
        if outlet.threshold > 0 and (current_a > outlet.threshold or current_b > outlet.threshold):
            Alert.objects.create(
                outlet=outlet,
                alert_type='threshold',
                message=f'Threshold ({outlet.threshold}mA) exceeded! Socket A: {display_a}, Socket B: {display_b}'
            )
            EventLog.objects.create(
                user=outlet.user,
                source='SERVER',
                action_type='THRESHOLD_EXCEEDED',
                target_device=f'0x{outlet.device_id}',
                details=f'Threshold ({outlet.threshold}mA) exceeded on {outlet.name}! Socket A: {display_a}, Socket B: {display_b}'
            )
        
        # DB write: only persist sensor data every DB_LOG_INTERVAL
        saved_to_db = False
        last_entry = SensorData.objects.filter(outlet=outlet).first()
        if not last_entry or (now - last_entry.timestamp) >= DB_LOG_INTERVAL:
            SensorData.objects.create(
                outlet=outlet,
                current_a=current_a,
                current_b=current_b,
                is_overload=is_overload,
            )
            saved_to_db = True
        
        # WebSocket: ALWAYS broadcast for real-time UI
        try:
            channel_layer = get_channel_layer()
            async_to_sync(channel_layer.group_send)(
                f'sensor_{outlet.device_id}',
                {
                    'type': 'sensor_update',
                    'data': {
                        'outlet_name': outlet.name,
                        'device_id': outlet.device_id,
                        'relay_a': outlet.relay_a,
                        'relay_b': outlet.relay_b,
                        'current_a': current_a,
                        'current_b': current_b,
                        'is_overload': is_overload,
                        'timestamp': now.isoformat(),
                    }
                }
            )
        except Exception:
            pass  # WebSocket push is best-effort
        
        return JsonResponse({
            'success': True,
            'message': 'Data received' + (' (logged to DB)' if saved_to_db else ' (WebSocket only)'),
            'relay_a': outlet.relay_a,
            'relay_b': outlet.relay_b,
        })
        
    except json.JSONDecodeError:
        return JsonResponse({
            'success': False,
            'message': 'Invalid JSON format'
        }, status=400)
    except Exception as e:
        return JsonResponse({
            'success': False,
            'message': str(e)
        }, status=500)


@csrf_exempt
@require_http_methods(["POST"])
def receive_breaker_data(request):
    """
    API endpoint for CCU to send main breaker (SCT-013) readings.
    URL: POST /api/breaker-data/
    
    Hybrid approach:
      - ALWAYS broadcasts via WebSocket for real-time UI
      - Only saves to DB every 5 minutes (DB_LOG_INTERVAL)
    
    Expected JSON payload from CCU firmware:
    {
        "ccu_id": "01",
        "current_ma": 4500
    }
    """
    try:
        data = json.loads(request.body)
        
        # Validate required fields
        required_fields = ['ccu_id', 'current_ma']
        if not all(field in data for field in required_fields):
            return JsonResponse({
                'success': False,
                'message': f'Missing required fields. Required: {required_fields}'
            }, status=400)
        
        ccu_id = str(data['ccu_id']).upper().zfill(2)  # '1' → '01' to match registered format
        current_ma = max(0, int(data['current_ma']))
        
        # Noise floor filter: SCT-013 sensor outputs baseline noise with no load.
        BREAKER_NOISE_FLOOR_MA = 280
        if 0 < current_ma <= BREAKER_NOISE_FLOOR_MA:
            current_ma = 0
        
        now = timezone.now()
        
        # Look up registered CCU (if exists) and capture IP
        ccu_obj = CentralControlUnit.objects.filter(ccu_id=ccu_id).first()
        _update_ccu_ip(ccu_obj, _get_client_ip(request))
        
        # DB write: only persist every DB_LOG_INTERVAL
        saved_to_db = False
        last_entry = MainBreakerReading.objects.filter(ccu_id=ccu_id).first()
        if not last_entry or (now - last_entry.timestamp) >= DB_LOG_INTERVAL:
            MainBreakerReading.objects.create(
                ccu_id=ccu_id,
                ccu_device=ccu_obj,
                current_ma=current_ma,
                threshold=ccu_obj.breaker_threshold if ccu_obj else 0,
            )
            saved_to_db = True
        
        # WebSocket: ALWAYS broadcast for real-time UI
        try:
            channel_layer = get_channel_layer()
            async_to_sync(channel_layer.group_send)(
                f'sensor_breaker_{ccu_id}',
                {
                    'type': 'sensor_update',
                    'data': {
                        'ccu_id': ccu_id,
                        'current_ma': current_ma,
                        'current_amps': round(current_ma / 1000.0, 2),
                        'timestamp': now.isoformat(),
                    }
                }
            )
        except Exception:
            pass  # WebSocket is best-effort
        
        return JsonResponse({
            'success': True,
            'message': 'Breaker data received' + (' (logged to DB)' if saved_to_db else ' (WebSocket only)'),
        })
        
    except json.JSONDecodeError:
        return JsonResponse({
            'success': False,
            'message': 'Invalid JSON format'
        }, status=400)
    except Exception as e:
        return JsonResponse({
            'success': False,
            'message': str(e)
        }, status=500)


@require_http_methods(["GET"])
def get_outlet_status(request, device_id):
    """
    API endpoint for CCU to get outlet relay states.
    URL: GET /api/outlet-status/<device_id>/
    """
    try:
        outlet = Outlet.objects.get(device_id=device_id)
        
        return JsonResponse({
            'success': True,
            'device_id': outlet.device_id,
            'name': outlet.name,
            'relay_a': outlet.relay_a,
            'relay_b': outlet.relay_b,
            'threshold': outlet.threshold,
        })
        
    except Outlet.DoesNotExist:
        return JsonResponse({
            'success': False,
            'message': f'Outlet with device_id {device_id} not found'
        }, status=404)

@require_http_methods(["POST"])
def queue_command(request, device_id, command):
    """
    API endpoint for the UI to queue a new command for an outlet.
    URL: POST /api/commands/<device_id>/<command>/
    """
    if not request.user.is_authenticated:
        return JsonResponse({'success': False, 'message': 'Authentication required'}, status=401)
        
    try:
        # Check permissions: does the user own this outlet?
        outlet = Outlet.objects.get(device_id=device_id, user=request.user)
        
        valid_commands = [c[0] for c in PendingCommand.COMMAND_CHOICES]
        if command not in valid_commands:
            return JsonResponse({'success': False, 'message': 'Invalid command'}, status=400)
            
        socket = request.POST.get('socket', '').lower()
        if socket not in ['a', 'b', '']:
            return JsonResponse({'success': False, 'message': 'Invalid socket'}, status=400)
            
        value = request.POST.get('value')
        if value is not None:
            try:
                value = int(value)
            except ValueError:
                return JsonResponse({'success': False, 'message': 'Invalid value format'}, status=400)
        # If toggling relay, attempt direct HTTP to ESP32 first
        if command in ('relay_on', 'relay_off'):
            state = True if command == 'relay_on' else False
            state_label = 'ON' if state else 'OFF'
            direct_success = False

            # Try direct communication if CCU is known and recently online
            if outlet.ccu and outlet.ccu.ip_address and outlet.ccu.last_seen:
                age = timezone.now() - outlet.ccu.last_seen
                if age < DIRECT_CMD_TIMEOUT:
                    direct_success, resp_data = _send_direct_to_esp32(
                        outlet.ccu.ip_address, device_id, command, socket
                    )

            if direct_success:
                # ESP32 confirmed — update DB with real state
                if socket == 'a':
                    outlet.relay_a = state
                    outlet.save(update_fields=['relay_a', 'updated_at'])
                elif socket == 'b':
                    outlet.relay_b = state
                    outlet.save(update_fields=['relay_b', 'updated_at'])

                EventLog.objects.create(
                    user=request.user,
                    source='WEB_DASHBOARD',
                    action_type='TOGGLE_RELAY',
                    target_device=f'0x{outlet.device_id}',
                    details=f'Socket {socket.upper()} turned {state_label} on {outlet.name} (direct)'
                )
                return JsonResponse({'success': True, 'message': f'Socket {socket.upper()} turned {state_label} (confirmed)', 'direct': True})
            else:
                # Fallback — queue PendingCommand for polling
                PendingCommand.objects.filter(
                    outlet=outlet, command=command, socket=socket, is_executed=False
                ).delete()
                PendingCommand.objects.create(
                    outlet=outlet, command=command, socket=socket, value=value
                )
                # Speculative DB update
                if socket == 'a':
                    outlet.relay_a = state
                    outlet.save(update_fields=['relay_a', 'updated_at'])
                elif socket == 'b':
                    outlet.relay_b = state
                    outlet.save(update_fields=['relay_b', 'updated_at'])

                EventLog.objects.create(
                    user=request.user,
                    source='WEB_DASHBOARD',
                    action_type='TOGGLE_RELAY',
                    target_device=f'0x{outlet.device_id}',
                    details=f'Socket {socket.upper()} turned {state_label} on {outlet.name} (queued)'
                )
                return JsonResponse({'success': True, 'message': f'Command queued — will execute within 2s', 'direct': False})
                
        elif command == 'set_threshold' and value is not None:
            # Try direct for threshold too
            direct_success = False
            if outlet.ccu and outlet.ccu.ip_address and outlet.ccu.last_seen:
                age = timezone.now() - outlet.ccu.last_seen
                if age < DIRECT_CMD_TIMEOUT:
                    direct_success, _ = _send_direct_to_esp32(
                        outlet.ccu.ip_address, device_id, command, '', value
                    )

            if not direct_success:
                PendingCommand.objects.filter(
                    outlet=outlet, command=command, is_executed=False
                ).delete()
                PendingCommand.objects.create(
                    outlet=outlet, command=command, socket=socket, value=value
                )

            outlet.threshold = value
            outlet.save(update_fields=['threshold', 'updated_at'])
            
            EventLog.objects.create(
                user=request.user,
                source='WEB_DASHBOARD',
                action_type='SET_THRESHOLD',
                target_device=f'0x{outlet.device_id}',
                details=f'Threshold set to {value}mA on {outlet.name}{" (direct)" if direct_success else " (queued)"}'
            )
            method = 'direct' if direct_success else 'queued'
            return JsonResponse({'success': True, 'message': f'Threshold set to {value}mA ({method})', 'direct': direct_success})

        else:
            # Other commands (read_sensors, ping) — always queue
            PendingCommand.objects.filter(
                outlet=outlet, command=command, socket=socket, is_executed=False
            ).delete()
            PendingCommand.objects.create(
                outlet=outlet, command=command, socket=socket, value=value
            )
            
        return JsonResponse({'success': True, 'message': f'Command {command} queued successfully', 'direct': False})
        
    except Outlet.DoesNotExist:
        return JsonResponse({'success': False, 'message': 'Outlet not found or unauthorized'}, status=404)



@csrf_exempt
@require_http_methods(["GET"])
def get_pending_commands(request, device_id):
    """
    API endpoint for CCU to poll for pending commands from the UI.
    URL: GET /api/commands/<device_id>/
    
    Returns pending commands and marks them as executed.
    
    Response format:
    {
        "success": true,
        "commands": [
            {"command": "relay_on", "socket": "a", "value": null},
            {"command": "set_threshold", "socket": "", "value": 5000}
        ]
    }
    """
    try:
        outlet = Outlet.objects.get(device_id=device_id)
        
        # Get all unexecuted commands for this outlet
        pending = PendingCommand.objects.filter(
            outlet=outlet,
            is_executed=False
        ).order_by('created_at')
        
        commands = []
        for cmd in pending:
            commands.append({
                'command': cmd.command,
                'socket': cmd.socket,
                'value': cmd.value,
            })
        
        # Mark all fetched commands as executed
        pending.update(is_executed=True)
        
        return JsonResponse({
            'success': True,
            'commands': commands,
        })
        
    except Outlet.DoesNotExist:
        return JsonResponse({
            'success': False,
            'message': f'Outlet with device_id {device_id} not found'
        }, status=404)

@csrf_exempt
@require_http_methods(["GET"])
def get_registered_outlets(request):
    """
    API endpoint for CCU to fetch the master list of registered outlets.
    URL: GET /api/devices/
    
    Returns: { "success": true, "devices": ["FE", "FD"] }
    """
    outlets = Outlet.objects.values_list('device_id', flat=True)
    return JsonResponse({
        'success': True,
        'devices': list(outlets)
    })


@csrf_exempt
@require_http_methods(["POST"])
def log_event(request):
    """
    API endpoint for the UI to log a custom event (e.g. Cut All Power).
    URL: POST /api/log-event/
    """
    if not request.user.is_authenticated:
        return JsonResponse({'success': False, 'message': 'Authentication required'}, status=401)
    
    try:
        data = json.loads(request.body)
        action_type = data.get('action_type', '')
        details = data.get('details', '')
        target_device = data.get('target_device', 'All Devices')
        source = data.get('source', 'WEB_DASHBOARD')
        
        EventLog.objects.create(
            user=request.user,
            source=source,
            action_type=action_type,
            target_device=target_device,
            details=details,
        )
        return JsonResponse({'success': True, 'message': 'Event logged'})
    except Exception as e:
        return JsonResponse({'success': False, 'message': str(e)}, status=500)


@csrf_exempt
@require_http_methods(["POST"])
def set_breaker_threshold(request):
    """
    API endpoint to save the main breaker threshold.
    URL: POST /api/breaker-threshold/
    Updates the threshold on the latest MainBreakerReading for the user's CCU.
    """
    if not request.user.is_authenticated:
        return JsonResponse({'success': False, 'message': 'Authentication required'}, status=401)

    try:
        data = json.loads(request.body)
        threshold = int(data.get('threshold', 0))

        if threshold <= 0:
            return JsonResponse({'success': False, 'message': 'Threshold must be greater than 0'}, status=400)

        # Get user's CCU and update its threshold
        ccu = CentralControlUnit.objects.filter(user=request.user).first()

        if not ccu:
            return JsonResponse({'success': False, 'message': 'No CCU registered'}, status=404)

        ccu.breaker_threshold = threshold
        ccu.save(update_fields=['breaker_threshold'])

        return JsonResponse({
            'success': True,
            'message': f'Breaker threshold set to {threshold} mA',
            'threshold': threshold,
        })
    except Exception as e:
        return JsonResponse({'success': False, 'message': str(e)}, status=500)


@csrf_exempt
@require_http_methods(["GET"])
def export_for_sheets(request):
    """
    API endpoint for Google Apps Script to fetch sensor data for spreadsheet.
    URL: GET /api/export/sheets/?key=<SHEETS_API_KEY>&days=30

    Returns sensor data grouped by outlet + main breaker readings.
    Protected by a simple API key (no login required).
    """
    from django.conf import settings as django_settings

    # Validate API key
    api_key = request.GET.get('key', '')
    expected_key = getattr(django_settings, 'SHEETS_API_KEY', '')
    if not expected_key or api_key != expected_key:
        return JsonResponse({
            'success': False,
            'message': 'Invalid or missing API key'
        }, status=403)

    # Date range filter (default: last 30 days)
    try:
        days = int(request.GET.get('days', 30))
    except (ValueError, TypeError):
        days = 30
    cutoff = timezone.now() - timedelta(days=days)

    # --- Sensor Data per Outlet ---
    outlets = Outlet.objects.all()
    outlets_data = {}
    for outlet in outlets:
        readings = SensorData.objects.filter(
            outlet=outlet,
            timestamp__gte=cutoff
        ).order_by('timestamp')

        outlets_data[outlet.device_id] = {
            'name': outlet.name,
            'device_id': outlet.device_id,
            'threshold': outlet.threshold,
            'readings': [
                {
                    'timestamp': r.timestamp.astimezone(
                        timezone.get_current_timezone()
                    ).strftime('%Y-%m-%d %I:%M:%S %p'),
                    'current_a': r.current_a,
                    'current_b': r.current_b,
                    'threshold': outlet.threshold,
                    'is_overload': r.is_overload,
                }
                for r in readings
            ]
        }

    # --- Main Breaker Data ---
    breaker_readings = MainBreakerReading.objects.filter(
        timestamp__gte=cutoff
    ).order_by('timestamp')

    breaker_data = [
        {
            'timestamp': r.timestamp.astimezone(
                timezone.get_current_timezone()
            ).strftime('%Y-%m-%d %I:%M:%S %p'),
            'ccu_id': r.ccu_id,
            'current_ma': r.current_ma,
            'threshold': r.threshold,
        }
        for r in breaker_readings
    ]

    return JsonResponse({
        'success': True,
        'outlets': outlets_data,
        'breaker': breaker_data,
    })


# ═══════════════════════════════════════════════════════════
#   FOCUS DEVICE — Which outlet the user is currently viewing
# ═══════════════════════════════════════════════════════════

@csrf_exempt
@require_http_methods(["POST"])
def set_focus_device(request, device_id):
    """
    Set the focused device for the CCU.
    URL: POST /api/focus/<device_id>/
    Called when user expands a device card on the web dashboard.
    """
    device_id = device_id.upper()
    
    # Find the CCU (use first available — single-CCU setup)
    ccu = CentralControlUnit.objects.first()
    if not ccu:
        return JsonResponse({'success': False, 'message': 'No CCU registered'}, status=404)
    
    # Verify the outlet exists
    if not Outlet.objects.filter(device_id__iexact=device_id).exists():
        return JsonResponse({'success': False, 'message': f'Outlet {device_id} not found'}, status=404)
    
    ccu.focused_device = device_id
    ccu.save(update_fields=['focused_device'])
    
    return JsonResponse({'success': True, 'device_id': device_id})


@csrf_exempt
@require_http_methods(["POST", "DELETE"])
def clear_focus_device(request):
    """
    Clear the focused device (no outlet expanded).
    URL: POST /api/focus/clear/ or DELETE /api/focus/clear/
    Called when user collapses a device card.
    """
    ccu = CentralControlUnit.objects.first()
    if not ccu:
        return JsonResponse({'success': False, 'message': 'No CCU registered'}, status=404)
    
    ccu.focused_device = ''
    ccu.save(update_fields=['focused_device'])
    
    return JsonResponse({'success': True, 'device_id': None})


@csrf_exempt
@require_http_methods(["GET"])
def get_focus_device(request):
    """
    Get the currently focused device.
    URL: GET /api/focus/
    ESP32 polls this to know which device to read sensors for.
    """
    ccu = CentralControlUnit.objects.first()
    if not ccu:
        return JsonResponse({'success': True, 'device_id': None})
    
    focused = ccu.focused_device if ccu.focused_device else None
    return JsonResponse({'success': True, 'device_id': focused})