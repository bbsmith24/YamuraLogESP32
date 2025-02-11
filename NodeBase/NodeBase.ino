//----------------------------------------------------------------------------------------
// YamuraLog - ESP32 Node test
//----------------------------------------------------------------------------------------

#ifndef ARDUINO_ARCH_ESP32
  #error "Select an ESP32 board"
#endif
#define YAMURANODE_ID 10
//#define DEBUG_VERBOSE

//----------------------------------------------------------------------------------------
//   Include files
//----------------------------------------------------------------------------------------

#include <ACAN_ESP32.h>

//----------------------------------------------------------------------------------------
//  ESP32 Desired Bit Rate
//----------------------------------------------------------------------------------------

static const uint32_t DESIRED_BIT_RATE = 1000UL * 1000UL;

CANMessage sendFrame;
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
  // Configure ESP32 CAN
  Serial.printf("\n\nYamuraLogESP32 - Node Base CAN ID 0x%04X\n\n", YAMURANODE_ID);
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
  if((logging == true) && (sendDataTime < millis()))
  {
    sendDataTime += 10;
    SendFrame();
  }
  ReceiveFrame();
  delay(1);
}
void SendFrame()
{
  sendFrame.id = YAMURANODE_ID;
  sendFrame.rtr = false;
  sendFrame.ext = false;
  sendFrame.len = 8;
  sendFrame.data32[0] = millis() + remoteTimeOffset;;
  sendFrame.data_s8[4] = 0xA;
  sendFrame.data_s8[5] = 0xB;
  sendFrame.data_s8[6] = 0xC;
  sendFrame.data_s8[7] = 0xD;
  #ifdef DEBUG_VERBOSE
  Serial.printf("%d\tOUT\tid 0x%03x\tbytes\t%d\tdata", millis() + remoteTimeOffset,
                                                       sendFrame.id,
                                                       sendFrame.len);
  for(int idx = 0; idx < 8; idx++)
  {
    Serial.printf("\t0x%02X", (sendFrame.data_s8[idx] & 0xFF));
  }
  #endif
  if (!ACAN_ESP32::can.tryToSend (sendFrame)) 
  {
    #ifdef DEBUG_VERBOSE
    Serial.printf("\tFAILED TO SEND FRAME ID 0x%X!!!", sendFrame.id);
    #endif
  }
  #ifdef DEBUG_VERBOSE
  Serial.println();      
  #endif
}
void ReceiveFrame()
{
  if (ACAN_ESP32::can.receive (rcvFrame)) 
  {
    Serial.printf("%d\tIN\tid 0x%03x\tbytes\t%d\tdata", millis() + remoteTimeOffset,
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