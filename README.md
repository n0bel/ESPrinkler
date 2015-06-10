# ESPrinkler
An ESP8266 based WiFi lawn/garden sprinkler control system.

Work in progress alpha development

Progress has been made...

All the basics have been coded at this point.  It works in AP, STA, and STA+AP modes.
It does require internet access to get the time.   I may add an RTC at some point.
It controls up to 8 Relays for 8 Zones of water valves.  You can control each relay
manually, or through a schedule.  Everything is configured through web pages served
directly from the esp8266.  It will require an ESP8266 wite more GPIOs broken out than
the venerable ESP01.  I'm using an ESP12.  A NodeMCU-devkit will work too.

You can take this code and load it on your ESP8266 and it will work.

For those of you more advanced people that don't need step by step instructions...

A 74HC595 is connected as an i/o expander as follows:

* GPIO2 to Pin 14 Data
* GPIO5 to Pin 11 Shift (careful, some ESP07 and ESP12 have the wrong silkscreen)
* GPIO4 to Pin 12 Latch
* Pin 13 /OE, Pin 10 /MR to GND of course
* The outputs appear in order Q0 thru Q7 as Zones/Relays 1 thru 8, active high.

I will be giving detailed documentation and instructions as time goes on.

### Important Notes

Note that my development platform is Windows and Eclipse. The .settings files,
and the Makefile represent a windows setup. However you can adjust the Makefile
with the proper paths and it will compile under Linux, as long as the toolchains
are in the PATH.  The windows eclipse platform is based on this: http://www.esp8266.com/viewtopic.php?f=9&t=820 and this: https://github.com/CHERTS/esp8266-devkit

Also note this software requires SDK 1.1.0 (esp_iot_sdk_v1.0.1).
  