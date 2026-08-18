# ESP32 LED Matrix Weather Sign

A smart LED matrix display powered by an ESP32 that fetches live weather data and syncs NTP time, complete with a web interface for custom messages and night mode settings.

## Features
* Live weather updates via Open-Meteo API
* Automatic NTP time synchronization
* Web-based configuration dashboard
* Customizable night mode schedule

## Hardware Required
* ESP32 Development Board
* MAX7219 LED Matrix Display (4-in-1 module)
* Jumper Wires

## Setup Instructions
1. Clone this repository.
2. Open the project in Arduino IDE.
3. Update `ssid`, `password`, `LAT`, and `LON` in the source code with your network details and location.
4. Upload to your ESP32.

## License
MIT
