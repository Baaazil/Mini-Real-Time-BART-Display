/***************************************************
  BART Real-Time Departures on ER-OLEDM2004-1 (20x4 char OLED, I2C)
  Board: NodeMCU ESP-12E (ESP8266)

  Alternates between two screens on a timer:
    Screen 0: Northbound departures
    Screen 1: Southbound departures

  Data source: BART "legacy" ETD API (HTTPS).
  Docs: https://api.bart.gov/docs/etd/etd.aspx
  Get your own free key at: https://api.bart.gov/docs/overview/index.aspx

  == Hardware connection (NodeMCU ESP-12E) ==
  VCC -> 3V3/5V, GND -> GND, DB1/DB2(SCL) -> D2 (GPIO4), DB0(SDA) -> D1 (GPIO5)
  RES -> D0 (GPIO16)

  Libraries needed: ArduinoJson (Library Manager)
****************************************************/

#include <Arduino.h>
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "er_oled.h"

// ---------- USER CONFIG ----------
const char* WIFI_SSID = "Dogma";
const char* WIFI_PASSWORD = "percycute";

const char* BART_API_KEY = "ZQAL-52IK-93LT-DWEI";  // BART key
const char* BART_STATION = "EMBR";                 // Embarcadero

const unsigned long DATA_POLL_INTERVAL_MS = 30000;     // how often to hit the API
const unsigned long SCREEN_SWITCH_INTERVAL_MS = 10000;  // how often to flip screens

const uint8_t I2C_SDA = 4;
const uint8_t I2C_SCL = 5;
// ----------------------------------

struct Departure {
  String dest;
  String minutes;
  String color;
};

const int MAX_PER_DIRECTION = 3;
Departure northDeps[MAX_PER_DIRECTION];
Departure southDeps[MAX_PER_DIRECTION];
int northCount = 0;
int southCount = 0;

unsigned long lastPoll = 0;
unsigned long lastScreenSwitch = 0;
uint8_t currentScreen = 0;  // 0 = North, 1 = South

WiFiClientSecure wifiClient;

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected, IP: " + WiFi.localIP().toString());
}

// Fetches ETD for BART_STATION once, splits results into northDeps/southDeps
// by each departure's direction field. Updates northCount/southCount.
void fetchDepartures() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  String url = String("https://api.bart.gov/api/etd.aspx?cmd=etd&orig=") + BART_STATION + "&key=" + BART_API_KEY + "&json=y";

  http.begin(wifiClient, url);
  int httpCode = http.GET();
  if (httpCode != 200) {
    Serial.printf("HTTP GET failed, code: %d\n", httpCode);
    http.end();
    return;
  }

  String payload = http.getString();
  http.end();

  DynamicJsonDocument doc(6144);
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.print("JSON parse failed: ");
    Serial.println(err.c_str());
    return;
  }

  northCount = 0;
  southCount = 0;

  JsonArray etdArray = doc["root"]["station"][0]["etd"].as<JsonArray>();
  for (JsonObject etd : etdArray) {
    const char* dest = etd["destination"];
    JsonArray estimates = etd["estimate"].as<JsonArray>();
    if (estimates.size() == 0) continue;

    JsonObject first = estimates[0];
    const char* minutes = first["minutes"];
    const char* color = first["color"];
    const char* direction = first["direction"];  // "North" or "South"

    Departure d;
    d.dest = String(dest);
    d.minutes = String(minutes);
    d.color = String(color);

    if (strcmp(direction, "North") == 0 && northCount < MAX_PER_DIRECTION) {
      northDeps[northCount++] = d;
    } else if (strcmp(direction, "South") == 0 && southCount < MAX_PER_DIRECTION) {
      southDeps[southCount++] = d;
    }
  }
}

String fitToWidth(String s, uint8_t len) {
  if (s.length() > len) s = s.substring(0, len);
  while (s.length() < len) s += ' ';
  return s;
}

// Draws one direction's screen from already-fetched data (no network call).
void showDirection(const char* label, Departure* deps, int count) {
  er_oled_clear();

  String header = String(BART_STATION) + " " + label;
  er_oled_string(0, 0, fitToWidth(header, WIDTH).c_str(), 0);

  for (int i = 0; i < MAX_PER_DIRECTION; i++) {
    String line;
    if (i < count) {
      String dest = deps[i].dest;
      String mins = deps[i].minutes;
      String tail = (mins == "Leaving") ? "Now" : (mins + "m");
      String left = fitToWidth(dest, 14);
      String right = fitToWidth(tail, 6);
      right.trim();
      while (right.length() < 6) right = " " + right;
      line = left + right;
    } else {
      line = fitToWidth("", WIDTH);
    }
    er_oled_string(0, i + 1, fitToWidth(line, WIDTH).c_str(), 0);
  }
}

void showMessage(const char* line0, const char* line1 = "") {
  er_oled_clear();
  er_oled_string(0, 0, fitToWidth(line0, WIDTH).c_str(), 0);
  if (strlen(line1)) er_oled_string(0, 1, fitToWidth(line1, WIDTH).c_str(), 0);
}

// Redraws whichever screen is currently active, from cached data.
void renderCurrentScreen() {
  if (currentScreen == 0) {
    showDirection("Northbound", northDeps, northCount);
  } else {
    showDirection("Southbound", southDeps, southCount);
  }
}

void setup() {
  Serial.begin(9600);
  Wire.begin(I2C_SDA, I2C_SCL);
  er_oled_begin();

  wifiClient.setInsecure();  // skip TLS cert validation - fine for public data

  showMessage("Connecting WiFi...");
  connectWiFi();

  showMessage("WiFi connected", WiFi.localIP().toString().c_str());
  delay(1000);

  lastPoll = 0;  // force immediate fetch
  lastScreenSwitch = millis();
  currentScreen = 0;
}

void loop() {
  unsigned long now = millis();

  // --- Refresh data from BART (slow timer) ---
  if (now - lastPoll >= DATA_POLL_INTERVAL_MS || lastPoll == 0) {
    lastPoll = now;
    fetchDepartures();
    renderCurrentScreen();  // reflect fresh data immediately on current screen
  }

  // --- Flip between North/South screens (fast timer) ---
  if (now - lastScreenSwitch >= SCREEN_SWITCH_INTERVAL_MS) {
    lastScreenSwitch = now;
    currentScreen = 1 - currentScreen;  // toggle 0 <-> 1
    renderCurrentScreen();
  }

  delay(100);
}
