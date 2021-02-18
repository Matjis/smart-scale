/*
This is smart scale project by Matiss Vaivodins for his bachelor's thesis project part.

 */

#include "HX711.h" // for hx712 chip
#include <ESP8266WiFi.h> //for ESP8266
#include <ESP8266WebServer.h> //for ESP8266
#include <ESP8266HTTPClient.h> //for ESP8266
#include <WiFiUdp.h> //for OTA
#include <ArduinoOTA.h> //for OTA
#include "FS.h" //for SPIFFS
#include <BlynkSimpleEsp8266.h> //for blynk

ESP8266WebServer server(80); // Create a webserver object that listens for HTTP request on port 80
HTTPClient http;


//spiffs variable setup
const String configPath = "/config.txt";
String _ssid = ""; // for spiffs memory
String _pass = "";
String _email;
String _jar;
float _weight; //weigth value sent to blynk app + for SPIFFS
bool _configured;

//ap setup
char* _ap = "TupperWise"; //Access points name for the first time connection

//HX712 setup
#define DOUT  D2 // data line for HX712 chip
#define CLK   D1 // clock line for HX712 chip
#define powerHX712 D6 //HX712 chip is connected to GPIO12, and from this GPIO it gets its power supply
HX711 scale; //
const float calibration_factor = 1015; // slope of the line for grams
const long offset_predefined = -62064; // value that lets you get the right weight after restart of device

//adc setup
const int adcValue = A0; //ADC for battery voltage mesurements

//blynk setup
char auth[] = "aKOM844_Z-TPJ8hFr4GrqYCcwwwwrTpw"; 
float batteryVoltageVal=1;

//esp sleep times
int normalSleepTime = 6e7; //should be 60 sec
//int powerOffSleepTime = 5e8;


void writeConfig()
{
  Serial.println("Writing config");

  if (SPIFFS.exists(configPath)) //checks if there is such path
  {
    SPIFFS.remove(configPath); //delets that path
  }

  File f = SPIFFS.open(configPath, "w+"); // w+ : Open for reading and writing. The file is created if it does not exist, otherwise it is truncated. The stream is positioned at the beginning of the file.
  f.println(_ssid);
  f.println(_pass);
  f.println(_email);
  f.println(_jar);
  f.println(String(_weight));

  f.flush();
  _configured = true;

  Serial.println("Done");
}

void readConfig()
{
  Serial.println("Reading config");

  File f = SPIFFS.open(configPath, "r+");
  if (!f)
  {
    Serial.println("File not found");
  }
  else
  {
    for (int i = 1; i <= 5; i++) {
      String s = f.readStringUntil('\n');
      String smod = s.substring(0, s.length() - 1);
 
      switch (i)
      {
        case 1:
          _ssid = smod;
          break;
        case 2:
          _pass = smod;
          break;
        case 3:
          _email = smod;
          break;
        case 4:
          _jar = smod;
          break;
        case 5:
          _weight = smod.toFloat();
          break;
      }
    }

    f.flush();
    Serial.println("Done");
  }
}

void connectWifi()
{
  Serial.println("Connecting");

  char userSsid[_ssid.length()];
  char userPass[_pass.length()];
  _ssid.toCharArray(userSsid, _ssid.length()+1); //Stores the router name in char
  _pass.toCharArray(userPass, _pass.length()+1); //Stores the password in char

  WiFi.mode(WIFI_STA);
  WiFi.begin(userSsid, userPass);

  int elapsed = 0;
  while (WiFi.status() != WL_CONNECTED) {
    if (elapsed > 500 * 2 * 30) {
      /* Timeout */
      Serial.println("Connect Timeout");
      return;
    }
    delay(500);
    Serial.print(".");
    elapsed += 500;
  }

  Serial.println("WiFi connected");
}

void handleRoot() {
 
  int args = server.args(); // Getting information about request arguments, args - get arguments count


  Serial.println("ARGS: " + String(args));
  for (int i = 0; i < args; i++)
  {
    Serial.println(server.argName(i) + " : " + server.arg(i));

    if (server.argName(i) == "ssid") _ssid = server.arg(i);
    if (server.argName(i) == "pass") _pass = server.arg(i);
    if (server.argName(i) == "email") _email = server.arg(i);
    if (server.argName(i) == "jar") _jar = server.arg(i);
    if (server.argName(i) == "weight") _weight = server.arg(i).toFloat();
  }

  if (_ssid != "" && _pass != "" && _email != "" && _jar != "" && _weight > 0)
  {
    delay(500);
    writeConfig();
    server.send(200, "text/html", "<h1>SUBMITTED</h1>");
    Serial.println("ESP restart");
    ESP.restart();
  }
  else
  {
    Serial.println("All fields are not fild");
    Serial.print("Sending error message ");
    server.send(200, "text/html", "<h1>USER MUST FILL ALL THE FIELDS</h1>");
    Serial.println("Sent");
  }
}

void OTA_setup(){
  Serial.println("Setting up OTA configuration");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
}

void scaleSetup(){
  digitalWrite(powerHX712,HIGH); //turns on HX712
  scale.begin(DOUT, CLK);
  scale.set_scale(calibration_factor); //Adjust to this calibration factor
  digitalWrite(powerHX712,LOW); //turns off HX712
}

void scaleStart(){
  digitalWrite(powerHX712,HIGH); //turns on HX712
  /*Serial.print("Svars: ");
  Serial.print(scale.get_units(), 1); //value already adjusted with calibration factor to g
  Serial.println(" g"); //
  */_weight = scale.get_units(); // for blynk application
  digitalWrite(powerHX712,LOW); //turns off HX712
  //delay(100); // is it needed???
}

void sleep(int sleepTime){ //esp8266 with hx712 goes to sleep
  Serial.println("Deep sleep on");
  ESP.deepSleep(sleepTime); // value in braces shows time in uS, 5e6 = 5sec and if the value is 0, then sleeps indefinetly
}

void blynkData(){
  Blynk.virtualWrite(V1, batteryVoltageVal);
  delay(500);
  Blynk.virtualWrite(V3, _weight);
  delay(500);
}

void batteryVoltage(){
  float adcMesurement = analogRead(adcValue);
  float adcInputVoltage = adcMesurement / 1024; //ACP resolution is  1024 - 10bits
  batteryVoltageVal = adcInputVoltage / 0.2123; // ADC output voltage coefficient
  Serial.print("Battery voltage: ");
  Serial.println(batteryVoltageVal);
}

void setup() {
  Serial.begin(115200);
  pinMode(powerHX712, OUTPUT); //HX712 GPIO12 is output
  
  SPIFFS.begin();
  _configured = SPIFFS.exists(configPath);
  //Serial.printf("_conf state: [%u]\n", _configured);
  
    if (_configured)
  {
    /* If a configuration file exists, read config, connect to configured wifi */
    //Serial.println("path exists");
    readConfig();
    connectWifi();
    Blynk.config(auth);
    scaleSetup();
    OTA_setup();
  }
  else
  {
    /* Otherwise, start a hotspot for configuration */
    WiFi.softAP(_ap); // WiFi.softAP(ssid, password, channel, hidden, max_connection) https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/soft-access-point-class.html
    Serial.print("Access Point \"");
    Serial.print(_ap);
    Serial.println("\" started");

    Serial.print("IP address:\t");
    Serial.println(WiFi.softAPIP()); // Return IP address of the soft access point’s network interface https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/soft-access-point-class.html

    server.on("/", handleRoot); // Client request handlers, skatās pēc / simbola pēc linka, piem., 192.168.4.1/ tad dod tālāk to kas notiek ar otru parametru
    server.begin(); // Starting the server
    Serial.println("HTTP server started");

  }
}

void loop() {
  if (_configured)
  {
    Serial.println("Configuration successful");
    ArduinoOTA.handle();        
    scaleStart();
    batteryVoltage();
    Blynk.run();
    blynkData();
    sleep(normalSleepTime);    
  }
  else
  {
    server.handleClient();
  } 
}
