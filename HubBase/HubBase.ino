//----------------------------------------------------------------------------------------
// YamuraLog - ESP32 Hub test
//----------------------------------------------------------------------------------------
#ifndef ARDUINO_ARCH_ESP32
  #error "Select an ESP32 board"
#endif

//----------------------------------------------------------------------------------------
//   Include files
//----------------------------------------------------------------------------------------

#include <Arduino.h>
#include"rom/gpio.h"
#include <ACAN_ESP32.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include <esp_task_wdt.h>
#include <Wire.h>
#include <DS3231-RTC.h>

#define YAMURANODE_ID 0
//#define DEBUG_VERBOSE
#define INITIAL_DELAY  10000
#define LOG_START      30000
#define LOG_END        60000
#define LOG_DURATION 60000
// buffer and index to next store point in current logging buffer
// loadBufferIdx is 0 or 1, don't add to buffer if loadBufferIdx == storeBufferIdx
int loadBufferIdx = 0;
int loadBufferByteIdx[2] = {0, 0};
int logFileIdx = 0;
// -1 for available (nothing ready to write)
// 0 or 1 for store buffer to microSD
int storeBufferIdx = -1;

File logFile;

int coreLED[2] = {14, 12};

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
//----------------------------------------------------------------------------------------
//   SETUP
//----------------------------------------------------------------------------------------
void setup () 
{
  pinMode(coreLED[0], OUTPUT);
  pinMode(coreLED[1], OUTPUT);
  digitalWrite(coreLED[0], LOW);
  digitalWrite(coreLED[1], HIGH);
  for(int idx = 0; idx < 20; idx++)
  {
      digitalWrite(coreLED[0], !digitalRead(coreLED[0]));
      digitalWrite(coreLED[1], !digitalRead(coreLED[1]));
      delay(100);
  }
  digitalWrite(coreLED[0], LOW);
  digitalWrite(coreLED[1], LOW);
  Serial.begin (115200);
  delay (1000);
  Serial.println("=============================");
  Serial.println("| YamuraLogESP32 - Hub Base |");
  Serial.println("=============================");
  // microSD setup
  Serial.println("microSD card setup");
  Serial.println("==================");
  //SdFile::SD_dateTimeCallback(SD_DateTime);
  while(!SD.begin(15))
  {
    Serial.println("Card Mount Failed");
    delay(1000);
  }
  uint8_t cardType = SD.cardType();

  while(cardType == CARD_NONE)
  {
    Serial.println("No SD card attached");
    delay(1000);
    cardType = SD.cardType();
  }
  Serial.println("SD card OK");

  Serial.print("SD Card Type: ");
  if(cardType == CARD_MMC)
  {
    Serial.println("MMC");
  }
  else if(cardType == CARD_SD)
  {
    Serial.println("SDSC");
  }
  else if(cardType == CARD_SDHC)
  {
    Serial.println("SDHC");
  }
  else 
  {
    Serial.println("UNKNOWN");
  }
  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);
  // CAN setup
  Serial.println("CAN setup");
  Serial.println("=========");
  ACAN_ESP32_Settings settings (DESIRED_BIT_RATE);
  settings.mRequestedCANMode = ACAN_ESP32_Settings::NormalMode;
  // receive all standard IDs
  const ACAN_ESP32_Filter filter = ACAN_ESP32_Filter::acceptStandardFrames();
  // start CAN with desired filtering
  const uint32_t errorCode = ACAN_ESP32::can.begin (settings, filter);
  if (errorCode == 0) 
  {
    Serial.print("Bit Rate prescaler: ");
    Serial.println(settings.mBitRatePrescaler);
    Serial.print("Time Segment 1:     ");
    Serial.println(settings.mTimeSegment1);
    Serial.print("Time Segment 2:     ");
    Serial.println(settings.mTimeSegment2);
    Serial.print ("RJW:                ");
    Serial.println (settings.mRJW);
    Serial.print("Triple Sampling:    ");
    Serial.println(settings.mTripleSampling ? "yes" : "no");
    Serial.print("Actual bit rate:    ");
    Serial.print(settings.actualBitRate());
    Serial.println(" bit/s");
    Serial.print("Exact bit rate ?    ");
    Serial.println(settings.exactBitRate() ? "yes" : "no");
    Serial.print("Sample point:       ");
    Serial.print(settings.samplePointFromBitStart());
    Serial.println("%");
    Serial.println("Configuration OK!");
  }
  else
  {
    Serial.print ("Configuration error 0x");
    Serial.println (errorCode, HEX);
  }
  // RTC
  Wire.begin();
  actualYear = 2000 + (century ? 100 : 0) + rtcDevice.getYear();
	month = rtcDevice.getMonth(century);
  date = rtcDevice.getDate();
	dOW = rtcDevice.getDoW();
	hour = rtcDevice.getHour(h12Flag, pmFlag);
	minute = rtcDevice.getMinute();
	second = rtcDevice.getSecond();
	// Add AM/PM indicator
	if (h12Flag) 
  {
		if (pmFlag) 
    {
			sprintf(amPM, "PM");
		} 
    else 
    {
			sprintf(amPM, "AM");
		}
	} 
  else 
  {
  	sprintf(amPM, "24H");
	}
  Serial.printf("Current time %s %02d/%02d/%04d %02d:%02d:%02d %s\n", dowName[dOW], month, date, actualYear, hour, minute, second, amPM);
  //
  logging = false;
  Serial.println("Starting WriteToSDTask on core 0");
  Serial.println("================================");
  loadBufferIdx = 0;
  storeBufferIdx = -1;
  xTaskCreatePinnedToCore(
                    WriteToSDTask,   // Task function. 
                    "Task0",     // name of task.
                    10000,       // Stack size of task
                    NULL,        // parameter of the task
                    1,           // priority of the task
                    &Task0,      // Task handle to keep track of created task
                    0);          // pin task to core 0
  BaseType_t coreID = xPortGetCoreID();
  while(millis() < INITIAL_DELAY)
  {
    if ((millis()  % 2000) == 0)
    {
      digitalWrite(coreLED[coreID], HIGH);
    }
    else if ((millis()  % 1000) == 0)
    {
      digitalWrite(coreLED[coreID], LOW);
    }
  }
  digitalWrite(coreLED[coreID], LOW);
  Serial.printf("HUB ID 0x%02X Start!\n", YAMURANODE_ID);
}
//----------------------------------------------------------------------------------------
//   LOOP
//----------------------------------------------------------------------------------------
void loop() 
{
  BaseType_t coreID;
  if ((millis()  % 2000) == 0)
  {
    coreID = xPortGetCoreID();
    digitalWrite(coreLED[coreID], HIGH);
  }
  else if ((millis()  % 1000) == 0)
  {
    coreID = xPortGetCoreID();
    digitalWrite(coreLED[coreID], LOW);
  }

  if((logging == false) && (millis() % 10000 == 0))
  {
    SendFrame(0); // heartbeat
  }
  // start logging on any serial input
  if((logging == false) && (Serial.available() > 0))
  {
    if(Serial.read() == 'S')
    {
      logFileIdx++;
      sprintf(logFileName, "/logFile%d.log", logFileIdx);
      logEnd = millis() + LOG_DURATION;
      Serial.printf("START LOGGING to %s at %d, end at %d\n", logFileName, millis(), logEnd);
      deleteFile(SD, logFileName);
      logFile = SD.open(logFileName, FILE_WRITE);
      logFile.print("XXXXXXXXXXXX");
      if(!logFile)
      {
        Serial.printf("Failed to open %s for write - no logging\n", logFileName);
      }
      else
      {
        logging = true;
        coreID = xPortGetCoreID();
        Serial.print("Core ");
        Serial.print(coreID);
        Serial.print(" ");
        SendFrame(1); // start logging
      }
    }
    Serial.flush();
  }
  if((logging == true) && (millis() > logEnd))
  {
    logging = false;
    coreID = xPortGetCoreID();
    Serial.print("Core ");
    Serial.print(coreID);
    Serial.print(" ");
    Serial.printf("STOP LOGGING to %s at %d\n", logFileName, millis());
    logFile.close();
    SendFrame(2); // stop logging
  }
  ReceiveFrame();
  delay(1);
}
void SendFrame(int subID)
{
  sendFrame.id = YAMURANODE_ID;
  sendFrame.rtr = false;
  sendFrame.ext = false;
  sendFrame.len = 8;
  sendFrame.data32[0] = subID;
  sendFrame.data32[1] = millis();
  Serial.printf("%d\tOUT\tid 0x%03x\tbytes\t%d\tdata", millis(),
                                                       sendFrame.id,
                                                       sendFrame.len);
  for(int idx = 0; idx < 8; idx++)
  {
    Serial.printf("\t0x%02X", (sendFrame.data_s8[idx] & 0xFF));
  }

  if (!ACAN_ESP32::can.tryToSend (sendFrame)) 
  {
    Serial.printf("\tFAILED TO SEND FRAME ID 0x%X!!!", sendFrame.id);
  }
  Serial.println();      
}
//
//
//
void ReceiveFrame()
{
  // receive failed (nothing in buffer)
  if (!ACAN_ESP32::can.receive (rcvFrame)) 
  {
    return;
  }
  #ifdef DEBUG_VERBOSE
  Serial.printf("%d Receive ID 0x%02X\n", millis(), rcvFrame.id);
  #endif
  /*
  if((rcvFrame.id == 10) ||(rcvFrame.id == 11))
  {
    Serial.printf("%d Receive ID 0x%02X\n", millis(), rcvFrame.id);
  }
  */
  if(loadBufferIdx == storeBufferIdx)
  {
    digitalWrite(coreLED[0], HIGH);
    Serial.println("wait for microSD write to complete");
  }
  // check for available space in buffer for current message
  // if size will be >= limit, set current buffer to store, update current load buffer
  if((loadBufferByteIdx[loadBufferIdx] + 12) >= 1024)
  {
    storeBufferIdx = loadBufferIdx;
    loadBufferIdx =  loadBufferIdx == 0 ? 1 : 0;
    loadBufferByteIdx[loadBufferIdx] = 0;
    #ifdef DEBUG_VERBOSE
    Serial.printf("switch to loadBuffer %d, set storeBufferIdx %d\n", loadBufferIdx, storeBufferIdx);
    #endif
  }
  //Serial.printf("%d received message, store to dualBuffer[%d][%d]\n", millis(), loadBufferIdx, loadBufferByteIdx[loadBufferIdx]);
  // write message ID and data to store buffer
  memcpy(&dualBuffer[loadBufferIdx][loadBufferByteIdx[loadBufferIdx]], &rcvFrame.id, 4);
  loadBufferByteIdx[loadBufferIdx] += 4;
  memcpy(&dualBuffer[loadBufferIdx][loadBufferByteIdx[loadBufferIdx]], &rcvFrame.data64, 8);
  loadBufferByteIdx[loadBufferIdx] += 8;
}
//
//
//
void deleteFile(fs::FS &fs, const char * path)
{
  if(!fs.exists(path))
  {
    Serial.print("File ");
    Serial.print(path);
    Serial.println(" does not exist - no delete required");
    return;
  }
  Serial.printf("Deleting file: %s\n", path);
  if(fs.remove(path))
  {
    Serial.println("File deleted");
  }
  else 
  {
    Serial.println("Delete failed");
  }
}
//
//
//
void WriteToSDTask( void * pvParameters )
{
  BaseType_t coreID = xPortGetCoreID();
  Serial.print("WriteToSDTask running on core ");
  Serial.println(xPortGetCoreID());

  for(;;)
  {
    while(storeBufferIdx < 0)
    {
      delay(1);
    }
    unsigned long start = millis();
    #ifdef DEBUG_VERBOSE
    Serial.printf("%d\tmicroSD WRITE begin write 0x%.8X bytes from buffer %d\n", millis(), loadBufferByteIdx[storeBufferIdx], storeBufferIdx);
    #endif
    logFile.write(dualBuffer[storeBufferIdx], loadBufferByteIdx[storeBufferIdx]);
    logFile.flush();
    unsigned long writeEnd = millis() - start;
    #ifdef DEBUG_VERBOSE
    Serial.print("Core ");
    Serial.print(coreID);
    Serial.print(" ");
    Serial.printf("microSD WRITE complete (%lu ms)\n", writeEnd);
    #endif
    loadBufferByteIdx[storeBufferIdx] = 0;
    storeBufferIdx = -1;
  }
}
//
//
//
void SD_DateTime(uint16_t* date, uint16_t* time)
{
  actualYear = 2000 + (century ? 100 : 0) + rtcDevice.getYear();
	month =   rtcDevice.getMonth(century);
  byte day =rtcDevice.getDate();
	hour =    rtcDevice.getHour(h12Flag, pmFlag);
	minute =  rtcDevice.getMinute();
	second =  rtcDevice.getSecond();
  //*date =  FAT_DATE(actualYear, month, day);
  //*time =  FAT_TIME(hour, minute, second);
}