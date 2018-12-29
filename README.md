# arduino_valves

Use https://codebeautify.org/javascript-escape-unescape to escape and unescape HTML code for storing
in the EEPROM.

Use https://www.freeformatter.com/html-formatter.html to format and minify HTML for storing in EEPROM.

For embedding HTML code in the source code, or avoidance of, the source code is in includes and the
HTTP server reads the includes, streams them to the EEPROM.  Any time you want to change the HTML
source code you simply run a function which imports it all from the HOST, and afterwards the unit becomes
stand alone. 

