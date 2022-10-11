# MEP-SW-ESP32
Multipurpose Expansion Port (MEP) ESP32 Software for OSGP Smart Meters

This is the repository holding the Software for OSGP Smart Meters (i.e. Echelon or NES). Please:

- see https://github.com/OSGP-Alliance-MEP-and-Optical for further hardware specifiactions of these meters.
- see https://www.dabbler.dk for further information about this software project

Note:
- The software has been developed and compiled using Arduino IDE (we have tested v. 1.8.13 and 2.0.0, but will probably also work on other/later versions
- We have added this URL as "Additional Board Manager URLs" (the File/Properties menu): https://dl.espressif.com/dl/package_esp32_index.json
- During development we are using the "DOIT ESP32 DEVKIT V1" board from the "esp32 by Espressif Systems v. 1.0.6" package
- We are reducing upload speed to 115200 for stability when uploading through the FTDI232/J5 connector
- The previous dependency on "SimpleFTPServer by Renzo Mischianti v. 2.1.2" has been removed as we only needed it to debug the SPIFFS wwws upload etc.
- You need to obtain the MEP key (MBK key) yourself via your power company (where you buy yor power). It is a 20 ASCII character key (it might look like it is HEX, but it is NOT - so please do not try to convert it!). We actually only need the first 16 ASCII characters, but if you enter the full key the software will handle it correctly.

Initial startup procedure - when our software is NOT installed:
As we are using the SPIFFS filesystem for web files and your ESP32 most likely does not have our software installed, you need to follow this procedure for the initial startup of the module:
1. Warning: Make sure to adjust the DC-DC buck converter to 3.6v before sending any power to the ESP32 module. See the hardware notes.
2. Warning: Make sure your FTDI232 is only providng 3.3v when jumped to 3.3v. Some of them sends 5v power and that will fry your ESP32.
3. Install a jumper between pin 1 and 2, and remove the jumper between pin 3 and 4 on J7. This will make sure the ESP32 is in programming mode and gets power from the FTDI232 module
4. Insert a FTDI232 module in the J5 holes. If you angle it, the pin headers of the FTDI232 will make a fair connection to the PCB holes. Make sure to orient the FTDI232 module correctly: on MEP module version 1.10 and above, the square pad is GND.
5. You should now be able to compile and uploade the software to the ESP32 from the Arduino IDE using the reglar upload function
6. Remove the jumper between pin 1 and 2, and set the jumper between pin 3 and 4 on J7 for normal operation
7. Now insert the module in your meter (or power it on with a powersupply or similar)
8. The ESP32 will try to connect to the configured WiFi access point (AP) if any, but as it is not configured yet, this will be skipped
9. When no WiFi is configured or the module cannot connect to the configured WiFi (it will try for around one minute), it will go into AP Mode with the name "esp32-mep" (subject to change in the future). Connect a device with a browser to this AP. If you are asked for a password use 123456 or 12345678
10. Open the page "http://192.168.4.1/recovery". This recovery page supports formatting the SPIFFS file system, uploading of firmware (*.bin) files and web (*.wwws) files, and is independent of the web files on the SPIFFS (so it should always work). As just just programmed the initial firmware, you only have to format the SPIFFS file system and upload the web (*.wwws) file now. You will find the latest update.wwws file in the "NES-MEP-UI-SPIFFS"-folder
11. After uploading the update.wwws file, please reboot the module
12. Wait around a minute so the module enters AP Mode again, open the page http://192.168.4.1/. You will see a menu bar at top
13. Go to the Configure menu. If you are prompted for a login/password, just leave the fields empty as you have not setup any values yet and hit the login button. On the config page you should fill these fields: WiFi SSID and Password, the User login and Password you want on the module and the MBK key you got from your power company. Save and restart the module

Congratulations. Your module is now up and running and should connect to your WiFi.


Recovery startup procedure - when our software IS already installed:
1. Insert the module in your meter (or power it on with a powersupply or similar)
2. The ESP32 will try to connect to the configured WiFi access point (AP) if any, but as it is not configured yet, this will be skipped
3. When no WiFi is configured or the module cannot connect to the configured WiFi (it will try for around one minute), it will go into AP Mode with the name "esp32-mep" (subject to change in the future). Connect a device with a browser to this AP. If you are asked for a password use 123456 or 12345678
4. Open the page "http://192.168.4.1/recovery". This recovery page supports formatting the SPIFFS file system, uploading of firmware (*.bin) files and web (*.wwws) files, and is independent of the web files on the SPIFFS (so it should always work). As just just programmed the initial firmware, you only have to format the SPIFFS file system and upload the web (*.wwws) file now. You will find the latest update.wwws file in the "NES-MEP-UI-SPIFFS"-folder
5. Optional: If you think the SIPFFS file system is corrupted and/or would like to start over. Format it. It will take a few seconds, but the page should reload when the ESP32 is done
6. After uploading the .bin-file, please reboot the module
7. Wait a bit so the module enters AP Mode again (might be up to a minute or so if WiFi is configured), re-open the page http://192.168.4.1/revovery
8. After uploading the update.wwws file, please reboot the module
9. Wait a bit so the module enters AP Mode again (might be up to a minute or so if WiFi is configured), open the regular (non-recovery) start-page http://192.168.4.1/. You will see a menu bar at top
10. Go to the Configure menu. If you are prompted for a login/password, just leave the fields empty as you have not setup any values yet and hit the login button. On the config page you should fill these fields: WiFi SSID and Password, the User login and Password you want on the module and the MBK key you got from your power company. Save and restart the module

Congratulations. Your module is now up and running and should connect to your WiFi.


The Dashboard page should work without login/password, but for the rest of the pages you need to supply the login/password you chose on the configuration page (the module also accept your wifi name and password as a fail safe).

Regular software update procedure:
1. Get the new files from this repository. The latest *.bin file is located here "NES-MEP-UI\build\esp32.esp32.esp32doit-devkit-v1\NES-MEP-UI.ino.bin". The latest .wwws file is located here "NES-MEP-UI-SPIFFS\update.wwws"
2. Open the module start page and go to the "Upload firmware" menu. First upload the .bin file, let the module reboot and then upload the .wwws file
3. Close the browser page and reopen it

Note: If this fails for any reason, you can also use the recovery page (http://esp32-mep.local/recovery) to upload both files
