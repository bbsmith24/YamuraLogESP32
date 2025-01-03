//----------------------------------------------------------------------------------------
// YamuraLog - ESP32 GPS Node
//----------------------------------------------------------------------------------------

#ifndef ARDUINO_ARCH_ESP32
  #error "Select an ESP32 board"
#endif

#include <ACAN_ESP32.h>
#include <SparkFun_u-blox_GNSS_Arduino_Library.h> // ZOE u-blox GPS
#include <Wire.h>                                 //Needed for I2C to GNSS

#define YAMURANODE_ID 30
#define SAMPLE_INTERVAL 100
//#define DEBUG_VERBOSE

SFE_UBLOX_GNSS gnssDevice;

// CAN message structure to send
// 8 data bytes per CAN message, 4 frames, 32 bytes of data
#define GPS_SIZE 32
#define FRAME_COUNT 4
//
struct __attribute__((packed)) GPS_DataStructure
{
  uint32_t timeStamp;        // 4 bytes
  uint16_t gpsYear;          // 2
  uint8_t gpsMonth;          // 1
  uint8_t gpsDay;            // 1
  uint8_t gpsHour;           // 1
  uint8_t gpsMinute;         // 1
  uint8_t gpsSecond;         // 1
  int32_t latitude;          // 4
  int32_t longitude;         // 4
  int32_t course;            // 4
  int32_t speed;             // 4
  uint8_t SIV;               // 1
  int32_t accuracy;          // 4
};
union GPS_Structure
{
  GPS_DataStructure gpsData;
  byte gpsBytes[GPS_SIZE];
} gps;
//  ESP32 Desired Bit Rate
static const uint32_t DESIRED_BIT_RATE = 1000UL * 1000UL;

CANMessage sendFrame[FRAME_COUNT];
CANMessage rcvFrame;
unsigned long remoteTimeOffset;
static uint32_t sendDataTime = 0;
bool logging;
char buffer512[512];
//----------------------------------------------------------------------------------------
//   SETUP
//----------------------------------------------------------------------------------------
void setup () 
{
  pinMode(12, OUTPUT);
  digitalWrite(12, LOW);
  for(int idx = 0; idx < 20; idx++)
  {
      digitalWrite(12, !digitalRead(12));
      delay(100);
  }
  digitalWrite(12, HIGH);
  Serial.begin (115200);
  delay (1000);
  Serial.println("=============================");
  Serial.printf ("| YamuraLog - GPS Node 0x%02X |\n", YAMURANODE_ID);
  Serial.println("=============================");
  // Configure ESP32 CAN
  Serial.println("Initializing CAN");
  ACAN_ESP32_Settings settings (DESIRED_BIT_RATE);
  settings.mRequestedCANMode = ACAN_ESP32_Settings::NormalMode;
  // receive only standard id 0
  const ACAN_ESP32_Filter filter = ACAN_ESP32_Filter::singleStandardFilter (ACAN_ESP32_Filter::data, 0x0, 0x404);
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
  //
  // GPS initialization
  //
  sprintf(buffer512,"Initializing GNSS on I2C");
  Serial.println(buffer512);
  Wire.begin();
  Wire.setClock(400000);  
  while (gnssDevice.begin() == false) 
  {
    sprintf(buffer512,"GNSS not detected on I2C. Retry in 5 sec");
    Serial.println(buffer512);
    delay (5000);
  }
  Serial.println("u-blox GNSS detected on I2C bus");
  Serial.println("Begin u-blox reset to factory default...");
  /* start reset to defaults */
  gnssDevice.factoryDefault(); 
  delay(5000);
  Serial.println("Reset complete");
  /* end reset to defaults */
  // GPS is enabled and then disable Galileo, GLONASS, BeiDou, SBAS and QZSS to achieve 25Hz.
  gnssDevice.enableGNSS(true, SFE_UBLOX_GNSS_ID_GPS); // Make sure GPS is enabled (we must leave at least one major GNSS enabled!)
  gnssDevice.enableGNSS(true, SFE_UBLOX_GNSS_ID_GALILEO); // enable Galileo
  gnssDevice.enableGNSS(true, SFE_UBLOX_GNSS_ID_GLONASS); // enable GLONASS
  gnssDevice.enableGNSS(true, SFE_UBLOX_GNSS_ID_BEIDOU); // enable BeiDou
  gnssDevice.enableGNSS(true, SFE_UBLOX_GNSS_ID_SBAS); // enable SBAS
  gnssDevice.enableGNSS(true, SFE_UBLOX_GNSS_ID_IMES); // enable IMES
  gnssDevice.enableGNSS(true, SFE_UBLOX_GNSS_ID_QZSS); // enable QZSS
  delay(2000); // Give the module some extra time to get ready
  gnssDevice.setI2COutput(COM_TYPE_UBX);      // Set the I2C port to output UBX only (turn off NMEA noise)
  gnssDevice.setNavigationFrequency(10);      //Produce 'n' solutions per second
  //gnssDevice.setAutoPVT(true);                //Tell the GNSS to send each solution
  sprintf(buffer512,"u-blox GNSS initialized on I2C - update at %dHz\n", gnssDevice.getNavigationFrequency());
  Serial.print(buffer512);
  remoteTimeOffset = 0;
  logging = false;
  Serial.printf("Node ID 0x%02X Start!\n", YAMURANODE_ID);
  delay(2000); // Give the module some extra time to get ready
}
//----------------------------------------------------------------------------------------
//   LOOP
//----------------------------------------------------------------------------------------
void loop() 
{
  if (((millis() + remoteTimeOffset)  % 2000) == 0)
  {
    digitalWrite(12, HIGH);
  }
  else if (((millis() + remoteTimeOffset)  % 1000) == 0)
  {
    digitalWrite(12, LOW);
  }
  // check GPS for update
  // Calling getPVT returns true if there actually is a fresh navigation solution available.
  // Start the reading only when valid LLH is available
  if ((logging == true) && (gnssDevice.getPVT()))
  {
    gps.gpsData.timeStamp = millis() + remoteTimeOffset;
    gps.gpsData.gpsYear =   gnssDevice.getYear();
    gps.gpsData.gpsMonth =  gnssDevice.getMonth();
    gps.gpsData.gpsDay =    gnssDevice.getDay();
    gps.gpsData.gpsHour =   gnssDevice.getHour();
    gps.gpsData.gpsMinute = gnssDevice.getMinute();
    gps.gpsData.gpsSecond = gnssDevice.getSecond();
    gps.gpsData.latitude =  gnssDevice.getLatitude();
    gps.gpsData.longitude = gnssDevice.getLongitude();
    gps.gpsData.course =    gnssDevice.getHeading();
    gps.gpsData.speed =     gnssDevice.getGroundSpeed();
    gps.gpsData.SIV =       gnssDevice.getSIV();
    gps.gpsData.accuracy =  0;
    SendFrame();
  }
  ReceiveFrame();
  delay(1);
}
//
// send gps frames from gps structure
//
void SendFrame()
{
  sendFrame[0].id = YAMURANODE_ID;
  sendFrame[0].rtr = false;
  sendFrame[0].ext = false;
  sendFrame[0].len = 8;
  memcpy(&sendFrame[0].data64, &gps.gpsBytes[0], 8);
  sendFrame[1].id = YAMURANODE_ID + 1;
  sendFrame[1].rtr = false;
  sendFrame[1].ext = false;
  sendFrame[1].len = 8;
  memcpy(&sendFrame[1].data64, &gps.gpsBytes[8], 8);
  sendFrame[2].id = YAMURANODE_ID + 2;
  sendFrame[2].rtr = false;
  sendFrame[2].ext = false;
  sendFrame[2].len = 8;
  memcpy(&sendFrame[2].data64, &gps.gpsBytes[16], 8);
  sendFrame[3].id = YAMURANODE_ID + 3;
  sendFrame[3].rtr = false;
  sendFrame[3].ext = false;
  sendFrame[3].len = 8;
  memcpy(&sendFrame[3].data64, &gps.gpsBytes[24], 8);
  #ifdef DEBUG_VERBOSE
  for(int frameIdx = 0; frameIdx < FRAME_COUNT; frameIdx++)
  {
    Serial.printf("%d\tOUT\tid 0x%03x\tbytes\t%d\tdata", millis(),
                                                         sendFrame[frameIdx].id,
                                                         sendFrame[frameIdx].len);
    for(int byteIdx = 0; byteIdx < 8; byteIdx++)
    {
      Serial.printf("\t0x%02X", (sendFrame[frameIdx].data_s8[byteIdx] & 0xFF));
    }
  }
  #endif
  for(int frameIdx = 0; frameIdx < FRAME_COUNT; frameIdx++)
  {
    if (!ACAN_ESP32::can.tryToSend (sendFrame[frameIdx])) 
    {
      //#ifdef DEBUG_VERBOSE
      Serial.printf("\tFAILED TO SEND FRAME ID 0x%X sequence %d!!!", sendFrame[frameIdx].id, frameIdx);
      //#endif
    }
  }
  #ifdef DEBUG_VERBOSE
  Serial.println();      
  #endif
}
void ReceiveFrame()
{
  if (ACAN_ESP32::can.receive (rcvFrame)) 
  {
    Serial.printf("%d\t0x%02X\tIN\tid 0x%03x\tbytes\t%d\tdata", millis() + remoteTimeOffset,
                                                        YAMURANODE_ID,
                                                        rcvFrame.id,
                                                        rcvFrame.len);
    for(int idx = 0; idx < 8; idx++)
    {
      Serial.printf("\t0x%02X", (rcvFrame.data_s8[idx] & 0xFF));
    }
    // heartbeat message
    if(rcvFrame.data32[0] == 0)
    {
      remoteTimeOffset = rcvFrame.data32[1] - millis();
      Serial.printf("\theartbeat offset %d\t", remoteTimeOffset);
    }
    else if(rcvFrame.data32[0] == 1)
    {
      Serial.printf("\tstart logging at %d\t", millis() + remoteTimeOffset);
      logging = true;
    }
    else if(rcvFrame.data32[0] == 2)
    {
      Serial.printf("\tstop logging at %d\t", millis() + remoteTimeOffset);
      logging = false;
    }
    Serial.println();
  }
}