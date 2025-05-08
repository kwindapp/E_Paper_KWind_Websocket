#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <epd_driver.h>
#include "titel.h"
#include "logo.h"
#include "logo1.h"
#include "logo2.h"
#include "logo3.h"
#include "qr.h"
#include "dir.h"
#include "wind.h"
#include "temp.h"
#include "hum.h"
#include "bat.h"
#include "opensans12b.h"
#include "opensans18b.h"
#include "opensans24b.h"
// === UI Positions ===
int cursor_x = 0;
int cursor_y = 0;
int custom_y = 80;
const char* websocket_server = "api.kwind.app";
const uint16_t websocket_port = 443;
const char* websocket_path = "/";

WebSocketsClient webSocket;

bool isCon = false;
bool isConSer = false;
#ifndef BOARD_HAS_PSRAM
#error "Please enable PSRAM, Arduino IDE -> Tools -> PSRAM -> OPI !!!"
#endif
String localMac;
// === DISPLAY ===
uint8_t *framebuffer;
// === GET CARDINAL DIRECTION ===
String getCardinalDirection(int windDir) {
  if (windDir >= 0 && windDir < 22.5) return "N";
  if (windDir >= 22.5 && windDir < 67.5) return "NE";
  if (windDir >= 67.5 && windDir < 112.5) return "E";
  if (windDir >= 112.5 && windDir < 157.5) return "SE";
  if (windDir >= 157.5 && windDir < 202.5) return "S";
  if (windDir >= 202.5 && windDir < 247.5) return "SW";
  if (windDir >= 247.5 && windDir < 292.5) return "W";
  if (windDir >= 292.5 && windDir < 337.5) return "NW";
  return "N";
}

const char* id;
const char* name;
const char* countryCode;
const char* timezone;
const char* curTime;
const char* sunrise;
const char* sunset;
float windspeed, windspeedMax, temperature;
float humidity;
float pressure;
float density;
int direction;
int secPassed = 0;

void webSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.println("Disconnected from WebSocket server");
      break;

    case WStype_CONNECTED:
      Serial.println("Connected to WebSocket server");
      delay(40);
      isConSer = true;
      webSocket.sendTXT("{\"action\":\"subscribe\",\"channel\":{\"name\":\"station\",\"params\":{\"light\": true, \"where\":{\"_id\":\"630b8482358c9bfb807707d4\"}}}}");
      break;

    case WStype_TEXT: {
        Serial.printf("Message from server: %s\n", payload);

        StaticJsonDocument<1024> doc;
        DeserializationError error = deserializeJson(doc, payload);

        if (error) {
          Serial.print(F("deserializeJson() failed: "));
          Serial.println(error.f_str());
          return;
        }

        if (doc.containsKey("data")) {
          JsonObject data = doc["data"];

          id = data["id"] | "N/A";
          name = data["name"] | "N/A";
          countryCode = data["countryCode"] | "N/A";
          timezone = data["timezone"] | "N/A";
          curTime = data["currentTimeString"] | "N/A";
          sunrise = data["sunriseString"] | "N/A";
          sunset = data["sunsetString"] | "N/A";
          secPassed = 0;

          if (data.containsKey("lastWindData")) {
            JsonObject lastWindData = data["lastWindData"];
          
            windspeed = lastWindData["windspeed"] | -1.0;
            windspeedMax = lastWindData["windspeedHigh"] | -1.0;
            temperature = lastWindData["temperature"] | -1.0;
            direction = lastWindData["direction"] | 0;
            pressure = lastWindData["pressure"] | -1.0;
            humidity = lastWindData["humidity"] | -1;
            density = lastWindData["density"] | -1.0;
 
            Serial.println("------ Station Data ------");
            Serial.printf("Station ID: %s\n", id);
            Serial.printf("Name: %s\n", name);
            Serial.printf("Country Code: %s\n", countryCode);
            Serial.printf("Timezone: %s\n", timezone);
            Serial.printf("Current Time: %s\n", curTime);
            Serial.printf("Sunrise: %s\n", sunrise);
            Serial.printf("Sunset: %s\n", sunset);
            Serial.printf("Windspeed: %.2f m/s\n", windspeed);
            Serial.printf("Max Windspeed: %.2f m/s\n", windspeedMax);
            Serial.printf("Temperature: %.2f °C\n", temperature);
            Serial.printf("Direction: %d°\n", direction);
            Serial.printf("Pressure: %.1f hPa\n", pressure);
            Serial.printf("Humidity:%.1f  %%\n", humidity);
            Serial.printf("Density: %.4f kg/m³\n", density);
            Serial.println("--------------------------");
          
            refreshData();
          
          } else {
            Serial.println(F("lastWindData not found"));
          }
        } else {
          Serial.println(F("data not found"));
        }
        break;
      }

    case WStype_BIN:
      Serial.println("Binary message received");
      break;

    case WStype_PING:
      Serial.println("Ping received");
      break;

    case WStype_PONG:
      Serial.println("Pong received");
      break;

    case WStype_ERROR:
      Serial.println("WebSocket Error");
      break;
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  epd_init();
  framebuffer = (uint8_t *)ps_calloc(sizeof(uint8_t), EPD_WIDTH * EPD_HEIGHT / 2);
  if (!framebuffer) {
    Serial.println("❌ Framebuffer allocation failed!");
    while (true)
      ;
  }
  memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);  // clear

  epd_poweron();
  epd_clear();
  drawLayout();
  delay(1000);
  WiFiManager wifiManager;
  wifiManager.setConfigPortalTimeout(300);

  Serial.println("Connecting to Wi-Fi...");
  if (!wifiManager.autoConnect("KWind IoT", "")) {
    Serial.println("Failed to connect. Restarting...");
    delay(3000);
    ESP.restart();
  }

  Serial.println("Wi-Fi connected.");
  isCon = true;

  webSocket.beginSSL(websocket_server, websocket_port, websocket_path);
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);
}

void loop() {
  webSocket.loop();  // Needed to maintain connection
  delay(1);          // Prevent watchdog reset
}


void drawLayout() {
  // Initial display text, labels, and empty areas

  cursor_x = 440;
  cursor_y = 520;
  //
 
  // Clear the framebuffer
  memset(framebuffer, 0, sizeof(framebuffer));

  // Draw a horizontal "fat" line at a fixed y-coordinate
  int fixed_y = 80;         // Fixed y-coordinate for the horizontal line
  int line_thickness = 10;  // The thickness of the line

  // Draw multiple lines to create the "fat" line
  for (int i = 0; i < line_thickness; i++) {
    epd_draw_hline(0, 60 + i, EPD_WIDTH - 0, 0, framebuffer);  // Draw a line at y + offset
    epd_draw_hline(0, 520 + i, EPD_WIDTH - 0, 0, framebuffer);      // Draw a line at y + offset
  }

  epd_draw_grayscale_image(epd_full_screen(),framebuffer);

  cursor_x = 20;
  cursor_y = 140;
  writeln((GFXfont *)&OpenSans24B, "Wind_Speed", &cursor_x, &cursor_y, NULL);

  cursor_x = 20;
  cursor_y = 200;
  writeln((GFXfont *)&OpenSans24B, "Wind_Gust", &cursor_x, &cursor_y, NULL);

  cursor_x = 20;
  cursor_y = 260;
  writeln((GFXfont *)&OpenSans24B, "Dir", &cursor_x, &cursor_y, NULL);

  cursor_x = 20;
  cursor_y = 320;
  writeln((GFXfont *)&OpenSans24B, "Temp", &cursor_x, &cursor_y, NULL);

  cursor_x = 20;
  cursor_y = 380;
  writeln((GFXfont *)&OpenSans24B, "Hum", &cursor_x, &cursor_y, NULL);

  cursor_x = 710;
  cursor_y = 360;
writeln((GFXfont *)&OpenSans24B, "Scan Me", &cursor_x, &cursor_y, NULL);
cursor_x = 780;
cursor_y = 400;
writeln((GFXfont *)&OpenSans18B, "Get", &cursor_x, &cursor_y, NULL);

  cursor_x = 20;
  cursor_y = 440;
  //writeln((GFXfont *)&OpenSans24B, "bat", &cursor_x, &cursor_y, NULL);

  epd_draw_rect(10, (10, EPD_HEIGHT), (10, 60), (10, 120), 0, framebuffer);
///*
Rect_t areat = {
  .x = 130,
  .y = 0,  //titel
.width = titel_width,
  .height = titel_height,
};

//epd_draw_grayscale_image(areat, (uint8_t *)titel_data);
//epd_draw_image(areat, (uint8_t *)titel_data, BLACK_ON_WHITE);


  Rect_t area = {
        .x = 670,
        .y = 380,  //Kwind logo
      .width = logo1_width,
        .height = logo1_height,
   };

   epd_draw_grayscale_image(area, (uint8_t *)logo1_data);
  epd_draw_image(area, (uint8_t *)logo1_data, BLACK_ON_WHITE);



  Rect_t areaqr = {
    .x = 700,
    .y = 90,   //qr code
  .width = qr_width,
    .height = qr_height,
};

epd_draw_grayscale_image(areaqr, (uint8_t *)qr_data);
epd_draw_image(areaqr, (uint8_t *)qr_data, BLACK_ON_WHITE);


  Rect_t areap2 = {
    .x = 560,
    .y = 380,  //ecowitt logo
  .width = wind_width,
    .height = wind_height,
};


epd_draw_grayscale_image(areap2, (uint8_t *)wind_data);
epd_draw_image(areap2, (uint8_t *)wind_data, BLACK_ON_WHITE);

Rect_t areap4 = { //wind
  .x = 315,
  .y = 110,
.width = wind_width,
  .height = wind_height,
};

epd_draw_grayscale_image(areap4, (uint8_t *)wind_data);
epd_draw_image(areap4, (uint8_t *)wind_data, BLACK_ON_WHITE);

Rect_t areap3 = { //direction
  .x = 310,
  .y = 188,
.width = dir_width,
  .height = dir_height,
};

epd_draw_grayscale_image(areap3, (uint8_t *)dir_data);
epd_draw_image(areap3, (uint8_t *)dir_data, BLACK_ON_WHITE);


Rect_t areap5 = {
  .x = 330,
  .y = 285,
.width = temp_width,
  .height = temp_height,
};

epd_draw_grayscale_image(areap5, (uint8_t *)temp_data);
epd_draw_image(areap5, (uint8_t *)temp_data, BLACK_ON_WHITE);


Rect_t areap6 = {
  .x = 330,
  .y = 345,
.width = hum_width,
  .height = hum_height,
};

epd_draw_grayscale_image(areap6, (uint8_t *)hum_data);
epd_draw_image(areap6, (uint8_t *)hum_data, BLACK_ON_WHITE);

Rect_t areap7 = {
  .x = 330,
  .y = 405,
.width = bat_width,
  .height = bat_height,
};

//epd_draw_grayscale_image(areap7, (uint8_t *)bat_data);
//epd_draw_image(areap7, (uint8_t *)bat_data, BLACK_ON_WHITE);



}
void refreshData() {
  
  // Area 1: Update Wind Speed
  Rect_t area1 = { 430, 20 + custom_y, .width = 220, .height = 50 };
  epd_clear_area(area1);  // Clear previous data in the Wind Speed area
  char windSpeed[16];
  snprintf(windSpeed, sizeof(windSpeed), "%.1f  KNT", windspeed);
  cursor_x = 420;            // Starting X position for values
  cursor_y = 60 + custom_y;  // Starting Y position within the area
  writeln((GFXfont *)&OpenSans24B, windSpeed, &cursor_x, &cursor_y, NULL);


  // Area 2: Update Gust Speed
  Rect_t area2 = { 430, 80 + custom_y, .width = 220, .height = 50 };
  epd_clear_area(area2);  // Clear previous data in the Gust Speed area
  char windGust[16];
  snprintf( windGust, sizeof(windGust), "%.1f  KNT", windspeedMax);
  cursor_x = 420;             // Starting X position for values
  cursor_y = 120 + custom_y;  // Starting Y position within the area
  writeln((GFXfont *)&OpenSans24B,  windGust, &cursor_x, &cursor_y, NULL);

 // Area 3: Update Wind Direction
 Rect_t area3 = { 430, 140 + custom_y, .width = 260, .height = 50 };
 epd_clear_area(area3);  // Clear previous data in the Wind Direction area
 char windDirection[16];
 snprintf(windDirection, sizeof(windDirection), "%d° (%s)", direction, getCardinalDirection(direction).c_str());
 cursor_x = 420;             // Starting X position for values
 cursor_y = 180 + custom_y;  // Starting Y position within the area
 writeln((GFXfont *)&OpenSans24B, windDirection, &cursor_x, &cursor_y, NULL);

  
  //Area 4: Update Temperature
  Rect_t area4 = { 430, 200 + custom_y, .width = 220, .height = 50 };
  epd_clear_area(area4);  // Clear previous data in the Temperature area
  char temperatureC[16];
  snprintf(temperatureC, sizeof(temperatureC), "%.1f   °C", temperature);
  cursor_x = 420;             // Starting X position for values
  cursor_y = 240 + custom_y;  // Starting Y position within the area
  writeln((GFXfont *)&OpenSans24B, temperatureC, &cursor_x, &cursor_y, NULL);
 //Area 4: Update Temperature

 Rect_t area8 = { 430, 250 + custom_y, .width = 220, .height = 50 };
 epd_clear_area(area8);  // Clear previous data in the Temperature area
 char humidityC[16];
 snprintf(humidityC, sizeof(humidityC), "%.1f    %%", humidity);
 cursor_x = 420;             // Starting X position for values
 cursor_y = 300 + custom_y;  // Starting Y position within the area
 writeln((GFXfont *)&OpenSans24B, humidityC, &cursor_x, &cursor_y, NULL);


 // Area 6: Current Time
Rect_t area5 = { 380, 440 , .width = 200, .height = 40 };
epd_clear_area(area5);
char timeStr[32];
snprintf(timeStr, sizeof(timeStr), "%s", curTime);
cursor_x = 422;
cursor_y =480 ;
writeln((GFXfont *)&OpenSans24B, timeStr, &cursor_x, &cursor_y, NULL);

 // Area 6: Sunrise
Rect_t area6 = { 220, 320 + custom_y, .width = 120, .height = 50 };
epd_clear_area(area6);
char sunriseStr[32];
snprintf(sunriseStr, sizeof(sunriseStr), "Sunrise:      %s", sunrise);
cursor_x = 20;
cursor_y = 360 + custom_y;
writeln((GFXfont *)&OpenSans18B, sunriseStr, &cursor_x, &cursor_y, NULL);


// Area 7: Sunset
Rect_t area7 = { 220, 380 + custom_y, .width = 120, .height = 50 };
epd_clear_area(area7);
char sunsetStr[32];
snprintf(sunsetStr, sizeof(sunsetStr), "Sunset:       %s", sunset);
cursor_x = 20;
cursor_y = 420 + custom_y;
writeln((GFXfont *)&OpenSans18B, sunsetStr, &cursor_x, &cursor_y, NULL);

// Area 9: Station Name
Rect_t area9 = { 170, 0, .width = 400, .height = 40 };  // Adjust position as needed
epd_clear_area(area9);
char stationName[64];
snprintf(stationName, sizeof(stationName), "%s", name);
cursor_x = 240;
cursor_y = 50;
writeln((GFXfont *)&OpenSans24B, stationName, &cursor_x, &cursor_y, NULL);
}
