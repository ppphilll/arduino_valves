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
 * Displays available WIFI hotspots for connecting and displays available GPIO numbers 
 * for mapping the relays and labelling them for control through the UI.
 * 
 * Scripts and styles are stored in a separate EEPROM (32kb) and managed through 
 * the configuration interface.
 * 
 * An android app allows the setup of the device much like Alexa by Amazon where 
 * the app finds the Access Point, connects to it and then reconnects to the new device
 * on the current wifi so the user can access its features via the embedded web app.
 *  
 * */
#include <ESP8266WiFiMulti.h>
#include <WebSocketsClient.h>
#include <StreamString.h>
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
#include <FS.h>
#include "configuration.h"

ESP8266WiFiMulti WiFiMulti;
ESP8266WebServer server(80);
LiquidCrystal_I2C lcd(0x27, 20, 4);

wifi_configuration_t wifi_config;
WebSocketsClient webSocket;

uint64_t heartbeatTimestamp = 0;
bool isConnected = false;
bool isWifiConnected = false;
const int EEPROM_SIZE = 2560;
const uint8_t avail_pins[5] = {12, 2, 0, 14, 255};

#define HEARTBEAT_INTERVAL 300000
#define MyApiKey "0e918773-c3df-4281-a4f1-0ded191bb993"

//the place holder for the html source code
String configPage = "<div id=\"config\" class=\"center-div\"><div class=\"container\"><form action=\"/configuration.save\" method=\"POST\"><table><tr><th colspan=\"2\">WIFI Setup</th></tr><tr> <td><label for=\"_ssid\">SSID: </label></td><td><select name=\"_ssid\"><!--WIFI_OPTIONS--></select></td></tr><tr> <td><label for=\"_password\">Password: </label></td><td><input type=\"password\" name=\"_password\" required /></td></tr><tr> <td><label for=\"_host\">Host: </label></td><td><input type=\"text\" name=\"_host\" required  maxlength=\"15\"/></td></tr></table><!-- RELAYS --></div><div><div><input type=\"submit\" value=\"Save Configuration\"/></div></form><div><form action=\"/configuration.reset\" method=\"POST\"><input type=\"submit\" value=\"Factory Reset\"></form></div></div></div></div></div>";

//html includes
String redirect = "<html><head><meta http-equiv=\"refresh\" content=\"10;url=/\" /></head><body>Redirecting in 10 seconds...</body></html>";
String pagestart = "<html><head><script src=\"https://ajax.googleapis.com/ajax/libs/jquery/3.3.1/jquery.min.js\"></script><script src=\"/script.js\"></script><link rel=\"stylesheet\" href=\"/style.css\"></head><body><div class=\"header\"><a class=\"header-title\" href=\"/\">Valve Control System</a><span class=\"header-links\"><a href=\"/configuration.html\">Configure</a></span></div>";
String pageend = "</body></html>";

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      isConnected = false;    
      Serial.printf("[WSc] Webservice disconnected from sinric.com!\n");
      break;
    case WStype_CONNECTED: {
      isConnected = true;
      Serial.printf("[WSc] Service connected to sinric.com at url: %s\n", payload);
      Serial.printf("Waiting for commands from sinric.com ...\n");        
      }
      break;
    case WStype_TEXT: {
        Serial.printf("[WSc] get text: %s\n", payload);
          
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject((char*)payload); 
        String deviceId = json ["deviceId"];     
        String action = json ["action"];
        
        if(action == "setPowerState") { // Switch or Light
            String value = json ["value"];
            if(value == "ON") {
                toggleValve(deviceId, true);
            } else {
                toggleValve(deviceId, false);
            }
        }
        else if (action == "test") {
            Serial.println("[WSc] received test command from sinric.com");
        }
      }
      break;
    case WStype_BIN:
      Serial.printf("[WSc] get binary length: %u\n", length);
      break;
  }
}

void toggleValve(String deviceId, bool state){
  for (byte i=0; i<wifi_config.getValveCount(); i++){
    if (String(wifi_config.valveSinricIds[i]) == deviceId){
      Serial.println(String("FOUND ALEXA DEVICE: ") + deviceId);
      digitalWrite(wifi_config.valves[i], state);
      lcdAnimateLoadingAt(0, 3, String(wifi_config.valveLabels[i]) + String(state ? ": ON" : ": OFF"));
    }
  }
}

void lcdAnimateLoadingAt(int col, int row, String msg){
  //position the cursor
  lcd.setCursor(col + 2, row);
  lcd.print(msg);

  lcd.setCursor(col, row);

  //animate
  for (byte i=0; i<3; i++){
    lcd.print("|");
    delay(250);
    lcd.setCursor(col, row);
    lcd.print("/");
    delay(250);
    lcd.setCursor(col, row);
    lcd.print("-");
    delay(250);
    lcd.setCursor(col, row);
    lcd.write(byte(7));
    delay(250);
    lcd.setCursor(col, row);
   }

   lcd.setCursor(col, row);
   lcd.print("                    "); //20 spaces
}

void setup(void){
  Serial.begin(9600);
  EEPROM.begin(EEPROM_SIZE);
  SPIFFS.begin();
  
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

  byte customBackslash[8] = {
    0b00000,
    0b10000,
    0b01000,
    0b00100,
    0b00010,
    0b00001,
    0b00000,
    0b00000
  };

  lcd.createChar(7, customBackslash);

  Serial.println(F("Initializing display..."));

  //initialize the display
  lcd.init(D2, D1);
  lcd.backlight();
  lcd.setCursor(0, 0);

  //read a byte from the EEPROM to see if its blank
  byte contentsAtZero;
  EEPROM.get(0, contentsAtZero);
  Serial.println("EEPROM Contents");
  Serial.println(contentsAtZero);
  Serial.println("----"); 

  //check the EEPROM and/or setup some defaults
  if (!contentsAtZero){
    for (byte i=0; i<wifi_config.getValveCount(); i++){
      wifi_config.valves[i] = wifi_config.dummy_valve;
    }
    wifi_config.dummy_valve = 255;
  }else{
    EEPROM.get(0, wifi_config);
    wifi_config.dummy_valve = 255;
  }
  
  //print obtained values
  Serial.println(F("WIFI values in EEPROM"));
  Serial.println(String("SSID: ") + String(wifi_config.ssid));
  Serial.println(String("Password: ") + String(wifi_config.password));
  Serial.println(F("----"));
  
  //setup wifi with struct's WIFI credentials
  setupWIFI();

  //read file configuration
  if (isWifiConnected){
    Serial.println("Valve configuration stored in EEPROM");
    for (byte i=0; i<wifi_config.getValveCount(); i++){
      Serial.println(String("Valve # ") + String(i) + String(" at pin ") + String(wifi_config.valves[i]) + String(" with label ") + String(wifi_config.valveLabels[i]) + String(" with Sinric ID: ") + String(wifi_config.valveSinricIds[i]));
    }
    Serial.println("----");
  }

  //initialize the valves
  for (byte i=0; i<wifi_config.getValveCount(); i++){ 
    if (wifi_config.valves[i]!=wifi_config.dummy_valve){
      pinMode(wifi_config.valves[i], OUTPUT);
      digitalWrite(wifi_config.valves[i], LOW);
    }else{
      Serial.println(String("Valve ") + i + String(" at ") + wifi_config.valves[i] + String(" is DUMMY"));
    }
  }
  Serial.println("----");
  
  //attach a few handlers to the server
  server.on("/", handleRequest);
  server.on("/action", handleAjax);
  server.on("/configuration.html", handleAdminServe);
  server.on("/configuration.save", handleAdminSave);
  server.on("/configuration.reset", handleAdminReset);
  server.onNotFound(handleNotFound);
  server.begin();

  //debug finished initialization
  Serial.println(F("HTTP server started"));
}

bool loadFromSpiffs(String path){
  String dataType = "text/plain";
  if(path.endsWith("/")) path += "index.htm";
 
  if(path.endsWith(".src")) path = path.substring(0, path.lastIndexOf("."));
  else if(path.endsWith(".html")) dataType = "text/html";
  else if(path.endsWith(".htm")) dataType = "text/html";
  else if(path.endsWith(".css")) dataType = "text/css";
  else if(path.endsWith(".js")) dataType = "application/javascript";
  else if(path.endsWith(".png")) dataType = "image/png";
  else if(path.endsWith(".gif")) dataType = "image/gif";
  else if(path.endsWith(".jpg")) dataType = "image/jpeg";
  else if(path.endsWith(".ico")) dataType = "image/x-icon";
  else if(path.endsWith(".xml")) dataType = "text/xml";
  else if(path.endsWith(".pdf")) dataType = "application/pdf";
  else if(path.endsWith(".zip")) dataType = "application/zip";
  File dataFile = SPIFFS.open(path.c_str(), "r");
  if (server.hasArg("download")) dataType = "application/octet-stream";
  if (server.streamFile(dataFile, dataType) != dataFile.size()) {
  }
 
  dataFile.close();
  return true;
}

String getOptions(uint8_t selectedPin){
  //generate the options for the selects
  String options;
  for (byte i=0; i<sizeof(avail_pins); i++){
      options += "<option value=\"" + String(avail_pins[i]) + "\"";
      if (avail_pins[i] == selectedPin){
        options += " selected";
      }
      options += ">" + String(avail_pins[i]) + "</option>";
  }
  return options;
}

String getWifiSetup(){

  int wifiNetworkCount = WiFi.scanNetworks();
  
  //formats the relay input names and assigns their value
  String sHTML = "<tr><td>GPIO</td>";
         sHTML += "<td><select name=\"pin_$\" class=\"inputsmall\"><!--OPTIONS_$-->";
         sHTML += "</select></td>";
         sHTML += "<td>GPIO</td>";
         sHTML += "<td><select name=\"pin_#\" class=\"inputsmall\"><!--OPTIONS_#-->";
         sHTML += "</select></td></tr>";
         //break before the next field
         sHTML += "<tr><td>Label</td><td><input type=\"text\" name=\"label_$\" value=\"relay_label_value_$\" required maxlength=\"20\"/></td>";
         sHTML += "<td>Label</td><td><input type=\"text\" name=\"label_#\" value=\"relay_label_value_#\" required maxlength=\"20\"/></td></tr>";
         sHTML += "<tr class=\"spaceAfter\"><td>Sinric ID</td><td><input type=\"text\" name=\"sinric_$\" value=\"sinric_value_$\" class=\"inputlarge\"/></td>";
         sHTML += "<td>Sinric ID</td><td><input type=\"text\" name=\"sinric_#\" value=\"sinric_value_#\" class=\"inputlarge\"/></td></tr>";
          
  String out = "";
  
  for (int i=0; i<wifi_config.getValveCount()/2; i++){
    char valve_no[2];
    char valve_pin[2];
    String relay_label_value;
    char valve_no_advanced[2];
    char valve_pin_advanced[2];
    String relay_label_value_advanced;

    memset(valve_no, '\0', sizeof(valve_no));
    memset(valve_pin, '\0', sizeof(valve_pin));
    memset(valve_no_advanced, '\0', sizeof(valve_no_advanced));
    memset(valve_pin_advanced, '\0', sizeof(valve_pin_advanced));

    itoa((i + 1), valve_no, 10);
    itoa(wifi_config.valves[i], valve_pin, 10);
    itoa((i + 1 + wifi_config.getValveCount()/2), valve_no_advanced, 10);
    itoa((wifi_config.valves[(i + (wifi_config.getValveCount()/2))]), valve_pin_advanced, 10);
    relay_label_value = wifi_config.valveLabels[i];
    relay_label_value_advanced = wifi_config.valveLabels[(i + wifi_config.getValveCount()/2)];
    
    out += sHTML;
    
    out.replace("<!--OPTIONS_$-->", getOptions(wifi_config.valves[i]));
    out.replace("pin_$", String("pin_") + String(i));
    out.replace("label_$", String("label_") + String(i));
    out.replace("relay_label_value_$", relay_label_value);
    out.replace("sinric_$", String("sinric_") + String(i));
    out.replace("sinric_value_$", String(wifi_config.valveSinricIds[i]));
    
    if (i < (wifi_config.getValveCount()/2)){
      out.replace("<!--OPTIONS_#-->", getOptions(wifi_config.valves[i+wifi_config.getValveCount()/2]));
      out.replace("pin_#", String("pin_") + String(i+wifi_config.getValveCount()/2));
      out.replace("label_#", String("label_") + String(i+wifi_config.getValveCount()/2));
      out.replace("relay_label_value_#", relay_label_value_advanced);
      out.replace("sinric_#", String("sinric_") + String(i+wifi_config.getValveCount()/2));
      out.replace("sinric_value_#", String(wifi_config.valveSinricIds[i+wifi_config.getValveCount()/2]));
    }
  }

  String outtable = "<table><tr><th colspan=\"4\">Pin Assignment</th></tr><!-- RELAYS INNER --></table>";

  outtable.replace("<!-- RELAYS INNER -->", out);

  //replace the whole includes section with our HTML
  configPage.replace("<!-- RELAYS -->", outtable);

  //populate the HOST value
  configPage.replace("name=\"_host\"", "name=\"_host\" value=\"" + String(wifi_config.host) + "\"");

  //populate the wifi networks
  out = "";
  if (wifiNetworkCount==0){
      
  }else{
    for (int i=0; i<wifiNetworkCount; i++){
      out += "<option value=\"" + WiFi.SSID(i) + "\"";
      if (String(wifi_config.ssid)==WiFi.SSID(i)){
        out += " selected";
      }
      out += ">" + WiFi.SSID(i) + "</option>";
    }

    configPage.replace("<!--WIFI_OPTIONS-->", out);
  }
  
  return configPage;
}

String getPageStart(char host[]){
  pagestart.replace("HOST_$", String(host));
  return pagestart;
}

void loop(void){
  //process incoming requests
  server.handleClient();
  ArduinoOTA.handle();
  webSocket.loop();

  if (isConnected) {
      uint64_t now = millis(); 
      // Send heartbeat in order to avoid disconnections during ISP resetting IPs over night. Thanks @MacSass
      if((now - heartbeatTimestamp) > HEARTBEAT_INTERVAL) {
          heartbeatTimestamp = now;
          webSocket.sendTXT("H");          
      }
  }   
}

//handle a 404 not found
void handleNotFound(){
  if (loadFromSpiffs(server.uri())) return;
  
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
  String page = getPageStart(wifi_config.host);
  page += getWifiSetup();
  page += pageend;
  
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
    strcpy(wifi_config.ssid, _ssid);
    strcpy(wifi_config.password, _password);
    strcpy(wifi_config.host, _host);
  }else{
    //return error
    server.sendHeader("Location", "/configuration.html?error=1", true);
    flashLCDWith("ERROR");
    server.send(302, "text/plain", "");
    return;
  }

  Serial.println(F("Object stored values for configuration"));
  Serial.println(String("SSID: ") + wifi_config.ssid);
  Serial.println(String("Password: ") + wifi_config.password);
  Serial.println(String("Host: ") + wifi_config.host);
  Serial.println(F("-----"));

  Serial.println("Saving Valves...");
  //relay information
  for (byte i=0; i<wifi_config.getValveCount(); i++){
    uint8_t _pin = server.arg(String("pin_") + String(i)).toInt();
    String _label = server.arg(String("label_") + String(i));
    String _sinric = server.arg(String("sinric_") + String(i));

    Serial.println(String("Saving valve: ") + i + String(" Pin: ") + _pin + String(" Label: ") + _label + String(" Sinric ID: ") + _sinric);

    if (_label.length()!=0){
      //copy the pins and labels assigned
      wifi_config.valves[i] = _pin;
      strcpy(wifi_config.valveLabels[i], _label.c_str());
      strcpy(wifi_config.valveSinricIds[i], _sinric.c_str());
    }else{
      //return error
      server.sendHeader("Location", "/configuration.html?error=1", true);
      flashLCDWith("ERROR");
      server.send(302, "text/plain", "");
      return;
    }
  }
  Serial.println("----");
  //end relay information

  //do a final update on the eeprom
  EEPROM.put(0, wifi_config);
  EEPROM.commit();

  lcd.clear();
  lcd.print(F("CONFIG SAVED"));
  delay(2000);

  server.send(200, "text/html", redirect);
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
  
  int valveno = wifi_config.dummy_valve;
  bool valvestatus = false;  //fail-safe for all valves

  //page starts
  String page = "";
  page += getPageStart(wifi_config.host);

  Serial.println(String("Valve count: ") + sizeof(wifi_config.valves));
  Serial.println(String("Dummy valve: ") + wifi_config.dummy_valve);

  //valves
  page += "<div id='valves'>";
  for (byte i=0; i<wifi_config.getValveCount(); i++){

    //Serial.println(String("Processing valve relay: ") + wifi_config.valves[i]);
    
    bool isDummy = (wifi_config.valves[i] == wifi_config.dummy_valve);    
    page += makeValve(i, wifi_config.valveStatuses[i], isDummy);
  }
  page += "</div>";
  page += pageend;

  server.send(200, "text/html", page);
}

//handle all ajax requests for toggling the valves
void handleAjax(){
  int valveno = wifi_config.dummy_valve;
  bool valvestatus = false;  //fail-safe for all valves
  
  valveno = server.arg("valveno").toInt();
  valvestatus = (server.arg("valvestatus")=="on" ? true : (server.arg("valvestatus")=="off" ? false : false));  //default valves to off when undefined
  
  //do nothing for dummy valves
  if (valveno!=wifi_config.dummy_valve){
    //digitalWrite(0, valvestatus);
    if (wifi_config.valves[valveno]==0){
      digitalWrite(0, valvestatus ? HIGH : LOW);
      wifi_config.valveStatuses[valveno] = valvestatus;
    }else{
      digitalWrite(wifi_config.valves[valveno], valvestatus ? HIGH : LOW);
      wifi_config.valveStatuses[valveno] = valvestatus;
    }
  }

  Serial.println(F("Valve status change"));
  Serial.println(String("Valve no received: ") + valveno);
  Serial.println(String("Valve pin: ") + wifi_config.valves[valveno]);
  Serial.println(String("Valve status received: ") + valvestatus);
  Serial.println(F("----"));

  String out = "";
  StaticJsonBuffer<700> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  JsonArray& valves = root.createNestedArray("valves");

  //Serial.println(String("Processing valves count: ") + wifi_config.getValveCount());

  for (byte i=0; i<wifi_config.getValveCount(); i++){
    bool isDummy = (wifi_config.valves[i] == wifi_config.dummy_valve); //variable redeclaration ????
    String _state = (wifi_config.valveStatuses[i] ? "ON" : "OFF");
    if (isDummy) _state = "OFFLINE"; 

    Serial.println(String("Writing JSON for valve ") + i);

    JsonObject& valve = valves.createNestedObject();
    valve["no"] = i;
    valve["status"] = _state;
    valve["label"] = wifi_config.valveLabels[i];
  }

  //root.prettyPrintTo(Serial);
  root.prettyPrintTo(out);

  server.send(200, "text/html", out);
}

//build an SVG valve to display in the html
String makeValve(int i, bool state, bool isDummy){

  String dwg = "<div class='valve' status='status_$' id='valve_$'><svg xmlns='http://www.w3.org/2000/svg' xmlns:xlink='http://www.w3.org/1999/xlink' version='1.1' baseProfile='full' viewBox='0 0 590 590' width='128px' height='128px'>";
  dwg += "<g><g><g> <path id='valve_path_$' d='M490,250V80h-71.188C411.598,34.719,372.28,0,325,0c-47.28,0-86.598,34.719-93.812,80H190V62.293L116.241,50H0v230 h116.241L190,267.707V250h40v70h-20v90h20v80h190v-80h20v-90h-20v-70H490z M160,242.293L113.758,250H30v-36.667h85.417 L160,210.856V242.293z M160,180.81l-45.416,2.523H30v-36.667h84.584L160,149.19V180.81z M160,119.144l-44.583-2.477H30V80h83.758 L160,87.707V119.144z M325,30c35.841,0,65,29.159,65,65s-29.159,65-65,65s-65-29.159-65-65S289.159,30,325,30z M390,460H260v-50 h130V460z M410,380H240v-30h170V380z M390,220v100H260V220h-70V110h41.188c7.214,45.281,46.532,80,93.812,80 c47.28,0,86.598-34.719,93.812-80H460v110H390z' fill='COLOR_$'/>";
  dwg += "<text id='valve_text_$' class='valve_label' x='0' y='550' font-family='Tahoma' font-size='50'>TEXT_</text></g></g></g></svg></div>";

  String _state = (state ? "ON" : "OFF");

  if (isDummy) _state = "OFFLINE"; 

  //Serial.println("VALVE COLOR");
  //Serial.println((isDummy ? wifi_config.OFFL : (state ? wifi_config.ON : wifi_config.OFF)));
  //Serial.println("----");

  dwg.replace("COLOR_$", (isDummy ? "gray" : (state ? "#00D800" : "#FF0000")));
  dwg.replace("status_$", _state);
  dwg.replace("valve_$", String("valve_") += String(i));
  dwg.replace("valve_path_$", String("valve_path_") += String(i));
  dwg.replace("valve_text_$", String("valve_text_") += String(i));
  dwg.replace("TEXT_", String(wifi_config.valveLabels[i]) + String(": ") + _state);

  return dwg;
}

void handleAdminReset(){
  for (int i=0; i<EEPROM_SIZE; i++){
    EEPROM.write(i, 0);
  }
  EEPROM.commit();
  
  server.send(200, "text/html", redirect);
  delay(1000);

  ESP.restart();
}

void setupAP(){
  //de-initialize the wifi
  WiFi.disconnect();
  
  boolean result = WiFi.softAP("BETTINA", "password123");
  if (result){
    Serial.println(F("AP Mode started."));
    lcd.print(F("Connect to SSID"));
    lcd.setCursor(0, 1);
    IPAddress myIP = WiFi.softAPIP();
    lcd.print(String("IP ADDR: ") + myIP);
    lcd.setCursor(0, 2);
    lcd.print(F("SSID: BETTINA"));
    lcd.setCursor(0, 3);
    lcd.print(F("PASS: password123"));
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
  if (!strlen(wifi_config.ssid) || !strlen(wifi_config.password)){
    setupAP();
    return;
  }

  WiFiMulti.addAP(wifi_config.ssid, wifi_config.password);
  Serial.print("Connecting to Wifi: ");
  Serial.println(wifi_config.ssid);  
  lcd.print(F("CONNECTING "));

  // Waiting for Wifi connect
  int waitCount = 0;
  int waitMax = 30;
  while(WiFiMulti.run() != WL_CONNECTED) {
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
      Serial.println(F(""));
      Serial.println(String("Connection attempts to ") + wifi_config.ssid + String(" exceeded."));
      Serial.println(String("Setting up Access Point."));  
      setupAP();
      return;
    }
  }
  
  if(WiFiMulti.run() == WL_CONNECTED) {
    Serial.println("");
    Serial.print("WiFi connected. ");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }
  
  isWifiConnected = true;

  //destroy the access point, we're now on the network
  WiFi.softAPdisconnect(true);

  // server address, port and URL
  webSocket.begin("iot.sinric.com", 80, "/");

  // event handler
  webSocket.onEvent(webSocketEvent);
  webSocket.setAuthorization("apikey", MyApiKey);
  
  // try again every 5000ms if connection has failed
  webSocket.setReconnectInterval(5000);

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
  Serial.println(wifi_config.host);

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
