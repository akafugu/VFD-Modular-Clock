Modular VFD Clock Firmware
--------------------------

![Akafugu Modular VFD Clock](http://next.akafugu.jp/images/products/vfdclock/vfd-3.jpg)

Firmware for the [Akafugu Modular VFD Clock](http://www.akafugu.jp/posts/products/vfd-modular-clock/)

The VFD Modular Clock is a clock based on old-fashioned VFD Display Tubes.

VFD is short for Vacuum Flourescent Display. A VFD display is typically green or blue, and emits a bright light with high contrast. VFD Displays are often found in car radios.

A VFD Display tube looks like an old Vacuum Tube, the predecessor to the transistor. The inside of the tube contains segments that can be lit up to form numbers and letters. Most tubes contain segments for one digit, and several must be stacked together to make a complete display.

The clock itself is modular, it comes with a base board, which is powered by an ATMega328p microcontroller and contains a high-voltage VFD driver that is used to light up the display shield that sits on the top board.

Firmware
--------

The VFD Modular Clock is based on the ATMega328P microcontroller. The firmware is written for the avr-gcc compiler and covers all basic clock functionality such as setting time and alarm, brightness and 24h/12h time.

The clock comes pre-installed with firmware: To update it you will need to solder a 2x3 male header to the ISP port on the board and then use an ISP programmer and avrdude. see [here](http://next.akafugu.jp/posts/resources/avr-gcc/) for more instructions.
