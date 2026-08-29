#include <Fonts/Picopixel.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

#include <WiFi.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ESPAsyncWebServer.h>
#include <WebSerial.h>

#include "secrets.h"
#include "images.h"

const int trigPin = 21;
const int echoPin = 22;

float duration, distance;


// -------------------------------------
// WebSerial
// -------------------------------------

AsyncWebServer server(80);


// -------------------------------------
// Distance ranges
// -------------------------------------

// Full-screen red range
const float FULL_RED_MIN_DISTANCE = 1.0;
const float FULL_RED_MAX_DISTANCE = 20.0;

// Full-screen green range
const float FULL_GREEN_MIN_DISTANCE = 20.0;
const float FULL_GREEN_MAX_DISTANCE = 40.0;

// Rainbow bar range
const float BAR_MIN_DISTANCE = 40.0;
const float BAR_MAX_DISTANCE = 150.0;


// -------------------------------------
// Solid color timeout
// -------------------------------------

const unsigned long SOLID_COLOR_TIMEOUT = 60000;  // 60 seconds

unsigned long solidColorStartTime = 0;
bool solidColorActive = false;


// -------------------------------------
// Distance filtering
// -------------------------------------

const int SAMPLE_COUNT = 5;

float samples[SAMPLE_COUNT];
int sampleIndex = 0;
int validSamples = 0;


// -------------------------------------
// Matrix Config
// -------------------------------------

const int panelResX = 64;
const int panelResY = 64;
const int panel_chain = 1;


// -------------------------------------

MatrixPanel_I2S_DMA *dma_display = nullptr;

uint16_t myBLACK;
uint16_t myWHITE;
uint16_t myRED;
uint16_t myGREEN;
uint16_t myBLUE;
uint16_t myGRAY;


// -------------------------------------
// Convert rainbow hue to RGB
// hue: 0-255
// -------------------------------------

void hueToRGB(uint8_t hue, uint8_t &r, uint8_t &g, uint8_t &b) {

  if (hue < 43) {
    r = 255;
    g = hue * 6;
    b = 0;
  }
  else if (hue < 85) {
    r = 255 - (hue - 43) * 6;
    g = 255;
    b = 0;
  }
  else if (hue < 128) {
    r = 0;
    g = 255;
    b = (hue - 85) * 6;
  }
  else if (hue < 170) {
    r = 0;
    g = 255 - (hue - 128) * 6;
    b = 255;
  }
  else if (hue < 213) {
    r = (hue - 170) * 6;
    g = 0;
    b = 255;
  }
  else {
    r = 255;
    g = 0;
    b = 255 - (hue - 213) * 6;
  }
}


// -------------------------------------
// Calculate median of the samples
// -------------------------------------

float getMedian() {

  float sorted[SAMPLE_COUNT];

  // Copy samples
  for (int i = 0; i < validSamples; i++) {
    sorted[i] = samples[i];
  }

  // Simple sort
  for (int i = 0; i < validSamples - 1; i++) {
    for (int j = i + 1; j < validSamples; j++) {

      if (sorted[j] < sorted[i]) {
        float temp = sorted[i];
        sorted[i] = sorted[j];
        sorted[j] = temp;
      }
    }
  }

  // Odd number of samples -> middle value
  if (validSamples % 2 == 1) {
    return sorted[validSamples / 2];
  }

  // Even number of samples -> average middle two
  return (sorted[validSamples / 2 - 1] +
          sorted[validSamples / 2]) / 2.0;
}


// -------------------------------------
// Display setup
// -------------------------------------

void displaySetup() {

  HUB75_I2S_CFG mxconfig(
    panelResX,
    panelResY,
    panel_chain
  );

  mxconfig.gpio.e = 18;
  mxconfig.clkphase = false;

  // Enable double buffering
  mxconfig.double_buff = true;

  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();

  // dma_display->setBrightness8(64);

  myBLACK = dma_display->color565(0, 0, 0);
  myWHITE = dma_display->color565(255, 255, 255);
  myRED   = dma_display->color565(255, 0, 0);
  myGREEN = dma_display->color565(0, 255, 0);
  myBLUE  = dma_display->color565(0, 0, 255);
  myGRAY  = dma_display->color565(32, 32, 32);
}


// -------------------------------------
// Setup
// -------------------------------------

void setup() {

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  Serial.begin(115200);

  Serial.println();
  Serial.println("Starting garage proximity display...");


  // -------------------------------------
  // WiFi
  // -------------------------------------

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {

    delay(1000);

    Serial.println("Connecting to WiFi...");
  }

  Serial.println("Connected to WiFi");

  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());


  // -------------------------------------
  // WebSerial
  // -------------------------------------

  WebSerial.begin(&server);

  server.begin();

  WebSerial.println();
  WebSerial.println("==============================");
  WebSerial.println("Garage proximity display");
  WebSerial.println("WebSerial is ready");
  WebSerial.println("==============================");


  // -------------------------------------
  // OTA
  // -------------------------------------

  ArduinoOTA

    .onStart([]() {

      String type;

      if (ArduinoOTA.getCommand() == U_FLASH) {
        type = "sketch";
      }
      else {
        type = "filesystem";
      }

      Serial.println("Start updating " + type);
      WebSerial.println("Start updating " + type);
    })

    .onEnd([]() {

      Serial.println("\nOTA End");
      WebSerial.println("\nOTA End");
    })

    .onProgress([](unsigned int progress, unsigned int total) {

      unsigned int percent =
        (progress * 100) / total;

      Serial.printf(
        "\rOTA Progress: %u%%",
        percent
      );

    })

    .onError([](ota_error_t error) {

      Serial.printf(
        "\nOTA Error[%u]: ",
        error
      );

      switch (error) {

        case OTA_AUTH_ERROR:

          Serial.println("Auth Failed");
          WebSerial.println("OTA Error: Auth Failed");

          break;

        case OTA_BEGIN_ERROR:

          Serial.println("Begin Failed");
          WebSerial.println("OTA Error: Begin Failed");

          break;

        case OTA_CONNECT_ERROR:

          Serial.println("Connect Failed");
          WebSerial.println("OTA Error: Connect Failed");

          break;

        case OTA_RECEIVE_ERROR:

          Serial.println("Receive Failed");
          WebSerial.println("OTA Error: Receive Failed");

          break;

        case OTA_END_ERROR:

          Serial.println("End Failed");
          WebSerial.println("OTA Error: End Failed");

          break;

        default:

          Serial.println("Unknown error");
          WebSerial.println("OTA Error: Unknown error");

          break;
      }
    });


  ArduinoOTA.begin();


  // -------------------------------------
  // Display
  // -------------------------------------

  displaySetup();

  dma_display->clearScreen();
  dma_display->fillScreen(myBLACK);
  dma_display->setTextWrap(false);

  dma_display->drawRGBBitmap(
    0,
    0,
    epd_bitmap_car,
    64,
    64
  );

  dma_display->setCursor(2, 0);
  dma_display->setTextColor(myWHITE);
  dma_display->setTextSize(1);
  dma_display->setFont(&Picopixel);

  dma_display->println(
    "STARTING UP..."
  );

  dma_display->setCursor(2, 12);

  dma_display->println(
    "IP: " + WiFi.localIP().toString()
  );

  dma_display->flipDMABuffer();

  delay(1000);

  Serial.println("Ready");
  Serial.print("WebSerial: http://");
  Serial.print(WiFi.localIP());
  Serial.println("/webserial");

  WebSerial.println("Ready");
  WebSerial.print("WebSerial: http://");
  WebSerial.print(WiFi.localIP());
  WebSerial.println("/webserial");
}


// -------------------------------------
// Main loop
// -------------------------------------

void loop() {

  // -------------------------------------
  // Keep OTA responsive
  // -------------------------------------

  ArduinoOTA.handle();


  // -------------------------------------
  // Trigger ultrasonic sensor
  // -------------------------------------

  digitalWrite(
    trigPin,
    LOW
  );

  delayMicroseconds(2);

  digitalWrite(
    trigPin,
    HIGH
  );

  delayMicroseconds(10);

  digitalWrite(
    trigPin,
    LOW
  );


  // -------------------------------------
  // Read echo
  // -------------------------------------

  // 15ms timeout prevents pulseIn() from
  // blocking for approximately one second
  // when no echo is received.
  duration = pulseIn(
    echoPin,
    HIGH,
    15000
  );


  // -------------------------------------
  // Handle missing echo
  // -------------------------------------

  if (duration == 0) {

    Serial.println("No echo received");
    WebSerial.println("No echo received");

    ArduinoOTA.handle();

    delay(50);

    return;
  }


  float newDistance =
    (duration * 0.0343) / 2;


  // -------------------------------------
  // Add sample to circular buffer
  // -------------------------------------

  samples[sampleIndex] =
    newDistance;

  sampleIndex =
    (sampleIndex + 1) % SAMPLE_COUNT;

  if (validSamples < SAMPLE_COUNT) {
    validSamples++;
  }


  // -------------------------------------
  // Calculate median
  // -------------------------------------

  distance = getMedian();


  // -------------------------------------
  // Debug output
  // -------------------------------------

  // Limit debug output to 5 messages/second.
  static unsigned long lastDebugTime = 0;

  if (millis() - lastDebugTime >= 200) {

    lastDebugTime = millis();

    String debugMessage = "Raw: " + String(newDistance, 1) +
                          " cm  Median: " + String(distance, 1) +
                          " cm";

    Serial.println(debugMessage);
    WebSerial.println(debugMessage);
  }


  // -------------------------------------
  // Determine current color range
  // -------------------------------------

  bool inGreenRange =
    distance >= FULL_GREEN_MIN_DISTANCE &&
    distance <= FULL_GREEN_MAX_DISTANCE;

  bool inRedRange =
    distance >= FULL_RED_MIN_DISTANCE &&
    distance <= FULL_RED_MAX_DISTANCE;

  bool inSolidColorRange =
    inGreenRange || inRedRange;


  // -------------------------------------
  // Track time spent in solid color range
  // -------------------------------------

  if (inSolidColorRange) {

    if (!solidColorActive) {

      solidColorActive = true;

      solidColorStartTime =
        millis();
    }

  }
  else {

    solidColorActive = false;
  }


  // -------------------------------------
  // Determine whether solid color
  // should still be displayed
  // -------------------------------------

  bool solidColorTimedOut =
    solidColorActive &&
    (millis() - solidColorStartTime >=
     SOLID_COLOR_TIMEOUT);


  // -------------------------------------
  // Draw frame to back buffer
  // -------------------------------------

  dma_display->clearScreen();


  // -------------------------------------
  // Full-screen green range
  // -------------------------------------

  if (inGreenRange &&
      !solidColorTimedOut) {

    dma_display->drawRGBBitmap(
      0,
      0,
      epd_bitmap_smiling_cat_face_with_heart_eyes,
      64,
      64
    );
  }


  // -------------------------------------
  // Full-screen red range
  // -------------------------------------

  else if (inRedRange &&
           !solidColorTimedOut) {

    dma_display->drawRGBBitmap(
      0,
      0,
      epd_bitmap_woman_gesturing_no_2,
      64,
      64
    );
  }


  // -------------------------------------
  // Rainbow bar range
  //
  // BAR_MIN_DISTANCE = 100%
  // BAR_MAX_DISTANCE = 0%
  // -------------------------------------

  else if (
    distance > BAR_MIN_DISTANCE &&
    distance < BAR_MAX_DISTANCE
  ) {

    float progress =
      1.0 - (
        (distance - BAR_MIN_DISTANCE) /
        (BAR_MAX_DISTANCE - BAR_MIN_DISTANCE)
      );

    // Clamp to 0.0 - 1.0
    progress =
      constrain(progress, 0.0, 1.0);

    // Calculate number of pixels to display
    int width =
      progress * panelResX;


    // -------------------------------------
    // Draw rainbow
    // -------------------------------------

    for (int x = 0; x < width; x++) {

      uint8_t hue =
        map(
          x,
          0,
          panelResX - 1,
          0,
          255
        );

      uint8_t r;
      uint8_t g;
      uint8_t b;

      hueToRGB(
        hue,
        r,
        g,
        b
      );

      uint16_t color =
        dma_display->color565(
          r,
          g,
          b
        );

      dma_display->drawFastVLine(
        x,
        1,
        62,
        color
      );
    }


    // -------------------------------------
    // Draw border once
    // -------------------------------------

    dma_display->drawFastVLine(
      0,
      0,
      64,
      myGRAY
    );

    dma_display->drawFastVLine(
      63,
      0,
      64,
      myGRAY
    );

    dma_display->drawFastHLine(
      0,
      0,
      64,
      myGRAY
    );

    dma_display->drawFastHLine(
      0,
      63,
      64,
      myGRAY
    );
  }


  // -------------------------------------
  // Show completed frame
  // -------------------------------------

  dma_display->flipDMABuffer();


  // -------------------------------------
  // Keep OTA responsive
  // -------------------------------------

  ArduinoOTA.handle();

  delay(50);
}