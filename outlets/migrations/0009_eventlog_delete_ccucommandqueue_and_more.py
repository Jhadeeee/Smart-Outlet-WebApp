# Custom migration: Rename TestEventLog → EventLog, add user field,
# and clean up unused models.

import django.db.models.deletion
from django.conf import settings
from django.db import migrations, models


class Migration(migrations.Migration):

    dependencies = [
        ('outlets', '0008_merge_20260302_0608'),
        migrations.swappable_dependency(settings.AUTH_USER_MODEL),
    ]

    operations = [
        # 1. Clean up unused models first
        migrations.DeleteModel(
            name='CCUCommandQueue',
        ),
        migrations.DeleteModel(
            name='SmartOutletDevice',
        ),
        migrations.DeleteModel(
            name='TestTelemetryLog',
        ),

        # 2. Rename TestEventLog → EventLog (keeps the same table)
        migrations.RenameModel(
            old_name='TestEventLog',
            new_name='EventLog',
        ),

        # 3. Add the user ForeignKey column
        migrations.AddField(
            model_name='eventlog',
            name='user',
            field=models.ForeignKey(
                blank=True,
                null=True,
                on_delete=django.db.models.deletion.SET_NULL,
                related_name='event_logs',
                to=settings.AUTH_USER_MODEL,
            ),
        ),

        # 4. Update action_type choices to include new event types
        migrations.AlterField(
            model_name='eventlog',
            name='action_type',
            field=models.CharField(
                choices=[
                    ('TOGGLE_RELAY', 'Toggle Relay'),
                    ('SET_THRESHOLD', 'Set Threshold'),
                    ('SET_MASTER_ID', 'Set Master ID'),
                    ('ADD_DEVICE', 'Add Device'),
                    ('DELETE_DEVICE', 'Delete Device'),
                    ('OVERLOAD_TRIPPED', 'Overload Tripped'),
                    ('THRESHOLD_EXCEEDED', 'Threshold Exceeded'),
                    ('CUT_ALL_POWER', 'Cut All Power'),
                    ('ACK_RECEIVED', 'Command Acknowledged'),
                    ('SYSTEM_BOOT', 'System Boot'),
                    ('SYSTEM_CLEARED', 'System Cleared'),
                ],
                max_length=50,
            ),
        ),

        # 5. Update source choices
        migrations.AlterField(
            model_name='eventlog',
            name='source',
            field=models.CharField(
                choices=[
                    ('WEB_DASHBOARD', 'Web Dashboard'),
                    ('ESP32_AUTO_CUT', 'ESP32 Auto Cutoff'),
                    ('PIC_HARDWARE', 'PIC Hardware'),
                    ('ESP32_SYSTEM', 'ESP32 System Logic'),
                    ('SERVER', 'Server'),
                ],
                max_length=50,
            ),
        ),
    ]
