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

// #define DEBUG_SERIAL_LOGGING
// #define DEBUG_FORGET_SESSION_VARS

#if 0 // Defining ABP parameters statically
  // Device as part of Staging app
  // #define APPSKEY { 0x43, 0x0D, 0x53, 0xB2, 0x72, 0xA6, 0x47, 0xAF, 0x5D, 0xFF, 0x6A, 0x16, 0x7A, 0xB7, 0x9A, 0x20 }
  // #define NWKSKEY { 0x80, 0x42, 0x43, 0x64, 0x2C, 0x1E, 0x3B, 0x04, 0x36, 0x6D, 0x36, 0xC3, 0x90, 0x9F, 0xCA, 0xA2 }
  // #define DEVADDR { 0xDE, 0xAD, 0xAA, 0xAA }
  // Device as part of V2 app
  #define APPSKEY { 0xE7, 0xD0, 0x0E, 0xB6, 0xB7, 0x8D, 0xAC, 0x42, 0x97, 0xA4, 0x18, 0xD3, 0xFC, 0x8F, 0xA2, 0x4B }
  #define NWKSKEY { 0xCD, 0x67, 0x8D, 0x4F, 0x90, 0xAE, 0x3F, 0x16, 0x64, 0x55, 0x1E, 0x5A, 0x59, 0x68, 0x2C, 0xE6 }
  #define DEVADDR { 0x26, 0x01, 0x2B, 0x32 }
#endif
#if 1 // Defining OTAA parameters statically
  // MSB order
  #define APPKEY { 0xA6, 0xA0, 0x44, 0xC4, 0xCF, 0x74, 0xD3, 0x73, 0xB0, 0x2C, 0xAF, 0x63, 0xA9, 0xBD, 0x9F, 0x64 }
  // LSB order
  #define APPEUI { 0x38, 0x1C, 0x00, 0xF0, 0x7E, 0xD5, 0xB3, 0x70 }
  #define DEVEUI { 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00 }
#endif

#include "Lora.h"
#include "Bluetooth.h"
#include "Adafruit_BLE.h" // Define TimeoutTimer
#include "Logging.h"

#define offset(s, field) ((u1_t *)(&s.field) - (u1_t *)&s)

typedef struct {
#define PERSISTENT_FORMAT_UNINITIALIZED 0
#define PERSISTENT_FORMAT_V1 1
  uint32_t format;
  uint32_t flags; // Bit flags indicating what values in structure are set

  uint32_t seq_no;

#define FLAG_SESSION_VARS_SET (1 << 0) // Set of DevAddr, NwkSKey, and AppSKey set.
  u1_t DevAddr[4];
  u1_t NwkSKey[16];
  u1_t AppSKey[16];

// Separate set flags because BLE can't send all at once
#define FLAG_APP_KEY_SET (1 << 1)
  u1_t AppKey[16];
#define FLAG_APP_EUI_SET (1 << 2)
  u1_t AppEUI[8];
#define FLAG_DEV_EUI_SET (1 << 3)
  u1_t DevEUI[8];
} PersistentSettings;

PersistentSettings settings;

// Otherwise, this value connects a sendPacket request with tx result notification
static struct {
  bool active;
  u1_t bleSeq;
} CurrentTx = {false, 0};

extern "C" {
  void debugPrint(const char *msg) {
    Log.Debug(msg);
    Log.Debug(CR);
  }

  void debugLog(const char *msg, uint32_t value) {
    Log.Debug("%s %x" CR, msg, value);
  }

  void debugLogData(const char *msg, uint8_t data[], uint16_t len) {
    char buffer[200+1];
    Log.Debug("%s: " CR, msg);
    Log.Debug("Length: %d" CR, len);
    for(int i=0; i<min(len, sizeof(buffer)/2); ++i) {
      snprintf(buffer+2*i, sizeof(buffer)-2*i, "%02x", data[i]);
    }
    Log.Debug("%s" CR, buffer);
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

void enqueuePacket(uint8_t bleSeq, uint8_t data[], uint16_t len) {
  debugLog("sendPacket with BLE seq: ", bleSeq);
  debugLogData("sendPacket: ", data, len);
  if (!CurrentTx.active) {
    CurrentTx.active = true;
    CurrentTx.bleSeq = bleSeq;
    loraSendBytes(data, len);
  }
  else {
    debugPrint("Send ignored - active transmission not completed");
  }
}
void sendPacketCallback(uint8_t data[], uint16_t len) {
  enqueuePacket(0, data, len);
}

void sendPacketWithAckCallback(uint8_t data[], uint16_t len) {
  // Includes ble seq as first byte of packet. Don't send that out.
  enqueuePacket(data[0], data+1, len-1);
}

void saveSettingBytes(uint8_t offset, uint8_t *bytes, uint8_t length) {
  bool success = writeNVBytes(offset, bytes, length);
  if (!success) {
    debugPrint("ERROR: Failed to write settings bytes!");
    debugLog("Offset:", offset);
    debugLog("Length:", length);
    debugLogData("Data:", bytes, length);
  }
}

void saveSettingValue(uint8_t offset, uint32_t value) {
  bool success = writeNVInt(offset, value);
  if (!success) {
    debugPrint("ERROR: Failed to write settings value!");
    debugLog("Offset:", offset);
    debugLog("Value:", value);
  }
}

#define AssignSessionCallback(key) \
void assign##key##Callback(uint8_t data[], uint16_t len) { \
  debugLogData("assign" #key, data, len); \
  if (len==sizeof(settings.key)) {        \
    memcpy(settings.key, data, sizeof(settings.key)); \
    saveSettingBytes(offset(settings, key), settings.key, sizeof(settings.key)); \
    loraSetSessionKeys(settings.seq_no, settings.AppSKey, settings.NwkSKey, settings.DevAddr); \
  } \
}

AssignSessionCallback(DevAddr)
AssignSessionCallback(NwkSKey)
AssignSessionCallback(AppSKey)

#define AssignAppCallback(key, flag) \
void assign##key##Callback(uint8_t data[], uint16_t len) { \
  debugLogData("assign" #key, data, len); \
  if (len==sizeof(settings.key)) {        \
    memcpy(settings.key, data, sizeof(settings.key)); \
    saveSettingBytes(offset(settings, key), settings.key, sizeof(settings.key)); \
    settings.flags |= flag; \
    saveSettingValue(offset(settings, flags), settings.flags); \
  } \
}

AssignAppCallback(AppKey, FLAG_APP_KEY_SET)
AssignAppCallback(AppEUI, FLAG_APP_EUI_SET)
AssignAppCallback(DevEUI, FLAG_DEV_EUI_SET)

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
  // 0x08 write to device only, datatype 3 is int
  "AT+GATTADDCHAR=UUID=0x2AD0,PROPERTIES=0x08,MIN_LEN=2,MAX_LEN=2,DATATYPE=3,DESCRIPTION=Command",
  sendCommandCallback
},
{
  UNINITIALIZED,
  "AT+GATTADDCHAR=UUID=0x2AD1,PROPERTIES=0x08,MIN_LEN=1,MAX_LEN=20,DATATYPE=2,DESCRIPTION=Send packet",
  sendPacketCallback
},
#define GattDevAddr (charConfigs[2])
{
  UNINITIALIZED,
  // 0x0A read/write, datatype 2 is byte array
  "AT+GATTADDCHAR=UUID=0x2AD2,PROPERTIES=0x0A,MIN_LEN=4,MAX_LEN=4,DATATYPE=2,DESCRIPTION=DevAddr",
  assignDevAddrCallback
},
#define GattNwkSKey (charConfigs[3])
{
  UNINITIALIZED,
  "AT+GATTADDCHAR=UUID=0x2AD3,PROPERTIES=0x0A,MIN_LEN=16,MAX_LEN=16,DATATYPE=2,DESCRIPTION=NwkSKey",
  assignNwkSKeyCallback
},
#define GattAppSKey (charConfigs[4])
{
  UNINITIALIZED,
  "AT+GATTADDCHAR=UUID=0x2AD4,PROPERTIES=0x0A,MIN_LEN=16,MAX_LEN=16,DATATYPE=2,DESCRIPTION=AppSKey",
  assignAppSKeyCallback
},
#define GattAppKey (charConfigs[5])
{
  UNINITIALIZED,
  "AT+GATTADDCHAR=UUID=0x2AD7,PROPERTIES=0x0A,MIN_LEN=16,MAX_LEN=16,DATATYPE=2,DESCRIPTION=AppKey",
  assignAppKeyCallback
},
#define GattAppEUI (charConfigs[6])
{
  UNINITIALIZED,
  "AT+GATTADDCHAR=UUID=0x2AD8,PROPERTIES=0x0A,MIN_LEN=8,MAX_LEN=8,DATATYPE=2,DESCRIPTION=AppEUI",
  assignAppEUICallback
},
#define GattDevEUI (charConfigs[7])
{
  UNINITIALIZED,
  "AT+GATTADDCHAR=UUID=0x2AD9,PROPERTIES=0x0A,MIN_LEN=8,MAX_LEN=8,DATATYPE=2,DESCRIPTION=DevEUI",
  assignDevEUICallback
},
#define GattSF (charConfigs[8])
{
  UNINITIALIZED,
  "AT+GATTADDCHAR=UUID=0x2AD5,PROPERTIES=0x0A,MIN_LEN=1,MAX_LEN=1,DESCRIPTION=SF,VALUE=10",
  assignSpreadingFactorCallback
},
#define GattSendAckdPacket (charConfigs[9])
{
  UNINITIALIZED,
  "AT+GATTADDCHAR=UUID=0x2ADB,PROPERTIES=0x08,MIN_LEN=1,MAX_LEN=20,DATATYPE=2,DESCRIPTION=Send acknowledged packet",
  sendPacketWithAckCallback
},
};

static void logToBluetooth(const char *s) {
  if (Serial) {
    Serial.print(s);
  }
  sendLogMessage(s);
}
static char logBuffer[200];
LogBufferedPrinter BluetoothPrinter(logToBluetooth, logBuffer, sizeof(logBuffer));

bool loadStaticLoraDefines(PersistentSettings &settings) {
  debugPrint("loadStaticLoraDefines");
  bool write = false; // Can't just compare old and new flags because we may be changing a value already set - flag would stay the same
  #ifdef PROGMEM
    #define _memcpy memcpy_P
  #else
    #define _memcpy memcpy
  #endif
  #if defined(DEVADDR)
    static const PROGMEM u1_t devaddr[] = DEVADDR;
    static_assert(sizeof(settings.DevAddr) == sizeof(devaddr), "DEVADDR needs to be 4 bytes");
    _memcpy(settings.DevAddr, devaddr, sizeof(settings.DevAddr));
  #endif
  #if defined(APPSKEY)
    static const PROGMEM u1_t appskey[] = APPSKEY;
    static_assert(sizeof(settings.AppSKey) == sizeof(appskey), "APPSKEY needs to be 16 bytes");
    _memcpy(settings.AppSKey, appskey, sizeof(settings.AppSKey));
  #endif
  #if defined(NWKSKEY)
    static const PROGMEM u1_t nwkskey[] = NWKSKEY;
    static_assert(sizeof(settings.NwkSKey) == sizeof(nwkskey), "NWKSKEY needs to be 16 bytes");
    _memcpy(settings.NwkSKey, nwkskey, sizeof(settings.NwkSKey));
  #endif
  #if defined(DEVADDR) && defined(APPSKEY) && defined(NWKSKEY)
    settings.flags |= FLAG_SESSION_VARS_SET;
    write = true;
  #endif

  #if defined(APPKEY)
    static const PROGMEM u1_t appkey[] = APPKEY;
    static_assert(sizeof(settings.AppKey) == sizeof(appkey), "APPKEY needs to be 16 bytes");
    _memcpy(settings.AppKey, appkey, sizeof(settings.AppKey));
    settings.flags |= FLAG_APP_KEY_SET;
    debugLogData("Setting AppKey", settings.AppKey, sizeof(settings.AppKey));
    write = true;
  #endif
  #if defined(APPEUI)
    static const PROGMEM u1_t appeui[] = APPEUI;
    static_assert(sizeof(settings.AppEUI) == sizeof(appeui), "APPEUI needs to be 8 bytes");
    _memcpy(settings.AppEUI, appeui, sizeof(settings.AppEUI));
    settings.flags |= FLAG_APP_EUI_SET;
    debugLogData("Setting AppEUI", settings.AppEUI, sizeof(settings.AppEUI));
    write = true;
  #endif
  #if defined(DEVEUI)
    static const PROGMEM u1_t deveui[] = DEVEUI;
    static_assert(sizeof(settings.DevEUI) == sizeof(deveui), "DEVEUI needs to be 8 bytes");
    _memcpy(settings.DevEUI, deveui, sizeof(settings.DevEUI));
    settings.flags |= FLAG_DEV_EUI_SET;
    debugLogData("Setting DevEUI", settings.DevEUI, sizeof(settings.DevEUI));
    write = true;
  #endif

  debugLog("Flags: ", settings.flags);
  return write;
}

void reportSessionVars() {
  setBluetoothCharData(GattDevAddr.charId, settings.DevAddr, sizeof(settings.DevAddr));
  setBluetoothCharData(GattNwkSKey.charId, settings.NwkSKey, sizeof(settings.NwkSKey));
  setBluetoothCharData(GattAppSKey.charId, settings.AppSKey, sizeof(settings.AppSKey));
}

void onJoin(u1_t *appskey, u1_t *nwkskey, u1_t *devaddr) {
  if (appskey==NULL) {
    debugPrint("Join failed");
  }
  else {
    debugPrint("Join succeeded");
    memcpy(settings.AppSKey, appskey, sizeof(settings.AppSKey));
    memcpy(settings.NwkSKey, nwkskey, sizeof(settings.NwkSKey));
    memcpy(settings.DevAddr, devaddr, sizeof(settings.DevAddr));
    settings.flags |= FLAG_SESSION_VARS_SET;
    reportSessionVars();
    saveSettingBytes(0, (u1_t *)(&settings), sizeof(settings));
  }
}

void onTransmit(uint16_t error, uint32_t tx_seq_no, u1_t *received, u1_t length) {
  if (!CurrentTx.active) {
    debugLog("Received onTransmit callback without active transmission.", error);
  }
  else {
    CurrentTx.active = false;
    if (!error) {
      // Success!
      settings.seq_no = tx_seq_no + 1;
      debugLog("Successful transmission. Storing NEXT lora seq:", settings.seq_no);
      saveSettingValue(offset(settings, seq_no), settings.seq_no);

      debugLog("Successful transmission. Returning BLE seq:", CurrentTx.bleSeq);
      sendTxResult(CurrentTx.bleSeq, error, tx_seq_no);
    }
  }
}

void loadSettings() {
  if (readNVBytes(0, (u1_t *)(&settings), sizeof(settings))) {
    debugLogData("Read bytes from NVRAM", (u1_t *)(&settings), sizeof(settings));
    // int i = offset(settings, AppEUI);
    bool write = false;
    if (settings.format==PERSISTENT_FORMAT_UNINITIALIZED) {
      debugPrint("Updating uninitialized to V1");
      settings.format = PERSISTENT_FORMAT_V1;
      settings.flags = 0;
      write = true;
    }

    if (settings.format==PERSISTENT_FORMAT_V1) {
      write = write || loadStaticLoraDefines(settings);
      #if defined(DEBUG_FORGET_SESSION_VARS)
      settings.flags &= ~FLAG_SESSION_VARS_SET;  // Debug erase session vars set
      write = true;
      #endif
    }
    if (write) {
      debugLogData("Writing updated settings", (u1_t *)(&settings), sizeof(settings));
      saveSettingBytes(0, (u1_t *)(&settings), sizeof(settings));
    }
  }
  else {
    Log.Debug(F("Failed to read settings" CR));
    return;
  }

  if (settings.flags & FLAG_SESSION_VARS_SET) {
    debugPrint("Session vars set - initializing lora");
    loraSetSessionKeys(settings.seq_no, settings.AppSKey, settings.NwkSKey, settings.DevAddr);

    reportSessionVars();
  }
  else if ((settings.flags & FLAG_APP_KEY_SET) && (settings.flags & FLAG_APP_EUI_SET) && (settings.flags & FLAG_DEV_EUI_SET)) {
    debugPrint("Join keys set - initializing lora join");
    loraJoin(settings.seq_no, settings.AppKey, settings.AppEUI, settings.DevEUI, onJoin);
  }
  if (settings.flags & FLAG_APP_KEY_SET) {
    setBluetoothCharData(GattAppKey.charId, settings.AppKey, sizeof(settings.AppKey));
  }
  if (settings.flags & FLAG_APP_EUI_SET) {
    setBluetoothCharData(GattAppEUI.charId, settings.AppEUI, sizeof(settings.AppEUI));
  }
  if (settings.flags & FLAG_DEV_EUI_SET) {
    setBluetoothCharData(GattDevEUI.charId, settings.DevEUI, sizeof(settings.DevEUI));
  }
}

void setup() {
    #if defined(DEBUG_SERIAL_LOGGING)
    Log.Init(LOG_LEVEL_DEBUG, 115200);
    while (!Serial) {
      delay(1000);
    }
    #else
    Log.Init(LOG_LEVEL_DEBUG, BluetoothPrinter);
    #endif

    Log.Debug(F("Starting" CR));
    digitalWrite(LED_BUILTIN, LOW); // off

    Log.Debug(F("setupBluetooth" CR));
    setupBluetooth(charConfigs, COUNT(charConfigs));
    Log.Debug(F("loadSettings" CR));
    loadSettings();

    Log.Debug(F("setupLora" CR));
    setupLora(onTransmit);

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
