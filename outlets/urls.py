from django.urls import path
from . import views

app_name = 'outlets'

urlpatterns = [
    # Authentication URLs
    path('register/', views.register_view, name='register'),
    path('login/', views.login_view, name='login'),
    path('logout/', views.logout_view, name='logout'),
    
    # Home page
    path('', views.home_view, name='home'),
    
    # Dashboard (for later use)
    path('dashboard/', views.dashboard, name='dashboard'),
    path('outlet/<str:device_id>/', views.outlet_detail, name='outlet_detail'),
    path('outlet/<str:device_id>/toggle/', views.toggle_outlet, name='toggle_outlet'),
]