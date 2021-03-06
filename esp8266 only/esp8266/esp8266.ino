#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <FS.h>
#include <LittleFS.h>
#include <NTPClient.h>
#include <strings_en.h>
#include <WiFiManager.h>
#include <WiFiUdp.h>

#define ARDUINOJSON_USE_LONG_LONG 1
#include <ArduinoJson.h>

/*
Hi!
Faced the same problem, but analysis of the code showed that if the WEBSERVER_H directive is declared, then HTTP_GET is ignored ...
Therefore, to solve the problem, you need to do the following in your code:

#include "WiFiManager.h"
WiFiManager wifiManager;

#define WEBSERVER_H
#include "ESPAsyncWebServer.h"

--https://github.com/me-no-dev/ESPAsyncWebServer/issues/418
*/

#define WEBSERVER_H 
#include <ESPAsyncWebServer.h>

// WLAN VARIABLES

const char* configAPSSID = "ESP8266";
const char* configAPPass = "wificonfig"; // LENGTH MUST BE >= 8 CHARACTERS
const char* wmMessage = "Connect to ESP8266 and open 192.168.4.1 to access WiFi Manager (150s timeout)";
WiFiManager wifiManager;

// SERVER VARIABLES

const char* indexUser = "admin";
const char* indexPass = "admin";

char mainjs[24];
char jsonURLS[768];

unsigned long lastEventSend = 0;
unsigned long eventNotifyInterval = 2000;

AsyncWebServer server(80);
AsyncEventSource pinEvents("/readpinevent");

// TIME VARIABLES

boolean timeSynced = false;
unsigned int offset = 25200;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "0.id.pool.ntp.org", offset, 60000); // udp, ntp pool, offset, update interval (ms)
unsigned long lastUptimeUpdate = 0;
unsigned long uptime = 0;

// SCHEDULER VARIABLES

unsigned long lastSchedule = 0;
DynamicJsonDocument schedule(3072);
JsonArray scheduleOneTime;
JsonArray scheduleRepeating;

// PIN VARIABLES

byte pinCount = 4;
byte pinList[] = {4, 5, 12, 13}; // D2, D1, D6, D7
unsigned long timeArray[] = {-1, -1, -1, -1};
unsigned long onTimeArray[] = {0, 0, 0, 0};

// TRIGGER VARIABLES

boolean espRestartTrigger = false;
boolean startWifiManagerTrigger = false;
boolean wlanResetTrigger = false;

// PIN METHODS

void initPin(){
  for(int i = 0; i < pinCount; i++){
    pinMode(pinList[i], OUTPUT);
  }
}

String pinState(){
  StaticJsonDocument<192> doc;
  String temp = "";

  doc["success"] = true;
  JsonObject state = doc.createNestedObject("state");
  
  for(int i = 0; i < pinCount; i++){
    state[String(pinList[i])] = digitalRead(pinList[i]);
  }

  serializeJson(doc, temp);

  return temp;
}

boolean pinValid(byte pin){
  boolean isValid = false;

  for(byte i = 0; i < pinCount; i++){
    if(pin == pinList[i]){
      isValid = true;
      break;
    }
  }
  
  return isValid;
}

void setHigh(){
  for(byte i = 0; i < pinCount; i++){
    timeArray[i] = -1;
    onTimeArray[i] = 0;
    digitalWrite(pinList[i], HIGH);
  }
  eventPinState();
  Serial.println("SET HIGH");
}

void setLow(){
  for(byte i = 0; i < pinCount; i++){
    timeArray[i] = -1;
    onTimeArray[i] = 0;
    digitalWrite(pinList[i], LOW);
  }
  eventPinState();
  Serial.println("SET LOW");
}

void setPin(byte pin, byte value){
  for(byte i = 0; i < pinCount; i++){
    if(pin == pinList[i]){
      timeArray[i] = -1;
      onTimeArray[i] = 0;
      break;
    }
  }

  if(value == 0){
    digitalWrite(pin, LOW);
    Serial.println("SET PIN " + String(pin) + " LOW");
    eventPinState();
  }else if(value == 1){
    digitalWrite(pin, HIGH);
    Serial.println("SET PIN " + String(pin) + " HIGH");
    eventPinState();
  }
}

void setPinTimed(byte pin, unsigned long duration){
  for(byte i = 0; i < pinCount; i++){
    if(pinList[i] == pin){
      timeArray[i] = duration;
      Serial.println("SET PIN " + String(pin) + " HIGH FOR " + String(duration) + "ms");
      break;
    }
  }
}

void timedPin(){
  for(int i = 0; i < pinCount; i++){
    if(timeArray[i] != -1 && onTimeArray[i] == 0){
      onTimeArray[i] = millis();

      digitalWrite(pinList[i], HIGH);
      Serial.println("SET PIN " + String(pinList[i]) + " HIGH");
      eventPinState();
    }else{
      if(millis() - onTimeArray[i] >= timeArray[i]){
        timeArray[i] = -1;
        onTimeArray[i] = 0;
        
        digitalWrite(pinList[i], LOW);
        Serial.println("SET PIN " + String(pinList[i]) + " LOW");
        eventPinState();
      }
    }
  }
}

// METHODS

void espRestart(){
  espRestartTrigger = false;
  
  Serial.println("Restarting in 5s...");
  pinEvents.close();
  server.end();

  delay(5000);
  ESP.restart();
}

void eventNotify(){
  if(millis() - lastEventSend >= eventNotifyInterval){
    pinEvents.send("ONLINE", "notify");
    lastEventSend = millis();
  }
}

void eventPinState(){
  pinEvents.send(pinState().c_str(), "pinstate");
}

void garbageCollector(JsonDocument &data){
  String tempJson = "";
  serializeJson(data, tempJson);
  deserializeJson(data, tempJson);
}

String generateJSON(boolean success, String msgKey, String msgValue){
  StaticJsonDocument<256> doc;
  String result = "";

  doc["success"] = success;
  doc[msgKey] = msgValue;

  serializeJson(doc, result);

  return result;
}

String generateRandomString(int len){
  String randString = "";

  for(int i = 1; i <= len; i++){
    byte randomValue = random(0, 35);
    char letter = randomValue + 'a';
    
    if(randomValue >= 26){
      letter = (randomValue - 26) + '0';
    }
    
    randString += letter;
  }

  randomSeed(micros());

  return randString;
}

String generateUniqueID(){
  String randString = "";
  boolean duplicate = false;

  do{
    randString = generateRandomString(10);
    
    for (JsonVariant value : scheduleOneTime) {
      if(value["id"] == randString){
        duplicate = true;
        break;
      }

      if(duplicate){
        break;
      }
    }

    if(duplicate){
      continue;
    }

    for (JsonVariant value : scheduleRepeating) {
      if(value["id"] == randString){
        duplicate = true;
        break;
      }

      if(duplicate){
        break;
      }
    }
  }while(duplicate);

  return randString;
}

unsigned long getEpochTime(){
  return timeClient.getEpochTime() - offset;
}

String readFile(fs::FS &fs, const char* path){
  File file = fs.open(path, "r");
  
  if(!file || file.isDirectory()){
    return String();
  }

  String result;
  
  while(file.available()){
    result += (char) file.read();
  }
  
  return result;
}

void saveSchedule(){
  String temp = "";
  serializeJson(schedule, temp);
  
  if(writeFile(LittleFS, "/schedule.txt", temp.c_str())){
    Serial.println("SAVED");
  }
}

void scheduler(){
  if(timeClient.isTimeSet()){
    if(millis() - lastSchedule >= 250){
      boolean deleted = false;
      lastSchedule = millis();

      for(JsonArray::iterator it = scheduleOneTime.begin(); it != scheduleOneTime.end(); ++it){
        unsigned int pin = (*it)["pin"];
        unsigned long startTimestamp = (*it)["startTimestamp"];
        unsigned long endTimestamp = (*it)["endTimestamp"];
        boolean started = (*it)["started"];

        if(startTimestamp <= getEpochTime() && endTimestamp > getEpochTime() && !started){
          unsigned long duration = (endTimestamp - startTimestamp)*1000;
          setPinTimed(pin, duration);
          (*it)["started"] = true;
        }

        if(endTimestamp < getEpochTime()){
          scheduleOneTime.remove(it);
          deleted = true;
        }
      }
      
      for(JsonArray::iterator it = scheduleRepeating.begin(); it != scheduleRepeating.end(); ++it){
        unsigned int pin = (*it)["pin"];
        unsigned int hour = (*it)["hour"];
        unsigned int minute = (*it)["minute"];
        unsigned int duration = (*it)["duration"];
        unsigned long lastTrigger = (*it)["lastTrigger"];

        if(timeClient.getHours() == hour && timeClient.getMinutes() == minute && (lastTrigger + 86400 <= getEpochTime() || lastTrigger == 0)){
          (*it)["lastTrigger"] = getEpochTime();
          setPinTimed(pin, duration*1000);
        }
      }

      if(deleted){
        garbageCollector(schedule);
        saveSchedule();
      }
    }
  }
}

void startWifiManager(){
  startWifiManagerTrigger = false;
  pinEvents.close();
  server.end();

  Serial.println("Stopping server and starting WiFi Manager");
  Serial.println(wmMessage);
  Serial.println("Device will restart after config success");

  WiFi.disconnect(true);

  while(WiFi.status() != WL_CONNECTED){
    wifiManager.startConfigPortal(configAPSSID, configAPPass);
    
    if(WiFi.status() != WL_CONNECTED){
      Serial.println("Failed Connecting");
      Serial.println("Restarting WiFi Manager");
    }
  }

  Serial.print("Connected to ");
  Serial.println(WiFi.SSID());
  Serial.print("IP Address ");
  Serial.println(WiFi.localIP());
  
  espRestart();
}

void uptimeLoop(){
  if(millis() - lastUptimeUpdate >= 1000){
    uptime++;
    lastUptimeUpdate = millis();
  }
}

void wlanReset(){
  wlanResetTrigger = false;
  
  wifiManager.resetSettings();
  wifiManager.resetSettings();
  wifiManager.resetSettings();
  Serial.println("Resetting WLAN Config");
  espRestart();
}

boolean writeFile(fs::FS &fs, const char* path, const char* message){
  File file = fs.open(path, "w");
  
  if(!file){
    return false;
  }
  
  if(file.print(message)){
    return true;
  }else{
    return false;
  }
}

// SERVER METHODS

String getContentType(String filename){
  if(filename.endsWith(".gzp")){
    filename = filename.substring(0, filename.length() - 4);
  }
  
  if(filename.endsWith(".html")){
    return "text/html";
  }else if(filename.endsWith(".css")){
    return "text/css";
  }else if(filename.endsWith(".jpg")){
    return "image/jpeg";
  }else if(filename.endsWith(".png")){
    return "image/png";
  }else if(filename.endsWith(".js")){
    return "application/javascript";
  }
  
  return "text/plain";
}

String templateProcessor(const String& var){
  if(var == "URLS"){
    return String(jsonURLS);
  }else if(var == "PINSTATE"){
    return pinState();
  }else if(var == "SCHEDULE"){
    String temp = "";
    serializeJson(schedule, temp);
    return temp;
  }else if(var == "UPTIME"){
    return generateJSON(true, "message", String(uptime));
  }else if(var == "MAIN_JS"){
    return String(mainjs);
  }
    
  return String();
}

void indexPage(AsyncWebServerRequest *request){
  if(!request->authenticate(indexUser, indexPass)){
    return request->requestAuthentication();
  }
  
  request->send(LittleFS, "/index.html", "text/html", false, templateProcessor);
}

void logout(AsyncWebServerRequest *request){
  AsyncWebServerResponse* response = request->beginResponse(LittleFS, "/logout.html", "text/html");
  response->setCode(401);
  request->send(response);
}

void mainJSHandler(AsyncWebServerRequest *request){
  AsyncWebServerResponse* response = request->beginResponse(LittleFS, "/main.js.gzp", "application/javascript");
  response->addHeader("Cache-Control", "max-age=86400");
  response->addHeader("Content-Encoding", "gzip");
  
  request->send(response);
}

void setPinHandler(AsyncWebServerRequest *request){
  String msg = "";
  
  if(request -> hasParam("pin", true) && request -> hasParam("state", true)){
    unsigned int pin = request->getParam("pin", true)->value().toInt();
    boolean state = request->getParam("state", true)->value().toInt(); 
    
    if(pinValid(pin) && (state == 1 || state == 0)){
      setPin(pin, state);
      msg = generateJSON(true, "message", "OK");
    }else{
      msg = generateJSON(false, "message", "PIN OUT OF RANGE (2 - 11) OR STATE WRONG (0 OR 1)");
    }
  }else{
    msg = generateJSON(false, "message", "PARAM INVALID");
  }

  request -> send(200, "application/json", msg);
}

void setPinTimedHandler(AsyncWebServerRequest *request){
  String msg = "";
  
  if(request -> hasParam("pin", true) && request -> hasParam("duration", true)){
    unsigned int pin = request->getParam("pin", true)->value().toInt();
    unsigned long duration = strtoul(request->getParam("duration", true)->value().c_str(), NULL, 10);

    if(pinValid(pin) && duration > 0 && duration <= 604800000){
      setPinTimed(pin, duration);
      msg = generateJSON(true, "message", "OK");
    }else{
      msg = generateJSON(false, "message", "PIN OUT OF RANGE (2 - 11) OR DURATION OUT OF RANGE (1 - 604800000 ms)");
    }
  }else{
    msg = generateJSON(false, "message", "PARAM INVALID");
  }

  request -> send(200, "application/json", msg);
}

void readPinHandler(AsyncWebServerRequest *request){
  String msg = "";
  
  if(request -> hasParam("sseonly", true)){
    eventPinState();
    lastEventSend = millis();
    msg = generateJSON(true, "message", "OK");
  }else if(request -> hasParam("jsononly", true)){
    msg = pinState();
  }else{
    eventPinState();
    lastEventSend = millis();
    msg = pinState();
  }
 
  request -> send(200, "application/json", msg);
}

void setLowHandler(AsyncWebServerRequest *request){
  setLow();
  request -> send(200, "application/json", generateJSON(true, "message", "OK"));
}

void setHighHandler(AsyncWebServerRequest *request){
  setHigh();
  request -> send(200, "application/json", generateJSON(true, "message", "OK"));
}

void addSchedule(AsyncWebServerRequest *request){
  String msg = "";

  if(timeClient.isTimeSet()){
     if(request -> hasParam("type", true) && request -> hasParam("pin", true)){
      String type = request->getParam("type", true)->value();
      unsigned int pin = request->getParam("pin", true)->value().toInt(); 
  
      if(pinValid(pin) && (type == "onetime" || type == "repeating")){
        if(type == "onetime" && request -> hasParam("startTimestamp", true) && request -> hasParam("endTimestamp", true)){
          if(scheduleOneTime.size() < 10){
            unsigned long startTimestamp = strtoul(request->getParam("startTimestamp", true)->value().c_str(), NULL, 10);
            unsigned long endTimestamp = strtoul(request->getParam("endTimestamp", true)->value().c_str(), NULL, 10);
    
            if(startTimestamp > getEpochTime() && endTimestamp > startTimestamp){
              JsonObject schedObj = scheduleOneTime.createNestedObject();
              schedObj["id"] = generateUniqueID();
              schedObj["startTimestamp"] = startTimestamp;
              schedObj["endTimestamp"] = endTimestamp;
              schedObj["pin"] = pin;
              schedObj["started"] = false;
      
              msg = generateJSON(true, "message", "OK");
              saveSchedule();
            }else{
              msg = generateJSON(false, "message", "TIME INVALID OR BEFORE NOW");
            }
          }else{
            msg = generateJSON(false, "message", "ONE TIME SCHEDULER FULL");
          }        
        }else if(type == "repeating" && request -> hasParam("hour", true) && request -> hasParam("minute", true) && request -> hasParam("duration", true)){
          if(scheduleRepeating.size() < 10){
            unsigned int hour = request->getParam("hour", true)->value().toInt();
            unsigned int minute = request->getParam("minute", true)->value().toInt();
            unsigned int duration = request->getParam("duration", true)->value().toInt();
    
            if(hour <= 23 && minute <= 59 && duration >= 1 && duration <= 86400){
              JsonObject schedObj = scheduleRepeating.createNestedObject();
              schedObj["id"] = generateUniqueID();
              schedObj["hour"] = hour;
              schedObj["minute"] = minute;
              schedObj["pin"] = pin;
              schedObj["duration"] = duration;
              schedObj["lastTrigger"] = 0;
      
              msg = generateJSON(true, "message", "OK");
              saveSchedule();
            }else{
              msg = generateJSON(false, "message", "TIME INVALID (00:00 - 23:59) OR DURATION TOO LONG (<= 86400 seconds)");
            }
          }else{
            msg = generateJSON(false, "message", "REPEATING SCHEDULER FULL");
          }
        }else{
          msg = generateJSON(false, "message", "PARAM INVALID");
        }
      }else{
        msg = generateJSON(false, "message", "TYPE WRONG OR PIN OUT OF RANGE (2 - 11)");
      }
    }else{
      msg = generateJSON(false, "message", "PARAM INVALID");
    }
  }else{
    msg = generateJSON(false, "message", "TIME NOT SET. PLEASE CHECK YOUR INTERNET CONNECTION");
  } 
  
  request -> send(200, "application/json", msg);
}

void deleteSchedule(AsyncWebServerRequest *request){
  String msg = "";

  if(request -> hasParam("id", true)){
    boolean deleted = false;
    String id = request->getParam("id", true)->value();

    for (JsonArray::iterator it = scheduleOneTime.begin(); it != scheduleOneTime.end(); ++it) {
      if((*it)["id"] == id) {
         scheduleOneTime.remove(it);
         deleted = true;
         break;
      }
    }
    
    for (JsonArray::iterator it = scheduleRepeating.begin(); it != scheduleRepeating.end(); ++it) {
      if((*it)["id"] == id) {
         scheduleRepeating.remove(it);
         deleted = true;
         break;
      }
    }

    if(deleted){
      msg = generateJSON(true, "message", "OK");
      garbageCollector(schedule);
      saveSchedule();
    }else{
      msg = generateJSON(false, "message", "DATA NOT EXIST");
    }
  }else{
    msg = generateJSON(false, "message", "PARAM INVALID");
  }
  
  request -> send(200, "application/json", msg);
}

void getSchedule(AsyncWebServerRequest *request){
  String result = "";
  serializeJson(schedule, result);
  request -> send(200, "application/json", result);
}

void espRestartWeb(AsyncWebServerRequest *request){
  request -> send(200, "application/json", generateJSON(true, "message", "Restarting in 5s..."));
  espRestartTrigger = true;
}

void getUptime(AsyncWebServerRequest *request){
  request -> send(200, "application/json", generateJSON(true, "message", String(uptime)));
}

void getUrls(AsyncWebServerRequest *request){
  if(!request->authenticate(indexUser, indexPass)){
    return request->requestAuthentication();
  }
  
  request -> send(200, "application/json", jsonURLS);
}

void startWM(AsyncWebServerRequest *request){
  request -> send(200, "application/json", generateJSON(true, "message", String(wmMessage) + ". Device will restart after config success"));
  startWifiManagerTrigger = true;
}

void wlanResetWeb(AsyncWebServerRequest *request){
  request -> send(200, "application/json", generateJSON(true, "message", "Resetting WLAN Config and restarting in 5s. " + String(wmMessage)));
  wlanResetTrigger = true;
}

void notFound(AsyncWebServerRequest *request){
  if(request->url() == "/main.js"){
     request->send(404);
  }else{
    AsyncWebServerResponse* response = request->beginResponse(LittleFS, request->url(), getContentType(request->url()), false);
  
    if(response == NULL){
      request->send(404);
    }else{
      response->addHeader("Cache-Control", "max-age=86400");
      
      if(request->url().endsWith(".gzp")){
        response->addHeader("Content-Encoding", "gzip");
      }
      
      request->send(response);
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(25);

  initPin();

  delay(5000);

  WiFi.setOutputPower(19);
  wifiManager.setAPClientCheck(true);         //  WM timeout stop when client connected
  wifiManager.setBreakAfterConfig(true);      //  break after saving, otherwise WM keep running if failed connecting
  wifiManager.setCaptivePortalEnable(false);  
  wifiManager.setClass("invert");           
  wifiManager.setConfigPortalTimeout(150);    
  wifiManager.setConnectTimeout(15);          
  wifiManager.setDebugOutput(false);           
  wifiManager.setEnableConfigPortal(false);   //  show config portal after autoconnect fail (only autoconnect)
  wifiManager.setHostname("esp8266");        
  wifiManager.setScanDispPerc(true);      
  wifiManager.setShowDnsFields(true);  
  wifiManager.setShowStaticFields(true);
  wifiManager.setWiFiAPChannel(1);

  Serial.println();
  
  if(wifiManager.getWiFiIsSaved()){
    Serial.print("Connecting to ");
    Serial.println(wifiManager.getWiFiSSID());
  }else{
    Serial.println(wmMessage);
  }

  while(WiFi.status() != WL_CONNECTED){
    wifiManager.autoConnect(configAPSSID, configAPPass);

    if(WiFi.status() != WL_CONNECTED){
      Serial.println("Failed Connecting");
      Serial.println(wmMessage);
      
      wifiManager.startConfigPortal(configAPSSID, configAPPass);

      if(WiFi.status() != WL_CONNECTED){
        Serial.println("Failed Connecting");
        Serial.println("Trying to connect to " + wifiManager.getWiFiSSID() + " again");
      }
    }
  }
  
  Serial.print("Connected to ");
  Serial.println(WiFi.SSID());
  Serial.print("IP Address ");
  Serial.println(WiFi.localIP());

  schedule["available"] = false;
  scheduleOneTime = schedule.createNestedArray("onetime");
  scheduleRepeating = schedule.createNestedArray("repeating");
 
  timeClient.begin();
  
  if(!timeClient.update()){
    timeClient.forceUpdate();
  }

  if(timeClient.isTimeSet()){
    timeSynced = true;
    schedule["available"] = true;
    Serial.println("NTP synced");
  }else{
    Serial.println("NTP not synced");
    Serial.println("Schedule not available until NTP synced");
    Serial.println("Please check your internet connection");
  }

  for(byte i = 1; i <= 10; i++){
    randomSeed(micros() + getEpochTime() + random(1, 1000*i));
  }

  byte urlsCount = 14;
  byte urlsLength = 15;

  String urls[urlsCount];

  for(int i = 0; i < urlsCount; i++){
    String trueRandom = "";
    boolean duplicate = false;
    
    do{
      trueRandom = generateRandomString(urlsLength);

      for(int j = 0; j < i; j++){
        if(trueRandom == urls[j]){
          duplicate = true;
          break;
        }
      }
    }while(duplicate);
    
    urls[i] = "/" + trueRandom;
  }

  StaticJsonDocument<768> doc;

  doc["success"] = true;
  
  JsonObject urlsObject = doc.createNestedObject("urls");
  urlsObject["setpin"]          = urls[0];
  urlsObject["setpintimed"]     = urls[1];
  urlsObject["readpin"]         = urls[2];
  urlsObject["readpinevent"]    = urls[3];              
  urlsObject["setlow"]          = urls[4];
  urlsObject["sethigh"]         = urls[5];
  urlsObject["addschedule"]     = urls[6];
  urlsObject["deleteschedule"]  = urls[7];
  urlsObject["getschedule"]     = urls[8];
  urlsObject["esprestart"]      = urls[9];
  urlsObject["getuptime"]       = urls[10];
  urlsObject["startwm"]         = urls[11];
  urlsObject["wlanreset"]       = urls[12];
  urlsObject["mainjs"]          = urls[13];

  serializeJson(doc, jsonURLS);

  urls[13].toCharArray(mainjs, 24);

  AsyncEventSource pinEventsTemp(urls[3].c_str());
  pinEvents = pinEventsTemp;

  server.addHandler(&pinEvents);

  server.on("/", HTTP_GET, indexPage);
  server.on("/index", HTTP_GET, indexPage);
  server.on("/index.html", HTTP_GET, indexPage);
  server.on("/logout", HTTP_GET, logout);
  server.on("/logout.html", HTTP_GET, logout);
  server.on(urls[13].c_str(), HTTP_GET, mainJSHandler);
 
  server.on(urls[0].c_str(), HTTP_POST, setPinHandler);
  server.on(urls[1].c_str(), HTTP_POST, setPinTimedHandler);
  server.on(urls[2].c_str(), HTTP_POST, readPinHandler);
  server.on(urls[4].c_str(), HTTP_POST, setLowHandler);
  server.on(urls[5].c_str(), HTTP_POST, setHighHandler);
  
  server.on(urls[6].c_str(), HTTP_POST, addSchedule);
  server.on(urls[7].c_str(), HTTP_POST, deleteSchedule);
  server.on(urls[8].c_str(), HTTP_POST, getSchedule);

  server.on(urls[9].c_str(), HTTP_GET, espRestartWeb);
  server.on(urls[10].c_str(), HTTP_GET, getUptime);
  server.on(urls[11].c_str(), HTTP_GET, startWM);
  server.on(urls[12].c_str(), HTTP_GET, wlanResetWeb);

//  server.on("/setpin", HTTP_POST, setPin);
//  server.on("/setpintimed", HTTP_POST, setPinTimed);
//  server.on("/readpin", HTTP_POST, readPin);
//  server.on("/setlow", HTTP_POST, setLow);
//  server.on("/sethigh", HTTP_POST, setHigh);
//  
//  server.on("/addschedule", HTTP_POST, addSchedule);
//  server.on("/deleteschedule", HTTP_POST, deleteSchedule);
//  server.on("/getschedule", HTTP_POST, getSchedule);
//
//  server.on("/esprestart", HTTP_GET, espRestartWeb);
//  server.on("/getuptime", HTTP_GET, getUptime);
//  server.on("/startwm", HTTP_GET, startWM);
//  server.on("/wlanreset", HTTP_GET, wlanResetWeb);

  server.on("/geturls", HTTP_GET, getUrls);
  
  server.onNotFound(notFound);

  server.begin();
  
  if(LittleFS.begin()){
    FSInfo fs_info;
    LittleFS.info(fs_info);
    
    Serial.println("LittleFS Mount Success");
    Serial.print("Space Used : ");
    Serial.println(fs_info.usedBytes);
  }else{
    Serial.println("LittleFS Mount Fail");
  }

  String jsonFile = readFile(LittleFS, "/schedule.txt");

  if(jsonFile != ""){
    deserializeJson(schedule, jsonFile);
  }

  Serial.print("Free Heap : ");
  Serial.println(ESP.getFreeHeap(), DEC);

  Serial.println("ESP READY");

  uptime = millis()/1000 + 1;
  lastUptimeUpdate = millis();
}

void loop() {
  eventNotify();
  scheduler();
  timedPin();
  uptimeLoop();
  
  timeClient.update();

  if(!timeSynced && timeClient.isTimeSet()){
    timeSynced = true;
    schedule["available"] = true;
    Serial.println("NTP synced");
  }

 if(Serial.available() > 0){
    String input = Serial.readStringUntil('\n');
    
    input.trim();
    input.toLowerCase();

    String tokens[8];

    char* token = strtok((char*) input.c_str(), " ");
    byte tokenCount = 0;

    while(token != NULL && tokenCount < 8){
      tokens[tokenCount++] = token;
      token = strtok(NULL, " ");
    }

    if(tokens[0].equals("esprestart") && tokenCount == 1){
      espRestartTrigger = true;
    }else if(tokens[0].equals("wlanreset") && tokenCount == 1){
      wlanResetTrigger = true;
    }else if(tokens[0].equals("startwm") && tokenCount == 1){
      startWifiManagerTrigger = true;
    }else if(tokens[0].equals("set") && tokenCount == 3){
      byte pin = tokens[1].toInt();
      boolean state = tokens[2].toInt();

      if(pinValid(pin) && (state == 0 || state == 1)){
        if(timeArray[pin - 2] != -1){
          timeArray[pin - 2] = -1;
          onTimeArray[pin - 2] = 0;
        }
        
        setPin(pin, state);
      }else{
        Serial.println("PARAM OUT OF RANGE");
      }
    }else if(tokens[0].equals("settimed") && tokenCount == 3){
      byte pin = tokens[1].toInt();
      unsigned long duration = tokens[2].toInt();

      if(pinValid(pin) && duration > 0 && duration <= 604800000){ 
        setPinTimed(pin, duration);
      }else{
        Serial.println("PARAM OUT OF RANGE");
      }
    }else if(tokens[0].equals("read") && tokenCount == 1){
      Serial.println(pinState());
    }else if(tokens[0].equals("high") && tokenCount == 1){
      setHigh();
    }else if(tokens[0].equals("low") && tokenCount == 1){
      setLow();
    }else{
      Serial.println("INVALID_COMMAND");
    }
  }
  
  if(espRestartTrigger){
    espRestart();
  }else if(wlanResetTrigger){
    wlanReset();
  }else if(startWifiManagerTrigger){
    startWifiManager();
  } 
  
  if(WiFi.status() != WL_CONNECTED){
    WiFi.reconnect();
    
    Serial.println("WiFi Disconnected");
    Serial.print("Reconnecting");

    while(WiFi.status() != WL_CONNECTED){
      delay(1000);
      Serial.print(".");
    }

    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(WiFi.SSID());
    Serial.print("IP Address ");
    Serial.println(WiFi.localIP());

    if(!timeClient.update()){
      timeClient.forceUpdate();
    }

    if(timeClient.isTimeSet()){
      Serial.println("NTP synced");
    }else{
      Serial.println("NTP not synced");
      Serial.println("Schedule not available until NTP synced");
      Serial.println("Check for internet connection");
    }

    uptime = millis()/1000 + 1;
    lastUptimeUpdate = millis();
  }
}
