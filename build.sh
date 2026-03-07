#!/usr/bin/env bash
# Render build script for Smart Outlet Web App

set -o errexit  # Exit on error

pip install -r requirements.txt

python manage.py collectstatic --no-input

python manage.py migrate
