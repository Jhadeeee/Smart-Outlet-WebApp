import google.generativeai as genai
from decouple import config
from outlets.models import SensorData

class GeminiClient:
    def __init__(self):
        genai.configure(api_key=config('GEMINI_API_KEY'))
        self.model = genai.GenerativeModel('gemini-pro')
    
    def get_response(self, user_message, outlets):
        """
        Get AI response from Gemini based on user message and outlet context
        """
        # Build context from user's outlets
        context = self._build_context(outlets)
        
        # Create prompt with context
        prompt = f"""You are a helpful assistant for a smart outlet monitoring system. 
        
User's Smart Outlets:
{context}

User Question: {user_message}

Please provide a helpful and informative response. If the user asks about their energy consumption, 
power usage, or outlet status, refer to the data above. If you need more specific information, 
ask clarifying questions."""
        
        try:
            response = self.model.generate_content(prompt)
            return response.text
        except Exception as e:
            return f"Sorry, I encountered an error: {str(e)}"
    
    def _build_context(self, outlets):
        """Build context string from user's outlets"""
        if not outlets.exists():
            return "No outlets registered yet."
        
        context_lines = []
        for outlet in outlets:
            latest_data = outlet.sensor_data.first()
            
            if latest_data:
                context_lines.append(
                    f"- {outlet.name} ({outlet.location}): "
                    f"Status: {'ON' if outlet.is_active else 'OFF'}, "
                    f"Power: {latest_data.power}W, "
                    f"Voltage: {latest_data.voltage}V, "
                    f"Current: {latest_data.current}A, "
                    f"Energy: {latest_data.energy}kWh, "
                    f"Temperature: {latest_data.temperature}°C"
                )
            else:
                context_lines.append(
                    f"- {outlet.name} ({outlet.location}): "
                    f"Status: {'ON' if outlet.is_active else 'OFF'}, "
                    f"No sensor data available yet."
                )
        
        return "\n".join(context_lines)