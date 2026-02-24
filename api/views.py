from django.http import JsonResponse
from django.views.decorators.csrf import csrf_exempt
from django.views.decorators.http import require_http_methods
from outlets.models import Outlet, SensorData, TestTelemetryLog
from channels.layers import get_channel_layer
from asgiref.sync import async_to_sync
import json

@csrf_exempt
@require_http_methods(["POST"])
def receive_sensor_data(request):
    """
    API endpoint for ESP32 to send sensor data
    
    Expected JSON format:
    {
        "device_id": "ESP32_001",
        "voltage": 230.5,
        "current": 2.3,
        "power": 500.5,
        "energy": 1.25,
        "temperature": 35.2
    }
    """
    try:
        data = json.loads(request.body)
        
        # Validate required fields
        required_fields = ['device_id', 'voltage', 'current', 'power']
        if not all(field in data for field in required_fields):
            return JsonResponse({
                'success': False,
                'message': 'Missing required fields'
            }, status=400)
        
        # Get or create outlet
        try:
            outlet = Outlet.objects.get(device_id=data['device_id'])
        except Outlet.DoesNotExist:
            return JsonResponse({
                'success': False,
                'message': f'Outlet with device_id {data["device_id"]} not found'
            }, status=404)
        
        # Save sensor data
        sensor_data = SensorData.objects.create(
            outlet=outlet,
            voltage=float(data['voltage']),
            current=float(data['current']),
            power=float(data['power']),
            energy=float(data.get('energy', 0)),
            temperature=float(data.get('temperature')) if data.get('temperature') else None
        )
        
        # Send data via WebSocket to connected clients
        channel_layer = get_channel_layer()
        async_to_sync(channel_layer.group_send)(
            f'sensor_{outlet.device_id}',
            {
                'type': 'sensor_update',
                'data': {
                    'outlet_name': outlet.name,
                    'is_active': outlet.is_active,
                    'voltage': sensor_data.voltage,
                    'current': sensor_data.current,
                    'power': sensor_data.power,
                    'energy': sensor_data.energy,
                    'temperature': sensor_data.temperature,
                    'timestamp': sensor_data.timestamp.isoformat(),
                }
            }
        )
        
        return JsonResponse({
            'success': True,
            'message': 'Data received successfully',
            'outlet_status': outlet.is_active
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
    API endpoint for ESP32 to get outlet ON/OFF status
    """
    try:
        outlet = Outlet.objects.get(device_id=device_id)
        
        return JsonResponse({
            'success': True,
            'device_id': outlet.device_id,
            'is_active': outlet.is_active,
            'name': outlet.name
        })
        
    except Outlet.DoesNotExist:
        return JsonResponse({
            'success': False,
            'message': f'Outlet with device_id {device_id} not found'
        }, status=404)

# ==========================================
# TESTING & CALIBRATION DASHBOARD API
# ==========================================

@csrf_exempt
@require_http_methods(["POST"])
def receive_test_telemetry(request):
    """
    API endpoint for the Testing & Calibration Dashboard.
    Receives an aggregated JSON payload every ~10s from the ESP32 CCU.
    """
    try:
        payload = json.loads(request.body)
        
        # 1. Extract CCU Main Breaker readings
        ccu_data = payload.get('ccu', {})
        main_mA = ccu_data.get('main_breaker_mA', 0)
        main_limit = ccu_data.get('main_breaker_limit_mA', 15000)
        
        # 2. Extract Device array
        devices_data = payload.get('devices', [])
        
        # 3. Create a log entry for every connected device
        for dev in devices_data:
            sock_a = dev.get('socket_a', {})
            sock_b = dev.get('socket_b', {})
            
            TestTelemetryLog.objects.create(
                main_breaker_mA=main_mA,
                main_breaker_limit_mA=main_limit,
                device_id=dev.get('id', 'UNKNOWN'),
                device_limit_mA=dev.get('limit_mA', 5000),
                socket_a_state=bool(sock_a.get('state', 0)),
                socket_a_mA=sock_a.get('mA', -1),
                socket_b_state=bool(sock_b.get('state', 0)),
                socket_b_mA=sock_b.get('mA', -1)
            )
            
        # 4. In the future, check cache or queue for pending commands to return
        return JsonResponse({
            'status': 'success',
            'pending_command': None,
            'target': None,
            'socket': None
        })
        
    except json.JSONDecodeError:
        return JsonResponse({'status': 'error', 'message': 'Invalid JSON format'}, status=400)
    except Exception as e:
        return JsonResponse({'status': 'error', 'message': str(e)}, status=500)
