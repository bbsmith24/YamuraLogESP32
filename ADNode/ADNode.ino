//----------------------------------------------------------------------------------------
// YamuraLog - ESP32 Node test
//----------------------------------------------------------------------------------------

#ifndef ARDUINO_ARCH_ESP32
  #error "Select an ESP32 board"
#endif
#define YAMURANODE_ID 0x10
#define SAMPLE_INTERVAL 10
#define FRAME_COUNT 2
#define AD_SIZE 13
//#define DEBUG_VERBOSE
struct __attribute__((packed)) AD_DataStructure
{
  uint32_t timeStamp;        // 4 bytes
  uint16_t analog[4];        // 4 x 2 bytes = 8 bytes
  byte digital;              // 1 byte (channels 0-3 used)
};
union AD_Structure
{
  AD_DataStructure adData;
  byte adBytes[AD_SIZE];
} adStructure;

//----------------------------------------------------------------------------------------
//   Include files
//----------------------------------------------------------------------------------------

#include <ACAN_ESP32.h>

//----------------------------------------------------------------------------------------
//  ESP32 Desired Bit Rate
//----------------------------------------------------------------------------------------

static const uint32_t DESIRED_BIT_RATE = 1000UL * 1000UL;

CANMessage sendFrame[FRAME_COUNT];
CANMessage rcvFrame;
unsigned long remoteTimeOffset;
static uint32_t sendDataTime = 0;
bool logging;
int digitalPins[4] = {25, 26, 27, 14};
int analogPins[4] = {34, 35, 32, 33};
int ledPin = 12;
//----------------------------------------------------------------------------------------
//   SETUP
//----------------------------------------------------------------------------------------
void setup () 
{
  for(int idx = 0; idx < 4; idx++)
  {
    pinMode(digitalPins[idx], INPUT_PULLUP);
  }
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);
  for(int idx = 0; idx < 20; idx++)
  {
      digitalWrite(ledPin, !digitalRead(ledPin));
      delay(100);
  }
  digitalWrite(ledPin, HIGH);
  Serial.begin (115200);
  delay (1000);
  Serial.println("=========================");
  Serial.printf ("| YamuraLog - Node 0x%02X |\n", YAMURANODE_ID);
  Serial.printf ("| Analog/Digital %03d    |\n", SAMPLE_INTERVAL);
  Serial.println("=========================");
  // Configure ESP32 CAN
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
    Serial.println("CAN Configuration OK!");
  }
  else
  {
    Serial.print ("CAN Configuration error 0x");
    Serial.println (errorCode, HEX);
  }
  remoteTimeOffset = 0;
  logging = false;
  Serial.printf("Node ID 0x%02X Start!\n", YAMURANODE_ID);
}
//----------------------------------------------------------------------------------------
//   LOOP
//----------------------------------------------------------------------------------------
void loop() 
{
  if (((millis() + remoteTimeOffset)  % 2000) == 0)
  {
    digitalWrite(ledPin, HIGH);
  }
  else if (((millis() + remoteTimeOffset)  % 1000) == 0)
  {
    digitalWrite(ledPin, LOW);
  }
  if((logging == true) && (((millis() + remoteTimeOffset) % SAMPLE_INTERVAL) == 0))
  {
    adStructure.adData.timeStamp = millis() + remoteTimeOffset;
    adStructure.adData.digital = 0;
    for(int idx = 0; idx < 4; idx++)
    {
      adStructure.adData.digital |= (digitalRead(digitalPins[idx]) << idx);
      adStructure.adData.analog[idx]  = analogRead(analogPins[idx]);
    }
    SendFrame();
  }
  ReceiveFrame();
  delay(1);
}
//
// send accelerometer frames
// frame0 - timestamp, X and Y
// frame1 - sequence, Z
//
void SendFrame()
{
  /*
  struct __attribute__((packed)) AD_DataStructure
  {
    uint32_t timeStamp;        // 4 bytes
    uint16_t analog[4];        // 4 x 2 bytes = 8 bytes
    byte digital;              // 1 byte (channels 0-3 used)
  };
  union AD_Structure
  {
    AD_DataStructure imuData;
    byte adBytes[AD_SIZE];
  } adStructure;
  */
  sendFrame[0].id = YAMURANODE_ID;
  sendFrame[0].rtr = false;
  sendFrame[0].ext = false;
  sendFrame[0].len = 8;
  memcpy(&sendFrame[0].data64, &adStructure.adBytes[0], 8);
  sendFrame[1].id = YAMURANODE_ID + 1;
  sendFrame[1].rtr = false;
  sendFrame[1].ext = false;
  sendFrame[1].len = 8;
  memcpy(&sendFrame[1].data64, &adStructure.adBytes[8], 5);
  #ifdef DEBUG_VERBOSE
  for(int frameIdx = 0; frameIdx < FRAME_COUNT; frameIdx++)
  {
    Serial.printf("%d\tOUT\tid 0x%03x\tbytes\t%d\tdata", millis() + remoteTimeOffset,
                                                         sendFrame[frameIdx].id,
                                                         sendFrame[frameIdx].len);
    for(int byteIdx = 0; byteIdx < 8; byteIdx++)
    {
      Serial.printf("\t0x%02X", (sendFrame[frameIdx].data_s8[byteIdx] & 0xFF));
    }
    Serial.println();
  }
  #endif
  for(int frameIdx = 0; frameIdx < FRAME_COUNT; frameIdx++)
  {
    if (!ACAN_ESP32::can.tryToSend (sendFrame[frameIdx])) 
    {
      //#ifdef DEBUG_VERBOSE
      Serial.printf("\tFAILED TO SEND FRAME ID 0x%X sequence %d len %d!!!\n", sendFrame[frameIdx].id, frameIdx, sendFrame[frameIdx].len);
      //#endif
    }
  }
  #ifdef DEBUG_VERBOSE
  Serial.println();      
  #endif
}
void ReceiveFrame()
{
  if ((ACAN_ESP32::can.receive (rcvFrame)) && (rcvFrame.id == 0x0)) 
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
      // acknoledge
      sendFrame[0].id = YAMURANODE_ID;
      sendFrame[0].rtr = false;
      sendFrame[0].ext = false;
      sendFrame[0].len = 8;
      if (!ACAN_ESP32::can.tryToSend (sendFrame[0])) 
      {
        //#ifdef DEBUG_VERBOSE
        Serial.printf("\tFAILED TO SEND FRAME ID 0x%X sequence %d len %d!!!\n", sendFrame[0].id, 0, sendFrame[0].len);
        //#endif
      }
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