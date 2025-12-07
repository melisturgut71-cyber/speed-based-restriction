# speed-based-restriction
ESP32 Smart Speed Limit Assistant (ISA) Simulator
🌐 Overview
This project is a hybrid vehicle telematics simulation designed to test Intelligent Speed Assistance (ISA) algorithms.
Instead of risking safety by testing speeding scenarios in a real vehicle, this system allows for a safe desktop simulation:
Real-World Location: It uses a NEO-6M GPS to get the actual location of the device.
Simulated Dynamics: An Analog Joystick acts as the gas and brake pedals to accelerate or decelerate a "virtual vehicle" inside the microcontroller.
Live Speed Limits: The system queries the Overpass API (OpenStreetMap) to fetch the real-time speed limit of the road the device is currently on.
Safety Alerts: It compares the virtual speed against the real-world limit and flags violations via MQTT.

🚀 Key Features
Hybrid Simulation: Combines physical GPS coordinates with virtual velocity.
Dynamic Speed Limit Retrieval: Fetches metadata (Road Name, Type, Max Speed) from OpenStreetMap based on lat/lon.
Physics Simulation: Implements basic inertia, acceleration, and braking logic using joystick input.
Smart Fallback: Estimates speed limits based on road type (e.g., Residential = 50km/h) if specific tags are missing in the map data.
IoT Connectivity: Streams all telemetry data to an MQTT Broker in JSON format.

🛠️ Hardware Requirements
Microcontroller: ESP32 Development Board

Positioning: NEO-6M GPS Module

Input: Analog Joystick Module (XY Axis)
Power: USB or Battery Bank (for portable testing)
Component,Pin Name,ESP32 Pin,Note
GPS,TX,GPIO 16 (RX2),Hardware Serial RX
GPS,RX,GPIO 17 (TX2),Hardware Serial TX
Joystick,VRx (or VRy),GPIO 34,Analog Input (ADC1)
Joystick,VCC,3.3V / 5V,
Joystick,GND,GND,


🧠 System Logic
1. Speed Simulation (Physics)
The joystick value (0-4095) controls the virtual speed:
> 2200 (Push Forward): Acceleration (Speed++)
< 1500 (Pull Back): Braking (Speed--)

Center (Deadzone): Friction/Coasting (Speed -= 0.1)

2. The API Query (Overpass)
Every 15 seconds, the ESP32 sends a query to the Overpass API to avoid rate limiting. It asks for road data within a 25-meter radius of the current GPS fix.

Logic Flow:
Is there a maxspeed tag? -> Use it.

No tag? Check highway type:

motorway -> 130 km/h
primary -> 90 km/h
residential -> 50 km/h
