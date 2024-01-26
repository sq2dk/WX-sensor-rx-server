///////////////////////////////////////////////////////////////////////////////////////////////////
// Based on:
// BresserWeatherSensorOptions.ino
//
// https://github.com/matthias-bs/BresserWeatherSensorReceiver
//
// and on:
// https://esp32tutorials.com/esp32-bme280-web-server-esp-idf/ - if I remember correctly
//
///////////////////////////////////////////////////////////////////////////////////////////////////

#include <Arduino.h>
#include "WeatherSensorCfg.h"
#include "WeatherSensor.h"
#include <ESP8266WiFi.h>
#include "ESPAsyncWebServer.h"
#include <WiFiUdp.h>
#define UDP_PORT 4210

// Replace with your network credentials
const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";

// UDP
WiFiUDP UDP;
//char packet[255];
char incomingPacket[255];  // buffer for incoming packets
//char reply[] = "Packet received!";


// Set TIMEOUT to a relative small value to see the behaviour of different options -
// depending on the timing of sensor transmission, start of getData() call and selected option,
// getData() may succeed or encounter a timeout.
// For practical use, set TIMEOUT large enough that getData() will succeed in most cases.
#define TIMEOUT 45000 // Timeout in ms
#define BRESSER_5_IN_1

AsyncWebServer server(80);
AsyncEventSource events("/events");

unsigned long lastTime = 0;  
unsigned long timerDelay = 30000;  // send readings timer

float temperature;
float humidity;
float wind_dir;
float wind_avr;
float wind_gust;
float raindrop;
float rssi;

WeatherSensor weatherSensor;


String processor(const String& var){
  //getBME680Readings();
  //Serial.println(var);
  if(var == "TEMPERATURE"){
    return String(weatherSensor.sensor[0].temp_c,1);
  }
  else if(var == "HUMIDITY"){
    return String(weatherSensor.sensor[0].humidity);
  }
  else if(var == "WIND_AVR"){
    return String(weatherSensor.sensor[0].wind_avg_meter_sec,1);
  }
  else if(var == "WIND_DIR"){
    return String(weatherSensor.sensor[0].wind_direction_deg,0);
  }
 else if(var == "WIND_GUST"){
    return String(weatherSensor.sensor[0].wind_gust_meter_sec,1);
  }
   else if(var == "RAINDROP"){
    return String(weatherSensor.sensor[0].rain_mm,1);
  }
  else if(var == "RSSI"){
    return String(weatherSensor.sensor[0].rssi,0);
  }

  return String();
}

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>Meteo</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css" integrity="sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr" crossorigin="anonymous">
  <link rel="icon" href="data:,">
  <style>
    html {font-family: Arial; display: inline-block; text-align: center;}
    p {  font-size: 1.2rem;}
    body {  margin: 0;}
    .topnav { overflow: hidden; background-color: #4B1D3F; color: white; font-size: 1.7rem; }
    .content { padding: 20px; }
    .card { background-color: white; box-shadow: 2px 2px 12px 1px rgba(140,140,140,.5); }
    .cards { max-width: 700px; margin: 0 auto; display: grid; grid-gap: 2rem; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); }
    .reading { font-size: 2.8rem; }
    .card.temperature { color: #0e7c7b; }
    .card.humidity { color: #17bebb; }
    .card.wind_avr { color: #3fca6b; }
    .card.wind_dir { color: #d62246; }
    .card.wind_gust { color: #3f2246; }
    .card.rainfall { color: #d6bebb; }
    .card.rssi { color: #999999; }
  </style>
</head>
<body>
  <div class="topnav">
    <h3>SQ2DK METEO</h3>
  </div>
  <div class="content">
    <div class="cards">
      <div class="card temperature">
        <h4><i class="fas fa-thermometer-half"></i> TEMPERATURE</h4><p><span class="reading"><span id="temp">%TEMPERATURE%</span> &deg;C</span></p>
      </div>
      <div class="card humidity">
        <h4><i class="fas fa-tint"></i> HUMIDITY</h4><p><span class="reading"><span id="hum">%HUMIDITY%</span> &percnt;</span></p>
      </div>
      <div class="card wind_dir">
        <h4><i class="fa fa-arrows-alt"></i> WIND DIRECTION</h4><p><span class="reading"><span id="w_dir">%WIND_DIR%</span> &deg;</span></p>
      </div>
      <div class="card wind_avr">
        <h4><i class="fas fa-wind"></i> WIND AVERAGE</h4><p><span class="reading"><span id="w_avr">%WIND_AVR%</span> m/s</span></p>
      </div>
      <div class="card wind_gust">
        <h4><i class="fa fa-wind"></i> WIND GUST</h4><p><span class="reading"><span id="w_gust">%WIND_GUST%</span> m/s</span></p>
      </div>
       <div class="card rainfall">
        <h4><i class="fas fa-cloud-rain"></i> RAIN</h4><p><span class="reading"><span id="rain">%RAINDROP%</span> mm</span></p>
      </div>
      
       <div class="card rssi">
        <h4><i class="fa fa-signal"></i> SIGNAL</h4><p><span class="reading"><span id="rssi">%RSSI%</span> dB</span></p>
      </div>
    </div>
    </div>
  </div>
<script>
if (!!window.EventSource) {
 var source = new EventSource('/events');
 
 source.addEventListener('open', function(e) {
  console.log("Events Connected");
 }, false);
 source.addEventListener('error', function(e) {
  if (e.target.readyState != EventSource.OPEN) {
    console.log("Events Disconnected");
  }
 }, false);
 
 source.addEventListener('message', function(e) {
  console.log("message", e.data);
 }, false);
 
 source.addEventListener('temperature', function(e) {
  console.log("temperature", e.data);
  document.getElementById("temp").innerHTML = e.data;
 }, false);
 
 source.addEventListener('humidity', function(e) {
  console.log("humidity", e.data);
  document.getElementById("hum").innerHTML = e.data;
 }, false);
 
 source.addEventListener('wind_dir', function(e) {
  console.log("wind_dir", e.data);
  document.getElementById("w_dir").innerHTML = e.data;
 }, false);
 
 source.addEventListener('wind_avr', function(e) {
  console.log("wind_avr", e.data);
  document.getElementById("w_avr").innerHTML = e.data;
 }, false);

 source.addEventListener('wind_gust', function(e) {
  console.log("wind_gust", e.data);
  document.getElementById("w_gust").innerHTML = e.data;
 }, false);

 source.addEventListener('raindrop', function(e) {
  console.log("raindrop", e.data);
  document.getElementById("rain").innerHTML = e.data;
 }, false);

 source.addEventListener('rssi', function(e) {
  console.log("rssi", e.data);
  document.getElementById("rssi").innerHTML = e.data;
 }, false);
}
</script>
</body>
</html>)rawliteral";





void print_data(void)
{
  for (int i=0; i<NUM_SENSORS; i++) {
    if (!weatherSensor.sensor[i].valid)
      continue;

    Serial.printf("Id: [%8X] Typ: [%X] Battery: [%s] ",
        weatherSensor.sensor[i].sensor_id,
        weatherSensor.sensor[i].s_type,
        weatherSensor.sensor[i].battery_ok ? "OK " : "Low");
    #ifdef BRESSER_6_IN_1
        Serial.printf("Ch: [%d] ", weatherSensor.sensor[i].chan);
    #endif
    if (weatherSensor.sensor[i].temp_ok) {
        Serial.printf("Temp: [%5.1fC] ",
            weatherSensor.sensor[i].temp_c);
    } else {
        Serial.printf("Temp: [---.-C] ");
    }
    if (weatherSensor.sensor[i].humidity_ok) {
        Serial.printf("Hum: [%3d%%] ",
            weatherSensor.sensor[i].humidity);
    }
    else {
        Serial.printf("Hum: [---%%] ");
    }
    if (weatherSensor.sensor[i].wind_ok) {
        Serial.printf("Wind max: [%4.1fm/s] Wind avg: [%4.1fm/s] Wind dir: [%5.1fdeg] ",
                weatherSensor.sensor[i].wind_gust_meter_sec,
                weatherSensor.sensor[i].wind_avg_meter_sec,
                weatherSensor.sensor[i].wind_direction_deg);
    } else {
        Serial.printf("Wind max: [--.-m/s] Wind avg: [--.-m/s] Wind dir: [---.-deg] ");
    }
    if (weatherSensor.sensor[i].rain_ok) {
        Serial.printf("Rain: [%7.1fmm] ",
            weatherSensor.sensor[i].rain_mm);
    } else {
        Serial.printf("Rain: [-----.-mm] ");
    }
    #if defined BRESSER_6_IN_1 || defined BRESSER_7_IN_1
    if (weatherSensor.sensor[i].moisture_ok) {
        Serial.printf("Moisture: [%2d%%] ",
            weatherSensor.sensor[i].moisture);
    }
    else {
        Serial.printf("Moisture: [--%%] ");
    }
    #endif
    #if defined BRESSER_6_IN_1 || defined BRESSER_7_IN_1
    if (weatherSensor.sensor[i].uv_ok) {
        Serial.printf("UV index: [%1.1f] ",
            weatherSensor.sensor[i].uv);
    }
    else {
        Serial.printf("UV index: [-.-%%] ");
    }
    #endif
    #ifdef BRESSER_7_IN_1
    if (weatherSensor.sensor[i].light_ok) {
        Serial.printf("Light (Klux): [%2.1fKlux] ",
            weatherSensor.sensor[i].light_klx);
    }
    else {
        Serial.printf("Light (lux): [--.-Klux] ");
    }
    #endif
    Serial.printf("RSSI: [%6.1fdBm]\n", weatherSensor.sensor[i].rssi);
  } // for (int i=0; i<NUM_SENSORS; i++)
} // void print_data(void)

void setup() {
    Serial.begin(115200);
    Serial.setDebugOutput(true);
      // Set the device as a Station and Soft Access Point simultaneously
  WiFi.mode(WIFI_AP_STA);
  
  // Set device as a Wi-Fi Station
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Setting as a Wi-Fi Station..");
  }
  Serial.print("Station IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.println();


    weatherSensor.begin();

      // Handle Web Server
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });

  // Handle Web Server Events
  events.onConnect([](AsyncEventSourceClient *client){
    if(client->lastId()){
      Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
    }
    // send event with message "hello!", id current millis
    // and set reconnect delay to 1 second
    client->send("hello!", NULL, millis(), 10000);
  });
  server.addHandler(&events);
  server.begin();

   // Begin listening to UDP port
  UDP.begin(UDP_PORT);
  Serial.print("Listening on UDP port ");
  Serial.println(UDP_PORT);
 
}

void send_UDP(void)
{
   // Send return packet
    //reply=String(weatherSensor.sensor[0].temp_c,1);
    UDP.beginPacket(UDP.remoteIP(), UDP.remotePort());
    //UDP.write(reply);
    //UDP.write(&weatherSensor.sensor[0].temp_c,sizeof(weatherSensor.sensor[0].temp_c));
    UDP.print(weatherSensor.sensor[0].temp_c);
    UDP.print(";");
    UDP.print(weatherSensor.sensor[0].humidity);
    UDP.print(";");
    UDP.print(weatherSensor.sensor[0].wind_direction_deg);
    UDP.print(";");
    UDP.print(weatherSensor.sensor[0].wind_avg_meter_sec);
    UDP.print(";");
    UDP.print(weatherSensor.sensor[0].wind_gust_meter_sec);
    UDP.print(";");
    UDP.print(weatherSensor.sensor[0].rain_mm);
    UDP.print(";");
    UDP.print(weatherSensor.sensor[0].rssi);
    UDP.print(";");
    UDP.print('\n');
    //,wind_dir,";",
    //,wind_dir,";",wind_avr);
    //UDP.write(",");
    //,",",);
    UDP.endPacket();
}

void loop()
{
    bool decode_ok;

    // Clear all sensor data
    weatherSensor.clearSlots();

    // Try to receive data from at least one sensor,
    // finish even if data is incomplete.
    // (Data can be distributed across multiple radio messages.)
   // decode_ok = weatherSensor.getData(TIMEOUT);
   
    decode_ok = weatherSensor.getMessage();
   

    
    if (!decode_ok) {
     // Serial.printf("Sensor timeout\n");
    } else {
      print_data();

      temperature=weatherSensor.sensor[0].temp_c;
      humidity=weatherSensor.sensor[0].humidity;
      wind_dir=weatherSensor.sensor[0].wind_direction_deg;
      wind_avr=weatherSensor.sensor[0].wind_avg_meter_sec;
      wind_gust=weatherSensor.sensor[0].wind_gust_meter_sec;
      raindrop=weatherSensor.sensor[0].rain_mm;
      rssi=weatherSensor.sensor[0].rssi;
   
    }

 
    // Try to receive data from at least one sensor,
    // finish only if data is complete.
    // (Data can be distributed across multiple radio messages.)
    //weatherSensor.clearSlots();
    //decode_ok = weatherSensor.getData(TIMEOUT, DATA_COMPLETE);
    
    
    int packetSize = UDP.parsePacket();
    if (packetSize)
  {
    // receive incoming UDP packets
    Serial.printf("Received %d bytes from %s, port %d\n", packetSize, UDP.remoteIP().toString().c_str(), UDP.remotePort());
    int len = UDP.read(incomingPacket, 255);
    if (len > 0)
    {
      incomingPacket[len] = 0;
      incomingPacket[len-1] = 0;
    }
    Serial.printf("UDP packet contents: %s\n", incomingPacket);
     UDP.beginPacket(UDP.remoteIP(), UDP.remotePort());
     
     if (String(incomingPacket) == "temp") {UDP.print(weatherSensor.sensor[0].temp_c);}
     if (String(incomingPacket) == "hum") {UDP.print(weatherSensor.sensor[0].humidity);}
     if (String(incomingPacket) == "win_dir") {UDP.print(weatherSensor.sensor[0].wind_direction_deg);}
     if (String(incomingPacket) == "win_avr") {UDP.print(weatherSensor.sensor[0].wind_avg_meter_sec);}
     if (String(incomingPacket) == "win_gus") {UDP.print(weatherSensor.sensor[0].wind_gust_meter_sec);}
     if (String(incomingPacket) == "rain") {UDP.print(weatherSensor.sensor[0].rain_mm);}
     if (String(incomingPacket) == "rssi") {UDP.print(weatherSensor.sensor[0].rssi);}
     UDP.print('\n');
     UDP.endPacket();
    // send back a reply, to the IP address and port we got the packet from
    //send_UDP();
  }

 if ((millis() - lastTime) > timerDelay && decode_ok == DECODE_OK) {
          // Send Events to the Web Server with the Sensor Readings
    events.send("ping",NULL,millis());
    events.send(String(temperature,1).c_str(),"temperature",millis());
    events.send(String(humidity,0).c_str(),"humidity",millis());
    events.send(String(wind_dir,0).c_str(),"wind_dir",millis());
    events.send(String(wind_avr,1).c_str(),"wind_avr",millis());
    events.send(String(wind_gust,1).c_str(),"wind_gust",millis());
    events.send(String(raindrop,1).c_str(),"raindrop",millis());
    events.send(String(rssi,0).c_str(),"rssi",millis());
    
    lastTime = millis();
    }


     delay(100);

    
   
  
} // loop()
