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

    # Outlet management — 'add' must come before '<str:device_id>' wildcard
    path('outlet/add/', views.add_outlet, name='add_outlet'),
    path('outlet/<str:device_id>/', views.outlet_detail, name='outlet_detail'),
    path('outlet/<str:device_id>/toggle/', views.toggle_outlet, name='toggle_outlet'),
    path('outlet/<str:device_id>/delete/', views.delete_outlet, name='delete_outlet'),
    
    # CCU management
    path('ccu/add/', views.add_ccu, name='add_ccu'),
    path('ccu/<str:ccu_id>/delete/', views.delete_ccu, name='delete_ccu'),
    
    # Event History
    path('event-history/', views.event_history_view, name='event_history'),
]