/*
 * Name: VALVE MAKER 1.0
 * Author: dhtmldude / ppphilll
 * Board: ESP8266MOD (NodeMcu)
 * PCB: 4 relay board
 * 
 * ---- FEATURES ----
 * 
 * Creates SVG graphics from a template 
 * 
 * Creates an instance of an HTTP server and displays HTML for interaction with relays
 * on predefined or user defined pins via an external JSON configuration file.
 * 
 * Sets up an AJAX URL for interacting with the front end via the HTTP protocol, 
 * commands received either turn on or off relays or pins on the module.
 * 
 * Sets up an Access Point for accessing the WIFI configuration (SSID and PASSWORD), 
 * for later accessing via EEPROM and connecting autonomously when the unit is reset.
 * If the WIFI SSID is unavailable after 30 seconds, it sets up a temporary access point
 * for storing a new configuration.
 * 
 * */
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <EEPROM.h>
#include <ArduinoJson.h>
#include "configuration.h"

ESP8266WebServer server(80);
LiquidCrystal_I2C lcd(0x27, 20, 4);

bool isWifiConnected = false;
config_t configuration;
wifi_config_t wifi_configuration;

void setup(void){
  Serial.begin(9600);

  //OTA PROGRAMMING SECTION BEGINS
  ArduinoOTA.onStart([]() {
    Serial.println(F("Start"));
  });
  ArduinoOTA.onEnd([]() {
    Serial.println(F("\nEnd"));
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println(F("Auth Failed"));
    else if (error == OTA_BEGIN_ERROR) Serial.println(F("Begin Failed"));
    else if (error == OTA_CONNECT_ERROR) Serial.println(F("Connect Failed"));
    else if (error == OTA_RECEIVE_ERROR) Serial.println(F("Receive Failed"));
    else if (error == OTA_END_ERROR) Serial.println(F("End Failed"));
  });
  ArduinoOTA.begin();
  //OTA PROGRAMMING SECTION ENDS

  //initialize the valves
  for (byte i=0; i<sizeof(configuration.valves) && (configuration.valves[i] != configuration.dummy_valve); i++){
      pinMode(configuration.valves[i], OUTPUT);
      digitalWrite(configuration.valves[i], LOW);
  }

  Serial.println(F("Initializing display..."));

  //initialize the display
  lcd.init(D2, D1);
  lcd.backlight();
  lcd.setCursor(0, 0);

  //try to read config from EEPROM
  EEPROM.begin(512);
  EEPROM.get(0, wifi_configuration);

  //print obtained values
  Serial.println(F("WIFI values in EEPROM"));
  Serial.println(String("SSID: ") + String(wifi_configuration.ssid));
  Serial.println(String("Password: ") + String(wifi_configuration.password));
  Serial.println(F("----"));
  
  //setup wifi with struct's WIFI credentials
  setupWIFI();

  //read file configuration
  if (isWifiConnected) readConfiguration();
  
  //attach a few handlers to the server
  server.on("/", handleRequest);
  server.on("/action", handleAjax);
  server.on("/configuration.html", handleAdminServe);
  server.on("/configuration.save", handleAdminSave);
  server.onNotFound(handleNotFound);
  server.begin();

  //debug finished initialization
  Serial.println(F("HTTP server started"));
}

void loop(void){
  //process incoming requests
  server.handleClient();
  ArduinoOTA.handle();
}

//handle a 404 not found
void handleNotFound(){
  String message = "Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (byte i=0; i<server.args(); i++) message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  server.send(404, "text/html", message);
}

//configure the wifi and the valve names and relay addresses
void handleAdminServe(){
  //wifi credentials
  String page = configuration.pagestart_noscript;
         page += configuration.wifisetup;
         page += configuration.pageend;
  
  server.send(200, "text/html", page);
}

//save the valve configuration and wifi settings
void handleAdminSave(){

  char _ssid[20];
  memset(_ssid,'\0',sizeof(_ssid));
  char _password[20];
  memset(_password,'\0',sizeof(_password));
  char _host[15];
  memset(_host,'\0',sizeof(_host));

  Serial.println(F("RAW server args"));
  Serial.println(String("ssid: ") + server.arg("_ssid"));
  Serial.println(String("password: ") + server.arg("_password"));
  Serial.println(String("host: ") + server.arg("_host"));
  Serial.println(F("----"));

  strncpy(_ssid, server.arg("_ssid").c_str(), 20);
  strncpy(_password, server.arg("_password").c_str(), 20);
  strncpy(_host, server.arg("_host").c_str(), 15);

  Serial.println(F("Received values for configuration"));
  Serial.println(String("SSID: ") + _ssid);
  Serial.println(String("Password: ") + _password);
  Serial.println(String("Host: ") + _host);
  Serial.println(F("-----"));

  //update the ssid and password
  if (!(_ssid[0]&_password[0]&_host[0])){
    //update the ssid and password
    strcpy(wifi_configuration.ssid, _ssid);
    strcpy(wifi_configuration.password, _password);
    strcpy(wifi_configuration.host, _host);
  }else{
    //return error
    server.sendHeader("Location", "/configuration.html?error=1", true);
    flashLCDWith("ERROR");
    server.send(302, "text/plain", "");
    return;
  }

  Serial.println(F("Object stored values for configuration"));
  Serial.println(String("SSID: ") + wifi_configuration.ssid);
  Serial.println(String("Password: ") + wifi_configuration.password);
  Serial.println(String("Host: ") + wifi_configuration.host);
  Serial.println(F("-----"));

  //do a final update on the eeprom
  EEPROM.put(0, wifi_configuration);
  EEPROM.commit();

  lcd.clear();
  lcd.print(F("CONFIG SAVED"));
  delay(2000);

  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
  delay(1000);

  ESP.restart();
  
  return;
}

void flashLCDWith(String message){
  
  for(byte i=0;i<5;i++){
    lcd.clear();
    delay(500);
    lcd.print(message);
    delay(500);
  }

  delay(2000);

  lcd.clear();
}

//handle the root website request (main ui)
void handleRequest(){
  //process the AP mode or the LIVE mode
  if (!isWifiConnected){
    server.sendHeader("Location", "/configuration.html", true);
    server.send(302, "text/plain", "");
    return;  //it ends here with a redirect to set the login and password for the device's network
  }
  
  int valveno = configuration.dummy_valve;
  bool valvestatus = false;  //fail-safe for all valves

  //page starts
  String page = "";
  page += configuration.getPageStart(wifi_configuration.host);

  Serial.println(String("Valve count: ") + sizeof(configuration.valves));
  Serial.println(String("Dummy valve: ") + configuration.dummy_valve);

  //valves
  page += "<div id='valves'>";
  for (byte i=0; i<configuration.getValveCount(); i++){

    Serial.println(String("Processing valve relay: ") + configuration.valves[i]);
    
    bool isDummy = (configuration.valves[i] == configuration.dummy_valve);    
    page += makeValve(i, configuration.valveStatuses[i], isDummy);
  }
  page += "</div>";
  page += configuration.pageend;

  server.send(200, "text/html", page);
}

//handle all ajax requests for toggling the valves
void handleAjax(){
  int valveno = configuration.dummy_valve;
  bool valvestatus = false;  //fail-safe for all valves
  
  valveno = server.arg("valveno").toInt();

  valvestatus = (server.arg("valvestatus")=="on" ? true : (server.arg("valvestatus")=="off" ? false : false));  //default valves to off when undefined

  Serial.println(F("Valve status change"));
  Serial.println(String("Valve no received: ") + valveno);
  Serial.println(String("Valve status received: ") + valvestatus);
  Serial.println(F("----"));
  
  //do nothing for dummy valves
  if (valveno!=configuration.dummy_valve){
    digitalWrite(configuration.valves[valveno], valvestatus ? HIGH : LOW);
    configuration.valveStatuses[valveno] = valvestatus;
  }

  String out = "";
  StaticJsonBuffer<600> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  JsonArray& valves = root.createNestedArray("valves");

  //Serial.println(String("Processing valves count: ") + configuration.getValveCount());

  for (byte i=0; i<configuration.getValveCount(); i++){
    bool isDummy = (configuration.valves[i] == configuration.dummy_valve); //variable redeclaration ????
    String _state = (configuration.valveStatuses[i] ? "ON" : "OFF");
    if (isDummy) _state = "OFFLINE"; 

    JsonObject& valve = valves.createNestedObject();
    valve["no"] = i;
    valve["status"] = _state;
    valve["label"] = configuration.valveLabels[i];
  }

  //root.prettyPrintTo(Serial);
  root.prettyPrintTo(out);

  server.send(200, "text/html", out);
}

//build an SVG valve to display in the html
String makeValve(int i, bool state, bool isDummy){

  String dwg = "<div class='valve' status='status_$' id='valve_$'><svg xmlns='http://www.w3.org/2000/svg' xmlns:xlink='http://www.w3.org/1999/xlink' version='1.1' baseProfile='full' viewBox='0 0 590 590' width='128px' height='128px'>";
  dwg += "<g><g><g> <path id='valve_path_$' d='M490,250V80h-71.188C411.598,34.719,372.28,0,325,0c-47.28,0-86.598,34.719-93.812,80H190V62.293L116.241,50H0v230 h116.241L190,267.707V250h40v70h-20v90h20v80h190v-80h20v-90h-20v-70H490z M160,242.293L113.758,250H30v-36.667h85.417 L160,210.856V242.293z M160,180.81l-45.416,2.523H30v-36.667h84.584L160,149.19V180.81z M160,119.144l-44.583-2.477H30V80h83.758 L160,87.707V119.144z M325,30c35.841,0,65,29.159,65,65s-29.159,65-65,65s-65-29.159-65-65S289.159,30,325,30z M390,460H260v-50 h130V460z M410,380H240v-30h170V380z M390,220v100H260V220h-70V110h41.188c7.214,45.281,46.532,80,93.812,80 c47.28,0,86.598-34.719,93.812-80H460v110H390z' fill='COLOR_'/>";
  dwg += "<text id='valve_text_$' class='valve_label' x='0' y='550' font-family='Tahoma' font-size='50'>TEXT_</text></g></g></g></svg></div>";

  String _state = (state ? "ON" : "OFF");

  if (isDummy) _state = "OFFLINE"; 

  dwg.replace("COLOR_", (isDummy ? configuration.OFFL : (state==true ? configuration.ON : configuration.OFF)));
  dwg.replace("status_$", _state);
  dwg.replace("valve_$", String("valve_") += String(i));
  dwg.replace("valve_path_$", String("valve_path_") += String(i));
  dwg.replace("valve_text_$", String("valve_text_") += String(i));
  dwg.replace("TEXT_", String(configuration.valveLabels[i]) + String(": ") + _state);

  return dwg;
}

void readConfiguration(){
  HTTPClient http;

  //initialize http connection
  http.begin(String("http://") + wifi_configuration.host + String("/valves/configuration.json"));
  int httpCode = http.GET();

  //print http code and display error otherwise on screen
  Serial.println(String("HTTP Code for config file: ") + httpCode);

  if (httpCode!=200) flashLCDWith("CONFIG NOT FOUND");    

  //get the config file
  String payload = http.getString();
  //Serial.println(payload);

  //parse json object
  StaticJsonBuffer<2000> jsonBuffer;
  JsonVariant variant = jsonBuffer.parse(payload);

  //Serial.println("Variant JSON: ");
  //variant.prettyPrintTo(Serial);

  //Serial.println("Values in config file");

  //convert json object into configuration struct
  for (byte i=0; i<configuration.getValveCount(); i++){
    String _pin = variant["valves"][i]["pin"];
    String _label = variant["valves"][i]["label"];
    bool _status = (variant["valves"][i]["status"]=="ON") ? true : false;

    //Serial.println("-----");
    //Serial.println(String("PIN: ") + _pin);
    //Serial.println(String("label: ") + _label);
    //Serial.println(String("status: ") + _status);
    //Serial.println("-----");
    
    configuration.valves[i] = atoi(_pin.c_str());
    strcpy(configuration.valveLabels[i], _label.c_str());
    configuration.valveStatuses[i] = _status;
  }

}

bool saveConfiguration(){

  String out = "";
  StaticJsonBuffer<600> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  JsonArray& valves = root.createNestedArray("valves");

  for (byte i=0; i<configuration.getValveCount(); i++){
    bool isDummy = (configuration.valves[i] == configuration.dummy_valve); //again ???
    String _state = (configuration.valveStatuses[i] ? "ON" : "OFF");
    if (isDummy){ _state = "OFFLINE"; }

    JsonObject& valve = valves.createNestedObject();
    valve["no"] = i;
    valve["status"] = _state;
    valve["label"] = configuration.valveLabels[i];
  }

  root["ON"] = configuration.ON;
  root["OFF"] = configuration.OFF;
  root["OFFL"] = configuration.OFFL;

  root["pagestart"] = configuration.pagestart;
  root["pageend"] = configuration.pageend;

  //root.prettyPrintTo(Serial);
  root.prettyPrintTo(out);
  
  return true;
}
void setupAP(){
  boolean result = WiFi.softAP("BETTINA", "password");
  if (result){
    Serial.println(F("AP Mode started."));
    lcd.print(F("Connect to SSID"));
    lcd.setCursor(0, 1);
    lcd.print(F("SSID: BETTINA"));
    lcd.setCursor(0, 2);
    lcd.print(F("PASS: password"));
  }else{
    Serial.println(F("AP Mode failed."));
    lcd.clear();
    lcd.print(F("Unable to setup AP."));
  }
}

//tries to connect to the credentials supplied in the configuration page,
//otherwise sets up an access point for access to configuration.
//once configuration is saved it tries to connect to wifi for 30 seconds,\
//and afterwards it reverts back to setting up a temporary access point
//until the next reboot, since credentials are stored in eeprom. 
void setupWIFI(){
  //de-initialize the wifi
  WiFi.disconnect();

  //if either SSID or password is null, send them to the config page
  if (!strlen(wifi_configuration.ssid) || !strlen(wifi_configuration.password)){
    setupAP();
    return;
  }

  //initialize the wifi
  WiFi.begin(wifi_configuration.ssid, wifi_configuration.password);
  Serial.println(F(""));
  lcd.print(F("CONNECTING "));

  //wait for connection, print dots
  
  //###### what to do if connection fails
  int waitCount = 0;
  int waitMax = 30;  //wait 30 seconds to connect to wifi
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(F("."));
    lcd.print(F("."));  //print the dots showing a connection is being made
    waitCount++;
    lcd.setCursor(waitCount + 11, 0);  //reset the cursor after the dot and the word CONNECTING_ (11 chars)

    if (waitCount > waitMax){
      //when the wait count has exceeded then restore the access point
      lcd.clear();
      lcd.print(F("WIFI Unavailable"));
      delay(3000);
      Serial.println(String("Connection attempts to ") + wifi_configuration.ssid + String("exceeded.  Setting up Access Point."));
      setupAP();
      return;
    }
  }

  isWifiConnected = true;

  //erase the dots
  lcd.setCursor(0, 0);
  for (int i=0; i<waitCount; i++){
    lcd.print(F(" "));  
  }

  //print some debug stuff
  Serial.println(F(""));
  Serial.print(F("Connected to "));
  Serial.println(WiFi.SSID());
  Serial.print(F("Host IP address: "));
  Serial.println(WiFi.localIP());
  Serial.print(F("Using host as: "));
  Serial.println(wifi_configuration.host);

  //print info to the LCD
  lcd.clear();
  lcd.print(F("CONNECTED"));
  delay(2000);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("#"));
  lcd.print(WiFi.SSID());
  lcd.setCursor(0, 1);
  lcd.print(F("@"));
  lcd.print(WiFi.localIP());
}
