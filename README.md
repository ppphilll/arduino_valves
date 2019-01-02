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
 
 # arduino_valves

 #Use https://codebeautify.org/javascript-escape-unescape to escape and unescape HTML code for storing in the EEPROM.

 #Use https://www.freeformatter.com/html-formatter.html to format and minify HTML for storing in EEPROM.

 #For embedding HTML code in the source code, or avoidance of, the source code is in includes and the HTTP server reads the includes, streams them to the EEPROM.  Any time you want to change the HTML source code you simply run a function which imports it all from the HOST, and afterwards the unit becomes stand alone.