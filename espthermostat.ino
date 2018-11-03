/*
 * ESP32 Dev Module
 * Niall McAndrew 8-Oct-2018
 * 
 * to program:
 * hold reset, press and release other button, release reset
 * 
 * WEMOS LOLIN32, 80MHz, Default, 921600
 * 
 */

#include <SimpleTimer.h>
#include "SSD1306.h"
#include <WiFi.h>
#include <aREST.h>
#include <Wire.h>
#include <Math.h>

IPAddress ip(192, 168, 1, 109);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

SimpleTimer timer;
SimpleTimer timerdisp;
SimpleTimer timerboiler;

// SI7021 I2C address is 0x40(64)
#define si7021Addr 0x40

#define sda 13
#define scl 12

// boiler calibration to prevent hysteresis
#define calib 1

// thermostat calibration 
#define thermocalib -2

// Create aREST instance
aREST rest = aREST();

// WiFi parameters
const char* ssid = "TP-LINK_A005D1";
const char* password = "???????????????

// The port to listen for incoming TCP connections
#define LISTEN_PORT           80

// Create an instance of the server
WiFiServer server(LISTEN_PORT);

// Variables to be exposed to the API
double temperature;
double temporig;
double humidity;
double humidorig;

int boiler;
int boilerorig;
int settemp = 18;
int settemporig = 0;

int bluebuttonreading;
int redbuttonreading;
volatile int bluestate;

// Declare functions to be exposed to the API
int ledControl(String command);

#define ledpin 2  // this is the led showing boiler on/off
#define boilerpin 14  // this is the digital pin controlling the boiler relay
#define redbuttonpin 15
#define bluebuttonpin 16

int bluebuttonState;             // the current reading from the input pin
int bluelastButtonState = LOW;   // the previous reading from the input pin

int redbuttonState;             // the current reading from the input pin
int redlastButtonState = LOW;   // the previous reading from the input pin

// the following variables are unsigned longs because the time, measured in
// milliseconds, will quickly become a bigger number than can be stored in an int.
unsigned long redlastDebounceTime = 0;  // the last time the output pin was toggled
unsigned long debounceDelay = 50;    // the debounce time; increase if the output flickers

unsigned long bluelastDebounceTime = 0;  // the last time the output pin was toggled

 String ipaddress;
 
void setup()
{ 
  pinMode(ledpin, OUTPUT);
  // red and blue buttons (up and down for settemp)
  pinMode(bluebuttonpin, INPUT_PULLUP);
  pinMode(redbuttonpin, INPUT_PULLUP);
  pinMode(boilerpin, OUTPUT);

//  attachInterrupt(bluebuttonpin, onChangeblue, CHANGE);
//  attachInterrupt(redbuttonpin, onChangered, CHANGE);

  //pinwrite(boilerpin, LOW);
  
   // Initialising the UI will init the display too.
   // Initialize the OLED display using Wire library

  Serial.begin(115200);
  //Wire.beginTransmission(si7021Addr);
  //Wire.endTransmission();
  //delay(300);
  
 Serial.begin(115200);

 // every 5 seconds, call the function to read the temp/humidity
 timer.setInterval(5000, readtemp);

  // every 5 seconds, call the function to update OLED display
 timerdisp.setInterval(500, showstatus);
 
// every 1.5 seconds, turn boiler on/off (and led on/off) depending on temp vs settemp
 timerboiler.setInterval(1500, setboiler);

 rest.variable("temperature", &temperature);
 rest.variable("humidity", &humidity);
 rest.variable("settemp", &settemp);
 rest.variable("boiler", &boiler);

 // Function to be exposed
  rest.function("DoSetTemp",setControl);

 rest.set_id("2");
 rest.set_name("thermostat");

  // Connect to WiFi
  WiFi.config(ip, gateway, subnet);  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");


/*
 // Connect to WiFi
 WiFi.begin(ssid, password);
 while (WiFi.status() != WL_CONNECTED) {
   delay(500);
   Serial.print(".");
 }
 Serial.println("");
 Serial.println("WiFi connected");
*/


 // Start the server
 server.begin();
 Serial.println("Server started");

 // Print the IP address
 ipaddress = WiFi.localIP().toString();
 Serial.println(ipaddress);

 //Serial.println(WiFi.localIP());

}


int checkbluebutton()
{
int reading = digitalRead(bluebuttonpin);

  // check to see if you just pressed the button
  // (i.e. the input went from LOW to HIGH), and you've waited long enough
  // since the last press to ignore any noise:

  // If the switch changed, due to noise or pressing:
  if (reading != bluelastButtonState) {
    // reset the debouncing timer
    bluelastDebounceTime = millis();
  }

  if ((millis() - bluelastDebounceTime) > debounceDelay) {
    // whatever the reading is at, it's been there for longer than the debounce
    // delay, so take it as the actual current state:

    // if the button state has changed:
    if (reading != bluebuttonState) {
      bluebuttonState = reading;

      // only toggle the LED if the new button state is HIGH
      if (bluebuttonState == LOW) {
        Serial.println("Pressed blue button");
        if (settemp > 12) {settemp = settemp - 1;}
      }
    }
  }

  // save the reading. Next time through the loop, it'll be the lastButtonState:
  bluelastButtonState = reading;
}


int checkredbutton()
{
int reading = digitalRead(redbuttonpin);

  // check to see if you just pressed the button
  // (i.e. the input went from LOW to HIGH), and you've waited long enough
  // since the last press to ignore any noise:

  // If the switch changed, due to noise or pressing:
  if (reading != redlastButtonState) {
    // reset the debouncing timer
    redlastDebounceTime = millis();
  }

  if ((millis() - redlastDebounceTime) > debounceDelay) {
    // whatever the reading is at, it's been there for longer than the debounce
    // delay, so take it as the actual current state:

    // if the button state has changed:
    if (reading != redbuttonState) {
      redbuttonState = reading;

      // only toggle the LED if the new button state is HIGH
      if (redbuttonState == LOW) {
        Serial.println("Pressed red button");
        if (settemp < 23) { settemp = settemp + 1; }
      }
    }
  }

  // save the reading. Next time through the loop, it'll be the lastButtonState:
  redlastButtonState = reading;
}






void setboiler()
{

  pinMode(boilerpin, OUTPUT);
  
  if (temperature < settemp)
    {
      boiler = 1;
      //Serial.println("turning on boiler");
      digitalWrite(boilerpin, HIGH);
      digitalWrite(ledpin, HIGH);
    }
  if (temperature == settemp)
    {
      boiler = 0;
      //Serial.println("turning on boiler");
      digitalWrite(boilerpin, LOW);
      digitalWrite(ledpin, LOW);
    }
  if (temperature > (settemp + calib))
      {
      boiler = 0;
      //Serial.println("turning off boiler");
      digitalWrite(boilerpin, LOW);  
      digitalWrite(ledpin, LOW);
      }
    
  }


// Custom function accessible by the API
int setControl(String command) {

int ret = 0;
  // Get state from command
  int doset = command.toInt();
  if ((doset > 0) && (doset < 24))
    {
      Serial.println(String(doset));
      settemp = doset;
      ret = 1;
      }
  //digitalWrite(6,state);
  return ret;
}

  
void showstatus()
{  

  //if ((round(temperature) != temporig) || (round(humidity) != humidorig))

  //setboiler();

  if ((round(temperature) != temporig) || (settemp != settemporig) || (boilerorig != boiler))
    { 
       boilerorig = boiler;
       settemporig = settemp;
       temporig = round(temperature);
       //humidorig = round(humidity);

       //pinMode(boilerpin, INPUT);
       //boiler = digitalRead(boilerpin);
       
       String bstr = "On";
       if (boiler == 0) { bstr = "Off"; }
       
       //Serial.print("boilerpin: ");
       //Serial.println(boiler);
      
       SSD1306  display(0x3c,5, 4);         
       display.init();
       display.flipScreenVertically();
       
       display.setFont(ArialMT_Plain_10);
       display.drawString(0, 0, "IP: " + ipaddress);
      
      display.setFont(ArialMT_Plain_16);
      display.drawString(0, 15, "Temp: " +  String(round(temperature)) + " C");
      //display.drawString(0, 30, "Humidity: " + String(int(humidity)) + "%" );
      display.drawString(0, 30, "Set Temp: " + String(settemp) + " C");
      display.drawString(0,45, "Boiler: " + bstr);
      
      display.display(); 
      
      Serial.println("temp:" + String(temperature));
      Serial.println("humid:" + String(humidity));
      Serial.println("settemp:" + String(settemp));
      Serial.println("boiler:" + String(boiler));
    }
}



void readtemp()
{

  unsigned int data[2];

  Wire.begin(sda, scl);
  Wire.beginTransmission(si7021Addr);
  //Send humidity measurement command
  Wire.write(0xF5);
  Wire.endTransmission();
  delay(500);
     
  // Request 2 bytes of data
  Wire.requestFrom(si7021Addr, 2);
  // Read 2 bytes of data to get humidity
  if(Wire.available() == 2)
  {
    data[0] = Wire.read();
    data[1] = Wire.read();
  }
     
  // Convert the data
  float humid  = ((data[0] * 256.0) + data[1]);
  humidity = ((125 * humid) / 65536.0) - 6;
 
  Wire.beginTransmission(si7021Addr);
  // Send temperature measurement command
  Wire.write(0xF3);
  Wire.endTransmission();
  delay(500);
     
  // Request 2 bytes of data
  Wire.requestFrom(si7021Addr, 2);
   
  // Read 2 bytes of data for temperature
  if(Wire.available() == 2)
  {
    data[0] = Wire.read();
    data[1] = Wire.read();
  }
 
  // Convert the data
  float temp  = ((data[0] * 256.0) + data[1]);
  
  temperature =  ((175.72 * temp) / 65536.0) - 46.85;
  temperature = temperature + thermocalib;
  //delay(1000);

  //setboiler(); 
}
void loop()
{

timer.run();
timerdisp.run();
timerboiler.run();


pinMode(bluebuttonpin, INPUT_PULLUP);
pinMode(redbuttonpin, INPUT_PULLUP);

checkbluebutton();
checkredbutton();

//int rb = digitalRead(redbuttonpin);
//int bb = digitalRead(bluebuttonpin);
//Serial.println("RedButton:" + String(rb));
//Serial.println("BlueButton:" + String(bb));

//delay(1000);

//setboiler();

//readtemp();

//showstatus();
//delay(5000);

// Handle REST calls
 WiFiClient client = server.available();
 if (!client) {
   return;
 }
 while(!client.available()){
   delay(1);
 }
 rest.handle(client);
}
