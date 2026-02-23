from django.contrib import admin
from .models import Outlet, SensorData, OutletSchedule, Alert, UserProfile, PendingCommand

@admin.register(UserProfile)
class UserProfileAdmin(admin.ModelAdmin):
    list_display = ['user', 'country', 'barangay', 'phone_number', 'created_at']
    search_fields = ['user__username', 'country', 'barangay']
    readonly_fields = ['created_at']

@admin.register(Outlet)
class OutletAdmin(admin.ModelAdmin):
    list_display = ['name', 'device_id', 'user', 'location', 'relay_a', 'relay_b', 'threshold', 'created_at']
    list_filter = ['relay_a', 'relay_b', 'created_at']
    search_fields = ['name', 'device_id', 'location']
    readonly_fields = ['created_at', 'updated_at']

@admin.register(SensorData)
class SensorDataAdmin(admin.ModelAdmin):
    list_display = ['outlet', 'current_a', 'current_b', 'is_overload', 'timestamp']
    list_filter = ['outlet', 'is_overload', 'timestamp']
    search_fields = ['outlet__name']
    readonly_fields = ['timestamp']
    date_hierarchy = 'timestamp'

@admin.register(OutletSchedule)
class OutletScheduleAdmin(admin.ModelAdmin):
    list_display = ['outlet', 'start_time', 'end_time', 'days_of_week', 'is_enabled']
    list_filter = ['is_enabled', 'created_at']
    search_fields = ['outlet__name']

@admin.register(Alert)
class AlertAdmin(admin.ModelAdmin):
    list_display = ['outlet', 'alert_type', 'is_read', 'created_at']
    list_filter = ['alert_type', 'is_read', 'created_at']
    search_fields = ['outlet__name', 'message']
    readonly_fields = ['created_at']

@admin.register(PendingCommand)
class PendingCommandAdmin(admin.ModelAdmin):
    list_display = ['outlet', 'command', 'socket', 'value', 'is_executed', 'created_at']
    list_filter = ['command', 'is_executed', 'created_at']
    search_fields = ['outlet__name']
    readonly_fields = ['created_at']