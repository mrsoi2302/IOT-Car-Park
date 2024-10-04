#include <ESP8266WiFi.h>
#include <Servo.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <TimeLib.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
const char *ssid =  "TuanNM";     // Enter your WiFi Name
const char *pass =  "0898552488"; // Enter your WiFi Password

#define MQTT_SERV "io.adafruit.com"
#define MQTT_PORT 1883
#define MQTT_NAME "dungnt2302"
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 7*3600, 60000);
Servo entryGateServo;                          //servo as gate
Servo exitGateServo;                               //servo as gate

int carExitSensor = 0; // D3
int carEnterSensor = 4; // D2 
int slot2Sensor = 5; // D1
int slot1Sensor  = 16; // D0
int count =0; 
int CLOSE_ANGLE = 70;  // The closing angle of the servo motor arm
int STOP_ANGLE = 90;
int OPEN_ANGLE = 100;  // The opening angle of the servo motor arm              
int  hh, mm, ss;
int pos;
int pos1;

int timeToOpen = 500;
int timeHold = 2500;
int timeToClose = 500;

String statusEntryGate = "close";
String statusExitGate = "close";


String h, m,EntryTimeSlot1,ExitTimeSlot1, EntryTimeSlot2,ExitTimeSlot2, EntryTimeSlot3,ExitTimeSlot3;
boolean entrysensor, exitsensor,s1,s2,s3;

boolean s1_occupied = false;
boolean s2_occupied = false;
boolean s3_occupied = false;
boolean automatic = true;

WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, MQTT_SERV, MQTT_PORT, MQTT_NAME, MQTT_PASS);

//Set up the feed you're subscribing to
Adafruit_MQTT_Subscribe EntryGate = Adafruit_MQTT_Subscribe(&mqtt, MQTT_NAME "/f/EntryGate");
Adafruit_MQTT_Subscribe ExitGate = Adafruit_MQTT_Subscribe(&mqtt, MQTT_NAME "/f/ExitGate");
Adafruit_MQTT_Subscribe AutomaticMode = Adafruit_MQTT_Subscribe(&mqtt, MQTT_NAME "/f/AutomaticMode");

//Set up the feed you're publishing to
Adafruit_MQTT_Publish CarsParked = Adafruit_MQTT_Publish(&mqtt,MQTT_NAME "/f/CarsParked");
Adafruit_MQTT_Publish Slot1 = Adafruit_MQTT_Publish(&mqtt,MQTT_NAME "/f/Slot1");
Adafruit_MQTT_Publish Slot2 = Adafruit_MQTT_Publish(&mqtt,MQTT_NAME "/f/Slot2");
Adafruit_MQTT_Publish EntryEvent = Adafruit_MQTT_Publish(&mqtt,MQTT_NAME "/f/EntryEvent");
Adafruit_MQTT_Publish ExitEvent = Adafruit_MQTT_Publish(&mqtt,MQTT_NAME "/f/ExitEvent");

void setup() {
  Serial.begin (9600);
  mqtt.subscribe(&AutomaticMode); 
  mqtt.subscribe(&EntryGate);
  mqtt.subscribe(&ExitGate);
  timeClient.begin(); 
  entryGateServo.attach(14);      // D5
  exitGateServo.attach(12);       // D6
  pinMode(carEnterSensor, INPUT);     // ir as input
  pinMode(slot1Sensor, INPUT);
  pinMode(slot2Sensor, INPUT);
  pinMode(carExitSensor,INPUT);
  WiFi.begin(ssid, pass);                                     //try to connect with wifi
  Serial.print("Connecting to ");
  Serial.print(ssid);                          // display ssid
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");                          // if not connected print this
    delay(500);
  }
  MQTT_connect();                                       
  Serial.println();
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP Address is : ");
  Serial.println(WiFi.localIP());
}
int x = 0;
void loop() {
  timeClient.update();
  unsigned long epochTime = timeClient.getEpochTime();
  //#region handle time
  time_t rawTime = epochTime;
  struct tm * ti;
  ti = localtime(&rawTime);
  String dateString =String(ti->tm_mday) + "/" + String(ti->tm_mon + 1) + "/" + String(ti->tm_year + 1900) + " - " + String(ti->tm_hour) + ":" + String(ti->tm_min) + ":" + String(ti->tm_sec);
  //#endregion
  exitsensor = !digitalRead(carExitSensor);
  if(automatic == true) {
    handleEntryGate(dateString, false);
    handleExitGate(dateString, false);
  }
  handleSlot();
  handleRequestFromServer(dateString);
  delay(100);
}

long timeOpen;
void handleEntryGate(String dateStr, boolean reqFromServer) {
  const char* dateTime = dateStr.c_str();
  long time = millis();
  entrysensor = !digitalRead(carEnterSensor);
  if ( strcmp(statusEntryGate.c_str(),"close") == 0 && (reqFromServer || entrysensor == 1)) {                     
    timeOpen = millis();
    Serial.println("Đang xoay");
    if(! EntryEvent.publish(dateTime)) {}
    entryGateServo.write(OPEN_ANGLE);
    statusEntryGate = "opening"; 
  }
  if ( strcmp(statusEntryGate.c_str(),"opening") == 0) {
    if(time - timeOpen >= 500) {
      entryGateServo.write(STOP_ANGLE);
      statusEntryGate = "open";
      timeOpen = millis();
    }
  }
  if ( strcmp(statusEntryGate.c_str(),"open") == 0 && entrysensor == 0) {
    if(time - timeOpen >= 2500) {
      entryGateServo.write(CLOSE_ANGLE);
      statusEntryGate = "closing";
      timeOpen = millis();
    }
  }
  if ( strcmp(statusEntryGate.c_str(),"closing") == 0) {
    if(time - timeOpen >= 500) {
      entryGateServo.write(STOP_ANGLE);
      statusEntryGate = "close";
      timeOpen = millis();
    }
  }
}

long timeClose;
void handleExitGate(String dateStr, boolean reqFromServer) {
  exitsensor= !digitalRead(carExitSensor);
  const char* dateTime = dateStr.c_str();
  long time = millis();
  if ( strcmp(statusExitGate.c_str(),"close") == 0 && (reqFromServer || exitsensor == 1 )) {                     
    timeClose = millis();
    if(! ExitEvent.publish(dateTime)) {}
    exitGateServo.write(OPEN_ANGLE);
    statusExitGate = "opening"; 
  }
  if ( strcmp(statusExitGate.c_str(),"opening") == 0) {
    if(time - timeClose >= 500) {
      exitGateServo.write(STOP_ANGLE);
      statusExitGate = "open";
      timeClose = millis();
    }
  }
  if ( strcmp(statusExitGate.c_str(),"open") == 0 && exitsensor == 0) {
    if(time - timeClose >= 2500) {
      exitGateServo.write(CLOSE_ANGLE);
      statusExitGate = "closing";
      timeClose = millis();
    }
  }
  if ( strcmp(statusExitGate.c_str(),"closing") == 0) {
    if(time - timeClose >= 500) {
      exitGateServo.write(STOP_ANGLE);
      statusExitGate = "close";
      timeClose = millis();
    }
  }
}

void handleSlot() {
  s1 = !digitalRead(slot1Sensor);
  s2 = !digitalRead(slot2Sensor);
  count = 0;
  //Handle Slot1
  if (s1 == 1 && s1_occupied == false) {                     
    Serial.println("Occupied1 ");
    EntryTimeSlot1 =  h +" :" + m;          
    s1_occupied = true;
    count = count + 1;
    if (! Slot1.publish(1)){}
  }
  if(s1 == 0 && s1_occupied == true) {
    Serial.println("Available1 ");
    ExitTimeSlot1 =  h +" :" + m;  
    s1_occupied = false;
    if (! Slot1.publish(0)){} 
  }
  //HandleSlot2
  if (s2 == 1&& s2_occupied == false) {                     
    Serial.println("Occupied2 ");
    EntryTimeSlot2 =  h +" :" + m; 
    s2_occupied = true;
    count = count + 1;
    if (! Slot2.publish(1)){}
  }
  if(s2 == 0 && s2_occupied == true) {
    Serial.println("Available2 ");
    ExitTimeSlot2 =  h +" :" + m;
    s2_occupied = false;
    if (! Slot2.publish(0)){}
  }
  // if (! CarsParked.publish(count)) {}
}

void handleRequestFromServer(String dateStr) {
  Adafruit_MQTT_Subscribe * subscription;
    while ((subscription = mqtt.readSubscription(1000))){
      if (subscription == &EntryGate){
        //Print the new value to the serial monitor
        Serial.println((char*) EntryGate.lastread);
        if (!strcmp((char*) EntryGate.lastread, "OPEN")){
          handleEntryGate(dateStr,true);
        }
      }
      else if (subscription == &ExitGate){
        //Print the new value to the serial monitor
        Serial.println((char*) ExitGate.lastread);
        if (!strcmp((char*) ExitGate.lastread, "OPEN")){
          handleExitGate(dateStr,true);
        }
      }
      else if (subscription == &AutomaticMode){
        if(!strcmp((char*) AutomaticMode.lastread, "OFF")){
          automatic = false;
        } else {
          automatic = true;
        }
      }
    }  
}
void MQTT_connect() 
{
  int8_t ret;
  Serial.println("Đang kết mqtt");
  // Stop if already connected.
  while ((ret = mqtt.connect()) != 0) // connect will return 0 for connected
  { 
      Serial.println("Kết nối mqtt thất bại!");
      mqtt.disconnect();
      delay(100);
  }

  if (mqtt.connected()) 
  {
    Serial.println("Kết nối mqtt thành công!");
    return;
  }

  uint8_t retries = 3;
  
}