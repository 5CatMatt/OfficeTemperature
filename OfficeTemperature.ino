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

TFT_eSprite casicoTempRect = TFT_eSprite(&tft);
TFT_eSprite casicoHumCircle = TFT_eSprite(&tft);
TFT_eSprite casicoMainBody = TFT_eSprite(&tft);
TFT_eSprite casicoMainBodyTop = TFT_eSprite(&tft);
TFT_eSprite casicoMainBodyLeft = TFT_eSprite(&tft);
TFT_eSprite casicoMainBodyRight = TFT_eSprite(&tft);

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
    officeLightControl = "true";
  } else {
    analogWrite(BACKLIGHT, 0);
    officeLightControl = "false";
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
    case pageBlueRing: 
      DrawBlueRingPage();
      break;
    case pageGreenRing:
      DrawGreenRingPage();
      break;
    case pageGreen:
      DrawGreenPage();
      break;
    case pageCasico:
      // currentCasicoDrawMillis = millis();
      // if (currentCasicoDrawMillis - previousCasicoDrawMillis >= casicoDrawInterval) {
      //   previousCasicoDrawMillis = currentCasicoDrawMillis;
    
      //   DrawCasicoPage();
      // }    
      DrawCasicoPage();  
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

    tft.drawSmoothArc(centerX, centerY, maxRadius, maxRadius - ringWidth, 0, 360, colorWhite, colorBackground);
  }
  
  drawCenteredTextSprite(centerText, currentTemperature + "°", CenturyGothic60, 40, 90, MC_DATUM);

  drawCenteredTextSprite(centerSubText, currentHumidity + "%", CenturyGothic24, 80, 150, MC_DATUM);

  drawCenteredTextSprite(upperSubText, currentDownstairsTemp + "°", CenturyGothic24, 80, 30, MC_DATUM);
}

void DrawGreenRingPage() {
  // Green page with white outter ring and centered text elements
  colorBackground = colorMossGreen;
  colorText = colorWhite;

  if (displayUpdate) {
    displayUpdate = false;

    tft.fillScreen(colorMossGreen);

    tft.drawSmoothArc(centerX, centerY, maxRadius, maxRadius - ringWidth, 0, 360, colorWhite, colorBackground);
  }

  drawCenteredTextSprite(centerText, currentTemperature + "°", CenturyGothic60, 40, 90, MC_DATUM);

  drawCenteredTextSprite(centerSubText, currentHumidity + "%", CenturyGothic24, 80, 150, MC_DATUM);
}

void DrawGreenPage() {
  // Green page
  tft.fillScreen(colorMossGreen);
}

void DrawCasicoPage() {
  if (displayUpdate) {
    displayUpdate = false;

    // tft.fillScreen(colorCasioDarkBG);

    // tft.pushImage(77, 18, 90, 14, casicoLogo);
    // tft.pushImage(154, 88, 23, 24, casicoDegrees);

    tft.drawSmoothArc(centerX, centerY, maxRadius, maxRadius - ringWidth, 0, 360, colorCasioDarkRing, colorCasioDarkRing);

    tft.fillSmoothCircle(centerX, centerY, 114, colorCasioDarkBG, colorCasioDarkRing);

    tft.pushImage(77, 26, 87, 12, casicoBlueLogo);
    tft.pushImage(55, 42, 132, 8, casicoSubLogo);
    tft.pushImage(107, 64, 38, 10, casicoTemp);
    tft.pushImage(112, 95, 34, 11, casicoHum);

    tft.pushImage(40, 118, 121, 15, casicoArea1);
    tft.pushImage(161, 129, 29, 5, casicoArea2);
    tft.pushImage(186, 134, 18, 58, casicoArea3);
    tft.pushImage(46, 192, 148, 11, casicoArea4);
    tft.pushImage(36, 183, 13, 11, casicoArea5);
    tft.pushImage(32, 133, 13, 50, casicoArea6);

    tft.pushImage(146, 60, 63, 63, casicoCircle);

    tft.pushImage(32, 60, 74, 5, casicoSquare1);
    tft.pushImage(102, 65, 4, 45, casicoSquare2);
    tft.pushImage(32, 65, 4, 45, casicoSquare3);
    tft.pushImage(36, 106, 66, 4, casicoSquare4);

    // Main body polygon
    tft.fillRect(50, 133, 135, 59, colorCasioGrey);
    tft.fillRect(45, 133, 5, 50, colorCasioGrey);
    tft.fillRect(49, 183, 1, 9, colorCasioGrey);
    tft.fillRect(185, 134, 1, 58, colorCasioGrey);

    // Temperature rectange
    tft.fillRect(36, 65, 66, 41, colorCasioGrey);
    
  }

  colorBackground = colorCalmBlue;
  colorText = colorCasioDarkBG;

  // Main body polygon - time of day
  drawCenteredTextSprite(casicoMainBody, "1:34", Segment755, 56, 154, MC_DATUM);

  drawCenteredTextSprite(casicoMainBodyTop, "Monday", CenturyGothic24, 58, 125, MC_DATUM);

  drawCenteredTextSprite(casicoMainBodyLeft, "F", CenturyGothic24, 38, 140, MC_DATUM);

  drawCenteredTextSprite(casicoMainBodyRight, "A", CenturyGothic24, 182, 147, MC_DATUM);

  // Temperature rectange
  drawCenteredTextSprite(casicoTempRect, currentTemperature, Segment34, 36, 70, MC_DATUM);

  // Humidity circle
  drawCenteredTextSprite(casicoHumCircle, String(currentHumidityValue, 0), Segment24, 160, 80, MC_DATUM);

  int humidityFillValue = map(currentHumidityValue, 0, 100, 0, 360);
  tft.drawArc(177, 92, 26, 22, 2, humidityFillValue, colorLightBlue, colorCasioGrey);
  tft.drawArc(177, 92, 26, 22, humidityFillValue, 363, colorCasioDarkBG, colorCasioGrey);

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
    mqtt.publish(TOPIC_MOTION, officeLightControl.c_str());
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
  // Configure LCD initial state, tft can be used if the element is static, sprite should be used for dynamic objects
  // NOTE full screen sprite is not possible, memory will overflow without error. If the sprite can't create memory is the cause
  tft.init();
  tft.setRotation(3);
  tft.setSwapBytes(true);
  tft.fillScreen(colorCalmBlue);

  // This will dump creation errors to the serial port
  CreateAndFillSprite(centerText, 160, 60, colorCalmBlue, "centerText");

  CreateAndFillSprite(centerSubText, 80, 25, colorCalmBlue, "centerSubText");

  CreateAndFillSprite(upperSubText, 80, 25, colorCalmBlue, "upperSubText");

  CreateAndFillSprite(casicoTempRect, 66, 30, colorCasioGrey, "casicoTempRect");

  CreateAndFillSprite(casicoHumCircle, 35, 25, colorCasioGrey, "casicoHumCircle");

  CreateAndFillSprite(casicoMainBody, 126, 44, colorCasioGrey, "casicoMainBody");

  CreateAndFillSprite(casicoMainBodyTop, 90, 28, colorCasioGrey, "casicoMainBodyTop");

  CreateAndFillSprite(casicoMainBodyLeft, 17, 40, colorCasioGrey, "casicoMainBodyLeft");

  CreateAndFillSprite(casicoMainBodyRight, 15, 37, colorCasioGrey, "casicoMainBodyRight");

  // Backlight is defined in the tft_espi library
  analogWrite(BACKLIGHT, backlightLevel);

  touch.begin();
}

void CreateAndFillSprite(TFT_eSprite &sprite, int width, int height, uint16_t fillColor, const String &spriteName) {
  if (!sprite.createSprite(width, height)) {
    Serial.print("Sprite (");
    Serial.print(spriteName);
    Serial.println(") creation failed!");
  } else {
    sprite.fillSprite(fillColor);
  }
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
