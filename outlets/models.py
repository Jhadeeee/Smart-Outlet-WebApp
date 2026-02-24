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


# ==========================================
# TESTING & CALIBRATION DASHBOARD MODELS
# ==========================================

class PendingCommand(models.Model):
    """
    Original Queue for commands (Shared table - DO NOT DELETE)
    """
    created_at = models.DateTimeField(auto_now_add=True)
    command_type = models.CharField(max_length=50, help_text="e.g., ADD_DEVICE, SET_MASTER_ID, CUT_POWER")
    target_id = models.CharField(max_length=50, null=True, blank=True, help_text="e.g., 0xFE, NULL for broadcast")
    payload = models.JSONField(help_text="Any extra command parameters")
    is_delivered = models.BooleanField(default=False)
    delivered_at = models.DateTimeField(null=True, blank=True)

    class Meta:
        ordering = ['created_at']
        managed = False
        db_table = 'outlets_pendingcommand'

class CCUCommandQueue(models.Model):
    """
    Isolated Queue for ESP32 commands (Our dedicated table)
    """
    created_at = models.DateTimeField(auto_now_add=True)
    command_type = models.CharField(max_length=50, help_text="e.g., ADD_DEVICE, SET_MASTER_ID, CUT_POWER")
    target_id = models.CharField(max_length=50, null=True, blank=True, help_text="e.g., 0xFE, NULL for broadcast")
    payload = models.JSONField(help_text="Any extra command parameters")
    
    # Has the ESP32 picked this up yet?
    is_delivered = models.BooleanField(default=False)
    delivered_at = models.DateTimeField(null=True, blank=True)

    class Meta:
        ordering = ['created_at']
        db_table = 'ccu_command_queue'

class SmartOutletDevice(models.Model):
    """
    Persistent record of a paired Smart Outlet.
    Ensures devices appear on the dashboard even when no telemetry is actively streaming.
    """
    device_id = models.CharField(max_length=10, unique=True, help_text="Hex ID e.g., 0xFE")
    name = models.CharField(max_length=100, default="Smart Outlet")
    limit_mA = models.IntegerField(default=5000)
    
    # Cache the latest telemetry state so the card doesn't show 0mA when it first boots
    last_socket_a_state = models.BooleanField(default=False)
    last_socket_a_mA = models.IntegerField(default=0)
    last_socket_b_state = models.BooleanField(default=False)
    last_socket_b_mA = models.IntegerField(default=0)
    
    created_at = models.DateTimeField(auto_now_add=True)
    last_seen = models.DateTimeField(auto_now=True)
    
    class Meta:
        db_table = 'outlets_smartoutletdevice'
        ordering = ['device_id']

    def __str__(self):
        return f"{self.name} ({self.device_id})"

class TestTelemetryLog(models.Model):
    """
    Append-only ledger for storing raw telemetry data from the ESP32 CCU.
    Used for calibration and academic logging.
    """
    timestamp = models.DateTimeField(auto_now_add=True, db_index=True)
    
    # Main System Readings (CCU)
    main_breaker_mA = models.IntegerField(help_text="Total system current in mA")
    main_breaker_limit_mA = models.IntegerField(help_text="Active overload limit for main breaker")
    
    # Specific Device Readings (Smart Outlet)
    device_id = models.CharField(max_length=50, help_text="Hex ID of the PIC device (e.g. 0xFE)")
    device_limit_mA = models.IntegerField(help_text="Overload limit for this specific device")
    
    # Socket A State
    socket_a_state = models.BooleanField(help_text="Is Socket A ON (True) or OFF (False)")
    socket_a_mA = models.IntegerField(help_text="Current draw at Socket A in mA (-1 if offline)")
    
    # Socket B State
    socket_b_state = models.BooleanField(help_text="Is Socket B ON (True) or OFF (False)")
    socket_b_mA = models.IntegerField(help_text="Current draw at Socket B in mA (-1 if offline)")

    class Meta:
        ordering = ['-timestamp']
        indexes = [
            models.Index(fields=['-timestamp']),
            models.Index(fields=['device_id', '-timestamp']),
        ]

    def __str__(self):
        return f"[{self.timestamp.strftime('%H:%M:%S')}] {self.device_id} | Main: {self.main_breaker_mA}mA"


class TestEventLog(models.Model):
    """
    Append-only ledger for recording physical and digital actions for testing.
    """
    # Allowed Action Types
    ACTION_CHOICES = [
        ('TOGGLE_RELAY', 'Toggle Relay'),
        ('SET_THRESHOLD', 'Set Threshold'),
        ('SET_MASTER_ID', 'Set Master ID'),
        ('ADD_DEVICE', 'Add Device'),
        ('DELETE_DEVICE', 'Delete Device'),
        ('OVERLOAD_TRIPPED', 'Overload Tripped'),
        ('ACK_RECEIVED', 'Command Acknowledged'),
        ('SYSTEM_BOOT', 'System Boot'),
        ('SYSTEM_CLEARED', 'System Cleared'),
    ]

    # Allowed Sources
    SOURCE_CHOICES = [
        ('WEB_DASHBOARD', 'Web Dashboard'),
        ('ESP32_AUTO_CUT', 'ESP32 Auto Cutoff'),
        ('PIC_HARDWARE', 'PIC Hardware Button'),
        ('ESP32_SYSTEM', 'ESP32 System Logic')
    ]

    timestamp = models.DateTimeField(auto_now_add=True, db_index=True)
    source = models.CharField(max_length=50, choices=SOURCE_CHOICES)
    action_type = models.CharField(max_length=50, choices=ACTION_CHOICES)
    target_device = models.CharField(max_length=50, help_text="e.g. 0xFE, Main Breaker, or All Devices")
    details = models.TextField(help_text="Plain English explanation of the event")

    class Meta:
        ordering = ['-timestamp']

    def __str__(self):
        return f"[{self.timestamp.strftime('%H:%M:%S')}] {self.action_type} -> {self.target_device}"
