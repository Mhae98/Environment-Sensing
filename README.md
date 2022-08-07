# Environment-Sensing
Greate a environment sensing dashboard using an BME 680 sensor for getting the data, an Arduino nano RP2040  connect for transmitting the data

# Used hardware
- Microcontroller: [Arduino Nano RP2040 Connect](https://docs.arduino.cc/hardware/nano-rp2040-connect)
- Environmental Sensor: [Bosch BME680](https://www.bosch-sensortec.com/products/environmental-sensors/gas-sensors/bme680/)

# Arduino setup
## Boardmanager
To enable the full potential of the RP2040 the [board manager](https://github.com/earlephilhower/arduino-pico) of Earle F. Philhower wit the version 2.3.3 was used.
A guide for the installation of the board manager can be found [here](https://github.com/earlephilhower/arduino-pico), the documentation [here](https://arduino-pico.readthedocs.io/en/latest/).

## Used libraries
- [WiFiNINA_Generic](https://github.com/khoih-prog/WiFiNINA_Generic) Version 1.8.14-5 for WiFi connection
- [BSEC Software Library](https://www.bosch-sensortec.com/software-tools/software/bsec/) for getting the sensor data

Both libraries can be found in the library manager of the Arduino IDE.

# Using the code
For connecting to your network set the credentials in [secrets.h](./source/Arduino/secrets.h)