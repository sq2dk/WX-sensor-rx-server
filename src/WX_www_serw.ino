///////////////////////////////////////////////////////////////////////////////////////////////////
// Based on:
// BresserWeatherSensorOptions.ino
//
// https://github.com/matthias-bs/BresserWeatherSensorReceiver
//
// and on:
// https://esp32tutorials.com/esp32-bme280-web-server-esp-idf/ - if I remember correctly
//
//  TO DO - serial communication for data exchange
//  TO DO - average values for last 5 minutes
//  TO DO - variation of wind in time.
//  TO DO - rain during last 60min
//
///////////////////////////////////////////////////////////////////////////////////////////////////



#include <Arduino.h>
#include "WeatherSensorCfg.h"
#include "WeatherSensor.h"
#include <ESP8266WiFi.h>
#include "ESPAsyncWebServer.h"
#include <WiFiUdp.h>
#define UDP_PORT 4210

#define USE_CC1101
//#define ESP8266

// Replace with your network credentials

const char* ssid = "SSID";
const char* password = "password";

const char* station = "Your station";


// Set your Static IP address
IPAddress local_IP(192, 168, 1, 2);

// Set your Gateway IP address
IPAddress gateway(192, 168, 1, 1);

IPAddress subnet(255, 255, 255, 0);

IPAddress primaryDNS(8, 8, 8, 8);   //optional
IPAddress secondaryDNS(8, 8, 4, 4); //optional


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

#define INT_TIME 300000  //integration time in ms (5min)
//#define INT_TIME 100000  //integration time in ms (5min)

AsyncWebServer server(80);
AsyncEventSource events("/events");

float deg_to_rad = 0.01745329252;  //deg to rad calc

unsigned long lastTime = 0;  
unsigned long lastDecoded = 0;
unsigned long timerDelay = 30000;  // send readings timer

float last_time;

// avereage related 
unsigned long last_integr = 0;  //time for chceking last integration (averege value calculation)
unsigned long packet_no = 0;   //number of received packets in current integration

float sum_temperature = 0;
float sum_humidity = 0;
double sum_x_wind = 0;  //for vector sum calculation
double sum_y_wind = 0;  //for vector sum calculation
float sum_wind_avr = 0;
float max_wind_gust = 0;

float avr_temperature;
float avr_humidity;
float avr_wind_dir;  //for vector sum calculation
float avr_wind_avr;
float avr_wind_gust;
float avr_wind_consistency;
unsigned long avr_number;


float temperature;
float humidity;
float wind_dir;
float wind_avr;
float wind_gust;
float raindrop;
float rssi;



WeatherSensor ws;
byte Verbose_level=0;
byte avr_val_www = 1;  //1- show avereage values on WWW, 0 - sow current values


String processor(const String& var){
  //getBME680Readings();
  if (Verbose_level>4) {Serial.println(var);}
  
  if (avr_val_www) {
    if(var == "TEMPERATURE") { return String(avr_temperature,1); }
    else if(var == "HUMIDITY") { return String(avr_humidity,0); }
    else if(var == "WIND_AVR") { return String(avr_wind_avr,1); }
    else if(var == "WIND_DIR") { return String(avr_wind_dir,0); }
    else if(var == "WIND_GUST") {return String(avr_wind_gust,1); }
  }
  else if (avr_val_www==0) {
   if(var == "TEMPERATURE"){
     return String(ws.sensor[0].w.temp_c,1);
    }
   else if(var == "HUMIDITY"){
     return String(ws.sensor[0].w.humidity,0);
    }
   else if(var == "WIND_AVR"){
     return String(ws.sensor[0].w.wind_avg_meter_sec,1);
    }
   else if(var == "WIND_DIR"){
      return String(ws.sensor[0].w.wind_direction_deg,0);
    }
   else if(var == "WIND_GUST"){
     return String(ws.sensor[0].w.wind_gust_meter_sec,1);
    }
  }

  if(var == "RAINDROP"){
    return String(ws.sensor[0].w.rain_mm,1);
  }
  else if(var == "RSSI"){
    return String(ws.sensor[0].rssi,0);
  }
  else if(var == "STATION"){
    return String(station);
  }


  return String();
}

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>%STATION% Meteo</title>
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
    <h3>%STATION% METEO</h3>
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
    if (!ws.sensor[i].valid)
      continue;

    Serial.printf("Id: [%8X] Typ: [%X] Battery: [%s] ",
        ws.sensor[i].sensor_id,
        ws.sensor[i].s_type,
        ws.sensor[i].battery_ok ? "OK " : "Low");
    #ifdef BRESSER_6_IN_1
        Serial.printf("Ch: [%d] ", ws.sensor[i].chan);
    #endif
    if (ws.sensor[i].w.temp_ok) {
        Serial.printf("Temp: [%5.1fC] ",
            ws.sensor[i].w.temp_c);
    } else {
        Serial.printf("Temp: [---.-C] ");
    }
    if (ws.sensor[i].w.humidity_ok) {
        Serial.printf("Hum: [%3d%%] ",
            ws.sensor[i].w.humidity);
    }
    else {
        Serial.printf("Hum: [---%%] ");
    }
    if (ws.sensor[i].w.wind_ok) {
        Serial.printf("Wind max: [%4.1fm/s] Wind avg: [%4.1fm/s] Wind dir: [%5.1fdeg] ",
                ws.sensor[i].w.wind_gust_meter_sec,
                ws.sensor[i].w.wind_avg_meter_sec,
                ws.sensor[i].w.wind_direction_deg);
    } else {
        Serial.printf("Wind max: [--.-m/s] Wind avg: [--.-m/s] Wind dir: [---.-deg] ");
    }
    if (ws.sensor[i].w.rain_ok) {
        Serial.printf("Rain: [%7.1fmm] ",
            ws.sensor[i].w.rain_mm);
    } else {
        Serial.printf("Rain: [-----.-mm] ");
    }
    #if defined BRESSER_6_IN_1 || defined BRESSER_7_IN_1
    if (ws.sensor[i].w.uv_ok) {
        Serial.printf("UV index: [%1.1f] ",
            ws.sensor[i].w.uv);
    }
    else {
        Serial.printf("UV index: [-.-%%] ");
    }
    #endif
    #ifdef BRESSER_7_IN_1
    if (ws.sensor[i].w.light_ok) {
        Serial.printf("Light (Klux): [%2.1fKlux] ",
            ws.sensor[i].w.light_klx);
    }
    else {
        Serial.printf("Light (lux): [--.-Klux] ");
    }
    #endif
    Serial.printf("RSSI: [%6.1fdBm]\n", ws.sensor[i].rssi);
  } // for (int i=0; i<NUM_SENSORS; i++)
} // void print_data(void)

void setup() {
    Serial.begin(115200);
    Serial.setDebugOutput(true);
      // Set the device as a Station and Soft Access Point simultaneously
  WiFi.mode(WIFI_AP_STA);
  
    // Configures static IP address
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("STA Failed to configure");
  }

  // Set device as a Wi-Fi Station
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Setting as a Wi-Fi Station..");
  }

  Serial.print("Station IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  if (Verbose_level>2) { Serial.printf("Starting radio interface\n");}
  ws.begin();

      // Handle Web Server
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });

  // Handle Web Server Events
  events.onConnect([](AsyncEventSourceClient *client){
    if(client->lastId()){
      if (Verbose_level>1) { Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId()); }
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

//
//float asin(float c)
//{
//
//  float out;
//  out= ((c+(c*c*c)/6+(3*c*c*c*c*c)/40+(5*c*c*c*c*c*c*c)/112+
//  (35*c*c*c*c*c*c*c*c*c)/1152 +(c*c*c*c*c*c*c*c*c*c*c*0.022)+
//  (c*c*c*c*c*c*c*c*c*c*c*c*c*.0173)+(c*c*c*c*c*c*c*c*c*c*c*c*c*c*c*.0139)+
//  (c*c*c*c*c*c*c*c*c*c*c*c*c*c*c*c*c*0.0115)+(c*c*c*c*c*c*c*c*c*c*c*c*c*c*c*c*c*c*c*0.01)));
//  //asin
//  if(c>=.96 && c<.97){out=1.287+(3.82*(c-.96)); }
//  if(c>=.97 && c<.98){out=(1.325+4.5*(c-.97));} // arcsin
//  if(c>=.98 && c<.99){out=(1.37+6*(c-.98));}
//  if(c>=.99 && c<=1){out=(1.43+14*(c-.99));}  
//return out;
//
//}  //asin()

//float acos(float c){
//  
//  float out;
//  out=asin(sqrt(1-c*c));
//  return out;
//} //acos()

void send_UDP_value(float val_to_print)
{
UDP.beginPacket(UDP.remoteIP(), UDP.remotePort());
UDP.print(val_to_print*100);
UDP.print('\n');
UDP.endPacket();
}


void add_val_avr(void)
{
  double cos_res = cos(wind_dir * deg_to_rad);
  double sin_res = sin(wind_dir * deg_to_rad);
  sum_temperature+=temperature;
  sum_humidity+=humidity;
  sum_x_wind=sum_x_wind + cos_res;  //for vector sum calculation
  sum_y_wind=sum_y_wind + sin_res;  //for vector sum calculation
  sum_wind_avr+=wind_avr;
  if (max_wind_gust < wind_gust) 
     { max_wind_gust = wind_gust; }
  packet_no++;

  //send_UDP_value(cos_res);
}



void prep_avr_val(void)
{
  double xy_wind_len = sqrt( sq(sum_x_wind) + sq(sum_y_wind));
  if (packet_no>0)
  {
    avr_temperature = sum_temperature / packet_no;
    avr_humidity = sum_humidity / packet_no;
    avr_wind_avr = sum_wind_avr / packet_no;
    if (xy_wind_len > 0) {
    avr_wind_dir=(acos(abs(sum_x_wind)/xy_wind_len)*180/3.14);
    if ((sum_x_wind >= 0) && (sum_y_wind >= 0)) {avr_wind_dir= avr_wind_dir;}   
    if ((sum_x_wind < 0) && (sum_y_wind > 0)) {avr_wind_dir= 180 - avr_wind_dir;}
    if ((sum_x_wind <= 0) && (sum_y_wind <= 0)) {avr_wind_dir= 180 + avr_wind_dir;}
    if ((sum_x_wind > 0) && (sum_y_wind < 0)) {avr_wind_dir= 360 - avr_wind_dir;}
    avr_wind_consistency = (xy_wind_len / packet_no);
    }
    if (xy_wind_len == 0) {avr_wind_dir=0;}
    
  }

  //send_UDP_value(sum_x_wind);
  //send_UDP_value(xy_wind_len);
  //send_UDP_value(avr_wind_dir);
  
  avr_wind_gust = max_wind_gust;
  //avr_wind_gust = 22.3;

  sum_temperature = 0;
  sum_humidity = 0;
  sum_wind_avr = 0;
  max_wind_gust = 0;
  sum_x_wind = 0;
  sum_y_wind = 0;
  packet_no = 0;
  last_integr=millis();
  avr_number++;

}


void send_UDP(void)
{
   // Send return packet
    //reply=String(ws.sensor[0].wtemp_c,1);
    UDP.beginPacket(UDP.remoteIP(), UDP.remotePort());
    //UDP.write(reply);
    //UDP.write(&ws.sensor[0].wtemp_c,sizeof(ws.sensor[0].wtemp_c));
    UDP.print(ws.sensor[0].w.temp_c);
    UDP.print(";");
    UDP.print(ws.sensor[0].w.humidity);
    UDP.print(";");
    UDP.print(ws.sensor[0].w.wind_direction_deg);
    UDP.print(";");
    UDP.print(ws.sensor[0].w.wind_avg_meter_sec);
    UDP.print(";");
    UDP.print(ws.sensor[0].w.wind_gust_meter_sec);
    UDP.print(";");
    UDP.print(ws.sensor[0].w.rain_mm);
    UDP.print(";");
    UDP.print(ws.sensor[0].rssi);
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
    int decode_ok;

    // Clear all sensor data
    ws.clearSlots();

    // Try to receive data from at least one sensor,
    // finish even if data is incomplete.
    // (Data can be distributed across multiple radio messages.)
   // decode_ok = ws.getData(TIMEOUT);
   
    decode_ok = ws.getMessage();
   

    
    if (decode_ok == DECODE_OK) {
      
      lastDecoded=millis();
      if (Verbose_level>0) { print_data(); }

      temperature=ws.sensor[0].w.temp_c;
      humidity=ws.sensor[0].w.humidity;
      wind_dir=ws.sensor[0].w.wind_direction_deg;
      wind_avr=ws.sensor[0].w.wind_avg_meter_sec;
      wind_gust=ws.sensor[0].w.wind_gust_meter_sec;
      raindrop=ws.sensor[0].w.rain_mm;
      rssi=ws.sensor[0].rssi;
      
      add_val_avr();

     // send_UDP_value(wind_dir);
   
    }
    else {
          if (Verbose_level>4) { Serial.printf("Sensor timeout\n");}
         }

  if ((millis()-last_integr) > INT_TIME) {
      prep_avr_val();
     }

    // Try to receive data from at least one sensor,
    // finish only if data is complete.
    // (Data can be distributed across multiple radio messages.)
    //ws.clearSlots();
    //decode_ok = ws.getData(TIMEOUT, DATA_COMPLETE);
    
    
    int packetSize = UDP.parsePacket();
    if (packetSize)
  {
    // receive incoming UDP packets
    if (Verbose_level>2) {Serial.printf("Received %d bytes from %s, port %d\n", packetSize, UDP.remoteIP().toString().c_str(), UDP.remotePort());}
    int len = UDP.read(incomingPacket, 255);
    if (len > 0)
    {
      incomingPacket[len] = 0;
      incomingPacket[len-1] = 0;
    }
    if (Verbose_level>2) {Serial.printf("UDP packet contents: %s\n", incomingPacket);}
     UDP.beginPacket(UDP.remoteIP(), UDP.remotePort());
     last_time=float(int((millis()-lastDecoded)/1000));
     if (String(incomingPacket) == "temp") {UDP.print(ws.sensor[0].w.temp_c);}
     if (String(incomingPacket) == "hum") {UDP.print(ws.sensor[0].w.humidity);}
     if (String(incomingPacket) == "win_dir") {UDP.print(ws.sensor[0].w.wind_direction_deg);}
     if (String(incomingPacket) == "win_avr") {UDP.print(ws.sensor[0].w.wind_avg_meter_sec);}
     if (String(incomingPacket) == "win_gus") {UDP.print(ws.sensor[0].w.wind_gust_meter_sec);}
     if (String(incomingPacket) == "rain") {UDP.print(ws.sensor[0].w.rain_mm);}
     if (String(incomingPacket) == "rssi") {UDP.print(ws.sensor[0].rssi);}
     if (String(incomingPacket) == "last") {UDP.print(last_time);}

     if (String(incomingPacket) == "atemp") {UDP.print(avr_temperature);}
     if (String(incomingPacket) == "ahum") {UDP.print(avr_humidity);}
     if (String(incomingPacket) == "awin_dir") {UDP.print(avr_wind_dir);}
     if (String(incomingPacket) == "awin_avr") {UDP.print(avr_wind_avr);}
     if (String(incomingPacket) == "awin_gus") {UDP.print(avr_wind_gust);}
     if (String(incomingPacket) == "atime") {UDP.print(millis()-last_integr);}
     if (String(incomingPacket) == "anum") {UDP.print(avr_number);}
     if (String(incomingPacket) == "apack") {UDP.print(packet_no);}
     if (String(incomingPacket) == "win_qual") {UDP.print(avr_wind_consistency);}

     UDP.print('\n');
     UDP.endPacket();
    
    // send back a reply, to the IP address and port we got the packet from
    //send_UDP();
  }

 

 if ((millis() - lastTime) > timerDelay && decode_ok == DECODE_OK) {
          // Send Events to the Web Server with the Sensor Readings
    events.send("ping",NULL,millis());

    if (avr_val_www) {
        events.send(String(avr_temperature,1).c_str(),"temperature",millis());
        events.send(String(avr_humidity,0).c_str(),"humidity",millis());
        events.send(String(avr_wind_dir,0).c_str(),"wind_dir",millis());
        events.send(String(avr_wind_avr,1).c_str(),"wind_avr",millis());
        events.send(String(avr_wind_gust,1).c_str(),"wind_gust",millis());
    }
    else
    {
        events.send(String(temperature,1).c_str(),"temperature",millis());
        events.send(String(humidity,0).c_str(),"humidity",millis());
        events.send(String(wind_dir,0).c_str(),"wind_dir",millis());
        events.send(String(wind_avr,1).c_str(),"wind_avr",millis());
        events.send(String(wind_gust,1).c_str(),"wind_gust",millis());
    }
    events.send(String(raindrop,1).c_str(),"raindrop",millis());
    events.send(String(rssi,0).c_str(),"rssi",millis());
    
    lastTime = millis();
    }


     delay(100);

    
   
  
} // loop()
