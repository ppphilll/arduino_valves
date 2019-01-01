#ifndef _CONFIGURATION_H
#define _CONFIGURATION_H

#include <Arduino.h>

struct wifi_configuration_t
{
	//wifi
	char ssid[20];
	char password[20];
	
	//host
	char host[15]; //host for the includes

  //valves
  uint8_t dummy_valve;
  
  //valves
  uint8_t valves[8] = {12, 2, 0, 14, 255, 255, 255, 255};
  char valveLabels[8][20];
  bool valveStatuses[8] = {false, false, false, false, false, false, false, false};
  
  //getters
  int getValveCount(){
    return (sizeof(valves)/sizeof(*valves));
  }

  //ui config
  String ON = "#00D800";  //on color
  String OFF = "#FF0000";  //off color
  String OFFL = "gray";     //offline color
};

#endif
