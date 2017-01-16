/*********************************************************************
 Bluetooth operation code for MapTheThings.
 Presents LoRa service
 - SendPacket
 - Get Status
 - Get/Set DevAddr
 - Get/Set AppEUI
 - Get/Set NwkSKey
 - Get/Set AppSKey
 - Get/Set DevEUI
 - RequestJoin

 Adapted from an example for the nRF51822 based Bluefruit LE modules

 Pick one up today in the adafruit shop!

 Adafruit invests time and resources providing this open source code,
 please support Adafruit and open-source hardware by purchasing
 products from Adafruit!

 MIT license, check LICENSE for more information
 All text above, and the splash screen below must be included in
 any redistribution
*********************************************************************/

/*
    Please note the long strings of data sent mean the *RTS* pin is
    required with UART to slow down data sent to the Bluefruit LE!
*/

#define MAPTHETHINGS_SOFTWARE_VERSION "0.1.0"

#define MINIMUM_FIRMWARE_VERSION   "0.7.0" // Callback requires 0.7.0
#define MODE_LED_BEHAVIOUR          "SPI" // "MODE" // "SPI"

#include <Arduino.h>
#include <avr/pgmspace.h>
#include <SPI.h>
#if not defined (_VARIANT_ARDUINO_DUE_X_) && not defined (_VARIANT_ARDUINO_ZERO_)
  #include <SoftwareSerial.h>
#endif

#include "Adafruit_BLE.h"
#include "Adafruit_BluefruitLE_SPI.h"
//#include "Adafruit_BluefruitLE_UART.h"
#include "Adafruit_BLEGatt.h"

#include "BluefruitConfig.h"

#include "Bluetooth.h"
#include "Logging.h"

// Create the bluefruit object, either software serial...uncomment these lines
/*
SoftwareSerial bluefruitSS = SoftwareSerial(BLUEFRUIT_SWUART_TXD_PIN, BLUEFRUIT_SWUART_RXD_PIN);

Adafruit_BluefruitLE_UART ble(bluefruitSS, BLUEFRUIT_UART_MODE_PIN,
                      BLUEFRUIT_UART_CTS_PIN, BLUEFRUIT_UART_RTS_PIN);
*/

/* ...or hardware serial, which does not need the RTS/CTS pins. Uncomment this line */
// Adafruit_BluefruitLE_UART ble(BLUEFRUIT_HWSERIAL_NAME, BLUEFRUIT_UART_MODE_PIN);

/* ...hardware SPI, using SCK/MOSI/MISO hardware SPI pins and then user selected CS/IRQ/RST */
Adafruit_BluefruitLE_SPI ble(BLUEFRUIT_SPI_CS, BLUEFRUIT_SPI_IRQ, BLUEFRUIT_SPI_RST);

/* ...software SPI, using SCK/MOSI/MISO user-defined SPI pins and then user selected CS/IRQ/RST */
//Adafruit_BluefruitLE_SPI ble(BLUEFRUIT_SPI_SCK, BLUEFRUIT_SPI_MISO,
//                             BLUEFRUIT_SPI_MOSI, BLUEFRUIT_SPI_CS,
//                             BLUEFRUIT_SPI_IRQ, BLUEFRUIT_SPI_RST);

void bluetoothDisconnect() {
  ble.disconnect();
}

Adafruit_BLEGatt gatt(ble);

static CharacteristicConfigType *charConfigs;
static int32_t charConfigsCount;

void setBluetoothCharData(uint8_t charID, uint8_t const data[], uint8_t size) {
  gatt.setChar(charID, data, size);
}

void gattCallback(int32_t index, uint8_t data[], uint16_t len) {
  Log.Debug("gattCallback (index=%d)" CR, index);
  for (int i=0; i<charConfigsCount; ++i) {
    if (index==charConfigs[i].charId) {
//      for(int i=0; i<len; ++i) {
//        Log.Debug("%x", data[i]);
//      }
//      Log.Debug(CR);
      charConfigs[i].callback(data, len);
      return;
    }
  }
  Log.Error(F("Failed to find callback" CR));
}

/* The service information */

int32_t loraServiceId;
int32_t loraSendCharId;
int32_t loraTxResultCharId;

int32_t logServiceId;
int32_t logMessageCharId;

int32_t deviceInfoServiceId;
int32_t deviceInfoCharId;

int32_t batteryLevelServiceId;
int32_t batteryLevelCharId;

#define MAGIC_NUMBER_SIZE 4
#define MAGIC_NUMBER 0x40DE2017 // NODE 2017

bool initNVRam() {
  int32_t magic_number = 0;
  ble.readNVM(0, &magic_number);

  if ( magic_number != MAGIC_NUMBER )
  {
    /* Perform a factory reset to make sure everything is in a known state */
    Log.Debug(F("Magic not found: Performing a factory reset..." CR));
    if ( ! ble.factoryReset() ){
      Log.Error(F("Couldn't factory reset!"));
      return false;
    }

    // Write data to NVM
    Log.Debug( F("Write defined data to NVM" CR) );
    ble.writeNVM(0 , MAGIC_NUMBER);
  }
  else {
    Log.Debug(F("Magic found. OK!" CR));
  }
  return true;
}

/**************************************************************************/
/*!
    @brief  Sets up the HW an the BLE module (this function is called
            automatically on startup)
*/
/**************************************************************************/
bool setupBluetooth(CharacteristicConfigType *cconfigs, int32_t cccount, bool verbose)
{
  charConfigs = cconfigs;
  charConfigsCount = cccount;

  boolean success;

  /* Initialise the module */
  Log.Info(F("Initialising the Bluefruit LE module" CR));

  if ( !ble.begin(verbose) )
  {
    Log.Error(F("Couldn't find Bluefruit, make sure it's in CoMmanD mode & check wiring?" CR));
    return false;
  }

  // if ( ! ble.factoryReset() ){
  //   Log.Error(F("Couldn't factory reset!"));
  //   return;
  // }

  // Looks for magic number and initializes BT and RAM if not found.
  // Call before other setup because factoryReset will undo it all.
  initNVRam();

  /* Disable command echo from Bluefruit */
  ble.echo(false);

  if (verbose) {
    Log.Debug("Requesting Bluefruit info:" CR);
    /* Print Bluefruit information */
    ble.info();
  }

  if ( !ble.isVersionAtLeast(MINIMUM_FIRMWARE_VERSION) ) {
    Log.Error(F("MapTheThings requires Bluefruit firmware version " MINIMUM_FIRMWARE_VERSION "." CR));
    Log.Error(F("Perform update using Bluefruit LE Connect. See https://learn.adafruit.com/introducing-adafruit-ble-bluetooth-low-energy-friend/dfu-on-ios" CR));
    return false;
  }

  // this line is particularly required for Flora, but is a good idea
  // anyways for the super long lines ahead!
  // ble.setInterCharWriteDelay(5); // 5 ms

  /* Change the device name to make it easier to find */
  Log.Debug(F("Setting device name to 'MapTheThings':" CR));
  if (! ble.sendCommandCheckOK(F("AT+GAPDEVNAME=MapTheThings")) ) {
    Log.Error(F("Could not set device name?" CR));
    return false;
  }

  // LED Activity command is only supported from 0.6.6
  #define LED_MODE_FIRMWARE_VERSION "0.6.6"
  if ( ble.isVersionAtLeast(LED_MODE_FIRMWARE_VERSION) ) {
    // Change Mode LED Activity
    Log.Debug(F("Change LED activity to " MODE_LED_BEHAVIOUR CR));
    ble.sendCommandCheckOK("AT+HWModeLED=" MODE_LED_BEHAVIOUR);
  }


  Log.Debug(F("Clearing the GATT." CR));
  success = ble.atcommand( F("AT+GATTCLEAR") );
  if (! success) {
    Log.Error(F("Could not clear GATT!" CR));
    return false;
  }

  /* Add the LoRa Service definition */
  /* Service ID should be 1 */
  Log.Debug(F("Adding the LoRa Service definition (UUID = 0x1830): " CR));
  success = ble.sendCommandWithIntReply( F("AT+GATTADDSERVICE=UUID=0x1830"), &loraServiceId);
  if (! success) {
    Log.Error(F("Could not add LoRa service" CR));
    return false;
  }

  // 0x10 notify to bluetooth app only
  success = ble.sendCommandWithIntReply( F("AT+GATTADDCHAR=UUID=0x2ADA,PROPERTIES=0x10,MIN_LEN=1,MAX_LEN=16,DESCRIPTION=TX Result"), &loraTxResultCharId);
  if (! success) {
    Log.Error(F("Could not add TX Result characteristic" CR));
    return false;
  }

  /* Add the LoRa write characteristic */
  /* Chars ID for Measurement should be 1 */
  for (int i=0; i<cccount; ++i) {
    Log.Debug(F("Adding characteristic: "));
    Log.Debug(cconfigs[i].charDef);
    Log.Debug(CR);
    success = ble.sendCommandWithIntReply(cconfigs[i].charDef, &cconfigs[i].charId);
    if (! success) {
      Log.Error(F("Could not add characteristic" CR));
      return false;
    }
  }
  Log.Debug(F("Done adding LoRa service" CR));

  Log.Debug(F("Adding the Logging Service definition (UUID = 0x1831): " CR));
  success = ble.sendCommandWithIntReply( F("AT+GATTADDSERVICE=UUID=0x1831"), &logServiceId);
  if (! success) {
    Log.Error(F("Could not add Logging service" CR));
    return false;
  }
  Log.Debug(F("Adding the log message characteristic (UUID = 0x2AD6):" CR));
  success = ble.sendCommandWithIntReply( F("AT+GATTADDCHAR=UUID=0x2AD6,PROPERTIES=0x10,MIN_LEN=1,MAX_LEN=20"), &logMessageCharId);
  if (! success) {
    Log.Error(F("Could not add log message characteristic" CR));
    return false;
  }

  Log.Debug(F("Adding the Device Info service definition (UUID = 0x180A):" CR));
  success = ble.sendCommandWithIntReply( F("AT+GATTADDSERVICE=UUID=0x180A"), &deviceInfoServiceId);
  if (! success) {
    Log.Error(F("Could not add Device Info service" CR));
    return false;
  }
  Log.Debug(F("Adding the Device Info manufacturer name characteristic (UUID = 0x2A29):" CR));
  success = ble.sendCommandWithIntReply( F("AT+GATTADDCHAR=UUID=0x2A29,PROPERTIES=0x02,MIN_LEN=1,MAX_LEN=20,VALUE=TheThingsNYC"), &deviceInfoCharId);
  if (! success) {
    Log.Error(F("Could not add Device Info manufacturer name characteristic" CR));
    return false;
  }
  Log.Debug(F("Adding the Device Info software version characteristic (UUID = 0x2A28):" CR));
  success = ble.sendCommandWithIntReply( F("AT+GATTADDCHAR=UUID=0x2A28,PROPERTIES=0x02,MIN_LEN=1,MAX_LEN=20,VALUE=" MAPTHETHINGS_SOFTWARE_VERSION), &deviceInfoCharId);
  if (! success) {
    Log.Error(F("Could not add Device Info software version characteristic" CR));
    return false;
  }

  // Battery service: 0x180F
  // Battery level: 0x2A19, 1 byte, 0-100 values (read mandatory, notify optional)
  Log.Debug(F("Adding the Battery level service definition (UUID = 0x180F):" CR));
  success = ble.sendCommandWithIntReply( F("AT+GATTADDSERVICE=UUID=0x180F"), &batteryLevelServiceId);
  if (! success) {
    Log.Error(F("Could not add Battery level service"));
    return false;
  }
  Log.Debug(F("Adding the Battery level characteristic (UUID = 0x2A19):" CR));
  success = ble.sendCommandWithIntReply( F("AT+GATTADDCHAR=UUID=0x2A19,PROPERTIES=0x12,MIN_LEN=1,MAX_LEN=1,VALUE=00"), &batteryLevelCharId);
  if (! success) {
    Log.Error(F("Could not add Battery level characteristic" CR));
    return false;
  }

  /* Add the LoRa to the advertising data (needed for Nordic apps to detect the service) */
  Log.Debug(F("Adding LoRa and Device info UUIDs to the advertising payload:" CR));
  // 02-01-06 - len-flagtype-flags
  // 09-02 - len-16bitlisttype- 0x180A(Device) - 0x180F(Battery) - 0x1830(LoRa) - 0x1831(Logging)
  //   bit
  //    0 LE Limited Discoverable Mode - 180sec advertising
  //    1 LE General Discoverable Mode - Indefinite advertising time
  //    2 BR/EDR Not Supported
  //    3 Simultaneous LE and BR/EDR (Controller)
  //    4 Simultaneous LE and BR/EDR (Host)
  ble.sendCommandCheckOK( F("AT+GAPSETADVDATA=02-01-06-09-02-0A-18-0F-18-30-18-31-18") );

  /* Reset the device for the new service setting changes to take effect */
  Log.Debug(F("Performing a SW reset (service changes require a reset):" CR));
  ble.reset();

  Log.Debug(F("Signing up for callbacks on characteristic write: " CR));
  for (int i=0; i<cccount; ++i) {
    ble.setBleGattRxCallback(cconfigs[i].charId, gattCallback);
  }

  ble.verbose(verbose);
  return true;
}

static bool waitForOK(const char * op) {
  if ( ble.waitForOK() ) {
    return true;
  }
  else {
    Log.Error(F("Failed to get response! "));
    Log.Error(op);
    Log.Error(CR);
    return false;
  }
}

static bool logResult(bool result, const char * op) {
  if ( !result ) {
    Log.Error(F("Failed to get response! "));
    Log.Error(op);
    Log.Error(CR);
  }
  return result;
}

void sendBatteryLevel(uint8_t level) {
  /* Command is sent when \n (\r) or println is called */
  /* AT+GATTCHAR=CharacteristicID,value */
  ble.print( F("AT+GATTCHAR=") );
  ble.print( batteryLevelCharId );
  ble.print( F(",") );
  ble.println(level, HEX);

  waitForOK("Send battery level");
}

void sendTxResult(uint8_t bleSeq, uint16_t error, uint32_t seq_no) {
  // 8bit format, 8bit ble_seq, 16bit error, 32bit seq_no
  uint8_t buffer[8];
  #define TX_RESULT_FORMAT_V1 0x01
  buffer[0] = TX_RESULT_FORMAT_V1;
  buffer[1] = bleSeq;
  memcpy(&buffer[2*sizeof(uint8_t)], (uint8_t *)&error, sizeof(error));
  memcpy(&buffer[2*sizeof(uint8_t)+sizeof(uint16_t)], (uint8_t *)&seq_no, sizeof(seq_no));

  bool result = gatt.setChar(loraTxResultCharId, buffer, sizeof(buffer));

  logResult(result, "sendTxResult");
}

void sendLogMessage(const char *s) {
  // NOTE: Don't use Log.Debug because infinite recursion.
  // Serial.print(F("Sending message: ")); Serial.println(s);
  int len = strlen(s);
  // Break into 20 byte chunks
  while (len>20) {
    gatt.setChar(logMessageCharId, (const uint8_t *)s, 20);
    s += 20;
    len -= 20;
  }
  gatt.setChar(logMessageCharId, (const uint8_t *)s, len);
}

/* Writes NV bytes with offset after magic number
  Supports writing lengths of bytes larger than BLE_BUFSIZE
*/
bool writeNVBytes(uint8_t offset, uint8_t *bytes, uint8_t length) {
  offset += MAGIC_NUMBER_SIZE;
  bool success = true;
  for (uint8_t i = 0; i < length; i += BLE_BUFSIZE) {
    success = success && ble.writeNVM(offset + i, bytes + i, min(BLE_BUFSIZE, (length - i)));
  }
  return success;
}
bool writeNVInt(uint8_t offset, int32_t number) {
  return ble.writeNVM(offset+MAGIC_NUMBER_SIZE, number);
}

/* Reads NV bytes with offset after magic number
  Supports reading lengths of bytes larger than BLE_BUFSIZE
*/
bool readNVBytes(uint8_t offset, uint8_t *bytes, uint8_t length) {
  offset += MAGIC_NUMBER_SIZE;
  bool success = true;
  for (uint8_t i = 0; i < length; i += BLE_BUFSIZE) {
    success = success && ble.readNVM(offset + i, bytes + i, min(BLE_BUFSIZE, (length - i)));
  }
  return success;
}
bool readNVInt(uint8_t offset, int32_t *number) {
  return ble.readNVM(offset+MAGIC_NUMBER_SIZE, number);
}

void loopBluetooth(void) {
    ble.update(200);
}
