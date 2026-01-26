import json
from channels.generic.websocket import AsyncWebsocketConsumer

class ChatConsumer(AsyncWebsocketConsumer):
    async def connect(self):
        await self.accept()
        await self.send(text_data=json.dumps({
            'type': 'connection',
            'message': 'Connected to chat'
        }))
    
    async def disconnect(self, close_code):
        pass
    
    async def receive(self, text_data):
        """Receive message from WebSocket (optional for real-time chat)"""
        data = json.loads(text_data)
        message = data.get('message', '')
        
        # Echo back for now (can be enhanced later)
        await self.send(text_data=json.dumps({
            'type': 'message',
            'message': f'Received: {message}'
        }))