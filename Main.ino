#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include <time.h>
#include <WebServer.h>
#include <ArduinoOTA.h>

// ======= WiFi Credentials =======
const char* ssid     = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// Location Settings (Replace with your Latitude & Longitude)
const float LAT = 0.0000;
const float LON = 0.0000;

// Display Hardware Settings
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4
#define CS_PIN   15
#define CLK_PIN  18
#define DIN_PIN  23

MD_Parola display = MD_Parola(HARDWARE_TYPE, DIN_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);
WebServer server(80);

// Display & Weather Variables
float tempF = 0;
int humidity = 0;
String weatherDesc = "Loading...";
String timeStr = "12:00 PM";
String dayStr = "Wed";
String customText = "Hello World!";

int scrollSpeed = 55;      
int brightness = 5;         
bool inverted = false;      

// Night Mode Settings (24-Hour Format)
bool nightModeEnabled = true;
int nightStartHour = 23;    // 11:00 PM
int nightEndHour = 7;       // 07:00 AM
bool isNightTime = false;

// Short day lookup array (Sunday=0 to Saturday=6)
const char* shortDays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

char displayBuffer[128];
char msgArray[6][128];
int totalMessages = 0;
int msgIndex = 0;

unsigned long lastWeather = 0;
unsigned long lastTime = 0;

void fetchWeather() {
  HTTPClient http;
  String url = "https://api.open-meteo.com/v1/forecast?latitude=" + String(LAT, 4) +
               "&longitude=" + String(LON, 4) +
               "&current=temperature_2m,relative_humidity_2m,weathercode&temperature_unit=fahrenheit";
  
  http.begin(url);
  if (http.GET() == 200) {
    DynamicJsonDocument doc(2048);
    deserializeJson(doc, http.getString());
    tempF    = doc["current"]["temperature_2m"];
    humidity = doc["current"]["relative_humidity_2m"];
    int wcode = doc["current"]["weathercode"];
    
    if      (wcode == 0)  weatherDesc = "Clear Sky";
    else if (wcode <= 3)  weatherDesc = "Partly Cloudy";
    else if (wcode <= 67) weatherDesc = "Rainy";
    else if (wcode <= 77) weatherDesc = "Snowy";
    else if (wcode <= 82) weatherDesc = "Showers";
    else                  weatherDesc = "Stormy";
  }
  http.end();
}

void updateTimeAndDate() {
  struct tm t;
  if (getLocalTime(&t)) {
    // Update Time Format
    char bufTime[16];
    strftime(bufTime, sizeof(bufTime), "%I:%M %p", &t);
    timeStr = String(bufTime);

    // Update Short Day Format
    if (t.tm_wday >= 0 && t.tm_wday <= 6) {
      dayStr = String(shortDays[t.tm_wday]);
    }

    // Night Mode Hour Check
    int currentHour = t.tm_hour;
    if (nightModeEnabled) {
      if (nightStartHour > nightEndHour) {
        isNightTime = (currentHour >= nightStartHour || currentHour < nightEndHour);
      } else {
        isNightTime = (currentHour >= nightStartHour && currentHour < nightEndHour);
      }
      display.setIntensity(isNightTime ? 0 : brightness);
    } else {
      isNightTime = false;
      display.setIntensity(brightness);
    }
  }
}

void buildMessages() {
  int i = 0;
  if (customText.length() > 0) {
    snprintf(msgArray[i++], sizeof(msgArray[0]), "%s", customText.c_str());
  }
  snprintf(msgArray[i++], sizeof(msgArray[0]), "%s", dayStr.c_str());
  snprintf(msgArray[i++], sizeof(msgArray[0]), "%s", timeStr.c_str());
  snprintf(msgArray[i++], sizeof(msgArray[0]), "Temp: %dF", (int)tempF);
  snprintf(msgArray[i++], sizeof(msgArray[0]), "Hum: %d%%", humidity);
  snprintf(msgArray[i++], sizeof(msgArray[0]), "%s", weatherDesc.c_str());

  totalMessages = i;
}

void loadNextMessage() {
  snprintf(displayBuffer, sizeof(displayBuffer), "%s", msgArray[msgIndex]);
  display.displayText(displayBuffer, PA_LEFT, scrollSpeed, 0, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
  display.displayReset();
}

void handleRoot() {
  String html = R"(
<!DOCTYPE html>
<html>
<head>
  <meta name='viewport' content='width=device-width, initial-scale=1'>
  <title>LED Sign Dashboard</title>
  <style>
    body { font-family: -apple-system, BlinkMacSystemFont, Arial, sans-serif; background: #121212; color: #FFD700; text-align: center; padding: 15px; margin: 0; }
    .card { background: #1E1E1E; max-width: 420px; margin: 0 auto 15px; padding: 20px; border-radius: 12px; box-shadow: 0 4px 10px rgba(0,0,0,0.5); }
    h1 { font-size: 20px; margin-top: 0; color: #FFF; }
    label { font-size: 13px; font-weight: bold; display: block; margin-top: 10px; text-align: left; color: #AAA; }
    input[type=text], input[type=number] { width: 90%; padding: 10px; font-size: 15px; border-radius: 8px; border: 1px solid #FFD700; background: #2A2A2A; color: #FFF; margin: 6px 0; outline: none; }
    input[type=range] { width: 100%; margin: 8px 0; accent-color: #FFD700; }
    button { background: #FFD700; color: #111; border: none; padding: 12px 20px; font-size: 15px; font-weight: bold; border-radius: 8px; cursor: pointer; width: 100%; margin-top: 10px; }
    .row { display: flex; gap: 8px; }
    .weather { font-size: 14px; color: #FFD700; }
  </style>
</head>
<body>
  <div class='card'>
    <h1>⚡ Display Settings</h1>
    <form action='/set' method='GET'>
      <label>Custom Message:</label>
      <input type='text' name='msg' value=')" + customText + R"('>
      
      <label>Brightness (0 - 15):</label>
      <input type='range' name='bright' min='0' max='15' value=')" + String(brightness) + R"('>
      
      <label>Scroll Speed (Slower ◄ ► Faster):</label>
      <input type='range' name='speed' min='10' max='100' value=')" + String(scrollSpeed) + R"('>
      
      <button type='submit'>Save Settings</button>
    </form>
  </div>

  <div class='card'>
    <h1>🌙 Night Mode Hours</h1>
    <form action='/setNight' method='GET'>
      <div class='row'>
        <div style='flex:1;'>
          <label>Start Hour (0-23):</label>
          <input type='number' name='nstart' min='0' max='23' value=')" + String(nightStartHour) + R"('>
        </div>
        <div style='flex:1;'>
          <label>End Hour (0-23):</label>
          <input type='number' name='nend' min='0' max='23' value=')" + String(nightEndHour) + R"('>
        </div>
      </div>
      <button type='submit'>Set Night Schedule</button>
    </form>
  </div>

  <div class='card weather'>
    📅 Day: )" + dayStr + R"( &nbsp;|&nbsp; 🌡️ )" + String((int)tempF) + R"(°F &nbsp;|&nbsp; 💧 Hum: )" + String(humidity) + R"(%
  </div>
</body>
</html>
  )";
  server.send(200, "text/html", html);
}

void handleSet() {
  if (server.hasArg("msg")) customText = server.arg("msg");
  if (server.hasArg("bright")) {
    brightness = server.arg("bright").toInt();
    if (!isNightTime) display.setIntensity(brightness);
  }
  if (server.hasArg("speed")) scrollSpeed = server.arg("speed").toInt();

  buildMessages();
  msgIndex = 0;
  loadNextMessage();

  server.sendHeader("Location", "/");
  server.send(303);
}

void handleSetNight() {
  if (server.hasArg("nstart")) nightStartHour = server.arg("nstart").toInt();
  if (server.hasArg("nend")) nightEndHour = server.arg("nend").toInt();

  updateTimeAndDate();
  
  server.sendHeader("Location", "/");
  server.send(303);
}

void setup() {
  Serial.begin(115200);

  display.begin();
  display.setIntensity(brightness);

  snprintf(displayBuffer, sizeof(displayBuffer), "Connecting WiFi...");
  display.displayText(displayBuffer, PA_LEFT, scrollSpeed, 0, PA_SCROLL_LEFT, PA_SCROLL_LEFT);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    display.displayAnimate();
    delay(50);
  }

  ArduinoOTA.setHostname("ESP32-LED-Sign");
  ArduinoOTA.begin();

  String ip = WiFi.localIP().toString();
  snprintf(displayBuffer, sizeof(displayBuffer), "IP: %s", ip.c_str());
  display.displayText(displayBuffer, PA_LEFT, scrollSpeed, 1000, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
  
  while (!display.displayAnimate()) {
    delay(10);
  }

  server.on("/", handleRoot);
  server.on("/set", handleSet);
  server.on("/setNight", handleSetNight);
  server.begin();

  configTime(-18000, 3600, "pool.ntp.org");
  
  fetchWeather();
  updateTimeAndDate();
  buildMessages();

  loadNextMessage();
}

void loop() {
  ArduinoOTA.handle();
  server.handleClient();

  if (display.displayAnimate()) {
    msgIndex = (msgIndex + 1) % totalMessages;
    loadNextMessage();
  }

  if (millis() - lastWeather > 600000) {
    lastWeather = millis();
    fetchWeather();
    buildMessages();
  }

  if (millis() - lastTime > 30000) {
    lastTime = millis();
    updateTimeAndDate();
    buildMessages();
  }
}
