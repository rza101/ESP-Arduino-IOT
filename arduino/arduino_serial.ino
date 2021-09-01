#include <ArduinoJson.h>

#define START_PIN           2
#define END_PIN             11
#define ESP_OFF_TIME        250
#define ESP_ON_TIME         3000
#define ESP_DETECT_INTERVAL 60000

boolean espOnline = false;
boolean ledState = LOW;
unsigned long lastBlinkTime = 0;
unsigned long lastESP = 0;

unsigned long timeArray[10] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
unsigned long onTimeArray[10] {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

void espStatusLED(){
  unsigned short interval;
  
  if(espOnline){
    interval = ESP_ON_TIME;
  }else{
    interval = ESP_OFF_TIME;
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
  
  for(int i = START_PIN; i <= END_PIN; i++){
    doc[String(i)] = digitalRead(i);
  }

  serializeJson(doc, Serial);
  Serial.flush();
}

void setHigh(){
  for(int i = START_PIN; i <= END_PIN; i++){
    digitalWrite(i, HIGH);
  }
}

void setLow(){
  for(int i = START_PIN; i <= END_PIN; i++){
    digitalWrite(i, LOW);
  }
}

void setPin(int pin, int value){
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
      digitalWrite(i+2, HIGH);
    }else{
      //in seconds
      if(millis() - onTimeArray[i] >= timeArray[i]){
        timeArray[i] = -1;
        onTimeArray[i] = 0;

        Serial.print("UPDATEPIN");
        Serial.flush();
        
        digitalWrite(i+2, LOW);
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(25);

  pinMode(LED_BUILTIN, OUTPUT);

  for(int i = START_PIN; i <= END_PIN; i++){
    pinMode(i, OUTPUT);
  }
}

void loop() {
  espStatusLED();
  timedPin();
  
  if(Serial.available()){
    String param = Serial.readStringUntil(' '); // terminate if found space OR until timeout
    
    if(param == "SET"){
      lastESP = millis();
      
      byte pin = Serial.parseInt();
      boolean state = Serial.parseInt();

      if(pin >= START_PIN && pin <= END_PIN && (state == 0 || state == 1)){
        if(timeArray[pin - 2] != -1){
          timeArray[pin - 2] = -1;
          onTimeArray[pin - 2] = 0;
        }
        
        setPin(pin, state);
      }
    }else if(param == "SETTIMED"){
      lastESP = millis();

      byte pin = Serial.parseInt();
      unsigned long duration = Serial.parseInt();

      if(pin >= START_PIN && pin <= END_PIN && duration > 0 && duration <= 604800000){ 
        timeArray[pin - 2] = duration;
      }
    }else if(param == "READ"){
      lastESP = millis();
      readPin();
    }else if(param == "HIGH"){
      lastESP = millis();
      setHigh();
    }else if(param == "LOW"){
      lastESP = millis();
      setLow();
    }else if(param == "ESPONLINE"){
      lastESP = millis();
    }else if(param == "ESPOFFLINE"){
      lastESP = 0;
      lastBlinkTime = 0;
    }
  }

  if(millis() - lastESP >= ESP_DETECT_INTERVAL || lastESP == 0){
    espOnline = false;
  }else{
    espOnline = true;
  }
}
