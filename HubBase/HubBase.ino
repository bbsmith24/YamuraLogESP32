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
#include "TFT_UserMenus.h"

#define MAIN_MENU 0
#define SETTINGS_MENU 1
#define DRIVERS_MENU 2
#define YESNO_MENU 3
#define END_LOGGING_MENU 4

TFTMenus menuSystem;

// Replace with your network credentials
const char* ssid = "FamilyRoom";
const char* password = "ZoeyDora48375";

// FTP server credentials
const char* ftp_server = "192.168.0.20";
const char* ftp_user = "vito";
const char* ftp_pass = "passwrd";
const char* working_dir   = "/media/share/alarms/esp32cam1/"; // Change to the desired directory on FTP server

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

  Serial.println("===============================");
  Serial.println("| tft display and menus setup |");
  Serial.println("===============================");
  Serial.println("==================================");
  Serial.println("| TFT Display User Menu example  |");
  Serial.println("==================================");  

  Serial.println("Add menus");
  menuSystem.AddMenu("Main Menu", false);
  menuSystem.AddMenu("Settings", false);
  menuSystem.AddMenu("Drivers", false);
  menuSystem.AddMenu("Yes/No", true);
  menuSystem.AddMenu("End Logging", false);
  // main menu
  menuSystem.menus[MAIN_MENU].AddMenuChoice(0, "Start Logging", LogData);
  menuSystem.menus[MAIN_MENU].AddMenuChoice(1, "Brian", DriverSelection);
  menuSystem.menus[MAIN_MENU].AddMenuChoice(2, "Settings", Settings);
  menuSystem.ListChoices(MAIN_MENU);
  // settings submenu
  menuSystem.menus[SETTINGS_MENU].AddMenuChoice(0,  "Set Date/Time", SetDateTime);
  menuSystem.menus[SETTINGS_MENU].AddMenuChoice(1,  "Invert Screen", InvertScreen);
  menuSystem.menus[SETTINGS_MENU].AddMenuChoice(2,  "Font Size", SetFontSize);
  menuSystem.menus[SETTINGS_MENU].AddMenuChoice(3,  "Delete Data", DeleteData);
  menuSystem.menus[SETTINGS_MENU].AddMenuChoice(4,  "IP 192.168.4.1", NoAction);
  menuSystem.menus[SETTINGS_MENU].AddMenuChoice(5, "Password ZoeyDora48375", NoAction);
  menuSystem.menus[SETTINGS_MENU].AddMenuChoice(6, "Save Settings", SaveSettings);
  menuSystem.menus[SETTINGS_MENU].AddMenuChoice(7, "Exit", ExitSettings);
  menuSystem.ListChoices(SETTINGS_MENU);
  // driver/car submenu
  menuSystem.menus[DRIVERS_MENU].AddMenuChoice(0, "Brian", SetDriver);
  menuSystem.menus[DRIVERS_MENU].AddMenuChoice(1, "Mark", SetDriver);
  menuSystem.menus[DRIVERS_MENU].AddMenuChoice(2, "Rob", SetDriver);
  menuSystem.ListChoices(DRIVERS_MENU);
  //
  menuSystem.menus[YESNO_MENU].AddMenuChoice(0, "Yes", YesNo);
  menuSystem.menus[YESNO_MENU].AddMenuChoice(1, "No", YesNo);
  menuSystem.ListChoices(YESNO_MENU);
  //
  menuSystem.menus[END_LOGGING_MENU].AddMenuChoice(0, "End Logging", EndLogging);
  menuSystem.ListChoices(END_LOGGING_MENU);
  // set the main menu as current
  menuSystem.SetCurrentMenu(MAIN_MENU);

  tftDisplay.init();
  menuSystem.Setup(&tftDisplay);
  strcpy(buffer512, "  Yamura Motors LLC Data Logger  ");
  Serial.printf("Set banner text to: %s\n", buffer512);
  menuSystem.SetBannerText(buffer512);
  Serial.printf("Banner text set to: %s\n", menuSystem.bannerText);

  menuSystem.Banner();
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
  listDir(SD, "/", logFileIdx, 10, ".log");
  Serial.printf("Log file count %d\n", logFileIdx);
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
  menuSystem.CheckButtons(millis());
  menuSystem.DisplayMenu(MAIN_MENU);
}
//----------------------------------------------------------------------------------------
//   LOOP
//----------------------------------------------------------------------------------------
void loop() 
{
  BaseType_t coreID;
  if(millis() % 10000 == 0)
  {
    digitalWrite(coreLED[0], HIGH);
    SendFrame(0); // heartbeat
  }

  if ((millis() % 1000) == 0)
  {
    digitalWrite(coreLED[0], !digitalRead(coreLED[0]));
  }
  // check buttons
  menuSystem.GetUserInput();
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
//
//
void listDir(fs::FS &fs, const char *dirname, int &fileCount, uint8_t levels, char* ext) 
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
        listDir(fs, file.path(), fileCount, levels - 1, ext);
      }
    }
    else 
    {
      if(strstr(file.name(), ext) != NULL)
      {
        fileCount++;
      }
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("  SIZE: ");
      Serial.println(file.size());
      totalUsedSpace += (float)file.size();
    }
    file = root.openNextFile();
  }
}

//
// menu choice handlers
//
void LogData()
{
  menuSystem.tftDisplay->fillScreen(TFT_GREEN);
  menuSystem.tftDisplay->setTextColor(TFT_WHITE, TFT_GREEN);
  sprintf(logFileName, "/logFile%03d.log", logFileIdx);
  hour = rtcDevice.getHour(h12Flag, pmFlag);
  minute = rtcDevice.getMinute();
  second = rtcDevice.getSecond();
  sprintf(buffer512, " %02d:%02d:%02d START LOG %s          ", hour, minute, second, logFileName);

  textPosition[1] = fontHeight * 2;
  tftDisplay.drawString(buffer512, textPosition[0], textPosition[1], GFXFF);
  textPosition[1] += fontHeight;
  tftDisplay.drawString("Press SELECT to end", textPosition[0], textPosition[1], GFXFF);
  textPosition[1] += fontHeight;
  deleteFile(SD, logFileName);
  logFile = SD.open(logFileName, FILE_WRITE);
  logFile.print("XXXXXXXXXXXX");
  if(!logFile)
  {
    tftDisplay.drawString("Failed to open %s for write - not logging", textPosition[0], textPosition[1], GFXFF);
    delay(5000);
    menuSystem.DisplayMenu(MAIN_MENU);
    return;
  }
  logStart= millis();
  logging = true;
  SendFrame(1); // start logging
  menuSystem.buttons[0].buttonReleased = 0;
  menuSystem.buttons[0].buttonPressed = 0;
  menuSystem.buttons[1].buttonReleased = 0;
  menuSystem.buttons[1].buttonPressed = 0;
  menuSystem.buttons[2].buttonReleased = 0;
  menuSystem.buttons[2].buttonPressed = 0;
  while(true)
  {
    menuSystem.CheckButtons(millis());
    if((menuSystem.buttons[0].buttonReleased) ||
       (menuSystem.buttons[1].buttonReleased) ||
       (menuSystem.buttons[2].buttonReleased))
    {
      menuSystem.buttons[0].buttonReleased = 0;
      menuSystem.buttons[0].buttonPressed = 0;
      menuSystem.buttons[1].buttonReleased = 0;
      menuSystem.buttons[1].buttonPressed = 0;
      menuSystem.buttons[2].buttonReleased = 0;
      menuSystem.buttons[2].buttonPressed = 0;
      logging = false;
      hour = rtcDevice.getHour(h12Flag, pmFlag);
	    minute = rtcDevice.getMinute();
      second = rtcDevice.getSecond();

      logFile.close();
      logFileIdx++;
      SendFrame(2); // stop logging
      logEnd= millis();

      menuSystem.tftDisplay->fillScreen(TFT_RED);
      menuSystem.tftDisplay->setTextColor(TFT_WHITE, TFT_RED);
      textPosition[1] = fontHeight * 2;

      sprintf(buffer512, " %02d:%02d:%02d END LOG %s %0.2fs         ", hour, minute, second, logFileName, (float)(logEnd-logStart)/1000.0);
      tftDisplay.drawString(buffer512, textPosition[0], textPosition[1], GFXFF);
      delay(5000);
      break;
    }
    ReceiveFrame();
    delay(1);
  }
  menuSystem.tftDisplay->fillScreen(TFT_WHITE);
  menuSystem.tftDisplay->setTextColor(TFT_BLACK, TFT_WHITE);
  menuSystem.DisplayMenu(MAIN_MENU);
  return;
}
void EndLogging()
{
  logging = false;
  hour = rtcDevice.getHour(h12Flag, pmFlag);
	minute = rtcDevice.getMinute();
  second = rtcDevice.getSecond();

  logFile.close();
  SendFrame(2); // stop logging
  logEnd= millis();
  menuSystem.priorDisplayRange[0] = 0;
  menuSystem.priorDisplayRange[1] = menuSystem.menus[MAIN_MENU].menuLength;
  menuSystem.menus[MAIN_MENU].priorChoice = 0;
  menuSystem.currentDisplayRange[0] = 0;
  menuSystem.currentDisplayRange[1] = menuSystem.menus[MAIN_MENU].menuLength;
  menuSystem.menus[MAIN_MENU].curChoice = 0;
  menuSystem.DisplayMenu(MAIN_MENU);
  sprintf(buffer512, " %02d:%02d:%02d END LOG %s %0.2fs         ", hour, minute, second, logFileName, (float)(logEnd-logStart)/1000.0);
  tftDisplay.drawString(buffer512, textPosition[0], fontHeight * 8, GFXFF);
  Serial.println(buffer512);
  return;
}
void DriverSelection()
{
  Serial.println("DriverSelection");
  menuSystem.DisplayMenu(DRIVERS_MENU);
  return;
}
void Settings()
{
  Serial.println("Settings");
  menuSystem.DisplayMenu(SETTINGS_MENU);
  return;
}
void SetDateTime()
{
  Serial.println("Set Date/Time");
  return;
}
void InvertScreen()
{
  Serial.println("InvertScreen");
  return;
}
void SetFontSize()
{
  Serial.println("SetFontSize");
  return;
}
void DeleteData()
{
  while(true)
  {
    strcpy(menuSystem.menus[YESNO_MENU].menuName, "Delete all data?");
    menuSystem.DisplayMenu(YESNO_MENU);
    menuSystem.GetUserInput();
    if(menuSystem.GetCurrentSelection() == 0)
    {
      Serial.println("Delete Data");
      menuSystem.DrawText("Delete Data", menuSystem.textPosition[0], menuSystem.textPosition[1] + menuSystem.fontHeight, TFT_WHITE, TFT_RED);
      delay(2000);
      break;
    }
    else if(menuSystem.GetCurrentSelection() == 1)
    {
      Serial.println("Delete Data cancelled");
      menuSystem.DrawText("Delete Data cancelled", menuSystem.textPosition[0], menuSystem.textPosition[1] + menuSystem.fontHeight, TFT_WHITE, TFT_RED);
      delay(2000);
      break;
    }
  }
  menuSystem.SetCurrentMenu(MAIN_MENU);
  return;
}
void SaveSettings()
{
  Serial.println("SaveSettings");
  menuSystem.SetCurrentMenu(MAIN_MENU);
  return;
}
void ExitSettings()
{
  Serial.println("ExitSettings");
  menuSystem.DisplayMenu(MAIN_MENU);
  return;
}
void SetDriver()
{
  Serial.print("SetDriver - selected ");

  menuSystem.menus[MAIN_MENU].AddMenuChoice(1,
                                            menuSystem.menus[DRIVERS_MENU].choiceList[menuSystem.GetCurrentSelection()].description, 
                                            DriverSelection);

  Serial.println(menuSystem.GetCurrentSelection());
  menuSystem.DisplayMenu(MAIN_MENU);
  return;
}
//
void YesNo()
{
  return;
}
void NoAction()
{
  return;
}
void uploadFile(const char* path) {
    ftp.OpenConnection();
    ftp.ChangeWorkDir(working_dir);  // Change to the desired directory on FTP server
    ftp.InitFile("Type I");

    ftp.NewFile(path);
    
    File file = SD_MMC.open("/" + String(path));
    if (!file) {
        Serial.println("Failed to open file for reading");
        return;
    }

    const size_t bufferSize = 512;
    uint8_t buffer[bufferSize];
    while (file.available()) {
        size_t bytesRead = file.read(buffer, bufferSize);
        ftp.WriteData(buffer, bytesRead);
        Serial.println("File available");
        Serial.println(path);
        Serial.println("Uploading file"+ file);
    }

    ftp.CloseFile();
    ftp.CloseConnection();

    file.close();
    Serial.println("File uploaded successfully");
}

void listAndUploadFiles(fs::FS &fs, const char* dirname) 
{
    File root = fs.open(dirname);
    if (!root) {
        Serial.println("Failed to open directory");
        return;
    }
    if (!root.isDirectory()) {
        Serial.println("Not a directory");
        return;
    }

    File file = root.openNextFile();
    while (file) {
        if (file.isDirectory()) {
            listAndUploadFiles(fs, file.name());  // Recursively list files in subdirectories
        } else {
            // Serial.print("Uploading file: ");
            Serial.println(file.name());
            uploadFile(file.name());
           Serial.println("Uploading file" + file);
        }
        file = root.openNextFile();
    }
}