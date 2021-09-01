# ESP-Arduino-IOT

Using serial communication between Arduino and ESP8266 (because i use  ESP-01 and the GPIOs are limited and not practical to use)

 
**Features** 

* Supports control to all pins that specified in code (includes timing)
* Responsive web interface based on [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer) (API is available to be used in Android)
* Supports scheduling (one time and repeating every day) and stored in onboard flash

**Notes** 

* When compiling, use at least 512kB FS data  and use only lwIP v2 Lower Memory (Higher bandwidth version seems to be unstable, and lower memory version is fast enough)
* Don't be confused with .gzp extension. It is actually .gz file. This avoids auto downloading by external downloader, ex: IDM