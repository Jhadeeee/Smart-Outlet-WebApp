from django.urls import path
from . import views

app_name = 'outlets'

urlpatterns = [
    # Authentication URLs
    path('register/', views.register_view, name='register'),
    path('login/', views.login_view, name='login'),
    path('logout/', views.logout_view, name='logout'),
    
    # Home page
    path('', views.home_view, name='home'),
    
    # Dashboard (for later use)
    path('dashboard/', views.dashboard, name='dashboard'),
    path('outlet/<str:device_id>/', views.outlet_detail, name='outlet_detail'),
    path('outlet/<str:device_id>/toggle/', views.toggle_outlet, name='toggle_outlet'),
    
    # Testing & Calibration Dashboard (Bypass Login)
    path('test-dashboard/', views.test_dashboard_view, name='test_dashboard'),
    path('api/test-log/clear/', views.clear_test_logs, name='clear_test_logs'),
    path('api/test-log/', views.receive_test_log, name='receive_test_log'),
    path('api/test-command/', views.enqueue_command, name='enqueue_command'),
    path('api/test-command/fetch/', views.fetch_pending_commands, name='fetch_pending_commands'),
]