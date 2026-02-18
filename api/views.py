from django.http import JsonResponse
from django.views.decorators.csrf import csrf_exempt
from django.views.decorators.http import require_http_methods
from outlets.models import Outlet, SensorData, EventLogTest
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


# ============ EVENT LOG ENDPOINTS (Experimental) ============

@csrf_exempt
@require_http_methods(["POST"])
def receive_event_log(request):
    """
    API endpoint for ESP32 to send event logs.
    Only important events (overload, cutoff, power on/off) should be sent here.
    
    Expected JSON format:
    {
        "device_id": "SO-001",
        "event_type": "overload",
        "severity": "critical",
        "message": "Total current 5500 mA exceeds threshold 5000 mA",
        "socket_label": "A",
        "current_reading": 5500
    }
    """
    try:
        data = json.loads(request.body)
        
        # Validate required fields
        required_fields = ['device_id', 'event_type']
        if not all(field in data for field in required_fields):
            return JsonResponse({
                'success': False,
                'message': 'Missing required fields: device_id, event_type'
            }, status=400)
        
        # Validate event_type
        valid_event_types = [t[0] for t in EventLogTest.EVENT_TYPES]
        if data['event_type'] not in valid_event_types:
            return JsonResponse({
                'success': False,
                'message': f'Invalid event_type. Must be one of: {valid_event_types}'
            }, status=400)
        
        # Find the outlet
        try:
            outlet = Outlet.objects.get(device_id=data['device_id'])
        except Outlet.DoesNotExist:
            return JsonResponse({
                'success': False,
                'message': f'Outlet with device_id {data["device_id"]} not found'
            }, status=404)
        
        # Create event log
        event_log = EventLogTest.objects.create(
            outlet=outlet,
            event_type=data['event_type'],
            severity=data.get('severity', 'info'),
            message=data.get('message', ''),
            socket_label=data.get('socket_label', ''),
            current_reading=float(data['current_reading']) if data.get('current_reading') is not None else None,
        )
        
        return JsonResponse({
            'success': True,
            'message': 'Event logged successfully',
            'event_id': event_log.id,
            'timestamp': event_log.timestamp.isoformat()
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
def get_event_logs(request, device_id):
    """
    API endpoint to retrieve event logs for a specific outlet.
    
    Query params:
        ?event_type=overload    (filter by event type)
        ?severity=critical      (filter by severity)
        ?limit=50               (limit number of results, default 50)
    """
    try:
        outlet = Outlet.objects.get(device_id=device_id)
        
        # Start with all logs for this outlet
        logs = EventLogTest.objects.filter(outlet=outlet)
        
        # Apply filters from query params
        event_type = request.GET.get('event_type')
        if event_type:
            logs = logs.filter(event_type=event_type)
        
        severity = request.GET.get('severity')
        if severity:
            logs = logs.filter(severity=severity)
        
        # Limit results
        limit = int(request.GET.get('limit', 50))
        logs = logs[:limit]
        
        # Serialize
        data = []
        for log in logs:
            data.append({
                'id': log.id,
                'event_type': log.event_type,
                'event_type_display': log.get_event_type_display(),
                'severity': log.severity,
                'message': log.message,
                'socket_label': log.socket_label,
                'current_reading': log.current_reading,
                'timestamp': log.timestamp.isoformat(),
            })
        
        return JsonResponse({
            'success': True,
            'device_id': device_id,
            'outlet_name': outlet.name,
            'count': len(data),
            'logs': data
        })
        
    except Outlet.DoesNotExist:
        return JsonResponse({
            'success': False,
            'message': f'Outlet with device_id {device_id} not found'
        }, status=404)