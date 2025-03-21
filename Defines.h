#include <CenturyGothic24.h>
#include <CenturyGothic60.h>
#include <AlarmClock60.h> // no special chars, only nums
#include <Segment760.h> // all chars but not very close to old watch
#include <DigitalDisplayRegular60.h> // no special chars, only nums
#include <DigitalDisplayRegular76.h>
#include <Segment76.h>
#include <Segment34.h>
#include <Segment24.h>

#include <CasicoLogo.h>
#include <CasicoDegrees.h>

#include <casicoBlueLogo.h>
#include <casicoSubLogo.h>
#include <casicoTemp.h>
#include <casicoHum.h>
#include <casicoArea1.h>
#include <casicoArea2.h>
#include <casicoArea3.h>
#include <casicoArea4.h>
#include <casicoArea5.h>
#include <casicoArea6.h>

#include <casicoCircle.h>
#include <casicoSquare1.h>
#include <casicoSquare2.h>
#include <casicoSquare3.h>
#include <casicoSquare4.h>

#define SDA 23
#define SCL 22
#define RST 33
#define INT 32

// Pages to display, creates navigation via switch
const uint8_t pageBlueRing = 0;
const uint8_t pageGreenRing = 1;
const uint8_t pageGreen = 2;
const uint8_t pageCasico = 3;

uint8_t selectedPage = pageCasico;
uint8_t maxPageNumber = 3;
bool displayUpdate = true;  // Only draw the entire tft when the page needs to be wiped

// LCD colors
const uint16_t colorCalmBlue = 0x02F5;
const uint16_t colorLightBlue = 0x3BDB;
const uint16_t colorMossGreen = 0x4B28;
const uint16_t colorWhite = 0xFFFF;
const uint16_t colorBlack = 0x0000;
const uint16_t colorCasioGrey = 0xA553;
const uint16_t colorCasioDarkRing = 0x18E4;
const uint16_t colorCasioDarkBG = 0x424A;
const uint16_t colorCasioInnerBorder = 0x5AEB;
const uint16_t colorCasioOutterBorder = 0x2966;

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

// Timer for casico tft page updates
unsigned long currentCasicoDrawMillis = 0;
unsigned long previousCasicoDrawMillis = 0;
const long casicoDrawInterval = 1000;

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

