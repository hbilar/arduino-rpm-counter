

# Arduino based RPM counter 

Little Arduino hack to monitor a hall sensor and then display the RPM on
the serial port and also on a 7 segment LED display.


##Hardware:

* Arduino UNO
* Ebay arduino hall sensor (this seems to work exactly the opposite to a normal hall sensor)
* Ebay arduino 7 segment LED display (uses 2 x 74HC595 IC's).


##Pins:

2 - hall sensor input
4 - LED data out
7 - 74HC595 IC 1
8 - 74HC595 IC 2