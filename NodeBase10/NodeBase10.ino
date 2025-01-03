//----------------------------------------------------------------------------------------
// YamuraLog - ESP32 Node test
//----------------------------------------------------------------------------------------

#ifndef ARDUINO_ARCH_ESP32
  #error "Select an ESP32 board"
#endif
#define YAMURANODE_ID 10
#define SAMPLE_INTERVAL 10
#define FRAME_COUNT 2
//#define DEBUG_VERBOSE

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
  Serial.println("=========================");
  Serial.printf ("| YamuraLog - Node 0x%02X |\n", YAMURANODE_ID);
  Serial.printf ("| Accelerometer %03d     |\n", SAMPLE_INTERVAL);
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
    Serial.println("Configuration OK!");
  }
  else
  {
    Serial.print ("Configuration error 0x");
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
    digitalWrite(12, HIGH);
  }
  else if (((millis() + remoteTimeOffset)  % 1000) == 0)
  {
    digitalWrite(12, LOW);
  }
  if((logging == true) && (((millis() + remoteTimeOffset) % SAMPLE_INTERVAL) == 0))
  {
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
  sendFrame[0].id = YAMURANODE_ID;
  sendFrame[0].rtr = false;
  sendFrame[0].ext = false;
  sendFrame[0].len = 8;
  sendFrame[0].data32[0] = millis() + remoteTimeOffset;
  sendFrame[0].data16[2] = 0xA;  // X axis * 1000
  sendFrame[0].data16[3] = 0xB;  // Y axis * 1000
  sendFrame[1].id = YAMURANODE_ID + 1;
  sendFrame[1].rtr = false;
  sendFrame[1].ext = false;
  sendFrame[1].len = 8;
  sendFrame[1].data16[0] = 0xC;  // Z axis * 1000
  sendFrame[1].data16[1] = 0;    // sequence
  sendFrame[1].data16[2] = 0;    // sequence
  sendFrame[1].data16[3] = 0;
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
  }
  #endif
  for(int frameIdx = 0; frameIdx < FRAME_COUNT; frameIdx++)
  {
    if (!ACAN_ESP32::can.tryToSend (sendFrame[frameIdx])) 
    {
      #ifdef DEBUG_VERBOSE
      Serial.printf("\tFAILED TO SEND FRAME ID 0x%X sequence %d!!!", sendFrame[frameIdx].id, frameIdx);
      #endif
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