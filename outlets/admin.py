from django.contrib import admin
from .models import Outlet, SensorData, OutletSchedule, Alert, UserProfile, EventLogTest

@admin.register(UserProfile)
class UserProfileAdmin(admin.ModelAdmin):
    list_display = ['user', 'country', 'barangay', 'phone_number', 'created_at']
    search_fields = ['user__username', 'country', 'barangay']
    readonly_fields = ['created_at']

@admin.register(Outlet)
class OutletAdmin(admin.ModelAdmin):
    list_display = ['name', 'device_id', 'user', 'location', 'is_active', 'created_at']
    list_filter = ['is_active', 'created_at']
    search_fields = ['name', 'device_id', 'location']
    readonly_fields = ['created_at', 'updated_at']

@admin.register(SensorData)
class SensorDataAdmin(admin.ModelAdmin):
    list_display = ['outlet', 'voltage', 'current', 'power', 'energy', 'temperature', 'timestamp']
    list_filter = ['outlet', 'timestamp']
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

@admin.register(EventLogTest)
class EventLogTestAdmin(admin.ModelAdmin):
    list_display = ['outlet', 'event_type', 'severity', 'socket_label', 'current_reading', 'timestamp']
    list_filter = ['event_type', 'severity', 'outlet', 'timestamp']
    search_fields = ['outlet__name', 'message']
    readonly_fields = ['timestamp']
    date_hierarchy = 'timestamp'