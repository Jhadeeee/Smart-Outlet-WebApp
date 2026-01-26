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
        
        if data.get('type') == 'toggle_outlet':
            await self.toggle_outlet_status()
    
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
                    'is_active': outlet.is_active,
                    'voltage': latest.voltage,
                    'current': latest.current,
                    'power': latest.power,
                    'energy': latest.energy,
                    'temperature': latest.temperature,
                    'timestamp': latest.timestamp.isoformat(),
                }
            return None
        except Outlet.DoesNotExist:
            return None
    
    @database_sync_to_async
    def toggle_outlet_status(self):
        try:
            outlet = Outlet.objects.get(device_id=self.outlet_id)
            outlet.is_active = not outlet.is_active
            outlet.save()
            return outlet.is_active
        except Outlet.DoesNotExist:
            return None