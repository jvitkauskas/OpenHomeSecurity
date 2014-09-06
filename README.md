OpenHomeSecurity
================

This repository will contain the latest software for main board.

http://openhomesecurity.blogspot.com

This project should replace standard home security alarms, and in future gather statistics from various sensors.

It consists from one main alarm board with atmega 1284P that thas inputs for sensors and outputs to relays. Main board also hosts wiz820io Ethernet for configuration and overview, RFM12B for radio remote nodes (not yet implemented), communication module to wired nodes (RS485 protocol), UART for GSM modem, Battery backed up RTC, EEPROM for log, I2C expansion connector, and AC supply and battery monitoring.

Main board has 7 analog inputs that can recognize different events on sensors. And 4 digital inputs.

Remote nodes act as authentication units and currently they are set to receive iButtons as keys to arm/disarm the system.
