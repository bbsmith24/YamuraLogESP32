//----------------------------------------------------------------------------------------
// YamuraLog - ESP32 Hub
//----------------------------------------------------------------------------------------
#ifndef ARDUINO_ARCH_ESP32
  #error "Select an ESP32 board"
#endif

//----------------------------------------------------------------------------------------
//   Include files
//----------------------------------------------------------------------------------------

#include "HubBase.h"

//----------------------------------------------------------------------------------------
//   SETUP
//----------------------------------------------------------------------------------------
void setup () 
{
  int w = 0;
  int h = 0;
  pinMode(coreLED[0], OUTPUT);
  pinMode(coreLED[1], OUTPUT);
  digitalWrite(coreLED[0], LOW);
  digitalWrite(coreLED[1], HIGH);
  for(int idx = 0; idx < 30; idx++)
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

  sprintf(buffer512, "SDO: %d SDI: %d CLK %d TFT CS %d microSD CS %d\n", TFT_MOSI, TFT_MISO, TFT_SCLK, TFT_CS, SD_CS);
  Serial.print(buffer512);
  // microSD setup
  Serial.println("======================");
  Serial.println("| microSD card setup |");
  Serial.println("======================");
  //SdFile::SD_dateTimeCallback(SD_DateTime);
  bool alreadyFailed = false;
  // TODO - for some reason, this needs to be called prior to initing the TFT display
  //        if called after, microSD won't mount?
  while(!SD.begin(SD_CS))
  {
    if(!alreadyFailed)
    {
      Serial.println("Card Mount Failed");
      alreadyFailed = true;
    }
    digitalWrite(coreLED[0], !digitalRead(coreLED[0]));
    delay(1000);
  }
  digitalWrite(coreLED[0], LOW);

  Serial.println("=====================");
  Serial.println("| tft display setup |");
  Serial.println("=====================");
  tftDisplay.init();
  tftDisplay.invertDisplay(false);
  RotateDisplay(true);  
  w = tftDisplay.width();
  h = tftDisplay.height();
  textPosition[0] = 5;
  textPosition[1] = 0;
  // 0 portrait pins down
  // 1 landscape pins right
  // 2 portrait pins up
  // 3 landscape pins left
  tftDisplay.fillScreen(TFT_BLACK);
  YamuraBanner();
  SetFont(deviceSettings.fontPoints);
  tftDisplay.setTextColor(TFT_WHITE, TFT_BLACK);
  tftDisplay.drawString("Yamura Motors LLC Data Logger", textPosition[0], textPosition[1], GFXFF);
  textPosition[1] += fontHeight;
  delay (1000);

  // microSD setup
  Serial.println("======================");
  Serial.println("| microSD card setup |");
  Serial.println("======================");
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
  char magnitude[5][16] = {"bytes", "KB", "MB", "GB", "TB"};
  totalUsedSpace = 0.0;
  listDir(SD, "/", 10);
  float cardSize = (float)SD.cardSize();// / (1024 * 1024);
  float freeSize = cardSize - totalUsedSpace;
  int cardMagnitudeCount = 0;
  int freeMagnitudeCount = 0;
  int usedMagnitudeCount = 0;
  while(cardSize > 1024.0)
  {
    cardSize /= 1024.0;
    cardMagnitudeCount++;
  }
  while(freeSize > 1024.0)
  {
    freeSize /= 1024.0;
    freeMagnitudeCount++;
  }
  while(totalUsedSpace > 1024.0)
  {
    totalUsedSpace /= 1024.0;
    usedMagnitudeCount++;
  }

  sprintf(buffer512, "SD %0.2f%s used %0.2f%s free of %0.2f%s", totalUsedSpace, magnitude[usedMagnitudeCount],
                                                                freeSize, magnitude[freeMagnitudeCount],
                                                                cardSize, magnitude[cardMagnitudeCount]);
  Serial.println(buffer512);
  tftDisplay.drawString(buffer512, textPosition[0], textPosition[1], GFXFF);
  textPosition[1] += fontHeight;

  // RTC
  Serial.println("=============");
  Serial.println("| RTC setup |");
  Serial.println("=============");

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
  sprintf(buffer512, "%s %02d/%02d/%04d %02d:%02d:%02d %s", dowName[dOW], month, date, actualYear, hour, minute, second, amPM);
  Serial.println("RTC OK");
  Serial.println(buffer512);
  tftDisplay.drawString(buffer512, textPosition[0], textPosition[1], GFXFF);
  textPosition[1] += fontHeight;

  // CAN setup
  Serial.println("=============");
  Serial.println("| CAN setup |");
  Serial.println("=============");
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
    sprintf(buffer512, "CAN configuration OK!");
  }
  else
  {
    Serial.print ("Configuration error 0x");
    Serial.println (errorCode, HEX);
    sprintf(buffer512, "CAN configuration error 0x%X", errorCode);
  }
  tftDisplay.drawString(buffer512, textPosition[0], textPosition[1], GFXFF);
  textPosition[1] += fontHeight;
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
  while(millis() < INITIAL_DELAY)
  {
    if ((millis()  % 2000) == 0)
    {
      digitalWrite(coreLED[0], HIGH);
    }
    else if ((millis()  % 1000) == 0)
    {
      digitalWrite(coreLED[0], LOW);
    }
  }
  digitalWrite(coreLED[0], LOW);
  Serial.printf("HUB ID 0x%02X Start!\n", YAMURANODE_ID);
  Serial.println("================");
  Serial.println("| User buttons |");
  Serial.println("================");
  buttons[0].buttonPin = BUTTON_0;
  buttons[1].buttonPin = BUTTON_1;
  buttons[2].buttonPin = BUTTON_2;
  for(int idx = 0; idx < BUTTON_COUNT; idx++)
  {
    Serial.printf("Buttons[%d].buttonPin = %d\n", idx, buttons[idx].buttonPin);
    pinMode(buttons[idx].buttonPin, INPUT_PULLUP);
  }
  CheckButtons(millis());
}
//----------------------------------------------------------------------------------------
//   LOOP
//----------------------------------------------------------------------------------------
void loop() 
{
  BaseType_t coreID;
  if ((millis() % 2000) == 0)
  {
    digitalWrite(coreLED[0], HIGH);
  }
  else if ((millis()  % 1000) == 0)
  {
    digitalWrite(coreLED[0], LOW);
  }

  if((logging == false) && (millis() % 10000 == 0))
  {
    SendFrame(0); // heartbeat
  }
  // check buttons
  CheckButtons(millis());
  // start logging on any serial input
  if(buttons[0].buttonReleased == true)
  {
    buttons[0].buttonReleased = false;
    if(logging == false)
    {
      logFileIdx++;
      sprintf(logFileName, "/logFile%d.log", logFileIdx);
	    hour = rtcDevice.getHour(h12Flag, pmFlag);
      minute = rtcDevice.getMinute();
      second = rtcDevice.getSecond();
      sprintf(buffer512, " %02d:%02d:%02d START LOG %s          ", hour, minute, second, logFileName);
      Serial.println(buffer512);
      tftDisplay.drawString(buffer512, textPosition[0], fontHeight * 8, GFXFF);
      deleteFile(SD, logFileName);
      logFile = SD.open(logFileName, FILE_WRITE);
      logFile.print("XXXXXXXXXXXX");
      if(!logFile)
      {
        Serial.printf("Failed to open %s for write - no logging\n", logFileName);
      }
      else
      {
        logStart= millis();
        logging = true;
        SendFrame(1); // start logging
      }
    }
    else if(logging == true)
    {
      logging = false;
    	hour = rtcDevice.getHour(h12Flag, pmFlag);
	    minute = rtcDevice.getMinute();
  	  second = rtcDevice.getSecond();

      logFile.close();
      SendFrame(2); // stop logging
      logEnd= millis();
      sprintf(buffer512, " %02d:%02d:%02d END LOG %s %0.2fs         ", hour, minute, second, logFileName, (float)(logEnd-logStart)/1000.0);
      tftDisplay.drawString(buffer512, textPosition[0], fontHeight * 8, GFXFF);
      Serial.println(buffer512);
    }
  }
  ReceiveFrame();
  delay(1);
}
//
//
//
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
  // heartbeat ack
  if(!logging)
  {
    int nodeIdx = 0;
    while(expectedIDs[nodeIdx] > 0)
    {
      if((expectedIDs[nodeIdx] == rcvFrame.id) && (reportedIDs[nodeIdx] == false))
      {
        reportedIDs[nodeIdx] = true;
        sprintf(buffer512, "Node ID 0x%02X %s %s", expectedIDs[nodeIdx], 
                                                   expectedNames[nodeIdx], 
                                                   "OK");
        tftDisplay.drawString(buffer512, textPosition[0], ((4 + nodeIdx) * fontHeight), GFXFF);
      }
      nodeIdx++;
    }
  }
  if(loadBufferIdx == storeBufferIdx)
  {
    digitalWrite(coreLED[1], HIGH);
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
//
// rotate TFT display for right or left hand use
//
void RotateDisplay(bool rotateButtons)
{
  tftDisplay.setRotation(deviceSettings.screenRotation == 0 ? 1 : 3);
  tftDisplay.fillScreen(TFT_WHITE);
  /*
  if(rotateButtons)
  {
    int reverseButtons[BUTTON_COUNT];
    for(int btnIdx = 0; btnIdx < BUTTON_COUNT; btnIdx++)
    {
      reverseButtons[BUTTON_COUNT - (btnIdx + 1)] = buttons[btnIdx].buttonPin;
    }
    for(int btnIdx = 0; btnIdx < BUTTON_COUNT; btnIdx++)
    {
      buttons[btnIdx].buttonPin = reverseButtons[btnIdx];
    }
  }
  */
}
//
// draw the Yamura banner at bottom of TFT display
//
void YamuraBanner()
{
  Serial.println("Data Logger");
  tftDisplay.setTextColor(TFT_BLACK, TFT_YELLOW);
  SetFont(9);
  int xPos = tftDisplay.width()/2;
  int yPos = tftDisplay.height() - fontHeight/2;
  tftDisplay.setTextDatum(BC_DATUM);
  tftDisplay.drawString("  Yamura Motors LLC Data Logger  ",xPos, yPos, GFXFF);    // Print the font name onto the TFT screen
  tftDisplay.setTextDatum(TL_DATUM);
}
//
// set font for TFT display, update fontHeight used for vertical stepdown by line
//
void SetFont(int fontSize)
{
  Serial.printf("Font Size %d\n", fontSize);
  switch(fontSize)
  {
    case 9:
      tftDisplay.setFreeFont(FSS9);
      break;
    case 12:
      tftDisplay.setFreeFont(FSS12);
      break;
    case 18:
      tftDisplay.setFreeFont(FSS18);
      break;
    case 24:
      tftDisplay.setFreeFont(FSS24); // was FSS18
      break;
    default:
      tftDisplay.setFreeFont(FSS12);
      break;
  }
  tftDisplay.setTextDatum(TL_DATUM);
  fontHeight = tftDisplay.fontHeight(GFXFF);
  fontWidth = tftDisplay.textWidth("X");
}
//
// check all buttons for new press or release
//
void CheckButtons(unsigned long curTime)
{
  int curState;
  bool needNewLine = false;
  //Serial.printf("Check buttons at %d\n", curTime);
  for(byte btnIdx = 0; btnIdx < BUTTON_COUNT; btnIdx++)
  {
    // get current pin value
    // low (0) = pressed, high (1) = released
    curState = digitalRead(buttons[btnIdx].buttonPin);
    if((buttons[btnIdx].buttonCurrent != curState) &&
       ((curTime - buttons[btnIdx].lastChange) > BUTTON_DEBOUNCE_DELAY))
    {
      #ifdef DEBUG_VERBOSE
      Serial.printf("%d\tState changed to %s\t", buttons[btnIdx].buttonPin, curState == HIGH ? "RELEASE" : "PRESS");
      Serial.printf("%d\t%s\t", buttons[btnIdx].buttonPin, curState == HIGH ? "Actual release" : "Actual press");
      #endif
      buttons[btnIdx].buttonCurrent = curState;
      buttons[btnIdx].lastChange = curTime;
      needNewLine = true;
      if(buttons[btnIdx].buttonCurrent == LOW)
      {
        buttons[btnIdx].buttonPressed = true;
        buttons[btnIdx].buttonReleased = false;
      }
      else
      {
        buttons[btnIdx].buttonPressed =  false;
        buttons[btnIdx].buttonReleased = true;
      }
    }
  }
}
//
//
//
void listDir(fs::FS &fs, const char *dirname, uint8_t levels) 
{

  Serial.printf("Listing directory: %s\n", dirname);

  File root = fs.open(dirname);
  if (!root) 
  {
    Serial.println("Failed to open directory");
    return;
  }
  if (!root.isDirectory()) 
  {
    Serial.println("Not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file) 
  {
    if (file.isDirectory()) 
    {
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if (levels) 
      {
        listDir(fs, file.path(), levels - 1);
      }
    }
    else 
    {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("  SIZE: ");
      Serial.println(file.size());
      totalUsedSpace += (float)file.size();
    }
    file = root.openNextFile();
  }
}