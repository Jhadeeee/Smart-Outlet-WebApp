import google.generativeai as genai
from decouple import config
from pathlib import Path

class GeminiClient:
    def __init__(self):
        api_key = config('GEMINI_API_KEY', default='')
        if not api_key or api_key == 'your_gemini_api_key_here':
            raise ValueError("Gemini API key not configured. Please set GEMINI_API_KEY in your .env file.")
        
        genai.configure(api_key=api_key)
        self.model = genai.GenerativeModel('gemini-2.0-flash')
        self.system_prompt = self._load_system_prompt()
    
    def _load_system_prompt(self):
        """Load system prompt from the prompts directory"""
        prompt_path = Path(__file__).parent / 'prompts' / 'system_prompt.txt'
        try:
            with open(prompt_path, 'r', encoding='utf-8') as f:
                return f.read()
        except FileNotFoundError:
            return "You are a helpful assistant for a smart outlet monitoring system."
    
    def get_response(self, user_message, user=None):
        """
        Get AI response from Gemini based on user message and system prompt
        """
        # Build the full prompt with system instructions
        prompt = f"""{self.system_prompt}

---

User: {user_message}

Please respond helpfully based on the instructions above."""
        
        try:
            response = self.model.generate_content(prompt)
            return response.text
        except Exception as e:
            return f"Sorry, I encountered an error: {str(e)}"