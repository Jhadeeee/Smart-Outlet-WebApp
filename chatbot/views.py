from django.shortcuts import render
from django.contrib.auth.decorators import login_required
from django.http import JsonResponse
from django.views.decorators.http import require_http_methods
from .gemini_client import GeminiClient
import json

@login_required
def chat_page(request):
    """Chatbot interface page"""
    return render(request, 'chatbot/chat.html')

@login_required
@require_http_methods(["POST"])
def send_message(request):
    """Handle chatbot message and return AI response"""
    try:
        data = json.loads(request.body)
        user_message = data.get('message', '')
        
        if not user_message:
            return JsonResponse({
                'success': False,
                'message': 'No message provided'
            }, status=400)
        
        # Initialize Gemini client and get response
        gemini = GeminiClient()
        ai_response = gemini.get_response(user_message, user=request.user)
        
        return JsonResponse({
            'success': True,
            'response': ai_response
        })
        
    except ValueError as e:
        # API key not configured
        return JsonResponse({
            'success': False,
            'message': str(e)
        }, status=500)
    except Exception as e:
        return JsonResponse({
            'success': False,
            'message': f'An error occurred: {str(e)}'
        }, status=500)