#ifndef _CONFIGURATION_H
#define _CONFIGURATION_H

#include <Arduino.h>

struct wifi_config_t
{
	//wifi
	char ssid[20];
	char password[20];
	
	//host
	char host[15]; //host for the includes
};

struct config_t
{
	//valves
	int dummy_valve = 255;
	
	//valves
	uint8_t valves[8] = {D6, D4, D3, D5, 255, 255, 255, 255};
	char* valveLabels[8] = {"Garden Lights", "Gate", "Watering", "Unused", "Unused", "Unused", "Unused", "Unused"};
	bool valveStatuses[8] = {false, false, false, false, false, false, false, false};
	
	//getters
	int getValveCount(){
		return (sizeof(valves)/sizeof(*valves));
	}
	
	//ui config
	char ON[8]    = "#00D800";  //on color
	char OFF[8]   = "#FF0000";  //off color
	char OFFL[8]  = "gray";  //offline color
	
	//html includes
	String pagestart_noscript = "<html><head></head><body><div class='header'>Valve Control System</div>";
	String pagestart = "<html><head><script src=\"https://ajax.googleapis.com/ajax/libs/jquery/3.3.1/jquery.min.js\"></script><script src=\"http://HOST_$/valves/script.js\"></script><link rel=\"stylesheet\" href=\"http://HOST_$/valves/style.css\"></head><body><div class='header'>Valve Control System</div>";
	String pageend = "</body></html>";
	String wifisetup = "<form action=\"configuration.save\" method=\"POST\"><label for=\"_ssid\">SSID</label><input type=\"text\" name=\"_ssid\"/><label for=\"_password\">Password</label><input type=\"password\" name=\"_password\"/><label for=\"_host\">Host</label><input type=\"text\" name=\"_host\"/><input type=\"submit\" /></form>";
  		
	String getPageStart(char host[]){
    pagestart.replace("HOST_$", String(host));
    return pagestart;
	}
};

#endif
