//----------------------------------------------------------------------------------------
// YamuraLog - ESP32 Hub
//----------------------------------------------------------------------------------------
#ifndef ARDUINO_ARCH_ESP32
  #error "Select an ESP32 board"
#endif

#include <Arduino.h>
#include"rom/gpio.h"
#include <ACAN_ESP32.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include <esp_task_wdt.h>
#include <Wire.h>
#include <DS3231-RTC.h>
#include <TFT_eSPI.h>            // https://github.com/Bodmer/TFT_eSPI Graphics and font library for ST7735 driver chip
#include "Free_Fonts.h"          // Include the header file attached to this sketch

#define YAMURANODE_ID 0
//#define DEBUG_VERBOSE
#define INITIAL_DELAY  10000
#define LOG_START      30000
#define LOG_END        60000
#define LOG_DURATION 60000
#define  SD_CS 33
#define TFT_CS 15
// buffer and index to next store point in current logging buffer
// loadBufferIdx is 0 or 1, don't add to buffer if loadBufferIdx == storeBufferIdx
int loadBufferIdx = 0;
int loadBufferByteIdx[2] = {0, 0};
int logFileIdx = 0;
// -1 for available (nothing ready to write)
// 0 or 1 for store buffer to microSD
int storeBufferIdx = -1;

File logFile;

int coreLED[2] = {13, 12};

TaskHandle_t Task0;
TaskHandle_t Task1;
// 2 1024 byte buffers - one for active store from CAN, 1 for write to microSD
uint8_t dualBuffer[2][1024];
char logFileName[256];
unsigned long logEnd = 0;

//----------------------------------------------------------------------------------------
//  ESP32 Desired Bit Rate
//----------------------------------------------------------------------------------------
static const uint32_t DESIRED_BIT_RATE = 1000UL * 1000UL;
CANMessage sendFrame;
CANMessage rcvFrame;
uint32_t sendDataTime = 0;
bool logging;
//
// RTC device
//
DS3231 rtcDevice;
byte year;
int actualYear;
byte month;
byte date;
byte dOW;
byte hour;
byte minute;
byte second;
bool h12Flag;
bool pmFlag;
char amPM[16];
bool century = false;
char dowName[7][16] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };

// TFT display
TFT_eSPI tftDisplay = TFT_eSPI();
int fontHeight;
int fontWidth;
int textPosition[2] = {5, 0};
int screenRotation = 1;
// device settings structure
struct DeviceSettings
{
  char ssid[32] = "Yamura-Logger";
  char pass[32] = "ZoeyDora48375";
  int screenRotation = 1;
  bool tempUnits = false; // true for C, false for F
  bool is12Hour = true;   // true for 12 hour clock, false for 24 hour clock
  int fontPoints = 12;    // size of font to use for display
  float stableBand[2] = {-0.25, 0.25};
  unsigned long stableDelay = 500;
  int stableBuffer = 10;
};
DeviceSettings deviceSettings;
char buffer512[512];
