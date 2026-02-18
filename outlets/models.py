from django.db import models
from django.contrib.auth.models import User
from django.db.models.signals import post_save
from django.dispatch import receiver

class UserProfile(models.Model):
    """Extended user information"""
    user = models.OneToOneField(User, on_delete=models.CASCADE, related_name='profile')
    country = models.CharField(max_length=100, blank=True)
    barangay = models.CharField(max_length=100, blank=True)
    phone_number = models.CharField(max_length=20, blank=True)
    address = models.TextField(blank=True)
    created_at = models.DateTimeField(auto_now_add=True)
    
    def __str__(self):
        return f"{self.user.username}'s Profile"


# Signal to auto-create profile when user is created
@receiver(post_save, sender=User)
def create_user_profile(sender, instance, created, **kwargs):
    if created:
        UserProfile.objects.create(user=instance)

@receiver(post_save, sender=User)
def save_user_profile(sender, instance, **kwargs):
    if hasattr(instance, 'profile'):
        instance.profile.save()


class Outlet(models.Model):
    """Smart Outlet Device Model"""
    user = models.ForeignKey(User, on_delete=models.CASCADE, related_name='outlets')
    name = models.CharField(max_length=100)
    device_id = models.CharField(max_length=50, unique=True)
    location = models.CharField(max_length=100, blank=True)
    is_active = models.BooleanField(default=False)  # ON/OFF status
    created_at = models.DateTimeField(auto_now_add=True)
    updated_at = models.DateTimeField(auto_now=True)
    
    class Meta:
        ordering = ['-created_at']
    
    def __str__(self):
        return f"{self.name} ({self.device_id})"


class SensorData(models.Model):
    """Sensor readings from ESP32"""
    outlet = models.ForeignKey(Outlet, on_delete=models.CASCADE, related_name='sensor_data')
    voltage = models.FloatField(help_text="Voltage in Volts (V)")
    current = models.FloatField(help_text="Current in Amperes (A)")
    power = models.FloatField(help_text="Power in Watts (W)")
    energy = models.FloatField(default=0, help_text="Energy consumed in kWh")
    temperature = models.FloatField(null=True, blank=True, help_text="Temperature in Celsius")
    timestamp = models.DateTimeField(auto_now_add=True)
    
    class Meta:
        ordering = ['-timestamp']
        indexes = [
            models.Index(fields=['-timestamp']),
            models.Index(fields=['outlet', '-timestamp']),
        ]
    
    def __str__(self):
        return f"{self.outlet.name} - {self.timestamp.strftime('%Y-%m-%d %H:%M:%S')}"
    
    @property
    def power_factor(self):
        """Calculate power factor"""
        if self.voltage and self.current and self.voltage * self.current > 0:
            return self.power / (self.voltage * self.current)
        return 0


class OutletSchedule(models.Model):
    """Schedule for automatic outlet control"""
    outlet = models.ForeignKey(Outlet, on_delete=models.CASCADE, related_name='schedules')
    start_time = models.TimeField()
    end_time = models.TimeField()
    days_of_week = models.CharField(max_length=20, help_text="e.g., 'mon,wed,fri'")
    is_enabled = models.BooleanField(default=True)
    created_at = models.DateTimeField(auto_now_add=True)
    
    def __str__(self):
        return f"{self.outlet.name} - {self.start_time} to {self.end_time}"


class Alert(models.Model):
    """Alerts for abnormal conditions"""
    ALERT_TYPES = [
        ('high_power', 'High Power Consumption'),
        ('high_temp', 'High Temperature'),
        ('voltage_spike', 'Voltage Spike'),
        ('offline', 'Device Offline'),
    ]
    
    outlet = models.ForeignKey(Outlet, on_delete=models.CASCADE, related_name='alerts')
    alert_type = models.CharField(max_length=20, choices=ALERT_TYPES)
    message = models.TextField()
    is_read = models.BooleanField(default=False)
    created_at = models.DateTimeField(auto_now_add=True)
    
    class Meta:
        ordering = ['-created_at']
    
    def __str__(self):
        return f"{self.get_alert_type_display()} - {self.outlet.name}"


class EventLogTest(models.Model):
    """Experimental event logs from ESP32 (cutoff, overload, power on/off).
    
    Only important events are logged here to save database space.
    Regular sensor readings go through WebSocket only (no DB write).
    """
    EVENT_TYPES = [
        ('cutoff', 'Cut Off'),
        ('overload', 'Overload'),
        ('power_on', 'Power On'),
        ('power_off', 'Power Off'),
        ('warning', 'Warning'),
        ('reconnect', 'Reconnected'),
        ('disconnect', 'Disconnected'),
    ]
    SEVERITY_LEVELS = [
        ('info', 'Info'),
        ('warning', 'Warning'),
        ('critical', 'Critical'),
    ]

    outlet = models.ForeignKey(Outlet, on_delete=models.CASCADE, related_name='event_logs_test')
    event_type = models.CharField(max_length=20, choices=EVENT_TYPES)
    severity = models.CharField(max_length=10, choices=SEVERITY_LEVELS, default='info')
    message = models.TextField(blank=True)
    socket_label = models.CharField(max_length=2, blank=True, help_text="A, B, or AB")
    current_reading = models.FloatField(null=True, blank=True, help_text="Current in mA at time of event")
    timestamp = models.DateTimeField(auto_now_add=True)

    class Meta:
        ordering = ['-timestamp']
        indexes = [
            models.Index(fields=['-timestamp']),
            models.Index(fields=['outlet', '-timestamp']),
        ]

    def __str__(self):
        return f"{self.get_event_type_display()} - {self.outlet.name} ({self.timestamp.strftime('%Y-%m-%d %H:%M:%S')})"