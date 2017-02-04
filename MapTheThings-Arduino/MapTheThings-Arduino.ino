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

#define LOG_LEVEL LOG_LEVEL_INFOS //  _INFOS, _DEBUG, _VERBOSE, _NOOUTPUT
// Set to LOG_LEVEL_VERBOSE to see low level AT communication with BT module

#define DEBUG_SERIAL_LOGGING // Waits for Serial monitor before startup
// #define DEBUG_FORGET_SESSION_VARS // Stored settings include session keys - uncomment and run once to erase

// If you want to statically define ABP parameters, put them below and uncomment the lines
// #define DEVADDR { 0x26, 0x01, 0x2B, 0x32 }
// #define NWKSKEY { 0xCD, 0x67, 0x8D, 0x4F, 0x90, 0xAE, 0x3F, 0x16, 0x64, 0x55, 0x1E, 0x5A, 0x59, 0x68, 0x2C, 0xE6 }
// #define APPSKEY { 0xE7, 0xD0, 0x0E, 0xB6, 0xB7, 0x8D, 0xAC, 0x42, 0x97, 0xA4, 0x18, 0xD3, 0xFC, 0x8F, 0xA2, 0x4B }

// If you want to statically define ABP parameters, put them below and uncomment the lines
// #define APPKEY /* MSB */ { 0xA6, 0xA0, 0x44, 0xC4, 0xCF, 0x74, 0xD3, 0x73, 0xB0, 0x2C, 0xAF, 0x63, 0xA9, 0xBD, 0x9F, 0x64 }
// #define APPEUI /* LSB */ { 0x38, 0x1C, 0x00, 0xF0, 0x7E, 0xD5, 0xB3, 0x70 }
// #define DEVEUI /* LSB */ { 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00 }

#if (defined(DEVADDR) || defined(NWKSKEY) || defined(APPSKEY)) && (defined(APPKEY) || defined(APPEUI) || defined(DEVEUI))
#error Set either ABP or OTAA parameters (or neither), but not both.
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

// Flag bit #0 used to mean something. To reuse it, bump the format number.
#define FLAG_DEV_ADDR_SET (1 << 4)
  u1_t DevAddr[4];
#define FLAG_NWK_SKEY_SET (1 << 5)
  u1_t NwkSKey[16];
#define FLAG_APP_SKEY_SET (1 << 6)
  u1_t AppSKey[16];
#define FLAG_SESSION_VARS_SET (FLAG_DEV_ADDR_SET | FLAG_NWK_SKEY_SET | FLAG_APP_SKEY_SET)

// Separate set flags because BLE can't send all at once
#define FLAG_APP_KEY_SET (1 << 1)
  u1_t AppKey[16];
#define FLAG_APP_EUI_SET (1 << 2)
  u1_t AppEUI[8];
#define FLAG_DEV_EUI_SET (1 << 3)
  u1_t DevEUI[8];
#define FLAG_JOIN_VARS_SET (FLAG_APP_KEY_SET | FLAG_APP_EUI_SET | FLAG_DEV_EUI_SET)
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
    Log.Debug(F("%s %x" CR), msg, value);
  }

  void debugLogData(const char *msg, uint8_t data[], uint16_t len) {
    char buffer[200+1];
    Log.Debug(F("%s (Length: %d): "), msg, len);
    for(uint i=0; i<min(len, sizeof(buffer)/2); ++i) {
      snprintf(buffer+2*i, sizeof(buffer)-2*i, "%02x", data[i]);
    }
    Log.Debug_(F("%s" CR), buffer);
  }
} // extern "C".

#define CMD_DISCONNECT 1

void sendCommandCallback(uint8_t data[], uint16_t len) {
  uint16_t command = *(uint16_t *)data;
  Log.Debug(F("sendCommand: %d" CR), command);
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

#define AssignSessionCallback(key, flag) \
void assign##key##Callback(uint8_t data[], uint16_t len) { \
  debugLogData("assign" #key, data, len); \
  if (len==sizeof(settings.key)) {        \
    memcpy(settings.key, data, sizeof(settings.key)); \
    saveSettingBytes(offset(settings, key), settings.key, sizeof(settings.key)); \
    settings.flags |= flag; \
    saveSettingValue(offset(settings, flags), settings.flags); \
    if ((settings.flags & FLAG_SESSION_VARS_SET)==FLAG_SESSION_VARS_SET) { \
      loraSetSessionKeys(settings.seq_no, settings.AppSKey, settings.NwkSKey, settings.DevAddr); \
    } \
  } \
}

AssignSessionCallback(DevAddr, FLAG_DEV_ADDR_SET)
AssignSessionCallback(NwkSKey, FLAG_NWK_SKEY_SET)
AssignSessionCallback(AppSKey, FLAG_APP_SKEY_SET)

void onJoin(u1_t *appskey, u1_t *nwkskey, u1_t *devaddr);

#define AssignAppCallback(key, flag) \
void assign##key##Callback(uint8_t data[], uint16_t len) { \
  debugLogData("assign" #key, data, len); \
  if (len==sizeof(settings.key)) {        \
    memcpy(settings.key, data, sizeof(settings.key)); \
    saveSettingBytes(offset(settings, key), settings.key, sizeof(settings.key)); \
    settings.flags |= flag; \
    saveSettingValue(offset(settings, flags), settings.flags); \
    if ((settings.flags & FLAG_JOIN_VARS_SET)==FLAG_JOIN_VARS_SET) { \
      loraJoin(settings.seq_no, settings.AppKey, settings.AppEUI, settings.DevEUI, onJoin); \
    } \
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

  #define SIZE_STRING(x) #x
  #define STATIC_INIT(bytes, name, flag) \
    static const PROGMEM u1_t name[] = bytes; \
    static_assert(sizeof(settings.name) == sizeof(name), "DEVADDR needs to be " SIZE_STRING(sizeof(settings.name)) " bytes"); \
    _memcpy(settings.name, name, sizeof(settings.name)); \
    settings.flags |= flag; \
    Log.Info(F("Overriding " #name " with static initializer: " #bytes CR)); \
    debugLogData("Static " #name, settings.name, sizeof(settings.name)); \
    write = true;

  #if defined(DEVADDR)
    STATIC_INIT(DEVADDR, DevAddr, FLAG_DEV_ADDR_SET)
  #endif
  #if defined(NWKSKEY)
    STATIC_INIT(NWKSKEY, NwkSKey, FLAG_NWK_SKEY_SET)
  #endif
  #if defined(APPSKEY)
    STATIC_INIT(APPSKEY, AppSKey, FLAG_APP_SKEY_SET)
  #endif

  #if defined(APPKEY)
    STATIC_INIT(APPKEY, AppKey, FLAG_APP_KEY_SET)
  #endif
  #if defined(APPEUI)
    STATIC_INIT(APPEUI, AppEUI, FLAG_APP_EUI_SET)
  #endif
  #if defined(DEVEUI)
    STATIC_INIT(DEVEUI, DevEUI, FLAG_DEV_EUI_SET)
  #endif

  debugLog("Following static initialization, Flags: ", settings.flags);
  return write;
}

void reportSessionVars() {
  if (settings.flags & FLAG_DEV_ADDR_SET) {
    setBluetoothCharData(GattDevAddr.charId, settings.DevAddr, sizeof(settings.DevAddr));
  }
  if (settings.flags & FLAG_NWK_SKEY_SET) {
    setBluetoothCharData(GattNwkSKey.charId, settings.NwkSKey, sizeof(settings.NwkSKey));
  }
  if (settings.flags & FLAG_APP_SKEY_SET) {
    setBluetoothCharData(GattAppSKey.charId, settings.AppSKey, sizeof(settings.AppSKey));
  }
}

void reportJoinVars() {
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

void onJoin(u1_t *appskey, u1_t *nwkskey, u1_t *devaddr) {
  if (appskey==NULL) {
    debugPrint("Join failed");
  }
  else {
    Log.Info(F("Join succeeded" CR));
    memcpy(settings.AppSKey, appskey, sizeof(settings.AppSKey));
    memcpy(settings.NwkSKey, nwkskey, sizeof(settings.NwkSKey));
    memcpy(settings.DevAddr, devaddr, sizeof(settings.DevAddr));
    settings.flags |= FLAG_SESSION_VARS_SET;
    saveSettingBytes(0, (u1_t *)(&settings), sizeof(settings));
    reportSessionVars();
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

static bool checkBytesSameValue(uint8_t *bytes, uint size, uint16_t flag, uint8_t value, const char *name) {
  bool same = true;
  for (uint i = 0; i<size; ++i) {
    same = same && (bytes[i]==value);
  }
  if (same) {
    Log.Warn(F("Removing %s because it is all %d" CR), name, value);
    settings.flags &= ~flag;
  }
  return same;
}

static bool checkValidSettings() {
  bool write = false;

  #define CHECK_BYTES(name, flag)   \
    if (settings.flags & flag) {    \
      write = write || checkBytesSameValue(settings.name, sizeof(settings.name), flag, 0x00, #name);  \
      write = write || checkBytesSameValue(settings.name, sizeof(settings.name), flag, 0xFF, #name);  \
    }

  CHECK_BYTES(DevAddr, FLAG_DEV_ADDR_SET)
  CHECK_BYTES(NwkSKey, FLAG_NWK_SKEY_SET)
  CHECK_BYTES(AppSKey, FLAG_APP_SKEY_SET)
  CHECK_BYTES(AppKey, FLAG_APP_KEY_SET)
  CHECK_BYTES(AppEUI, FLAG_APP_EUI_SET)
  CHECK_BYTES(DevEUI, FLAG_DEV_EUI_SET)

  return write;
}

static void loadSettings() {
  Log.Info(F("Loading saved settings" CR));
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
    write = write || checkValidSettings();
    if (write) {
      debugLogData("Writing updated settings", (u1_t *)(&settings), sizeof(settings));
      saveSettingBytes(0, (u1_t *)(&settings), sizeof(settings));
    }
  }
  else {
    Log.Debug(F("Failed to read settings" CR));
    return;
  }

  if ((settings.flags & FLAG_SESSION_VARS_SET)==FLAG_SESSION_VARS_SET) {
    debugPrint("Session vars set - initializing lora");
    loraSetSessionKeys(settings.seq_no, settings.AppSKey, settings.NwkSKey, settings.DevAddr);

    reportSessionVars();
  }
  else if ((settings.flags & FLAG_JOIN_VARS_SET)==FLAG_JOIN_VARS_SET) {
    debugPrint("Join keys set - initializing lora join");
    loraJoin(settings.seq_no, settings.AppKey, settings.AppEUI, settings.DevEUI, onJoin);
  }
  reportSessionVars();
  reportJoinVars();
}

void setup() {
    #if defined(DEBUG_SERIAL_LOGGING)
      Log.Init(LOG_LEVEL, 115200);
      // Wait for 15 seconds. If no Serial by then, keep going. We are not connected.
      for (int timeout=0; timeout<15 && !Serial; ++timeout) {
        delay(1000);
      }
      Log.Warn(F(
        "Important: DEBUG_SERIAL_LOGGING is set. The node will wait for a Serial monitor before executing. This is very useful for debugging." CR
        "However, the node will wait for 15 seconds before startup when it is NOT connected to USB." CR
        "Disable DEBUG_SERIAL_LOGGING for immediate untethered execution." CR));
    #else
      Log.Init(LOG_LEVEL, BluetoothPrinter);
    #endif

    digitalWrite(LED_BUILTIN, LOW); // off

    bool btok = setupBluetooth(charConfigs, COUNT(charConfigs), (LOG_LEVEL==LOG_LEVEL_VERBOSE));
    if (!btok) {
      Log.Error(F("***** Failed to initialize Bluetooth subsystem." CR));
    }
    else {
      // Load settings only if Bluetooth initializes successfully - settings are stored in BT module
      loadSettings();
    }

    bool loraok = setupLora(onTransmit);
    if (!loraok) {
      Log.Error(F("***** Failed to initialize LoRa radio subsystem." CR));
    }

    if (btok && loraok) {
      Log.Info(F("Setup completed successfully" CR));
    }
}

void readBatteryLevel() {
    #define VBATPIN A7

    float measuredvbat = analogRead(VBATPIN);
    measuredvbat *= 2;    // we divided by 2, so multiply back
    measuredvbat *= 3.3;  // Multiply by 3.3V, our reference voltage
    measuredvbat /= 1024; // convert to voltage
    Log.Debug("VBat: %f", measuredvbat);

    // Normalize to 0-100
    float minv = 3.2;
    float maxv = 4.2;
    int level = 100 * (measuredvbat - minv) / (maxv - minv);
    Log.Debug_(" [VBat int: %d]" CR, level);
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
