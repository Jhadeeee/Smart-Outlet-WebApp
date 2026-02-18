from django.shortcuts import render, redirect, get_object_or_404
from django.contrib.auth import login, authenticate, logout
from django.contrib.auth.decorators import login_required
from django.contrib.auth.models import User
from django.contrib import messages
from django.http import JsonResponse
from .models import Outlet, SensorData, UserProfile, EventLogTest

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


# ============ HOME PAGE ============

@login_required
def home_view(request):
    """Homepage - Welcome message"""
    context = {
        'user': request.user,
    }
    return render(request, 'home.html', context)


@login_required
def data_logs_view(request):
    """Data Logs page — shows EventLogTest entries in a filterable table"""
    user_outlets = Outlet.objects.filter(user=request.user)
    
    # Start with all logs for user's outlets
    logs = EventLogTest.objects.filter(outlet__in=user_outlets)
    
    # Apply filters from query params
    device_filter = request.GET.get('device', '')
    if device_filter:
        logs = logs.filter(outlet__device_id=device_filter)
    
    event_filter = request.GET.get('event_type', '')
    if event_filter:
        logs = logs.filter(event_type=event_filter)
    
    severity_filter = request.GET.get('severity', '')
    if severity_filter:
        logs = logs.filter(severity=severity_filter)
    
    # Limit to latest 100
    logs = logs.select_related('outlet')[:100]
    
    context = {
        'logs': logs,
        'outlets': user_outlets,
        'event_types': EventLogTest.EVENT_TYPES,
        'severity_levels': EventLogTest.SEVERITY_LEVELS,
        'active_filters': {
            'device': device_filter,
            'event_type': event_filter,
            'severity': severity_filter,
        },
    }
    return render(request, 'data_logs.html', context)


# ============ DASHBOARD (for later) ============

@login_required
def dashboard(request):
    """Main dashboard showing all outlets"""
    outlets = Outlet.objects.filter(user=request.user)
    
    context = {
        'outlets': outlets,
    }
    return render(request, 'index.html', context)

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
    """Toggle outlet ON/OFF status"""
    if request.method == 'POST':
        outlet = get_object_or_404(Outlet, device_id=device_id, user=request.user)
        outlet.is_active = not outlet.is_active
        outlet.save()
        
        return JsonResponse({
            'success': True,
            'is_active': outlet.is_active,
            'message': f'Outlet turned {"ON" if outlet.is_active else "OFF"}'
        })
    
    return JsonResponse({'success': False, 'message': 'Invalid request method'})