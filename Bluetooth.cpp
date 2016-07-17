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
#define MODE_LED_BEHAVIOUR          "MODE" // "SPI"

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

Adafruit_BLEGatt gatt(ble);

static CharacteristicConfigType *charConfigs;
int32_t charConfigsCount;

void setBluetoothCharData(uint8_t charID, uint8_t const data[], uint8_t size) {
  gatt.setChar(charID, data, size);
}

void gattCallback(int32_t index, uint8_t data[], uint16_t len) {
  Serial.println("gattCallback");
  Serial.println(index);
  for (int i=0; i<charConfigsCount; ++i) {
    if (index==charConfigs[i].charId) {
      for(int i=0; i<len; ++i) {
        Serial.println(data[i], HEX);
      }
      charConfigs[i].callback(data, len);
      return;
    }
  }
  Serial.println("Failed to find callback");
}

// A small helper
void error(const __FlashStringHelper*err) {
  Serial.println(err);
  while (1);
}

/* The service information */

int32_t loraServiceId;
int32_t loraSendCharId;
//int32_t loraSendCharId;

int32_t deviceInfoServiceId;
int32_t deviceInfoCharId;

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

  Serial.println(F("Adafruit Bluefruit Heart Rate Monitor (HRM) Example"));
  Serial.println(F("---------------------------------------------------"));

  /* Initialise the module */
  Serial.print(F("Initialising the Bluefruit LE module: "));

  if ( !ble.begin(VERBOSE_MODE) )
  {
    error(F("Couldn't find Bluefruit, make sure it's in CoMmanD mode & check wiring?"));
  }
  Serial.println( F("OK!") );

  /* Perform a factory reset to make sure everything is in a known state */
  Serial.println(F("Performing a factory reset: "));
  if (! ble.factoryReset() ){
       error(F("Couldn't factory reset"));
  }

  /* Disable command echo from Bluefruit */
  ble.echo(false);

  Serial.println("Requesting Bluefruit info:");
  /* Print Bluefruit information */
  ble.info();

  // this line is particularly required for Flora, but is a good idea
  // anyways for the super long lines ahead!
  // ble.setInterCharWriteDelay(5); // 5 ms

  /* Change the device name to make it easier to find */
  Serial.println(F("Setting device name to 'MapTheThings': "));

  if (! ble.sendCommandCheckOK(F("AT+GAPDEVNAME=MapTheThings")) ) {
    error(F("Could not set device name?"));
  }

  // LED Activity command is only supported from 0.6.6
  if ( ble.isVersionAtLeast(MINIMUM_FIRMWARE_VERSION) )
  {
    // Change Mode LED Activity
    Serial.println(F("Change LED activity to " MODE_LED_BEHAVIOUR));
    ble.sendCommandCheckOK("AT+HWModeLED=" MODE_LED_BEHAVIOUR);
  }

  /* Add the LoRa Service definition */
  /* Service ID should be 1 */
  Serial.println(F("Adding the LoRa Service definition (UUID = 0x1830): "));
  success = ble.sendCommandWithIntReply( F("AT+GATTADDSERVICE=UUID=0x1830"), &loraServiceId);
  if (! success) {
    error(F("Could not add HRM service"));
  }

  /* Add the LoRa write characteristic */
  /* Chars ID for Measurement should be 1 */
  for (int i=0; i<cccount; ++i) {
    Serial.print(F("Adding characteristic: "));
    Serial.println(cconfigs[i].charDef);
    success = ble.sendCommandWithIntReply(cconfigs[i].charDef, &cconfigs[i].charId);
    if (! success) {
      error(F("Could not add characteristic"));
    }
  }

  Serial.println(F("Adding the Device Info service definition (UUID = 0x180A): "));
  success = ble.sendCommandWithIntReply( F("AT+GATTADDSERVICE=UUID=0x180A"), &deviceInfoServiceId);
  if (! success) {
    error(F("Could not add Device Info service"));
  }
  Serial.println(F("Adding the Device Info manufacturer name characteristic (UUID = 0x2A29): "));
  success = ble.sendCommandWithIntReply( F("AT+GATTADDCHAR=UUID=0x2A29,PROPERTIES=0x02,MIN_LEN=1,MAX_LEN=20,VALUE=TheThingsNYC"), &deviceInfoCharId);
  if (! success) {
    error(F("Could not add LoRa write characteristic"));
  }
  Serial.println(F("Adding the Device Info software version characteristic (UUID = 0x2A28): "));
  success = ble.sendCommandWithIntReply( F("AT+GATTADDCHAR=UUID=0x2A28,PROPERTIES=0x02,MIN_LEN=1,MAX_LEN=20,VALUE=" MAPTHETHINGS_SOFTWARE_VERSION), &deviceInfoCharId);
  if (! success) {
    error(F("Could not add Device Info software version characteristic"));
  }
  
  /* Add the Heart Rate Service to the advertising data (needed for Nordic apps to detect the service) */
  Serial.print(F("Adding LoRa and Device info UUIDs to the advertising payload: "));
  // 02-01-06 - len-flagtype-flags
  // 05-02 - len-16bitlisttype- 0x180A(Device) - 0x180D(Heart Rate)
  //   bit
  //    0 LE Limited Discoverable Mode - 180sec advertising
  //    1 LE General Discoverable Mode - Indefinite advertising time
  //    2 BR/EDR Not Supported
  //    3 Simultaneous LE and BR/EDR (Controller)
  //    4 Simultaneous LE and BR/EDR (Host)
  ble.sendCommandCheckOK( F("AT+GAPSETADVDATA=02-01-06-05-02-0A-18-30-18") );

  /* Reset the device for the new service setting changes to take effect */
  Serial.print(F("Performing a SW reset (service changes require a reset): "));
  ble.reset();

  Serial.println(F("Signing up for callbacks on characteristic write: "));
  for (int i=0; i<cccount; ++i) {
    Serial.println(cconfigs[i].charId);
    ble.setBleGattRxCallback(cconfigs[i].charId, gattCallback);
  }
  
  ble.verbose(false);
  Serial.println();
}

void sendHeartrate(void) {
  int heart_rate = random(50, 100);

//  Serial.print(F("Updating HRM value to "));
//  Serial.print(heart_rate);
//  Serial.println(F(" BPM"));

  /* Command is sent when \n (\r) or println is called */
  /* AT+GATTCHAR=CharacteristicID,value */
//  ble.print( F("AT+GATTCHAR=") );
//  ble.print( hrmMeasureCharId );
//  ble.print( F(",00-") );
//  ble.println(heart_rate, HEX);

  /* Check if command executed OK */
//  if ( !ble.waitForOK() )
//  {
//    Serial.println(F("Failed to get response!"));
//  }
}

const long updateInterval = 1000;
TimeoutTimer timer;

/** Send randomized heart rate data every bluetoothInterval **/
void loopBluetooth(void) {
    ble.update(500);
    if (timer.expired()) {
        timer.set(updateInterval);
        sendHeartrate();
    }
}
