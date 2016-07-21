/*******************************************************************************
 * Copyright (c) 2016 Frank Leon Rose
 *
 * Permission is hereby granted, free of charge, to anyone
 * obtaining a copy of this document and accompanying files,
 * to do whatever they want with them without any restriction,
 * including, but not limited to, copying, modification and redistribution.
 * NO WARRANTY OF ANY KIND IS PROVIDED.
 *
 *******************************************************************************/

#include "Lora.h"
#include "Bluetooth.h"
#include "Adafruit_BLE.h" // Define TimeoutTimer

extern "C"{
  void debugLog(char *msg, uint16_t value);
}

void debugLog(char *msg, uint16_t value) {
  Serial.println(msg);
  Serial.println(value, HEX);
}

#define CMD_DISCONNECT 1

void sendCommandCallback(uint8_t data[], uint16_t len) {
  uint16_t command = *(uint16_t *)data;
  Serial.print("sendCommand: ");
  Serial.println(command);
  switch (command) {
    case CMD_DISCONNECT:
      bluetoothDisconnect();
      break;
  }
}

void sendPacketCallback(uint8_t data[], uint16_t len) {
  Serial.print("sendPacket: ");
  for(int i=0; i<len; ++i) {
    Serial.print(data[i], HEX);
  }
  Serial.println("");
  loraSendBytes(data, len);
}

void assignDevAddrCallback(uint8_t data[], uint16_t len) {
  Serial.println("assignDevAddr");
}

void assignNwkSKeyCallback(uint8_t data[], uint16_t len) {
  Serial.println("assignNwkSKey");
}

void assignAppSKeyCallback(uint8_t data[], uint16_t len) {
  Serial.println("assignAppSKey");
}

CharacteristicConfigType charConfigs[] = {
{
  UNINITIALIZED,
  // 0x08 write only, datatype 3 is int
  "AT+GATTADDCHAR=UUID=0x2AD0,PROPERTIES=0x08,MIN_LEN=2,MAX_LEN=2,DATATYPE=3,DESCRIPTION=Command",
  sendCommandCallback
},
{
  UNINITIALIZED,
  "AT+GATTADDCHAR=UUID=0x2AD1,PROPERTIES=0x08,MIN_LEN=1,MAX_LEN=20,DATATYPE=2,DESCRIPTION=Send packet",
  sendPacketCallback
},
{
  UNINITIALIZED,
  // 0x0A read/write, datatype 2 is byte array
  "AT+GATTADDCHAR=UUID=0x2AD2,PROPERTIES=0x0A,MIN_LEN=4,MAX_LEN=4,DATATYPE=2,DESCRIPTION=DevAddr",
  assignDevAddrCallback
},
{
  UNINITIALIZED,
  "AT+GATTADDCHAR=UUID=0x2AD3,PROPERTIES=0x0A,MIN_LEN=16,MAX_LEN=16,DATATYPE=2,DESCRIPTION=NwkSKey",
  assignNwkSKeyCallback
},
{
  UNINITIALIZED,
  "AT+GATTADDCHAR=UUID=0x2AD4,PROPERTIES=0x0A,MIN_LEN=16,MAX_LEN=16,DATATYPE=2,DESCRIPTION=AppSKey",
  assignAppSKeyCallback
},
};

void setup() {
    Serial.begin(115200);
    Serial.println(F("Starting"));
    digitalWrite(LED_BUILTIN, LOW); // off

    setupBluetooth(charConfigs, COUNT(charConfigs));
    setupLora();

    uint32_t dev = __builtin_bswap32(DEVADDR);
    setBluetoothCharData(charConfigs[2].charId, (const uint8_t*)&dev, 4);
    setBluetoothCharData(charConfigs[3].charId, NWKSKEY, 16);
    setBluetoothCharData(charConfigs[4].charId, APPSKEY, 16);
}

void readBatteryLevel() {
    #define VBATPIN A7
       
    float measuredvbat = analogRead(VBATPIN);
    measuredvbat *= 2;    // we divided by 2, so multiply back
    measuredvbat *= 3.3;  // Multiply by 3.3V, our reference voltage
    measuredvbat /= 1024; // convert to voltage
    Serial.print("VBat: " ); Serial.println(measuredvbat);

    // Normalize to 0-100
    float minv = 3.2;
    float maxv = 4.2;
    int level = 100 * (measuredvbat - minv) / (maxv - minv);
    Serial.print("VBat int: " ); Serial.println(level);
    if (level<0) {
      level = 0;
    }
    else if (level>100) {
      level = 100;
    }
    sendBatteryLevel(level);
}

const static long batCheckInterval = 60000; // Every minute
static TimeoutTimer batCheckTimer;
static uint8_t batteryLevel;

void loop() {
    loopBluetooth();
    loopLora();

    if (batCheckTimer.expired()) {
        batCheckTimer.set(batCheckInterval);
        readBatteryLevel();
    }
}
