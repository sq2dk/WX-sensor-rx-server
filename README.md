# WX-sensor-rx-server
ESP32 internet server receiving data over the radio from wireless from weather sensor
Based on library from: https://github.com/matthias-bs/BresserWeatherSensorReceiver
Wich does all the heavy lifting when it comes to radio communication. 
Code equipped with simple WWWW server for displaying received data as well as UDP server for data exchange with other network devices.
Sample python code for data exchange over UDP provided.

![alt text](https://github.com/sq2dk/WX-sensor-rx-server/blob/main/wx_sensor.jpg?raw=true)

![alt text](https://github.com/sq2dk/WX-sensor-rx-server/blob/main/Screenshot.jpg?raw=rtue)


**List of UDP commands. All commands to end with [CR][LF]:**


**Commands for reading current vallues (latest readings):**
+ "temp" - returns current temperature in deg C.
+ "hum" - returns current relative hummidity in percent
+ "win_dir" - returns current wind direction in deg
+ "win_avr" - returns current average wind speed readed from sensor in m/s
+ "win_gus" - returns current gusts wind speed readed from sensor in m/s
+ "rain" - returns rain sensor counter readings in mm
+ "press" - returns current atm. pressure in Pa
+ "in_temp" - temperature inside - temperature of pressure sensor


**Commands returning averaged values (over 5 minutes):**

+ "atemp" - returns average temperature in deg C.
+ "ahum" - returns average relative hummidity in percent
+ "awin_dir" - returns aerage wind direction in deg
+ "awin_avr" - returns average wind speed readed from sensor in m/s
+ "awin_gus" - returns maximum gusts wind speed during last 5 minutes from sensor in m/s
+ "wind_qual" - quality factor of wind consistancy - 0 to 1, 1-wind direction steady, 0-wind direction changing
+ "apress" - returns average atm. pressure during last 5 min in Pa
+ "in_temp" - average temperature (5min) inside - temperature of pressure sensor

**Additional commands:**

+ "rssi" - returns signal strength of latest received packet (in dBm)
+ "last" - time in miliseconds since last packet from sensor was received
+ "atime" - time in ms since last averaging was completed
+ "anum" - number of averages performed since last reset (number of 5min periods)
+ "apack" - number of packets received in current averaging cycle
+ "rain_per" - amount of rainfall in mm per 24 hours (updated every 24 hours)
+ "rx_quality" - percentage of received packets during last 5 min
+ "battery" - returns 0 if sensor battery is low, 1 if it is ok.
+ "RESET" - resets the device

**Changes to the libraries:**

"ESPAsyncWebServer.h":
    change "SSE_MAX_QUEUED_MESSAGES" value to 14, oterwise not all the values on the WWW will be updated.
    
"WeatherSensorCfg.h":
in --- Radio Transceiver --- - uncomment "define USE_CC1101" and comment all other
Since I have incidentally switched pins in my PCB I had to changed definition of #define PIN_RECEIVER_IRQ to 5 and PIN_RECEIVER_GPIO to 4

