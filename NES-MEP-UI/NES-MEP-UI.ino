// Links:
// Over the Air Update: https://lastminuteengineers.com/esp32-ota-web-updater-arduino-ide/
// Store Preferences in flash - see reboot command: https://randomnerdtutorials.com/esp32-save-data-permanently-preferences/
// AP mode: https://randomnerdtutorials.com/esp32-access-point-ap-web-server/

// Preferences
#include <Preferences.h>
#include <nvs_flash.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include "SPIFFS.h"
#include "NES-MEP.h"
#include "NES-MEP-RecoveryWebPages.h"
#include "NES-MEP-WebPages.h"

Preferences preferences;
IPAddress IP;
WebServer MyWebServer(80);

MEPQueueStruct MEPQueue[MaxMEPBuffer];
byte MEPQueueNextIndex = 0;
byte MEPQueueSendIndex = 0;
unsigned long LastSentMillis = 0;
unsigned long LastReceiveMillis = 0;
boolean SentAwaitingReply = false;
boolean FirstRequestOk = false;
boolean GetConfigurationRequestsSent = false;
boolean APMode = false;
long UserLoginSession = random(LONG_MAX);
long SecretLoginSession = random(LONG_MAX);
char wifi_ssid[33] = "";
char wifi_password[65] = "";
char user_login[33] = "";
char user_password[65] = "";
char mep_key[21] = "";
const char* host = "esp32-mep";
uint32_t previousMillis; 
byte InputBuffer[MaxMEPReplyLength];
unsigned long InputBufferLength = 0;

bool PreferencesOk(void) {
  return(preferences.getString(host,"") == host);
}

void InitializePreferences(void) {
  preferences.end();
  nvs_flash_erase();
  nvs_flash_init();
  preferences.begin(host, false); 
  preferences.putString(host, host);
}

/* setup function */
void setup(void) {
  Serial.begin(115200);
  Serial2.begin(9600);

  randomSeed(analogRead(0)); // Should be before WiFI to prevent "__analogRead(): GPIO0: ESP_ERR_TIMEOUT: ADC2 is in use by Wi-Fi."

  // Preferences
  preferences.begin(host, false); 
  if(PreferencesOk()) {
    Serial.println("Preferences are valid");   
  }
  else {
    InitializePreferences();
    Serial.println("Preferences initialized");
  }

  // Get preferences
  preferences.getString("wifi_ssid","").toCharArray(wifi_ssid,sizeof(wifi_ssid));
  preferences.getString("wifi_password","").toCharArray(wifi_password,sizeof(wifi_password));
  preferences.getString("user_login","mep").toCharArray(user_login,sizeof(user_login));
  preferences.getString("user_password","").toCharArray(user_password,sizeof(user_password));
  preferences.getString("mep_key","0000000000000000000000000000000000000000").toCharArray(mep_key,sizeof(mep_key));

  if(String(mep_key) == "") 
  {
    String("0000000000000000000000000000000000000000").toCharArray(mep_key,sizeof(mep_key));
  }

  if(String(user_password) == "") {
    UserLoginSession = SecretLoginSession; // We are always authenticated if no user-login/password is setup
  }

  // Connect to WiFi network
  Serial.printf("wifi_ssid: '%s'\r\nwifi_pwd: '%s'\r\n",wifi_ssid,wifi_password);
  Serial.printf("user_login: '%s'\r\nuser_pwd: '%s'\r\n",user_login,user_password);
  Serial.printf("mep_key: '%s'",mep_key);

  if(wifi_password == "") {
    WiFi.begin(wifi_ssid);
  }
  else {
    WiFi.begin(wifi_ssid,wifi_password);
  } 
  Serial.println("");

  // Wait for connection
  previousMillis = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    
    if((millis()-previousMillis > 30000) || String(wifi_ssid) == "") {
      Serial.print("Setting up AP (Access Point) ");
      Serial.println(host);
      WiFi.softAP(host, "12345678"); // TO-DO: Brug host + måler SN som SSID og måler SN som password

      IP = WiFi.softAPIP();
      Serial.print("AP IP address: ");
      Serial.println(IP);
      UserLoginSession = SecretLoginSession; // We are always authenticated in AP mode
      APMode = true;
      break;
    }
  }

  if(WiFi.status() == WL_CONNECTED)
  {
    IP = WiFi.localIP();
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(wifi_ssid);
    Serial.print("IP address: ");
    Serial.println(IP);
  }
   
  /*use mdns for host name resolution*/
  if (!MDNS.begin(host)) { //http://<host>.local
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }
  Serial.println("mDNS responder started");

  SetupRecoveryWebPages();
  SetupWebPages();
  MyWebServer.begin();

  SPIFFS.begin(true); // true=format if we cannot mount
  RS3232Enable(true);
  delay(1000);
  MEPEnable(true);
  delay(1000);
  SerialEvent2();
  InputBufferLength = 0;

  // Get started by requesting the UTC Clock
  if(String(mep_key) == "00000000000000000000") {
    Serial.println("No MEP key set, disabling MEP communication until next reboot...");
  }
  else {
    queueRequest("300034",mep_key,MEPQueue,&MEPQueueNextIndex,None); // BT52: UTC Clock
  }
}

void SerialEvent2() {
  while(Serial2.available() && InputBufferLength < MaxMEPReplyLength) {
    InputBuffer[InputBufferLength] = (byte)Serial2.read();
    //Serial.printf("Received character code: %i\r\n",InputBuffer[InputBufferLength]);
    InputBufferLength++;
    LastReceiveMillis = millis();
  }
}

void loop(void) {
  byte j;

  if((!APMode) && (WiFi.status() != WL_CONNECTED)) {
    Serial.println("Disconnected from WIFI access point");
    Serial.println("Reconnecting...");
    if(wifi_password == "") {
      WiFi.begin(wifi_ssid);
    }
    else {
      WiFi.begin(wifi_ssid,wifi_password);
    } 
    delay(30000);
    Serial.println("");
  }
   
  // Run web server
  MyWebServer.handleClient();

// TO-DO: Hvad hvis vi aldrig får svar?

  // Receiving packages from meter

  // Timeout waiting for answer from meter
  if(millis() < LastSentMillis) {
    LastSentMillis = millis();
  }
  if(SentAwaitingReply) {
    if(millis() - LastSentMillis > 10000) {
      MEPEnable(false);
      RS3232Enable(false);
      Serial.printf("RS3232 reset 1/2. Dropping buffer with this contents:\r\n");
      dumpByteArray(InputBuffer,InputBufferLength);       
      InputBufferLength = 0;
      SentAwaitingReply = false;
      delay(1000);
      RS3232Enable(true);
      delay(1000);
      MEPEnable(true);
      delay(1000);
      SerialEvent2();
      Serial.printf("RS3232 reset 2/2. Dropping buffer with this contents:\r\n");
      dumpByteArray(InputBuffer,InputBufferLength);       
      InputBufferLength = 0;
    }
  }
 
  SerialEvent2();
  if(InputBufferLength > 0) {
    if(PackageIsValid(InputBuffer,InputBufferLength))
    {
      HandleAlertSequence(InputBuffer,&InputBufferLength,mep_key,MEPQueue,&MEPQueueNextIndex);

      if(InputBufferLength >= MaxMEPReplyLength) {
        queueResponseWithNoRequest(InputBuffer,InputBufferLength,MEPQueue,&MEPQueueNextIndex);
        Serial.printf("Error: Input buffer overflow. InputBufferLength: %i Dropping buffer with this contents:\r\n",InputBufferLength);
        dumpByteArray(InputBuffer,InputBufferLength);       
        InputBufferLength = 0;
      }

      if(InputBufferLength > 0)
      {
        if(!SentAwaitingReply)
        {
          queueResponseWithNoRequest(InputBuffer,InputBufferLength,MEPQueue,&MEPQueueNextIndex);
          Serial.printf("Error: InputBufferLength: %i, got this reply from the meter, but did not expect one:\r\n",InputBufferLength);
          dumpByteArray(InputBuffer,InputBufferLength);       
          InputBufferLength = 0;
        }
        else
        {
          MEPQueue[MEPQueueSendIndex].ReplyLength = GetPackageLength(InputBuffer);
          memcpy(MEPQueue[MEPQueueSendIndex].Reply,InputBuffer,MEPQueue[MEPQueueSendIndex].ReplyLength);
          memcpy(InputBuffer,InputBuffer+MEPQueue[MEPQueueSendIndex].ReplyLength,InputBufferLength-MEPQueue[MEPQueueSendIndex].ReplyLength);
          InputBufferLength -= MEPQueue[MEPQueueSendIndex].ReplyLength;
  
          HandleInvalidSequenceNumber(mep_key,MEPQueue,MEPQueueSendIndex,&MEPQueueNextIndex);
  
          Serial.printf("Saved request response at index %i\r\n",MEPQueueSendIndex);

          if(!GetConfigurationRequestsSent)
          {
            if(!FirstRequestOk) // Read UTC clock is first request (queued in Setup)
            {
              FirstRequestOk = (MEPQueue[MEPQueueSendIndex].Reply[2] == 0x00);
            }
            
            if(FirstRequestOk)
            {
              // Read the meter configs we need to decode other requests
              queueRequest("300001",mep_key,MEPQueue,&MEPQueueNextIndex,None); // BT01: General Manufacturer Information
              queueRequest("300015",mep_key,MEPQueue,&MEPQueueNextIndex,None); // BT21: Actual Register
              queueRequest("300803",mep_key,MEPQueue,&MEPQueueNextIndex,None); // ET03: Utility Information
              queueRequest("30080B",mep_key,MEPQueue,&MEPQueueNextIndex,None); // ET11: MFG Dimension Table
              queueRequest("30080D",mep_key,MEPQueue,&MEPQueueNextIndex,None); // ET13: MEP/M-Bus Device Configuration
              queueRequest("300832",mep_key,MEPQueue,&MEPQueueNextIndex,None); // ET50: MEP Inbound Data Space
              GetConfigurationRequestsSent = true;
            }
          }

          if(FirstRequestOk)
            ReplyData2String(MEPQueue,MEPQueueSendIndex,true);
          
          HandleNextAction(mep_key,MEPQueue,MEPQueueSendIndex,&MEPQueueNextIndex);
          
          SentAwaitingReply = false;
          
          IncreaseMEPQueueIndex(&MEPQueueSendIndex);
        }
      }
    }
    else {
      if((!SentAwaitingReply) && (millis()-LastReceiveMillis > 100)) {
        Serial.printf("Error: InputBufferLength: %i, got this garbage from the meter, but did not expect a package:\r\n",InputBufferLength);
        dumpByteArray(InputBuffer,InputBufferLength);       
        InputBufferLength = 0;
      }
    }
  }

  // Sending packages to meter
  if(!SentAwaitingReply)
  {
    j = 0;
    while((MEPQueue[MEPQueueSendIndex].ReplyLength > 0) && (j <= MaxMEPBuffer)) // (MEPQueue[MEPQueueSendIndex].RequestLength == 0) && 
    {
      IncreaseMEPQueueIndex(&MEPQueueSendIndex);
      j++;
    }
    if((MEPQueue[MEPQueueSendIndex].RequestLength > 0) && (MEPQueue[MEPQueueSendIndex].ReplyLength == 0)) {
      if(MEPQueue[MEPQueueSendIndex].SendAttempts >= MaxSendAttempts) {
        Serial.printf("Giving up on reqest at index %i - too may retries\r\n");
        IncreaseMEPQueueIndex(&MEPQueueSendIndex);
      }
      else
      {
        MEPQueue[MEPQueueSendIndex].SendAttempts++;
        Serial.printf("Transmitting request at index %i (attempt %i)\r\n",MEPQueueSendIndex,MEPQueue[MEPQueueSendIndex].SendAttempts);
        for(unsigned long i = 0; i < MEPQueue[MEPQueueSendIndex].RequestLength; i++) {
          Serial2.write((byte)MEPQueue[MEPQueueSendIndex].Request[i]);
        }    
        LastSentMillis = millis();
        SentAwaitingReply = true;
      }
    }
    else
    {
      if(millis()-LastSentMillis > 2000) {
        if(FirstRequestOk)
        {
          // Update consumption data
          queueRequest("300017" + MaxMEPReplyLengthAsHex(),mep_key,MEPQueue,&MEPQueueNextIndex,None);
          queueRequest("30001C" + MaxMEPReplyLengthAsHex(),mep_key,MEPQueue,&MEPQueueNextIndex,None);
        }
      }
    }
  }
}
