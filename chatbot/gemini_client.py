import google.generativeai as genai
from decouple import config
from pathlib import Path

class GeminiClient:
    def __init__(self):
        api_key = config('GEMINI_API_KEY', default='')
        if not api_key or api_key == 'your_gemini_api_key_here':
            raise ValueError("Gemini API key not configured. Please set GEMINI_API_KEY in your .env file.")
        
        genai.configure(api_key=api_key)
        self.model = genai.GenerativeModel('gemini-2.5-flash')
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
            error_msg = str(e)
            if '429' in error_msg:
                return ("⏳ **Rate limit reached!** The free tier allows a limited number of requests per minute.\n\n"
                        "Please wait **~60 seconds** and try again. This is a Google API limit, not a bug.\n\n"
                        "💡 *Tip: The free tier allows ~15 requests/minute and ~1,500 requests/day.*")
            return f"Sorry, I encountered an error: {error_msg}"