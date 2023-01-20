#define ENABLE_GxEPD2_GFX 0
#include <GxEPD2_7C.h>
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "config.h"
#include "config_private.h"

#define GxEPD2_DISPLAY_CLASS GxEPD2_7C
#define GxEPD2_DRIVER_CLASS GxEPD2_565c

// Set page height to 1 to ensure we allocate the least overhead possible, we don't need pages
GxEPD2_DISPLAY_CLASS<GxEPD2_DRIVER_CLASS, 1> display(GxEPD2_DRIVER_CLASS(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));

// Calculate bytes needed for buffer (+1 to force round up)
const uint32_t display_buffer_size = ((display.height() * display.width()) + 1) / 2;
#define buffer_malloc ps_malloc
uint8_t* display_buffer;

// Pixel format: 4-bit-per-pixel, indexed colors: 
#define COLOR_BLACK 0x00
#define COLOR_WHITE 0x01
#define COLOR_GREEN 0x02
#define COLOR_BLUE 0x03
#define COLOR_RED 0x04
#define COLOR_YELLOW 0x05
#define COLOR_ORANGE 0x06

WiFiClientSecure client;
RTC_DATA_ATTR char token[128];

void setup() {
  #ifdef PAPER_POWER_PIN
  pinMode(PAPER_POWER_PIN, OUTPUT);
  #endif

  Serial.begin(115200);

  SPI.end();
  SPI.begin(EPD_CLK, EPD_DOUT, EPD_DIN, EPD_CS);

  display_buffer = (uint8_t*)buffer_malloc(display_buffer_size);

  Serial.println("Init complete! Connecting WiFi...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("WiFi Failed!");
    return;
  }

  client.setCACert(IMAGE_URL_CA);

  Serial.println("WiFi connected!");
}

static bool drawNextImage() {
  token[sizeof(token) - 1] = 0;
  Serial.printf("Downloading image with token \"%s\"...", token); Serial.println();

  if (!client.connect(IMAGE_URL_HOST, 443)) {
    Serial.println("Connection failed!");
    return false;
  }

  client.printf("GET " IMAGE_URL " HTTP/1.1", token); client.println();
  client.println("Host: " IMAGE_URL_HOST);
  client.println("Connection: close");
  client.println();

  while (client.connected()) {
    const String line = client.readStringUntil('\n');

    const int idx = line.indexOf(":");
    if (idx > 0) {
      String headerName = line.substring(0, idx);
      headerName.toLowerCase();
      if (headerName == "token") {
        String headerValue = line.substring(idx + 1);
        headerValue.trim();
        strncpy(token, headerValue.c_str(), sizeof(token));
        Serial.printf("Got new token: %s", token); Serial.println();
      }
    }

    if (line == "\r") {
      Serial.println("Headers received. Fetching body...");
      break;
    }
  }

  uint32_t buffer_load_left = display_buffer_size;
  uint8_t* buffer_load = display_buffer;
  while ((client.connected() || client.available()) && buffer_load_left > 0) {
    int len = client.read(buffer_load, buffer_load_left);
    if (len <= 0) {
      continue;
    }
    buffer_load += len;
    buffer_load_left -= len;
  }

  const int data_left = client.available();
  client.stop();

  if (data_left) {
    Serial.print("Failed to download image! Too big: ");
    Serial.println(data_left);
    return false;
  }

  if (buffer_load_left > 0) {
    Serial.print("Failed to download image! Too small! Missing bytes: ");
    Serial.println(buffer_load_left);
    return false;
  }

  Serial.println("Downloading complete. Drawing image...");
  display.drawNative(display_buffer, 0, 0, 0, display.width(), display.height(), false, false, false);
  Serial.println("Drawing done!");
  return true;
}

void loop() {
  Serial.println("Begin loop()");

  #ifdef PAPER_POWER_PIN
  digitalWrite(PAPER_POWER_PIN, PAPER_POWER_PIN_ON);
  #endif
  display.init(115200);

  unsigned long sleepTime = PAPER_ESP_ERROR_SLEEP;
  Serial.println("Begin drawNextImage()");
  if (drawNextImage()) {
    sleepTime = PAPER_ESP_OK_SLEEP;
  }
  Serial.println("End drawNextImage()");

  Serial.println("Begin display.hibernate()");
  display.hibernate();
  Serial.println("End display.hibernate()");

  #ifdef PAPER_POWER_PIN
  digitalWrite(PAPER_POWER_PIN, PAPER_POWER_PIN_OFF);
  #endif

  Serial.println("End loop()");
  PaperESPSleep(sleepTime);
}
