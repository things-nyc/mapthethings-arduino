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
#include "Logging.h"

extern "C"{
  void debugPrint(const char *msg);
  void debugLog(const char *msg, uint16_t value);
  void debugLogData(const char *msg, uint8_t data[], uint16_t len);
}

extern "C" {
  void debugPrint(const char *msg) {
    Log.Debug(msg);
    Log.Debug(CR);
  }

  void debugLog(const char *msg, uint16_t value) {
    Log.Debug("%s %x" CR, msg, value);
  }

  void debugLogData(const char *msg, uint8_t data[], uint16_t len) {
    Log.Debug("%s: ", msg);
    for(int i=0; i<len; ++i) {
      Log.Debug("%x", data[i]);
    }
    Log.Debug(CR);
  }
} // extern "C".

#define CMD_DISCONNECT 1

void sendCommandCallback(uint8_t data[], uint16_t len) {
  uint16_t command = *(uint16_t *)data;
  Log.Debug("sendCommand: %d" CR, command);
  switch (command) {
    case CMD_DISCONNECT:
      bluetoothDisconnect();
      break;
  }
}

void sendPacketCallback(uint8_t data[], uint16_t len) {
  debugLogData("sendPacket: ", data, len);
  loraSendBytes(data, len);
}

void assignDevAddrCallback(uint8_t data[], uint16_t len) {
  debugLogData("assignDevAddr", data, len);
}

void assignNwkSKeyCallback(uint8_t data[], uint16_t len) {
  debugLogData("assignNwkSKey", data, len);
}

void assignAppSKeyCallback(uint8_t data[], uint16_t len) {
  debugLogData("assignAppSKey", data, len);
}

void assignSpreadingFactorCallback(uint8_t data[], uint16_t len) {
  if (len==1) {
    uint sf = data[0];
    Log.Debug("assignSpreadingFactor: %d" CR, sf);
    loraSetSF(sf);
  }
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
{
  UNINITIALIZED,
  "AT+GATTADDCHAR=UUID=0x2AD5,PROPERTIES=0x0A,MIN_LEN=1,MAX_LEN=1,DESCRIPTION=SF,VALUE=10",
  assignSpreadingFactorCallback
},
};

static void logToBluetooth(const char *s) {
  Serial.print(s);
  sendLogMessage(s);
}
static char logBuffer[200];
LogBufferedPrinter BluetoothPrinter(logToBluetooth, logBuffer, sizeof(logBuffer));

void setup() {
    // Log.Init(LOG_LEVEL_DEBUG, 115200);
    Log.Init(LOG_LEVEL_DEBUG, BluetoothPrinter);
    delay(1000);

    Log.Debug(F("Starting" CR));
    digitalWrite(LED_BUILTIN, LOW); // off

    Log.Debug(F("setupBluetooth" CR));
    setupBluetooth(charConfigs, COUNT(charConfigs));
    Log.Debug(F("setupLora" CR));
    setupLora();

    uint32_t dev = __builtin_bswap32(DEVADDR);
    setBluetoothCharData(charConfigs[2].charId, (const uint8_t*)&dev, 4);
    setBluetoothCharData(charConfigs[3].charId, NWKSKEY, 16);
    setBluetoothCharData(charConfigs[4].charId, APPSKEY, 16);
    Log.Debug(F("setup done" CR));
}

void readBatteryLevel() {
    #define VBATPIN A7

    float measuredvbat = analogRead(VBATPIN);
    measuredvbat *= 2;    // we divided by 2, so multiply back
    measuredvbat *= 3.3;  // Multiply by 3.3V, our reference voltage
    measuredvbat /= 1024; // convert to voltage
    Log.Debug("VBat: %f" CR, measuredvbat);

    // Normalize to 0-100
    float minv = 3.2;
    float maxv = 4.2;
    int level = 100 * (measuredvbat - minv) / (maxv - minv);
    Log.Debug("VBat int: %d" CR, level);
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

void loop() {
    loopBluetooth();
    loopLora();

    if (batCheckTimer.expired()) {
        batCheckTimer.set(batCheckInterval);
        readBatteryLevel();
    }
}
