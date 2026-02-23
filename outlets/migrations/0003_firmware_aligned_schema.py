"""
Custom migration: Align DB with firmware data model.

Actual DB state (confirmed via inspection):
- outlets_sensordata: id, current, timestamp, outlet_id
- outlets_outlet: id, name, device_id, location, is_active, created_at, updated_at, user_id

Target state:
- outlets_sensordata: id, current_a, current_b, is_overload, timestamp, outlet_id
- outlets_outlet: id, name, device_id, location, relay_a, relay_b, threshold, created_at, updated_at, user_id
- outlets_pendingcommand: new table
"""

from django.db import migrations, models
import django.db.models.deletion


class Migration(migrations.Migration):

    dependencies = [
        ('outlets', '0002_userprofile'),
    ]

    operations = [
        # ─── Outlet: Remove is_active, add relay_a/relay_b/threshold ───
        migrations.RemoveField(
            model_name='outlet',
            name='is_active',
        ),
        migrations.AddField(
            model_name='outlet',
            name='relay_a',
            field=models.BooleanField(default=False, help_text='Socket A relay state (ON/OFF)'),
        ),
        migrations.AddField(
            model_name='outlet',
            name='relay_b',
            field=models.BooleanField(default=False, help_text='Socket B relay state (ON/OFF)'),
        ),
        migrations.AddField(
            model_name='outlet',
            name='threshold',
            field=models.IntegerField(default=0, help_text='Current threshold in mA'),
        ),
        migrations.AlterField(
            model_name='outlet',
            name='device_id',
            field=models.CharField(help_text="Hex device ID, e.g. 'FE'", max_length=10, unique=True),
        ),

        # ─── SensorData: Remove old 'current', add current_a/current_b/is_overload ───
        migrations.RemoveField(
            model_name='sensordata',
            name='current',
        ),
        migrations.AddField(
            model_name='sensordata',
            name='current_a',
            field=models.IntegerField(default=0, help_text='Socket A current in mA'),
        ),
        migrations.AddField(
            model_name='sensordata',
            name='current_b',
            field=models.IntegerField(default=0, help_text='Socket B current in mA'),
        ),
        migrations.AddField(
            model_name='sensordata',
            name='is_overload',
            field=models.BooleanField(default=False, help_text='True if 0xFFFF overload trip detected'),
        ),

        # ─── Alert: Update alert_type choices ───
        migrations.AlterField(
            model_name='alert',
            name='alert_type',
            field=models.CharField(
                choices=[('overload', 'Overload Trip'), ('threshold', 'Threshold Exceeded'), ('offline', 'Device Offline')],
                max_length=20,
            ),
        ),

        # ─── PendingCommand: New model ───
        migrations.CreateModel(
            name='PendingCommand',
            fields=[
                ('id', models.BigAutoField(auto_created=True, primary_key=True, serialize=False, verbose_name='ID')),
                ('command', models.CharField(
                    choices=[
                        ('relay_on', 'Relay ON'),
                        ('relay_off', 'Relay OFF'),
                        ('set_threshold', 'Set Threshold'),
                        ('read_sensors', 'Read Sensors'),
                        ('ping', 'Ping'),
                    ],
                    max_length=20,
                )),
                ('socket', models.CharField(blank=True, help_text="'a', 'b', or '' for device-level commands", max_length=1)),
                ('value', models.IntegerField(blank=True, help_text='For threshold values (mA)', null=True)),
                ('created_at', models.DateTimeField(auto_now_add=True)),
                ('is_executed', models.BooleanField(default=False)),
                ('outlet', models.ForeignKey(
                    on_delete=django.db.models.deletion.CASCADE,
                    related_name='pending_commands',
                    to='outlets.outlet',
                )),
            ],
            options={
                'ordering': ['created_at'],
            },
        ),

        # ─── OutletSchedule: Create if not exists ───
        # (The model exists in code but the table was never created)
        migrations.CreateModel(
            name='OutletSchedule',
            fields=[
                ('id', models.BigAutoField(auto_created=True, primary_key=True, serialize=False, verbose_name='ID')),
                ('start_time', models.TimeField()),
                ('end_time', models.TimeField()),
                ('days_of_week', models.CharField(help_text="e.g., 'mon,wed,fri'", max_length=20)),
                ('is_enabled', models.BooleanField(default=True)),
                ('created_at', models.DateTimeField(auto_now_add=True)),
                ('outlet', models.ForeignKey(
                    on_delete=django.db.models.deletion.CASCADE,
                    related_name='schedules',
                    to='outlets.outlet',
                )),
            ],
        ),
    ]
