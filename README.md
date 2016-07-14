# MapTheThings-Arduino

The Arduino app portion of [MapTheThings](http://map.thethings.nyc), the
global coverage map for The Things Network (TTN).

## Using the code
- Install [Adafruit's Adafruit_BluefruitLE_nRF51 library](https://github.com/adafruit/Adafruit_BluefruitLE_nRF51) Arduino library
- Install [Matthijs Kooijman's LMIC library](https://github.com/matthijskooijman/arduino-lmic)
- Configure appropriately

## Node Responsibilities
- Advertise capabilities via BLE
- Respond to scan from a BLE Center (the MapTheThings-iOS app)
- Serve LoRa configuration, status, and responses as BLE characteristics
- Accept LoRa configuration and transmission commands as BLE characteristic

## License
Source code for Map The Things is released under the MIT License,
which can be found in the [LICENSE](LICENSE) file.
