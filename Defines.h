#include <century_gothic_60.h>
#include <century_24.h>

#define SDA 23
#define SCL 22
#define RST 33
#define INT 32

// Pages to display, creates navigation via switch
const uint8_t pageBlueRing = 0;
const uint8_t pageGreenRing = 1;

uint8_t selectedPage = pageBlueRing;
uint8_t maxPageNumber = 1;
bool displayUpdate = true;  // Only draw the entire tft when the page needs to be wiped

// LCD colors
const uint16_t colorCalmBlue = 0x02F5;
const uint16_t colorLightBlue = 0x3BDB;
const uint16_t colorMossGreen = 0x4B28;
const uint16_t colorWhite = 0xFFFF;
const uint16_t colorBlack = 0x0000;

uint16_t colorBackground = colorCalmBlue;
uint16_t colorText = colorWhite;

// LCD constants
const int centerX = 120;    // Center X of the display
const int centerY = 120;    // Center Y of the display
const int maxRadius = 120;  // Maximum visible radius of the round display, waveshare 1.28"
const int ringWidth = 6;    // Width of the ring

// GPIO
const uint8_t powerEnable = 26;
const uint8_t batteryPin = 35;
const uint8_t motionSensePin = 34;

const uint16_t backlightLevel = 255;    // Pin defined in tft_espi library

// Timer for sensor updates
unsigned long previousTimeCheckMillis = 0;
const long sensorCheckInterval = 5000;

// Touchscreen items - slow down response to limit multitouch events
bool touchNavDebounce = false;
bool touchNavEnabled = false;
unsigned long previousMillisTouchEvent = 0;
uint16_t touchNavigationDebounceTime = 150;

// Battery level items
const float R1 = 100000.0; // 100k ohms resistor divider
const float R2 = 100000.0;

const int adcResolution = 4095; // 12-bit ADC resolution
const float adcReferenceVoltage = 3.3;  // ADC reference voltage 
const float calibrationFactor = 1.101; // Adjust this factor to calibrate the measured voltage

// Sensor values and MQTT strings, battery is a string return function
String batteryVoltage = "V";
String currentTemperature = "T";
String currentHumidity = "H";
String currentDownstairsTemp = "D";
float currentTemperatureValue = 0.0;
float currentHumidityValue = 0.0;
String officeLightControl = "false";

