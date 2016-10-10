# MapTheThings-Arduino

The Arduino app portion of [MapTheThings](http://map.thethings.nyc), the
global coverage map for The Things Network (TTN).

## Using the code

### Using with platformIO
- ```platformio run``` (to install libraries and build code)
- ```platformio upload``` (to install code on a device)

### Using with Arduino IDE
- Install [Adafruit's Adafruit_BluefruitLE_nRF51 library](https://github.com/adafruit/Adafruit_BluefruitLE_nRF51) Arduino library
- Install [The Things Network New York's version of the IBM LMIC library](https://github.com/things-nyc/arduino-lmic)
- Verify and Upload code

## Node Responsibilities
- Advertise capabilities via BLE
- Respond to scan from a BLE Center (the MapTheThings-iOS app)
- Serve LoRa configuration, status, and responses as BLE characteristics
- Accept LoRa configuration and transmission commands as BLE characteristic

## License
Source code for Map The Things is released under the MIT License,
which can be found in the [LICENSE](LICENSE) file.
