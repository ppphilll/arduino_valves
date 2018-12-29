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
  int dummy_valve = 255;
  
  //valves
  uint8_t valves[8] = {D6, D4, D3, D5, 255, 255, 255, 255};
  char* valveLabels[8] = {"Garden Lights", "Gate", "Watering", "Unused", "Unused", "Unused", "Unused", "Unused"};
  bool valveStatuses[8] = {false, false, false, false, false, false, false, false};
  
  //getters
  int getValveCount(){
    return (sizeof(valves)/sizeof(*valves));
  }

  //colors for valves
  char ON[7] = "lime"; 
  char OFF[7] = "red";
  char OFFL[7] = "grey";

  //html source code for UI
  //char valve[1050] = "<div class='valve' status='status_$' id='valve_$'><svg xmlns='http://www.w3.org/2000/svg' xmlns:xlink='http://www.w3.org/1999/xlink' version='1.1' baseProfile='full' viewBox='0 0 590 590' width='128px' height='128px'><g><g><g> <path id='valve_path_$' d='M490,250V80h-71.188C411.598,34.719,372.28,0,325,0c-47.28,0-86.598,34.719-93.812,80H190V62.293L116.241,50H0v230 h116.241L190,267.707V250h40v70h-20v90h20v80h190v-80h20v-90h-20v-70H490z M160,242.293L113.758,250H30v-36.667h85.417 L160,210.856V242.293z M160,180.81l-45.416,2.523H30v-36.667h84.584L160,149.19V180.81z M160,119.144l-44.583-2.477H30V80h83.758 L160,87.707V119.144z M325,30c35.841,0,65,29.159,65,65s-29.159,65-65,65s-65-29.159-65-65S289.159,30,325,30z M390,460H260v-50 h130V460z M410,380H240v-30h170V380z M390,220v100H260V220h-70V110h41.188c7.214,45.281,46.532,80,93.812,80 c47.28,0,86.598-34.719,93.812-80H460v110H390z' fill='COLOR_'/><text id='valve_text_$' class='valve_label' x='0' y='550' font-family='Tahoma' font-size='50'>TEXT_</text></g></g></g></svg></div>";
};

#endif
