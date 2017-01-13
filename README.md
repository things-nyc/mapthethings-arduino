[![Build Status](https://travis-ci.org/things-nyc/mapthethings-arduino.svg?branch=master)](https://travis-ci.org/things-nyc/mapthethings-arduino)

# MapTheThings-Arduino

This repository is the Arduino portion of [**The Things Network New York's**](https://github.com/things-nyc) [**Map The Things app**](http://map.thethings.nyc) the global coverage map for The Things Network (TTN).

The app was developed primarily by [Frank Leon Rose](https://github.com/frankleonrose). Huge thanks to **Frank** for his hard work and ongoing patience, as well as to [Terry Moore](https://github.com/terrillmoore), [Chris Merck](https://github.com/chrismerck), [Mimi Flynn](https://github.com/mimiflynn), [Manny Tsarnas](https://github.com/etsarnas), and [Forrest Filler](https://github.com/forrestfiller) for their ongoing efforts with the software and hardware components.

Please fork the [master repos](https://github.com/things-nyc) and lend a hand if you identify areas for improvement or wish to create new features. This and the other related **Map The Things** repos are under active development.

## Hardware Wiring

### Using [RFM95 LoRa Breakout](https://www.adafruit.com/product/3072) and [Feather M0 Bluefruit](https://www.adafruit.com/product/2995)
- [LoRa transceiver pins are defined in software here](https://github.com/things-nyc/mapthethings-arduino/blob/b47e33881d88afeec336cf7f758cd791c54c9a01/MapTheThings-Arduino/Lora.cpp#L49)


RFM95 | Feather
----- | -------
VIN   | 3V
GND   | GND
G0    | A2
CS    | A5
RST   | A4
SCK   | SCK
MOSI  | MOSI
MISO  | MISO
G2    | 6
G1    | 5

( You can connect either to a [li-poly battery](https://www.adafruit.com/products/2750) or power via [USB cable](https://www.adafruit.com/products/2008) )


![alt text](https://github.com/forrestfiller/mapthethings-arduino/blob/master/images/completed-node.jpg "assembled map the things node")

Here is [Adafruit's](https://github.com/adafruit) schematic for their Bluefruit module:

![alt text](https://github.com/forrestfiller/mapthethings-arduino/blob/master/images/feather_M0_bluefruit_adafruit_products_2995_pinout_v1_1.jpg "feather M0 bluefruit")


In lieu of the above breakout board, you could alternatively use the [LoRa Radio Featherwing](https://www.adafruit.com/products/3231) in conjunction with the [Feather M0 Bluefruit](https://www.adafruit.com/product/2995). You'll want to solder up the connections like so:


![alt text](https://github.com/forrestfiller/mapthethings-arduino/blob/master/images/feather-radioWing-front-web.jpg "image showing front of a lora radio featherwing")



![alt text](https://github.com/forrestfiller/mapthethings-arduino/blob/master/images/feather-radioWing-rear-web.jpg "image showing rear of a lora radio featherwing")


## Using the code
### Using with platformIO
- Inside the root level directory of your copy of this repo run the following commands:

- ```platformio run``` (to install libraries and build code)
- ```platformio upload``` (to install code on a device)

### Using with Arduino IDE
- Install [Adafruit's Adafruit_BluefruitLE_nRF51 library](https://github.com/adafruit/Adafruit_BluefruitLE_nRF51) Arduino library
- Install [The Things Network New York's version of the IBM LMIC library](https://github.com/things-nyc/arduino-lmic)
- Install [Frank's enhanced Arduino logging library that supports redirection](https://github.com/frankleonrose/Arduino-logging-library)
- Verify and Upload code

## Node Responsibilities
- Advertise capabilities via BLE
- Respond to scan from a BLE Center (the MapTheThings-iOS app)
- Serve LoRa configuration, status, and responses as BLE characteristics
- Accept LoRa configuration and transmission commands as BLE characteristic
- Store device EUI and sequence number in NVRAM

## License
Source code for Map The Things is released under the MIT License,
which can be found in the [LICENSE](LICENSE) file.
