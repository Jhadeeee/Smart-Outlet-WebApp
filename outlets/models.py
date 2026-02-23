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
    """Smart Outlet Device Model — matches CCU firmware OutletDevice"""
    user = models.ForeignKey(User, on_delete=models.CASCADE, related_name='outlets')
    name = models.CharField(max_length=100)
    device_id = models.CharField(max_length=10, unique=True, help_text="Hex device ID, e.g. 'FE'")
    location = models.CharField(max_length=100, blank=True)
    relay_a = models.BooleanField(default=False, help_text="Socket A relay state (ON/OFF)")
    relay_b = models.BooleanField(default=False, help_text="Socket B relay state (ON/OFF)")
    threshold = models.IntegerField(default=0, help_text="Current threshold in mA")
    created_at = models.DateTimeField(auto_now_add=True)
    updated_at = models.DateTimeField(auto_now=True)
    
    class Meta:
        ordering = ['-created_at']
    
    def __str__(self):
        return f"{self.name} ({self.device_id})"


class SensorData(models.Model):
    """
    Current readings from Smart Outlets via CCU.
    
    Each PIC16F88 outlet reports current per socket (A/B) in mA.
    A reading of 0xFFFF indicates an overload trip.
    """
    outlet = models.ForeignKey(Outlet, on_delete=models.CASCADE, related_name='sensor_data')
    current_a = models.IntegerField(default=0, help_text="Socket A current in mA")
    current_b = models.IntegerField(default=0, help_text="Socket B current in mA")
    is_overload = models.BooleanField(default=False, help_text="True if 0xFFFF overload trip detected")
    timestamp = models.DateTimeField(auto_now_add=True)
    
    class Meta:
        ordering = ['-timestamp']
        indexes = [
            models.Index(fields=['-timestamp']),
            models.Index(fields=['outlet', '-timestamp']),
        ]
    
    def __str__(self):
        return f"{self.outlet.name} - {self.timestamp.strftime('%Y-%m-%d %H:%M:%S')}"


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
        ('overload', 'Overload Trip'),
        ('threshold', 'Threshold Exceeded'),
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


class PendingCommand(models.Model):
    """
    Commands queued from the Online UI for the CCU to pick up via polling.
    
    The CCU periodically GETs /api/commands/<device_id>/ to fetch and execute
    pending commands, then marks them as executed.
    """
    COMMAND_CHOICES = [
        ('relay_on', 'Relay ON'),
        ('relay_off', 'Relay OFF'),
        ('set_threshold', 'Set Threshold'),
        ('read_sensors', 'Read Sensors'),
        ('ping', 'Ping'),
    ]
    
    outlet = models.ForeignKey(Outlet, on_delete=models.CASCADE, related_name='pending_commands')
    command = models.CharField(max_length=20, choices=COMMAND_CHOICES)
    socket = models.CharField(max_length=1, blank=True, help_text="'a', 'b', or '' for device-level commands")
    value = models.IntegerField(null=True, blank=True, help_text="For threshold values (mA)")
    created_at = models.DateTimeField(auto_now_add=True)
    is_executed = models.BooleanField(default=False)
    
    class Meta:
        ordering = ['created_at']
    
    def __str__(self):
        return f"{self.command} → {self.outlet.name} ({self.created_at.strftime('%H:%M:%S')})"