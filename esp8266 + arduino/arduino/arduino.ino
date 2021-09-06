#include <ArduinoJson.h>

int espOffDelay = 250;
int espOnDelay = 3000;
int espDetectInterval = 60000;

boolean espOnline = false;
boolean isESP = false;
boolean isSerial = false;
boolean ledState = LOW;

unsigned long lastBlinkTime = 0;
unsigned long lastESP = 0;

byte pinCount = 10;
byte pinList[] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
unsigned long timeArray[] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
unsigned long onTimeArray[] {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

void initPin(){
  for(int i = 0; i < pinCount; i++){
    pinMode(pinList[i], OUTPUT);
  }
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

void espStatusLED(){
  unsigned short interval;
  
  if(espOnline){
    interval = espOnDelay;
  }else{
    interval = espOffDelay;
  }

  if(millis() - lastBlinkTime >= interval){
    lastBlinkTime = millis();
    
    if(ledState == LOW){
      ledState = HIGH;
    }else{
      ledState = LOW;
    }
    
    digitalWrite(LED_BUILTIN, ledState);
  }
}

void readPin(){
  StaticJsonDocument<192> doc;
  String temp = "";
  
  for(int i = 0; i < pinCount; i++){
    doc[String(pinList[i])] = digitalRead(pinList[i]);
  }

  serializeJson(doc, temp);
  Serial.println(temp);
}

void setHigh(){
  for(byte i = 0; i < pinCount; i++){
    timeArray[i] = -1;
    onTimeArray[i] = 0;
    digitalWrite(pinList[i], HIGH);
  }
}

void setLow(){
  for(byte i = 0; i < pinCount; i++){
    timeArray[i] = -1;
    onTimeArray[i] = 0;
    digitalWrite(pinList[i], LOW);
  }
}

void setPin(int pin, int value){
  for(byte i = 0; i < pinCount; i++){
    if(pin == pinList[i]){
      timeArray[i] = -1;
      onTimeArray[i] = 0;
      break;
    }
  }

  if(value == 0){
    digitalWrite(pin, LOW);
  }else if(value == 1){
    digitalWrite(pin, HIGH);
  }
}

void timedPin(){
  for(int i = 0; i < 10; i++){
    if(timeArray[i] != -1 && onTimeArray[i] == 0){
      onTimeArray[i] = millis();
      digitalWrite(pinList[i], HIGH);
    }else{
      if(millis() - onTimeArray[i] >= timeArray[i]){
        timeArray[i] = -1;
        onTimeArray[i] = 0;

        Serial.println("UPDATEPIN");
        
        digitalWrite(pinList[i], LOW);
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(25);

  pinMode(LED_BUILTIN, OUTPUT);

  initPin();
}

void loop() {
  espStatusLED();
  timedPin();

  if(Serial.available()){
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

    if(tokens[0].equals("set") && tokenCount == 3){
      lastESP = millis();
      
      byte pin = tokens[1].toInt();
      boolean state = tokens[2].toInt();

      if(pinValid(pin) && (state == 0 || state == 1)){
        setPin(pin, state);
      }
    }else if(tokens[0].equals("settimed") && tokenCount == 3){
      lastESP = millis();

      byte pin = tokens[1].toInt();
      unsigned long duration = tokens[2].toInt();

      if(pinValid(pin) && duration > 0 && duration <= 604800000){ 
        timeArray[pin - 2] = duration;
      }
    }else if(tokens[0].equals("read") && tokenCount == 1){
      lastESP = millis();
      readPin();
    }else if(tokens[0].equals("high") && tokenCount == 1){
      lastESP = millis();
      setHigh();
    }else if(tokens[0].equals("low") && tokenCount == 1){
      lastESP = millis();
      setLow();
    }else if(tokens[0].equals("esponline") && tokenCount == 1){
      lastESP = millis();
    }else if(tokens[0].equals("espoffline") && tokenCount == 1){
      lastESP = 0;
      lastBlinkTime = 0;
    }else if(tokens[0].equals("esp") && tokenCount == 1){
      isESP = true;
      isSerial = false;
    }else if(tokens[0].equals("serial") && tokenCount == 1){
      isESP = false;
      isSerial = true;
    }else{
      if(!isESP && isSerial){
        Serial.println("INVALID COMMAND");
      }
    }
  }

  if(millis() - lastESP >= espDetectInterval || lastESP == 0){
    espOnline = false;
  }else{
    espOnline = true;
  }
}
