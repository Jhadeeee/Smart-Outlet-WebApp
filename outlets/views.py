from django.shortcuts import render, redirect, get_object_or_404
from django.contrib.auth import login, authenticate, logout
from django.contrib.auth.decorators import login_required
from django.contrib.auth.models import User
from django.contrib import messages
from django.http import JsonResponse
from django.views.decorators.http import require_http_methods
from django.views.decorators.csrf import csrf_exempt
import json
from .models import Outlet, SensorData, UserProfile, TestTelemetryLog, TestEventLog, PendingCommand

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


# ==========================================
# TESTING & CALIBRATION DASHBOARD
# ==========================================

from .models import TestTelemetryLog, TestEventLog

def test_dashboard_view(request):
    """
    Public bypassing dashboard for testing and calibration.
    Displays live telemetry and event logs without requiring login.
    """
    # Get latest 50 records of each
    telemetry_logs = TestTelemetryLog.objects.all()[:50]
    event_logs = TestEventLog.objects.all()[:50]
    
    # Get the absolute most recent telemetry data to populate the visual controls
    latest_telemetry = TestTelemetryLog.objects.first()
    
    # Extract all active devices (they will share the most recent timestamp in the payload)
    active_devices = []
    if latest_telemetry:
        active_devices = TestTelemetryLog.objects.filter(timestamp=latest_telemetry.timestamp)
    
    context = {
        'telemetry_logs': telemetry_logs,
        'event_logs': event_logs,
        'latest_telemetry': latest_telemetry,
        'active_devices': active_devices
    }
    return render(request, 'outlets/test_dashboard.html', context)

@require_http_methods(["POST"])
def clear_test_logs(request):
    """
    Clears all telemetry and event logs from the database.
    Used for 'academic logging' cleanup before a new test run.
    """
    try:
        TestTelemetryLog.objects.all().delete()
        TestEventLog.objects.all().delete()
        
        TestEventLog.objects.create(
            source='WEB_DASHBOARD',
            action_type='SYSTEM_CLEARED',
            target_device='ALL',
            details='All testing logs were manually cleared by the user.'
        )
        
        return JsonResponse({'status': 'success', 'message': 'Logs cleared successfully.'})
    except Exception as e:
        return JsonResponse({'status': 'error', 'message': str(e)}, status=500)

@csrf_exempt
@require_http_methods(["POST"])
def receive_test_log(request):
    """
    Receives JSON telemetry payloads from the ESP32 CCU.
    Stores each device as a separate row in TestTelemetryLog with the same timestamp.
    """
    try:
        data = json.loads(request.body)
        ccu_data = data.get('ccu', {})
        devices = data.get('devices', [])
        
        main_mA = ccu_data.get('main_breaker_mA', 0)
        main_limit = ccu_data.get('main_breaker_limit_mA', 15000)
        
        if not devices:
            # Log just the CCU state if no devices connected
            TestTelemetryLog.objects.create(
                main_breaker_mA=main_mA,
                main_breaker_limit_mA=main_limit,
                device_id="NONE",
                device_limit_mA=0,
                socket_a_state=False, socket_a_mA=0,
                socket_b_state=False, socket_b_mA=0
            )
        else:
            # Log each device
            for dev in devices:
                socket_a = dev.get('socket_a', {})
                socket_b = dev.get('socket_b', {})
                
                TestTelemetryLog.objects.create(
                    main_breaker_mA=main_mA,
                    main_breaker_limit_mA=main_limit,
                    device_id=dev.get('id', '??'),
                    device_limit_mA=dev.get('limit_mA', 5000),
                    socket_a_state=bool(socket_a.get('state', 0)),
                    socket_a_mA=socket_a.get('mA', 0),
                    socket_b_state=bool(socket_b.get('state', 0)),
                    socket_b_mA=socket_b.get('mA', 0)
                )
                
        # Check if there are any pending commands for the ESP32
        pending = PendingCommand.objects.filter(is_delivered=False).order_by('created_at').first()
        
        response_data = {
            'status': 'success',
            'pending_command': 'NONE'
        }
        
        if pending:
            response_data['pending_command'] = {
                'id': pending.id,
                'command': pending.command_type,
                'target': pending.target_id,
                'payload': pending.payload
            }
            # Mark as delivered (assume it will be picked up)
            pending.is_delivered = True
            pending.delivered_at = timezone.now()
            pending.save()
            
            TestEventLog.objects.create(
                action_type="ACK_RECEIVED",
                source="WEB_DASHBOARD",
                target_device=pending.target_id or "ALL",
                details=f"Sent {pending.command_type} to ESP32: {pending.target_id} {pending.payload}"
            )
            
        return JsonResponse(response_data)
        
    except json.JSONDecodeError:
        return JsonResponse({'status': 'error', 'message': 'Invalid JSON'}, status=400)
    except Exception as e:
        return JsonResponse({'status': 'error', 'message': str(e)}, status=500)

@csrf_exempt
@require_http_methods(["POST"])
def enqueue_command(request):
    """
    Called by the web frontend via AJAX when the user clicks 'Add Device' or 'Set'.
    Adds a command to the queue for the ESP32 to pick up on its next heartbeat.
    """
    try:
        data = json.loads(request.body)
        cmd_type = data.get('command')
        target = data.get('target', None)
        payload = data.get('payload', {})
        
        if not cmd_type:
            return JsonResponse({'status': 'error', 'message': 'Missing command type'}, status=400)
            
        # Create the pending command
        PendingCommand.objects.create(
            command_type=cmd_type,
            target_id=target,
            payload=payload
        )
        
        # Map command types to their corresponding Action Types for the log
        action_mapping = {
            "CMD_ADD_DEVICE": "ADD_DEVICE",
            "CMD_SET_ID_MASTER": "SET_MASTER_ID",
            "CMD_SET_LIMIT": "SET_THRESHOLD",
            "CMD_CUT_POWER": "TOGGLE_RELAY"
        }
        action = action_mapping.get(cmd_type, "SET_THRESHOLD")
        
        TestEventLog.objects.create(
            action_type=action,
            source="WEB_DASHBOARD",
            target_device=target or "ALL",
            details=f"Queued {cmd_type} for ESP32 delivery"
        )
        
        return JsonResponse({'status': 'success', 'message': 'Command added to queue.'})
        
    except Exception as e:
        return JsonResponse({'status': 'error', 'message': str(e)}, status=500)
