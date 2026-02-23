import json
from channels.generic.websocket import AsyncWebsocketConsumer
from channels.db import database_sync_to_async
from .models import Outlet, SensorData

class SensorDataConsumer(AsyncWebsocketConsumer):
    async def connect(self):
        self.outlet_id = self.scope['url_route']['kwargs']['outlet_id']
        self.room_group_name = f'sensor_{self.outlet_id}'
        
        # Join room group
        await self.channel_layer.group_add(
            self.room_group_name,
            self.channel_name
        )
        
        await self.accept()
        
        # Send initial data
        initial_data = await self.get_latest_sensor_data()
        await self.send(text_data=json.dumps({
            'type': 'initial_data',
            'data': initial_data
        }))
    
    async def disconnect(self, close_code):
        # Leave room group
        await self.channel_layer.group_discard(
            self.room_group_name,
            self.channel_name
        )
    
    async def receive(self, text_data):
        """Receive message from WebSocket"""
        data = json.loads(text_data)
        
        if data.get('type') == 'toggle_relay':
            socket = data.get('socket', 'a')
            await self.toggle_relay(socket)
    
    async def sensor_update(self, event):
        """Send sensor data to WebSocket"""
        await self.send(text_data=json.dumps({
            'type': 'sensor_data',
            'data': event['data']
        }))
    
    @database_sync_to_async
    def get_latest_sensor_data(self):
        try:
            outlet = Outlet.objects.get(device_id=self.outlet_id)
            latest = outlet.sensor_data.first()
            
            if latest:
                return {
                    'outlet_name': outlet.name,
                    'device_id': outlet.device_id,
                    'relay_a': outlet.relay_a,
                    'relay_b': outlet.relay_b,
                    'current_a': latest.current_a,
                    'current_b': latest.current_b,
                    'is_overload': latest.is_overload,
                    'timestamp': latest.timestamp.isoformat(),
                }
            return {
                'outlet_name': outlet.name,
                'device_id': outlet.device_id,
                'relay_a': outlet.relay_a,
                'relay_b': outlet.relay_b,
                'current_a': 0,
                'current_b': 0,
                'is_overload': False,
                'timestamp': None,
            }
        except Outlet.DoesNotExist:
            return None
    
    @database_sync_to_async
    def toggle_relay(self, socket):
        try:
            outlet = Outlet.objects.get(device_id=self.outlet_id)
            if socket == 'a':
                outlet.relay_a = not outlet.relay_a
            elif socket == 'b':
                outlet.relay_b = not outlet.relay_b
            outlet.save()
            return {'relay_a': outlet.relay_a, 'relay_b': outlet.relay_b}
        except Outlet.DoesNotExist:
            return None