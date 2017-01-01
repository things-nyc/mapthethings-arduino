[![Build Status](https://travis-ci.org/things-nyc/mapthethings-arduino.svg?branch=master)](https://travis-ci.org/things-nyc/mapthethings-arduino)

# MapTheThings-Arduino

The Arduino app portion of [MapTheThings](http://map.thethings.nyc), the
global coverage map for The Things Network (TTN).

## Hardware Wiring

### Using [RFM95 LoRa Breakout](https://www.adafruit.com/product/3072) and [Feather M0 Bluefruit](https://www.adafruit.com/product/2995)
- [LoRa transceiver pins are defined in software here](https://github.com/forrestfiller/mapthethings-arduino/blob/9658f6d9188a7f15bafe98ab11a1953692ebf456/MapTheThings-Arduino/Lora.cpp#L53)
RMF95 | Feather
----- | -------
VIN   | 3V
GND   | GND
G0    | A2
CS    | A4
RST   | A5
SCK   | SCK
MOSI  | MOSI
MISO  | MISO
G2    | 6
G1    | 5

( You can connect either to a [li-poly battery](https://www.adafruit.com/products/2750) or power via [USB cable](https://www.adafruit.com/products/2008) )


![alt text](https://github.com/forrestfiller/mapthethings-arduino/blob/master/images/assembled-on-breadboard-web.jpg "assembled map the things node")

---

In lieu of the above breakout board, you could alternatively use the [LoRa Radio Featherwing](https://www.adafruit.com/products/3231) in conjunction with the above Bluefruit. You'll want to solder up the connects like so:


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

## License
Source code for Map The Things is released under the MIT License,
which can be found in the [LICENSE](LICENSE) file.
