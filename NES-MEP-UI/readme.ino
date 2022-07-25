/*
https://techtutorialsx.com/2019/07/21/esp32-arduino-updating-firmware-from-the-spiffs-file-system/
https://github.com/tobozo/ESP32-targz

- beskyt recovery den med ssid/kode

0. Skift alarm procedurekald til at bruge config værdi-tabellen fremfor at spørge efter dem 
0.1 MAC som en del af navn: https://www.esp32.com/viewtopic.php?t=2914
0.2 Måske skal vi enable noget watchdog (https://iotassistant.io/esp32/enable-hardware-watchdog-timer-esp32-arduino-ide/) eller måske er det WiFi der dør så vi skal holde øje med det og reboote når det sker?
0.3 Check Digest på retursvar fra måler
0.4 Håndter delays når måler ikke svarer korrekt...
0.5 Fjern FTP fra og list i stedet SPIFF filer med størrelse/dato på recovery siden
1. Index -> Request no (fortløbende) på tabel
2. Sorter tabel kronologisk bagfra (nyeste først)
3. Origin på tabel (System, user raw, user predefined, dashboard, webservice, mqtt, mrtg etc. etc.
4. Millis -> Time Date på tabel
5. grafisk dashboard
6. webservice
7. MQTT
8. MRTG
9. simpel e-mail-advarsel
10. simpel e-mail forbrugsaflæsning
11. RJ12 protokoller
*/
