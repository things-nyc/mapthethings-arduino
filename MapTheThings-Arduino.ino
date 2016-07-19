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


extern "C"{
  void debugLog(char *msg, uint16_t value);
}

void debugLog(char *msg, uint16_t value) {
  Serial.println(msg);
  Serial.println(value, HEX);
}

void sendCommandCallback(uint8_t data[], uint16_t len) {
  Serial.println("sendCommand");
  uint16_t command = *(uint16_t *)data;
  Serial.println(command);
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

void loop() {
    loopBluetooth();
    loopLora();
}
