import os
import django

os.environ.setdefault('DJANGO_SETTINGS_MODULE', 'config.settings')
django.setup()

from django.db import connection

with connection.cursor() as cursor:
    cursor.execute('DROP TABLE IF EXISTS outlets_pendingcommand CASCADE;')
    cursor.execute("DELETE FROM django_migrations WHERE app='outlets' AND name='0004_pendingcommand';")

print("Successfully dropped stale table and migration record.")
