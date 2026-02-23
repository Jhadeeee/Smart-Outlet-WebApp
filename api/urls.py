from django.urls import path
from . import views

app_name = 'api'

urlpatterns = [
    path('data/', views.receive_sensor_data, name='receive_sensor_data'),
    path('outlet-status/<str:device_id>/', views.get_outlet_status, name='get_outlet_status'),
    path('commands/<str:device_id>/', views.get_pending_commands, name='get_pending_commands'),
]