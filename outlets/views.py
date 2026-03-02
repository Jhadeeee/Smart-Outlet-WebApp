from django.shortcuts import render, redirect, get_object_or_404
from django.contrib.auth import login, authenticate, logout
from django.contrib.auth.decorators import login_required
from django.contrib.auth.models import User
from django.contrib import messages
from django.http import JsonResponse
from .models import Outlet, SensorData, UserProfile, PendingCommand, MainBreakerReading, CentralControlUnit, EventLog

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
    
    # Get user's registered CCUs
    user_ccus = CentralControlUnit.objects.filter(user=request.user)
    user_ccu_ids = list(user_ccus.values_list('ccu_id', flat=True))
    
    # Get total load from SCT-013 sensor (main breaker) — filtered by user's CCUs
    total_current = 0
    first_ccu_id = ''
    if user_ccu_ids:
        latest_breaker = MainBreakerReading.objects.filter(ccu_id__in=user_ccu_ids).first()
        total_current = latest_breaker.current_ma if latest_breaker else 0
        first_ccu_id = user_ccu_ids[0]
    
    active_count = sum(1 for o in outlets if o.relay_a or o.relay_b)
    inactive_count = len(outlet_data) - active_count
    
    context = {
        'user': request.user,
        'outlet_data': outlet_data,
        'total_current_amps': round(total_current / 1000.0, 2),
        'user_ccus': user_ccus,
        'ccu_id': first_ccu_id,
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


@login_required
def add_outlet(request):
    """Register a new smart outlet device."""
    if request.method == 'POST':
        name = request.POST.get('name', '').strip()
        device_id = request.POST.get('device_id', '').strip().upper()
        location = request.POST.get('location', '').strip()

        if not name or not device_id:
            messages.error(request, 'Name and Device ID are required.')
            return redirect('outlets:home')

        # Validate hex ID (1-2 hex chars)
        try:
            int(device_id, 16)
        except ValueError:
            messages.error(request, 'Device ID must be a valid hex value (e.g. FE).')
            return redirect('outlets:home')

        # Check for duplicate device_id
        if Outlet.objects.filter(device_id=device_id).exists():
            messages.error(request, f'Device ID "{device_id}" is already registered.')
            return redirect('outlets:home')

        Outlet.objects.create(
            user=request.user,
            name=name,
            device_id=device_id,
            location=location,
        )
        messages.success(request, f'Outlet "{name}" (0x{device_id}) added successfully!')
        return redirect('outlets:home')

    return redirect('outlets:home')


@login_required
def delete_outlet(request, device_id):
    """Remove a registered smart outlet."""
    if request.method == 'POST':
        outlet = get_object_or_404(Outlet, device_id=device_id, user=request.user)
        name = outlet.name
        outlet.delete()
        messages.success(request, f'Outlet "{name}" removed.')
        return redirect('outlets:home')

    return redirect('outlets:home')


# ============ CCU MANAGEMENT ============

@login_required
def add_ccu(request):
    """Register a new Central Control Unit (ESP32) to the user."""
    if request.method == 'POST':
        ccu_id = request.POST.get('ccu_id', '').strip().upper()
        name = request.POST.get('name', '').strip() or 'My CCU'
        location = request.POST.get('location', '').strip()

        if not ccu_id:
            messages.error(request, 'CCU ID is required.')
            return redirect('outlets:home')

        # Check for duplicate ccu_id
        if CentralControlUnit.objects.filter(ccu_id=ccu_id).exists():
            messages.error(request, f'CCU ID "{ccu_id}" is already registered.')
            return redirect('outlets:home')

        CentralControlUnit.objects.create(
            user=request.user,
            ccu_id=ccu_id,
            name=name,
            location=location,
        )
        messages.success(request, f'CCU "{name}" (ID: {ccu_id}) registered successfully!')
        return redirect('outlets:home')

    return redirect('outlets:home')


@login_required
def delete_ccu(request, ccu_id):
    """Remove a registered Central Control Unit."""
    if request.method == 'POST':
        ccu = get_object_or_404(CentralControlUnit, ccu_id=ccu_id, user=request.user)
        name = ccu.name
        ccu.delete()
        messages.success(request, f'CCU "{name}" removed.')
        return redirect('outlets:home')

    return redirect('outlets:home')


# ============ EVENT HISTORY ============

@login_required
def event_history_view(request):
    """Event History page — shows all logged events for the current user."""
    filter_type = request.GET.get('type', '')
    
    events = EventLog.objects.filter(user=request.user)
    
    if filter_type:
        events = events.filter(action_type=filter_type)
    
    # Limit to latest 200 events for performance
    events = events[:200]
    
    context = {
        'events': events,
        'filter_type': filter_type,
        'action_choices': EventLog.ACTION_CHOICES,
        'user': request.user,
    }
    return render(request, 'event_history.html', context)