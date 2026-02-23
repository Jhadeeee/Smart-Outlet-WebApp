from django.shortcuts import render, redirect, get_object_or_404
from django.contrib.auth import login, authenticate, logout
from django.contrib.auth.decorators import login_required
from django.contrib.auth.models import User
from django.contrib import messages
from django.http import JsonResponse
from .models import Outlet, SensorData, UserProfile, PendingCommand, MainBreakerReading

# ============ AUTHENTICATION VIEWS ============

def register_view(request):
    """User registration page"""
    if request.user.is_authenticated:
        return redirect('outlets:home')
    
    if request.method == 'POST':
        username = request.POST.get('username')
        email = request.POST.get('email', '')  # Optional, can be empty
        password = request.POST.get('password')
        password2 = request.POST.get('password2')
        first_name = request.POST.get('first_name', '')
        last_name = request.POST.get('last_name', '')
        country = request.POST.get('country', '')
        barangay = request.POST.get('barangay', '')
        
        # Validation
        if password != password2:
            messages.error(request, 'Passwords do not match!')
            return render(request, 'auth/register.html')
        
        if User.objects.filter(username=username).exists():
            messages.error(request, 'Username already exists!')
            return render(request, 'auth/register.html')
        
        # Email duplication is allowed (no check for duplicate emails)
        
        # Create user
        user = User.objects.create_user(
            username=username,
            email=email,  # Can be empty or duplicate
            password=password,
            first_name=first_name,
            last_name=last_name
        )
        
        # Update profile with country and barangay
        user.profile.country = country
        user.profile.barangay = barangay
        user.profile.save()
        
        messages.success(request, 'Account created successfully! Please login.')
        return redirect('outlets:login')
    
    return render(request, 'auth/register.html')


def login_view(request):
    """User login page"""
    if request.user.is_authenticated:
        return redirect('outlets:home')
    
    if request.method == 'POST':
        username = request.POST.get('username')
        password = request.POST.get('password')
        
        user = authenticate(request, username=username, password=password)
        
        if user is not None:
            login(request, user)
            messages.success(request, f'Welcome back, {user.first_name or user.username}!')
            return redirect('outlets:home')
        else:
            messages.error(request, 'Invalid username or password!')
    
    return render(request, 'auth/login.html')


def logout_view(request):
    """User logout"""
    logout(request)
    messages.success(request, 'You have been logged out successfully!')
    return redirect('outlets:login')


# ============ HOME PAGE / DASHBOARD ============

@login_required
def home_view(request):
    """Homepage — Main Dashboard with real outlet data"""
    outlets = Outlet.objects.filter(user=request.user)
    
    # Build outlet data with latest sensor readings
    outlet_data = []
    
    for outlet in outlets:
        latest = outlet.sensor_data.first()  # ordered by -timestamp
        cur_a = latest.current_a if latest else 0
        cur_b = latest.current_b if latest else 0
        is_overload = latest.is_overload if latest else False
        
        outlet_data.append({
            'outlet': outlet,
            'current_a': cur_a,
            'current_b': cur_b,
            'current_a_amps': round(cur_a / 1000.0, 2),
            'current_b_amps': round(cur_b / 1000.0, 2),
            'total_amps': round((cur_a + cur_b) / 1000.0, 2),
            'is_overload': is_overload,
            'has_data': latest is not None,
        })
    
    # Get total load from SCT-013 sensor (main breaker)
    # The CCU sends breaker readings with its ccu_id
    latest_breaker = MainBreakerReading.objects.first()  # ordered by -timestamp
    total_current = latest_breaker.current_ma if latest_breaker else 0
    
    active_count = sum(1 for o in outlets if o.relay_a or o.relay_b)
    inactive_count = len(outlet_data) - active_count
    
    context = {
        'user': request.user,
        'outlet_data': outlet_data,
        'total_current_amps': round(total_current / 1000.0, 2),
        'active_count': active_count,
        'inactive_count': inactive_count,
        'outlet_count': len(outlet_data),
    }
    return render(request, 'home.html', context)


@login_required
def dashboard(request):
    """Redirect dashboard to home"""
    return redirect('outlets:home')

@login_required
def outlet_detail(request, device_id):
    """Detail view for a specific outlet"""
    outlet = get_object_or_404(Outlet, device_id=device_id, user=request.user)
    latest_data = outlet.sensor_data.first()
    recent_data = outlet.sensor_data.all()[:50]
    
    context = {
        'outlet': outlet,
        'latest_data': latest_data,
        'recent_data': recent_data,
    }
    return render(request, 'outlets/outlet_detail.html', context)

@login_required
def toggle_outlet(request, device_id):
    """
    Toggle outlet relay ON/OFF.
    Expects POST with 'socket' param ('a' or 'b').
    Creates a PendingCommand for the CCU to pick up.
    """
    if request.method == 'POST':
        outlet = get_object_or_404(Outlet, device_id=device_id, user=request.user)
        socket = request.POST.get('socket', 'a').lower()
        
        if socket not in ('a', 'b'):
            return JsonResponse({'success': False, 'message': 'Invalid socket. Use "a" or "b".'})
        
        # Toggle the relay state in DB
        if socket == 'a':
            outlet.relay_a = not outlet.relay_a
            new_state = outlet.relay_a
        else:
            outlet.relay_b = not outlet.relay_b
            new_state = outlet.relay_b
        outlet.save()
        
        # Queue command for CCU to pick up
        PendingCommand.objects.create(
            outlet=outlet,
            command='relay_on' if new_state else 'relay_off',
            socket=socket,
        )
        
        return JsonResponse({
            'success': True,
            'relay_a': outlet.relay_a,
            'relay_b': outlet.relay_b,
            'message': f'Socket {socket.upper()} turned {"ON" if new_state else "OFF"}'
        })
    
    return JsonResponse({'success': False, 'message': 'Invalid request method'})