# RogueAP for ESP32
The roge access point software uses the ESP32's capability to function as an access point. It sets an configurable SSID and upon a client associating with the AP, the software redirects any HTTP query (through DNS redirection) to a portal website. The portal webpage can be customized to mimic a website where the user has to enter credentials in order to proceed. Once, credentials have been send by the user, they are stored in the ESP32's flash memory and an error message is displayed to the user.

For more info check: [RogueAP Wiki](../../wiki)

### DISCLAIMER
```
This software is for educational and research purposes only.

Do not attempt to violate the law with anything contained here. If this is your intention,
then LEAVE NOW! The authors of this software, or anyone else affiliated in any way, is going
to accept responsibility for your actions. We do NOT promote Hacking! We are documenting the
way hackers steal and perform activities. So it can be useful to Protect yourself.
```

## requires
* Arduino IDE
* ESP32 Arduino core SDK - [https://github.com/espressif/arduino-esp32](https://github.com/espressif/arduino-esp32)
* ESP32Webserver - [https://github.com/nhatuan84/esp32-webserver.git](https://github.com/nhatuan84/esp32-webserver.git)


## additional tools/resources
* Compress a webpage into an single html file - [https://github.com/lemariva/SquirelCrawl](https://github.com/lemariva/SquirelCrawl)
* CSS reference - [https://css-tricks.com](https://css-tricks.com/)

## TODO
- Upload page for new portals

## Known Bugs
-
