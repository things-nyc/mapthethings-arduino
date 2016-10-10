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
  Log.Debug("gattCallback (index=%d)"CR, index);
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
  Log.Error(F("Failed to find callback"CR));
}

/* The service information */

int32_t loraServiceId;
int32_t loraSendCharId;

int32_t deviceInfoServiceId;
int32_t deviceInfoCharId;

int32_t batteryLevelServiceId;
int32_t batteryLevelCharId;

/**************************************************************************/
/*!
    @brief  Sets up the HW an the BLE module (this function is called
            automatically on startup)
*/
/**************************************************************************/
void setupBluetooth(CharacteristicConfigType *cconfigs, int32_t cccount)
{
  charConfigs = cconfigs;
  charConfigsCount = cccount;

  boolean success;

  /* Initialise the module */
  Log.Debug(F("Initialising the Bluefruit LE module: "CR));

  if ( !ble.begin(!VERBOSE_MODE) )
  {
    Log.Error(F("Couldn't find Bluefruit, make sure it's in CoMmanD mode & check wiring?"CR));
    return;
  }
  Log.Debug( F("OK!"CR) );

  /* Perform a factory reset to make sure everything is in a known state */
  Log.Debug(F("Performing a factory reset: "CR));
  if (! ble.factoryReset() ){
       Log.Error(F("Couldn't factory reset"CR));
       return;
  }

  /* Disable command echo from Bluefruit */
  ble.echo(false);

  Log.Debug("Requesting Bluefruit info:"CR);
  /* Print Bluefruit information */
  ble.info();

  // this line is particularly required for Flora, but is a good idea
  // anyways for the super long lines ahead!
  // ble.setInterCharWriteDelay(5); // 5 ms

  /* Change the device name to make it easier to find */
  Log.Debug(F("Setting device name to 'MapTheThings':"CR));

  if (! ble.sendCommandCheckOK(F("AT+GAPDEVNAME=MapTheThings")) ) {
    Log.Error(F("Could not set device name?"CR));
    return;
  }

  // LED Activity command is only supported from 0.6.6
  if ( ble.isVersionAtLeast(MINIMUM_FIRMWARE_VERSION) )
  {
    // Change Mode LED Activity
    Log.Debug(F("Change LED activity to " MODE_LED_BEHAVIOUR CR));
    ble.sendCommandCheckOK("AT+HWModeLED=" MODE_LED_BEHAVIOUR);
  }

  /* Add the LoRa Service definition */
  /* Service ID should be 1 */
  Log.Debug(F("Adding the LoRa Service definition (UUID = 0x1830): "CR));
  success = ble.sendCommandWithIntReply( F("AT+GATTADDSERVICE=UUID=0x1830"), &loraServiceId);
  if (! success) {
    Log.Error(F("Could not add LoRa service"CR));
    return;
  }

  Log.Debug(F("Adding the Logging Service definition (UUID = 0x1831): "CR));
  success = ble.sendCommandWithIntReply( F("AT+GATTADDSERVICE=UUID=0x1831"), &loraServiceId);
  if (! success) {
    Log.Error(F("Could not add Logging service"CR));
    return;
  }

  /* Add the LoRa write characteristic */
  /* Chars ID for Measurement should be 1 */
  for (int i=0; i<cccount; ++i) {
    Log.Debug(F("Adding characteristic: "));
    Log.Debug(cconfigs[i].charDef);
    Log.Debug(CR);
    success = ble.sendCommandWithIntReply(cconfigs[i].charDef, &cconfigs[i].charId);
    if (! success) {
      Log.Error(F("Could not add characteristic"CR));
      return;
    }
  }

  Log.Debug(F("Adding the Device Info service definition (UUID = 0x180A):"CR));
  success = ble.sendCommandWithIntReply( F("AT+GATTADDSERVICE=UUID=0x180A"), &deviceInfoServiceId);
  if (! success) {
    Log.Error(F("Could not add Device Info service"CR));
    return;
  }
  Log.Debug(F("Adding the Device Info manufacturer name characteristic (UUID = 0x2A29):"CR));
  success = ble.sendCommandWithIntReply( F("AT+GATTADDCHAR=UUID=0x2A29,PROPERTIES=0x02,MIN_LEN=1,MAX_LEN=20,VALUE=TheThingsNYC"), &deviceInfoCharId);
  if (! success) {
    Log.Error(F("Could not add Device Info manufacturer name characteristic"CR));
    return;
  }
  Log.Debug(F("Adding the Device Info software version characteristic (UUID = 0x2A28):"CR));
  success = ble.sendCommandWithIntReply( F("AT+GATTADDCHAR=UUID=0x2A28,PROPERTIES=0x02,MIN_LEN=1,MAX_LEN=20,VALUE=" MAPTHETHINGS_SOFTWARE_VERSION), &deviceInfoCharId);
  if (! success) {
    Log.Error(F("Could not add Device Info software version characteristic"CR));
    return;
  }

  // Battery service: 0x180F
  // Battery level: 0x2A19, 1 byte, 0-100 values (read mandatory, notify optional)
  Log.Debug(F("Adding the Battery level service definition (UUID = 0x180F):"CR));
  success = ble.sendCommandWithIntReply( F("AT+GATTADDSERVICE=UUID=0x180F"), &batteryLevelServiceId);
  if (! success) {
    Log.Error(F("Could not add Battery level service"));
    return;
  }
  Log.Debug(F("Adding the Battery level characteristic (UUID = 0x2A19):"CR));
  success = ble.sendCommandWithIntReply( F("AT+GATTADDCHAR=UUID=0x2A19,PROPERTIES=0x12,MIN_LEN=1,MAX_LEN=1,VALUE=00"), &batteryLevelCharId);
  if (! success) {
    Log.Error(F("Could not add Battery level characteristic"CR));
    return;
  }

  /* Add the LoRa to the advertising data (needed for Nordic apps to detect the service) */
  Log.Debug(F("Adding LoRa and Device info UUIDs to the advertising payload:"CR));
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
  Log.Debug(F("Performing a SW reset (service changes require a reset):"CR));
  ble.reset();

  Log.Debug(F("Signing up for callbacks on characteristic write: "CR));
  for (int i=0; i<cccount; ++i) {
    ble.setBleGattRxCallback(cconfigs[i].charId, gattCallback);
  }

  ble.verbose(false);
}

static void waitForOK(const char * op) {
  if ( !ble.waitForOK() ) {
    Log.Error(F("Failed to get response! "));
    Log.Error(op);
    Log.Error(CR);
  }
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

const static long updateInterval = 1000;
static TimeoutTimer timer;

/** Send randomized heart rate data every bluetoothInterval **/
void loopBluetooth(void) {
    ble.update(200);
}
