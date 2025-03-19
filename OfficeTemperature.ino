#include <WiFi.h>
#include <ArduinoOTA.h>
#include <PicoMQTT.h>
#include <Wire.h>
#include <Adafruit_SHT4x.h>

#include <Defines.h>
#include <Secrets.h>

PicoMQTT::Client mqtt(BROKER_IP);

// Environment sensor
#include <Adafruit_SHT4x.h>
Adafruit_SHT4x sht4 = Adafruit_SHT4x();

// Graphics
#include <TFT_eSPI.h>
TFT_eSPI tft = TFT_eSPI();

TFT_eSprite centerText = TFT_eSprite(&tft);
TFT_eSprite centerSubText = TFT_eSprite(&tft);
TFT_eSprite upperSubText = TFT_eSprite(&tft);

// Touchscreen
#include <CST816S.h>
CST816S touch(SDA, SCL, RST, INT);

void setup() {
  Serial.begin(115200);

  // setup GPIO
  pinMode(powerEnable, OUTPUT);
  digitalWrite(powerEnable, HIGH);  // LOW state disables the esp32 EN pin resulting in full shutdown - TODO wakeup not supported yet

  pinMode(motionSensePin, INPUT);

  // Enable i2c on custom pins
  Wire.begin(SDA, SCL);

  // Climate sensor
  SetupSHT41();

  // Enable draw and touch
  SetupLCD();

  // Enable wireless
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" CONNECTED");

  SetupOTA();

  // Start MQTT as client
  mqtt.begin();

  mqtt.subscribe("Outside Temperature", [](const String& topic, const String& payload) {
    if (topic == "Outside Temperature") {
      currentDownstairsTemp = payload;
    }
  });
}

void loop() {
  // Network items
  ArduinoOTA.handle();
  mqtt.loop();

  // Handle touch events - TODO orientation is not implemented yet
  NavigationDebounce();

  if (touch.available()) {
    if (touchNavEnabled) {
      selectedPage++;
      if (selectedPage > maxPageNumber) { selectedPage = 0; }
      displayUpdate = true;
      touchNavEnabled = false;
    }
  }

  // Motion detection, TODO handle low power items and announce state via MQTT for home assistant functions
  if (digitalRead(motionSensePin)) {
    analogWrite(BACKLIGHT, backlightLevel);
  } else {
    analogWrite(BACKLIGHT, 0);
  }

  // Page navigation
  PageNavigation();
  
  // Timed events - currently sensors and MQTT publishing
  unsigned long currentMillis = millis();

  if (currentMillis - previousTimeCheckMillis >= sensorCheckInterval) {
    previousTimeCheckMillis = currentMillis;

    ProcessSensors();
  }
}

// **************** Draw Functions - Pages **************** //

void PageNavigation() {
  switch (selectedPage) {
    case 0: 
      DrawBlueRingPage();
      break;
    case 1:
      DrawGreenRingPage();
      break;
  }
}

void DrawBlueRingPage() {
  // Blue page with white outter ring and centered text elements
  colorBackground = colorCalmBlue;
  colorText = colorWhite;

  if (displayUpdate) {
    displayUpdate = false;

    tft.fillScreen(colorCalmBlue);

    tft.drawArc(centerX, centerY, maxRadius, maxRadius - ringWidth, 0, 360, colorWhite, colorWhite);
  }
  
  drawCenteredTextSprite(centerText, currentTemperature + "°", cenGoth60, 40, 90, MC_DATUM);

  drawCenteredTextSprite(centerSubText, currentHumidity + "%", cenGoth24, 80, 150, MC_DATUM);

  drawCenteredTextSprite(upperSubText, currentDownstairsTemp + "°", cenGoth24, 80, 30, MC_DATUM);
}

void DrawGreenRingPage() {
  // Green page with white outter ring and centered text elements
  colorBackground = colorMossGreen;
  colorText = colorWhite;

  if (displayUpdate) {
    displayUpdate = false;

    tft.fillScreen(colorMossGreen);

    tft.drawArc(centerX, centerY, maxRadius, maxRadius - ringWidth, 0, 360, colorWhite, colorWhite);
  }

  drawCenteredTextSprite(centerText, currentTemperature + "°", cenGoth60, 40, 90, MC_DATUM);

  drawCenteredTextSprite(centerSubText, currentHumidity + "%", cenGoth24, 80, 150, MC_DATUM);
}

// **************** Draw Functions - Helpers **************** //

void drawCenteredTextSprite(TFT_eSprite &sprite, const String &text, const uint8_t* font, int x, int y, uint8_t textDatum) {
  sprite.fillSprite(colorBackground);
  sprite.setTextColor(colorText, colorBackground);
  sprite.setTextDatum(textDatum);

  sprite.loadFont(font);
  sprite.drawString(text, sprite.width() / 2, sprite.height() / 2);
  sprite.unloadFont();

  sprite.pushSprite(x, y);
}

// **************** Sensor Functions **************** //

void ProcessSensors() {
  // Populate temp and humidity objects with fresh data
  sensors_event_t humidity, temp;

  sht4.getEvent(&humidity, &temp);

  // Convert to F and round to one decimal, both values are floats
  currentTemperatureValue = round((9.0/5.0 * temp.temperature + 32) * 10) / 10;  
  currentHumidityValue = round(humidity.relative_humidity * 10) / 10;

  // Update global strings with new data, rounding leaves a zero so trim to one decimal place
  currentTemperature = String(currentTemperatureValue, 1);
  currentHumidity = String(currentHumidityValue, 1);
  
  // Get the battery voltage, resistor divider on ADC pin
  batteryVoltage = CheckBattery();

  // Update the network with sensor values
  PublishMQTT();
}

String CheckBattery() {
  const int numSamples = 10; // Number of samples to average
  int totalValue = 0;

  // Take multiple readings and calculate the average, improves accuracy a bit, delay (5) might be a bit long but is working
  for (int i = 0; i < numSamples; i++) {
    totalValue += analogRead(batteryPin);
    delay(5);
  }

  int averagedValue = totalValue / numSamples;

  // Calculate the voltage at the pin, calibrationFactor was set manually with meter, accuracy is decent
  float measuredVoltage = (averagedValue * adcReferenceVoltage) / adcResolution * calibrationFactor;

  // Calculate the actual battery voltage
  float batteryVoltage = measuredVoltage * (R1 + R2) / R2;

  // Convert the battery voltage to a string
  String batteryVoltageString = String(batteryVoltage, 2);

  // Return the battery voltage as a string
  return batteryVoltageString;
}

void SetupSHT41() {
  // Initialize the temp/hum sensor (adafruit breakout SHT41)
  // Most of this is serial port updates, can be removed as OTA does not provide serial
  Serial.println("Adafruit SHT4x test");
  if (!sht4.begin()) {
    Serial.println("Couldn't find SHT4x");
    while (1) delay(1);
  }
  Serial.println("Found SHT4x sensor");
  Serial.print("Serial number 0x");
  Serial.println(sht4.readSerial(), HEX);

  sht4.setPrecision(SHT4X_HIGH_PRECISION);
  switch (sht4.getPrecision()) {
    case SHT4X_HIGH_PRECISION: 
      Serial.println("High precision");
      break;
    case SHT4X_MED_PRECISION: 
      Serial.println("Med precision");
      break;
    case SHT4X_LOW_PRECISION: 
      Serial.println("Low precision");
      break;
  }

  sht4.setHeater(SHT4X_NO_HEATER);
}

// **************** Network Functions **************** //

void SubscribeMQTT() {

}

void PublishMQTT() {
  if (mqtt.connected()) {
    mqtt.publish(TOPIC_BATTERY, batteryVoltage.c_str());
    mqtt.publish(TOPIC_TEMPERATURE, currentTemperature.c_str());
    mqtt.publish(TOPIC_HUMIDITY, currentHumidity.c_str());
  } else {
    // Just keep going, MQTT failed but will reconnect automatically
    Serial.println("MQTT not connected.");
  }
}

void SetupOTA() {
  // Authentication details
  ArduinoOTA.setHostname(OTA_HOST_NAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);

  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH) {
        type = "sketch";
      } else {  // U_SPIFFS
        type = "filesystem";
      }

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) {
        Serial.println("Auth Failed");
      } else if (error == OTA_BEGIN_ERROR) {
        Serial.println("Begin Failed");
      } else if (error == OTA_CONNECT_ERROR) {
        Serial.println("Connect Failed");
      } else if (error == OTA_RECEIVE_ERROR) {
        Serial.println("Receive Failed");
      } else if (error == OTA_END_ERROR) {
        Serial.println("End Failed");
      }
    });

  ArduinoOTA.begin();
}

// **************** User Interface Functions **************** //

void SetupLCD() {
  // Configure LCD initial state, tft can be used if the page is static, sprite should be used for dynamic objects
  // NOTE full screen sprite is not possible, memory will overflow without error. If the sprite can't create memory is the cause
  tft.init();
  tft.setRotation(3);
  tft.setSwapBytes(true);
  tft.fillScreen(colorCalmBlue);

  if (!centerText.createSprite(160, 60)) { Serial.println("Sprite (centerText) creation failed!"); }
  centerText.fillSprite(colorCalmBlue);

  if (!centerSubText.createSprite(80, 25)) { Serial.println("Sprite (centerSubText) creation failed!"); }
  centerSubText.fillSprite(colorCalmBlue);

  if (!upperSubText.createSprite(80, 25)) { Serial.println("Sprite (upperSubText) creation failed!"); }
  upperSubText.fillSprite(colorCalmBlue);

  analogWrite(BACKLIGHT, backlightLevel);

  touch.begin();
}

void NavigationDebounce() {
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillisTouchEvent >= touchNavigationDebounceTime) {
    previousMillisTouchEvent = currentMillis;

    if (touchNavDebounce) {
      touchNavEnabled = true;
    } else {
      touchNavEnabled = false;
      if (touch.available()) {}
    }

    touchNavDebounce = !touchNavDebounce;
  }

}
