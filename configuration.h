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
  String valveSinricIds[8] = {"5c2d7118a4410b42470f1189", "5c2d75cca4410b42470f119e", "5c2d782da4410b42470f11a0", "5c2d8834a651bf490ee59408", "", "", "", ""};
  char valveLabels[8][20];
  bool valveStatuses[8] = {false, false, false, false, false, false, false, false};
  
  //getters
  int getValveCount(){
    return (sizeof(valves)/sizeof(*valves));
  }
};

#endif
