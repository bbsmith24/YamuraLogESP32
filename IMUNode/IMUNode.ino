//----------------------------------------------------------------------------------------
// YamuraLog - IMU Node
//----------------------------------------------------------------------------------------

#ifndef ARDUINO_ARCH_ESP32
  #error "Select an ESP32 board"
#endif
#define YAMURANODE_ID 0x20
#define SAMPLE_INTERVAL 10
#define FRAME_COUNT 2
//#define DEBUG_VERBOSE

//----------------------------------------------------------------------------------------
//   Include files
//----------------------------------------------------------------------------------------

#include <ACAN_ESP32.h>
#include <Wire.h> // I2C library included for SparkFunLSM9DS1
#include <Arduino_LSM9DS1.h>

//----------------------------------------------------------------------------------------
//  ESP32 Desired Bit Rate
//----------------------------------------------------------------------------------------

static const uint32_t DESIRED_BIT_RATE = 1000UL * 1000UL;

CANMessage sendFrame[2];
CANMessage rcvFrame;
unsigned long curMillis = 0;
unsigned long lastSent = 0;
unsigned long remoteTimeOffset;
static uint32_t sendDataTime = 0;
bool logging;
float x, y, z;
unsigned long lastOn = 0;

// CAN message structure to send
// 8 data bytes per CAN message, 4 frames, 32 bytes of data
#define IMU_SIZE 16
#define FRAME_COUNT 2
//
struct __attribute__((packed)) IMU_DataStructure
{
  uint32_t timeStamp;        // 4 bytes
  float xAccel;              // 4 bytes
  float yAccel;              // 4 bytes
  float zAccel;              // 4 bytes
};
union IMU_Structure
{
  IMU_DataStructure imuData;
  byte imuBytes[IMU_SIZE];
} imuStructure;
TaskHandle_t Task0;
TaskHandle_t Task1;

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
  Serial.printf ("| Analog/Digital %03d    |\n", SAMPLE_INTERVAL);
  Serial.println("=========================");
  // Configure ESP32 CAN
  Serial.println("CAN Bus setup");
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
  Serial.println("IMU setup");
  if (!IMU.begin()) {
    Serial.println("Failed to initialize IMU!");
    while (1);
  }

  Serial.print("Accelerometer sample rate = ");
  Serial.print(IMU.accelerationSampleRate());
  Serial.println(" Hz");
  remoteTimeOffset = 0;
  logging = false;
  Serial.printf("Node ID 0x%02X Start!\n", YAMURANODE_ID);
  xTaskCreatePinnedToCore(
                    HeartbeatLEDs,   // Task function. 
                    "Task0",     // name of task.
                    10000,       // Stack size of task
                    NULL,        // parameter of the task
                    1,           // priority of the task
                    &Task0,      // Task handle to keep track of created task
                    0);          // pin task to core 0  
}
//----------------------------------------------------------------------------------------
//   LOOP
//----------------------------------------------------------------------------------------
void loop() 
{
  curMillis = millis();
  if((logging == true) && (((curMillis + remoteTimeOffset) - lastSent) >= SAMPLE_INTERVAL))
  {
    lastSent = (curMillis + remoteTimeOffset);
    IMU.readAcceleration(x, y, z);
    imuStructure.imuData.timeStamp = curMillis + remoteTimeOffset;
    imuStructure.imuData.xAccel = x;
    imuStructure.imuData.yAccel = y;
    imuStructure.imuData.zAccel = z;
    SendFrame();
  }
  ReceiveFrame();
  delay(1);
}
//
// send IMU 
// frame0 - timestamp, 1st 32 bits
//          X acceleration (32 bit float)
// frame1 - Y, Z acceleration (32 bit float)
// actual data depends on configuration
//
void SendFrame()
{
  sendFrame[0].id = YAMURANODE_ID;
  sendFrame[0].rtr = false;
  sendFrame[0].ext = false;
  sendFrame[0].len = 8;
  memcpy(&sendFrame[0].data64, &imuStructure.imuBytes[0], 8);
  sendFrame[1].id = YAMURANODE_ID + 1;
  sendFrame[1].rtr = false;
  sendFrame[1].ext = false;
  sendFrame[1].len = 8;
  memcpy(&sendFrame[1].data64, &imuStructure.imuBytes[8], 8);
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
      #ifdef DEBUG_VERBOSE
      Serial.printf("\tFAILED TO SEND FRAME ID 0x%X sequence %d!!!\n", sendFrame[frameIdx].id, frameIdx);
      #endif
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
    // heartbeat message
    if(rcvFrame.data32[0] == 0)
    {
      remoteTimeOffset = rcvFrame.data32[1] - millis();
      remoteTimeOffset += ((millis() + remoteTimeOffset) % 1000);
      digitalWrite(12, HIGH);
      lastOn = (millis() + remoteTimeOffset);
      Serial.printf("%d IMU heartbeat offset %d on core %d\t", millis() + remoteTimeOffset, remoteTimeOffset, xPortGetCoreID());
      // acknoledge
      sendFrame[0].id = YAMURANODE_ID;
      sendFrame[0].rtr = false;
      sendFrame[0].ext = false;
      sendFrame[0].len = 8;
      if (!ACAN_ESP32::can.tryToSend (sendFrame[0])) 
      {
        #ifdef DEBUG_VERBOSE
        Serial.printf("\tFAILED TO SEND FRAME ID 0x%X sequence %d len %d!!!\n", sendFrame[0].id, 0, sendFrame[0].len);
        #endif
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
//
//
//
void HeartbeatLEDs(void * pvParameters)
{
  for(;;)
  {
    if (((millis() + remoteTimeOffset) - lastOn) >= 1000)
    {
      digitalWrite(12, !digitalRead(12));
      lastOn = (millis() + remoteTimeOffset);
    }
    delay(1);
  }
}