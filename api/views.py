from django.http import JsonResponse
from django.views.decorators.csrf import csrf_exempt
from django.views.decorators.http import require_http_methods
from django.utils import timezone
from datetime import timedelta
from outlets.models import Outlet, SensorData, Alert, PendingCommand, MainBreakerReading, CentralControlUnit, EventLog
from channels.layers import get_channel_layer
from asgiref.sync import async_to_sync
import json

# How often to persist sensor data to the database (in minutes).
# Data is always pushed to WebSocket for real-time UI updates.
DB_LOG_INTERVAL = timedelta(seconds=30)

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
        
        # Parse values
        is_overload = data.get('is_overload', False)
        current_a = int(data['current_a'])
        current_b = int(data['current_b'])
        
        if current_a == 65535 or current_b == 65535:
            is_overload = True
        
        now = timezone.now()
        
        # Update outlet relay states immediately (always)
        relay_updated = False
        if 'relay_a' in data:
            outlet.relay_a = bool(data['relay_a'])
            relay_updated = True
        if 'relay_b' in data:
            outlet.relay_b = bool(data['relay_b'])
            relay_updated = True
        if relay_updated:
            outlet.save()
        
        # Alerts always fire immediately (critical events)
        if is_overload:
            Alert.objects.create(
                outlet=outlet,
                alert_type='overload',
                message=f'Overload trip detected! Current A: {current_a}mA, Current B: {current_b}mA'
            )
            EventLog.objects.create(
                user=outlet.user,
                source='PIC_HARDWARE',
                action_type='OVERLOAD_TRIPPED',
                target_device=f'0x{outlet.device_id}',
                details=f'Overload trip on {outlet.name}! Socket A: {current_a}mA, Socket B: {current_b}mA. Relay auto-cutoff triggered.'
            )
        
        if outlet.threshold > 0 and (current_a > outlet.threshold or current_b > outlet.threshold):
            Alert.objects.create(
                outlet=outlet,
                alert_type='threshold',
                message=f'Threshold ({outlet.threshold}mA) exceeded! A: {current_a}mA, B: {current_b}mA'
            )
            EventLog.objects.create(
                user=outlet.user,
                source='SERVER',
                action_type='THRESHOLD_EXCEEDED',
                target_device=f'0x{outlet.device_id}',
                details=f'Threshold ({outlet.threshold}mA) exceeded on {outlet.name}! Socket A: {current_a}mA, Socket B: {current_b}mA'
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
        current_ma = int(data['current_ma'])
        now = timezone.now()
        
        # Look up registered CCU (if exists)
        ccu_obj = CentralControlUnit.objects.filter(ccu_id=ccu_id).first()
        
        # DB write: only persist every DB_LOG_INTERVAL
        saved_to_db = False
        last_entry = MainBreakerReading.objects.filter(ccu_id=ccu_id).first()
        if not last_entry or (now - last_entry.timestamp) >= DB_LOG_INTERVAL:
            MainBreakerReading.objects.create(
                ccu_id=ccu_id,
                ccu_device=ccu_obj,
                current_ma=current_ma,
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
                
        # Optimization: Clear any existing unexecuted identical commands to prevent queue bloat
        PendingCommand.objects.filter(
            outlet=outlet, 
            command=command, 
            socket=socket, 
            is_executed=False
        ).delete()
        
        # Create the new pending command
        PendingCommand.objects.create(
            outlet=outlet,
            command=command,
            socket=socket,
            value=value
        )
        
        # If toggling relay, optionally update model speculatively
        # so the UI gets instant feedback on refresh before polling catches up
        if command == 'relay_on' or command == 'relay_off':
            state = True if command == 'relay_on' else False
            state_label = 'ON' if state else 'OFF'
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
                details=f'Socket {socket.upper()} turned {state_label} on {outlet.name}'
            )
                
        elif command == 'set_threshold' and value is not None:
            outlet.threshold = value
            outlet.save(update_fields=['threshold', 'updated_at'])
            
            EventLog.objects.create(
                user=request.user,
                source='WEB_DASHBOARD',
                action_type='SET_THRESHOLD',
                target_device=f'0x{outlet.device_id}',
                details=f'Threshold set to {value}mA on {outlet.name}'
            )
            
        return JsonResponse({'success': True, 'message': f'Command {command} queued successfully'})
        
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