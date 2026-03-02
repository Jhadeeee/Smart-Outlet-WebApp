from django.urls import path
from . import views

app_name = 'api'

urlpatterns = [
    path('data/', views.receive_sensor_data, name='receive_sensor_data'),
    path('breaker-data/', views.receive_breaker_data, name='receive_breaker_data'),
    path('outlet-status/<str:device_id>/', views.get_outlet_status, name='get_outlet_status'),
    path('commands/<str:device_id>/', views.get_pending_commands, name='get_pending_commands'),
    path('commands/<str:device_id>/<str:command>/', views.queue_command, name='queue_command'),
    path('devices/', views.get_registered_outlets, name='get_registered_outlets'),
    path('log-event/', views.log_event, name='log_event'),
]