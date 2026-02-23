from django.http import JsonResponse
from django.views.decorators.csrf import csrf_exempt
from django.views.decorators.http import require_http_methods
from outlets.models import Outlet, SensorData, Alert, PendingCommand, MainBreakerReading
from channels.layers import get_channel_layer
from asgiref.sync import async_to_sync
import json

@csrf_exempt
@require_http_methods(["POST"])
def receive_sensor_data(request):
    """
    API endpoint for CCU (ESP32) to send sensor data.
    URL: POST /api/data/
    
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
        
        # Check for overload (0xFFFF = 65535 mA)
        is_overload = data.get('is_overload', False)
        current_a = int(data['current_a'])
        current_b = int(data['current_b'])
        
        if current_a == 65535 or current_b == 65535:
            is_overload = True
        
        # Save sensor data
        sensor_data = SensorData.objects.create(
            outlet=outlet,
            current_a=current_a,
            current_b=current_b,
            is_overload=is_overload,
        )
        
        # Update outlet relay states if provided
        relay_updated = False
        if 'relay_a' in data:
            outlet.relay_a = bool(data['relay_a'])
            relay_updated = True
        if 'relay_b' in data:
            outlet.relay_b = bool(data['relay_b'])
            relay_updated = True
        if relay_updated:
            outlet.save()
        
        # Create alert on overload
        if is_overload:
            Alert.objects.create(
                outlet=outlet,
                alert_type='overload',
                message=f'Overload trip detected! Current A: {current_a}mA, Current B: {current_b}mA'
            )
        
        # Create alert if threshold exceeded
        if outlet.threshold > 0 and (current_a > outlet.threshold or current_b > outlet.threshold):
            Alert.objects.create(
                outlet=outlet,
                alert_type='threshold',
                message=f'Threshold ({outlet.threshold}mA) exceeded! A: {current_a}mA, B: {current_b}mA'
            )
        
        # Send real-time update via WebSocket
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
                        'current_a': sensor_data.current_a,
                        'current_b': sensor_data.current_b,
                        'is_overload': sensor_data.is_overload,
                        'timestamp': sensor_data.timestamp.isoformat(),
                    }
                }
            )
        except Exception:
            # WebSocket push is best-effort; don't fail the API call
            pass
        
        return JsonResponse({
            'success': True,
            'message': 'Data received successfully',
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
        
        ccu_id = str(data['ccu_id']).upper()
        current_ma = int(data['current_ma'])
        
        # Save breaker reading
        reading = MainBreakerReading.objects.create(
            ccu_id=ccu_id,
            current_ma=current_ma,
        )
        
        # Send real-time update via WebSocket
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
                        'timestamp': reading.timestamp.isoformat(),
                    }
                }
            )
        except Exception:
            pass  # WebSocket is best-effort
        
        return JsonResponse({
            'success': True,
            'message': 'Breaker data received successfully',
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