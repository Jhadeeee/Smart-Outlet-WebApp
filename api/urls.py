from django.urls import path
from . import views

app_name = 'api'

urlpatterns = [
    path('sensor-data/', views.receive_sensor_data, name='receive_sensor_data'),
    path('outlet-status/<str:device_id>/', views.get_outlet_status, name='get_outlet_status'),
    path('event-log/', views.receive_event_log, name='receive_event_log'),
    path('event-logs/<str:device_id>/', views.get_event_logs, name='get_event_logs'),
]