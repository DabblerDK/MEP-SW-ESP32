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
#include "NES-MEP-MQTT.cpp"
#include <PubSubClient.h> 
/*
extern MeterInfoStruct MeterInfo;
extern ConsumptionDataStruct ConsumptionData;
*/

void MqttSensorHomeAssistantAutoDisoverySetup();
void MqttConnect();
void MqttReadSendSensorData();
void MqttSetup();
void SerialEvent2();

WiFiClient espClient;
PubSubClient mqttclient(espClient);

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

const char* deviceId = "ESP32-MEP-Dabbler"; // CAN BE REMOVED
bool mqtt_enable = false;
bool hassAutodiscov = false;
char mqtt_topic[25];
char mqtt_server[15];
char mqtt_user[33];
char mqtt_password[65];
char mqtt_topic_hass_create[100];
char mqtt_payload_hass_create[300];
String mqtt_sensor_array[25];

extern MeterInfoStruct MeterInfo;
extern ConsumptionDataStruct ConsumptionData;

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
  // MQTT TEST IMPLEMENTATION
  mqtt_enable = preferences.getBool("mqtt_enable",false);
  preferences.getString("mqtt_topic","").toCharArray(mqtt_topic,sizeof(mqtt_topic));
  preferences.getString("mqtt_server","").toCharArray(mqtt_server,sizeof(mqtt_server));
  preferences.getString("mqtt_user","").toCharArray(mqtt_user,sizeof(mqtt_user));
  preferences.getString("mqtt_password","").toCharArray(mqtt_password,sizeof(mqtt_password));
  hassAutodiscov = preferences.getBool("hassAutodiscov",false);
  // MQTT TEST IMPLEMENTATION END
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
  Serial.printf("mep_key: '%s'\r\n",mep_key);
  Serial.printf("mqtt enable: %d\r\nmqttserver: %s\r\n", mqtt_enable,mqtt_server);
  Serial.printf("mqttuser: %s\r\nmqttpw: %s\r\n", mqtt_user, mqtt_password);
  Serial.printf("hassAutodiscov: %d\r\n", hassAutodiscov);

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

// MQTT TEST IMPLEMENTATION 
  //READ TOGGLE ENABLE HASS AUTODISCOVERY FROM WEBINTERFACE BEFORE EXECUTING FUNCTION
  if(mqtt_enable) {
    MqttSetup(); //only makes sense to set up MQTT when MEP data is present
    if(hassAutodiscov) {
      MqttSensorHomeAssistantAutoDisoverySetup();
    }
  }
//MQTT TEST IMPLEMENTATION END
  
  
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

  // MQTT TEST IMPLEMENTATION

  if(mqtt_enable) {
    static unsigned long LastMQTTSentMillis = 0;

    mqttclient.loop();
    if(millis() - LastMQTTSentMillis > 10000) {
      if (!mqttclient.connected()) {
        MqttConnect();
        delay(2000);
      } else {
        MqttReadSendSensorData();
        LastMQTTSentMillis=millis();
      }
    }
    // MQTT TEST IMPLEMENTATION END
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

void MqttSetup() {
  mqttclient.setServer(mqtt_server, 1883);
  MqttConnect();

}

void MqttConnect() {
  if (!mqttclient.connected()) {
    bool mqttConnectResult=false;
    Serial.print("Reconnecting to MQTT broker... ");
    Serial.printf("mqtt_user: '%s'\r\n",mqtt_user);
    String localMqttUser = String(mqtt_user);
    localMqttUser.trim(); //remove spaces
    if (localMqttUser.isEmpty())
    {
      Serial.printf("mqtt connect without credentials\r\n");
      mqttConnectResult = mqttclient.connect(deviceId);
    }
    else
    {
      Serial.printf("mqtt connect with credentials\r\n");
      mqttConnectResult = mqttclient.connect(deviceId, mqtt_user, mqtt_password);
    }

    if (mqttConnectResult) {
      Serial.println("connected............ OK!"); //debug code can be removed?
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttclient.state());
      Serial.println(" trying again in next loop");
    }
  }
}

void MqttSensorHomeAssistantAutoDisoverySetup() {
  return; //mist1971 - commented out as I have not tested it

  //DATA NOT CURRENTLY BEING SENT - should only be initialized once if needed? Maybe enough just to pass information to the device class below
  char ESP_SW[15], ESP_SW_By[15], ESP_SW_Version[15], Meter_Manufacturer[15], Meter_Model[15], Meter_SW_Version[15], Utility_SN[15];
  sprintf(ESP_SW, "ESP32 MEP");
  sprintf(ESP_SW_By, "dabbler.dk");
  sprintf(ESP_SW_Version, "%s", MeterInfo.BT01_MainFirmwareVersionNumber);
  sprintf(Meter_Manufacturer, "%s", MeterInfo.BT01_Manufacturer);
  sprintf(Meter_Model, "%s", MeterInfo.BT01_Model);
  sprintf(Meter_SW_Version, "%i.%i", MeterInfo.BT01_FirmwareRevisionNumber);
  sprintf(Utility_SN, "%s", MeterInfo.ET03_UtilitySerialNumber);
//END OF DATA NOT CURRENTLY BEING SENT

  //maybe implement check for already existing sensors in HASS to ensure sensors are not wrongly duplicated?
  int i = 0;
    sprintf(mqtt_payload_hass_create, "{\
          \"uniq_id\":\"%s_time\",\
          \"name\":\"%s time\",\
          \"icon\":\"mdi:clock-outline\",\
          \"stat_t\":\"%s/sensors\",\
          \"val_tpl\":\"{{ value_json.CurrentDateTimeString}}\",\
          \"dev_cla\":\"timestamp\",\
          \"device\":{\"identifiers\":\"ESP32 MEP Interface @ Dabbler.dk\",\
            \"manufacturer\":\"%s\",\
            \"model\":\"%s\",\
            \"name\":\"dab_%s\",\
            \"sw_version\":\"%s\"\
            }\
          }", deviceId, deviceId, mqtt_topic, ESP_SW_By, ESP_SW, deviceId, ESP_SW_Version);
    mqtt_sensor_array[i] = mqtt_payload_hass_create;
    i++;
    sprintf(mqtt_payload_hass_create, "{\
          \"uniq_id\":\"%s_energy_actual_forward\",\
          \"name\":\"%s energy forward\",\
          \"icon\":\"mdi:sine-wave\",\
          \"stat_t\":\"%s/sensors\",\
          \"val_tpl\":\"{{ value_json.Fwd_Act_Wh}}\",\
          \"dev_cla\":\"energy\",\
          \"unit_of_meas\":\"Wh\",\
          \"device\":{\"identifiers\":\"ESP32 MEP Interface @ Dabbler.dk\",\
            \"manufacturer\":\"%s\",\
            \"model\":\"%s\",\
            \"name\":\"dab_%s\",\
            \"sw_version\":\"%s\"\
            }\
          }", deviceId, deviceId, mqtt_topic, ESP_SW_By, ESP_SW, deviceId, ESP_SW_Version);
    mqtt_sensor_array[i] = mqtt_payload_hass_create;
    i++;
    sprintf(mqtt_payload_hass_create, "{\
          \"uniq_id\":\"%s_energy_actual_reverse\",\
          \"name\":\"%s energy reverse\",\
          \"icon\":\"mdi:sine-wave\",\
          \"stat_t\":\"%s/sensors\",\
          \"val_tpl\":\"{{ value_json.Rev_Act_Wh}}\",\
          \"dev_cla\":\"energy\",\
          \"unit_of_meas\":\"Wh\",\
          \"device\":{\"identifiers\":\"ESP32 MEP Interface @ Dabbler.dk\",\
            \"manufacturer\":\"%s\",\
            \"model\":\"%s\",\
            \"name\":\"dab_%s\",\
            \"sw_version\":\"%s\"\
            }\
          }", deviceId, deviceId, mqtt_topic, ESP_SW_By, ESP_SW, deviceId, ESP_SW_Version);
    mqtt_sensor_array[i] = mqtt_payload_hass_create;
    i++;
    sprintf(mqtt_payload_hass_create, "{\
          \"uniq_id\":\"%s_P_forward\",\
          \"name\":\"%s P forward\",\
          \"icon\":\"mdi:sine-wave\",\
          \"stat_t\":\"%s/sensors\",\
          \"val_tpl\":\"{{ value_json.Fwd_W}}\",\
          \"dev_cla\":\"power\",\
          \"unit_of_meas\":\"W\",\
          \"device\":{\"identifiers\":\"ESP32 MEP Interface @ Dabbler.dk\",\
            \"manufacturer\":\"%s\",\
            \"model\":\"%s\",\
            \"name\":\"dab_%s\",\
            \"sw_version\":\"%s\"\
            }\
          }", deviceId, deviceId, mqtt_topic, ESP_SW_By, ESP_SW, deviceId, ESP_SW_Version);
    mqtt_sensor_array[i] = mqtt_payload_hass_create;
    i++;
    sprintf(mqtt_payload_hass_create, "{\
          \"uniq_id\":\"%s_P_reverse\",\
          \"name\":\"%s P reverse\",\
          \"icon\":\"mdi:sine-wave\",\
          \"stat_t\":\"%s/sensors\",\
          \"val_tpl\":\"{{ value_json.Rev_W}}\",\
          \"dev_cla\":\"power\",\
          \"unit_of_meas\":\"W\",\
          \"device\":{\"identifiers\":\"ESP32 MEP Interface @ Dabbler.dk\",\
            \"manufacturer\":\"%s\",\
            \"model\":\"%s\",\
            \"name\":\"dab_%s\",\
            \"sw_version\":\"%s\"\
            }\
          }", deviceId, deviceId, mqtt_topic, ESP_SW_By, ESP_SW, deviceId, ESP_SW_Version);
    mqtt_sensor_array[i] = mqtt_payload_hass_create;
    i++;
    sprintf(mqtt_payload_hass_create, "{\
          \"uniq_id\":\"%s_A_L1\",\
          \"name\":\"%s A L1\",\
          \"icon\":\"mdi:sine-wave\",\
          \"stat_t\":\"%s/sensors\",\
          \"val_tpl\":\"{{ value_json.L1_RMS_A}}\",\
          \"dev_cla\":\"current\",\
          \"unit_of_meas\":\"A\",\
          \"device\":{\"identifiers\":\"ESP32 MEP Interface @ Dabbler.dk\",\
            \"manufacturer\":\"%s\",\
            \"model\":\"%s\",\
            \"name\":\"dab_%s\",\
            \"sw_version\":\"%s\"\
            }\
          }", deviceId, deviceId, mqtt_topic, ESP_SW_By, ESP_SW, deviceId, ESP_SW_Version);
    mqtt_sensor_array[i] = mqtt_payload_hass_create;
    i++;
    sprintf(mqtt_payload_hass_create, "{\
          \"uniq_id\":\"%s_A_L2\",\
          \"name\":\"%s A L2\",\
          \"icon\":\"mdi:sine-wave\",\
          \"stat_t\":\"%s/sensors\",\
          \"val_tpl\":\"{{ value_json.L2_RMS_A}}\",\
          \"dev_cla\":\"current\",\
          \"unit_of_meas\":\"A\",\
          \"device\":{\"identifiers\":\"ESP32 MEP Interface @ Dabbler.dk\",\
            \"manufacturer\":\"%s\",\
            \"model\":\"%s\",\
            \"name\":\"dab_%s\",\
            \"sw_version\":\"%s\"\
            }\
          }", deviceId, deviceId, mqtt_topic, ESP_SW_By, ESP_SW, deviceId, ESP_SW_Version);
    mqtt_sensor_array[i] = mqtt_payload_hass_create;
    i++;
    sprintf(mqtt_payload_hass_create, "{\
          \"uniq_id\":\"%s_A_L3\",\
          \"name\":\"%s A L3\",\
          \"icon\":\"mdi:sine-wave\",\
          \"stat_t\":\"%s/sensors\",\
          \"val_tpl\":\"{{ value_json.L3_RMS_A}}\",\
          \"dev_cla\":\"current\",\
          \"unit_of_meas\":\"A\",\
          \"device\":{\"identifiers\":\"ESP32 MEP Interface @ Dabbler.dk\",\
            \"manufacturer\":\"%s\",\
            \"model\":\"%s\",\
            \"name\":\"dab_%s\",\
            \"sw_version\":\"%s\"\
            }\
          }", deviceId, deviceId, mqtt_topic, ESP_SW_By, ESP_SW, deviceId, ESP_SW_Version);
    mqtt_sensor_array[i] = mqtt_payload_hass_create;
    i++;
    sprintf(mqtt_payload_hass_create, "{\
          \"uniq_id\":\"%s_V_L1\",\
          \"name\":\"%s V L1\",\
          \"icon\":\"mdi:sine-wave\",\
          \"stat_t\":\"%s/sensors\",\
          \"val_tpl\":\"{{ value_json.L1_RMS_V}}\",\
          \"dev_cla\":\"voltage\",\
          \"unit_of_meas\":\"V\",\
          \"device\":{\"identifiers\":\"ESP32 MEP Interface @ Dabbler.dk\",\
            \"manufacturer\":\"%s\",\
            \"model\":\"%s\",\
            \"name\":\"dab_%s\",\
            \"sw_version\":\"%s\"\
            }\
          }", deviceId, deviceId, mqtt_topic, ESP_SW_By, ESP_SW, deviceId, ESP_SW_Version);
    mqtt_sensor_array[i] = mqtt_payload_hass_create;
    i++;
    sprintf(mqtt_payload_hass_create, "{\
          \"uniq_id\":\"%s_V_L2\",\
          \"name\":\"%s V L2\",\
          \"icon\":\"mdi:sine-wave\",\
          \"stat_t\":\"%s/sensors\",\
          \"val_tpl\":\"{{ value_json.L2_RMS_V}}\",\
          \"dev_cla\":\"voltage\",\
          \"unit_of_meas\":\"V\",\
          \"device\":{\"identifiers\":\"ESP32 MEP Interface @ Dabbler.dk\",\
            \"manufacturer\":\"%s\",\
            \"model\":\"%s\",\
            \"name\":\"dab_%s\",\
            \"sw_version\":\"%s\"\
            }\
          }", deviceId, deviceId, mqtt_topic, ESP_SW_By, ESP_SW, deviceId, ESP_SW_Version);
    mqtt_sensor_array[i] = mqtt_payload_hass_create;
    i++;
    sprintf(mqtt_payload_hass_create, "{\
          \"uniq_id\":\"%s_V_L3\",\
          \"name\":\"%s V L3\",\
          \"icon\":\"mdi:sine-wave\",\
          \"stat_t\":\"%s/sensors\",\
          \"val_tpl\":\"{{ value_json.L3_RMS_V}}\",\
          \"dev_cla\":\"voltage\",\
          \"unit_of_meas\":\"V\",\
          \"device\":{\"identifiers\":\"ESP32 MEP Interface @ Dabbler.dk\",\
            \"manufacturer\":\"%s\",\
            \"model\":\"%s\",\
            \"name\":\"dab_%s\",\
            \"sw_version\":\"%s\"\
            }\
          }", deviceId, deviceId, mqtt_topic, ESP_SW_By, ESP_SW, deviceId, ESP_SW_Version);
    mqtt_sensor_array[i] = mqtt_payload_hass_create;
    i++;
    sprintf(mqtt_payload_hass_create, "{\
          \"uniq_id\":\"%s_W_Forward_L1\",\
          \"name\":\"%s W Forward L1\",\
          \"icon\":\"mdi:sine-wave\",\
          \"stat_t\":\"%s/sensors\",\
          \"val_tpl\":\"{{ value_json.L1_Fwd_W}}\",\
          \"dev_cla\":\"voltage\",\
          \"unit_of_meas\":\"V\",\
          \"device\":{\"identifiers\":\"ESP32 MEP Interface @ Dabbler.dk\",\
            \"manufacturer\":\"%s\",\
            \"model\":\"%s\",\
            \"name\":\"dab_%s\",\
            \"sw_version\":\"%s\"\
            }\
          }", deviceId, deviceId, mqtt_topic, ESP_SW_By, ESP_SW, deviceId, ESP_SW_Version);
    mqtt_sensor_array[i] = mqtt_payload_hass_create;
    i++;
    sprintf(mqtt_payload_hass_create, "{\
          \"uniq_id\":\"%s_W_Forward_L2\",\
          \"name\":\"%s W Forward L2\",\
          \"icon\":\"mdi:sine-wave\",\
          \"stat_t\":\"%s/sensors\",\
          \"val_tpl\":\"{{ value_json.L2_Fwd_W}}\",\
          \"dev_cla\":\"voltage\",\
          \"unit_of_meas\":\"V\",\
          \"device\":{\"identifiers\":\"ESP32 MEP Interface @ Dabbler.dk\",\
            \"manufacturer\":\"%s\",\
            \"model\":\"%s\",\
            \"name\":\"dab_%s\",\
            \"sw_version\":\"%s\"\
            }\
          }", deviceId, deviceId, mqtt_topic, ESP_SW_By, ESP_SW, deviceId, ESP_SW_Version);
    mqtt_sensor_array[i] = mqtt_payload_hass_create;
    i++;
    sprintf(mqtt_payload_hass_create, "{\
          \"uniq_id\":\"%s_W_Forward_L3\",\
          \"name\":\"%s W Forward L3\",\
          \"icon\":\"mdi:sine-wave\",\
          \"stat_t\":\"%s/sensors\",\
          \"val_tpl\":\"{{ value_json.L3_Fwd_W}}\",\
          \"dev_cla\":\"voltage\",\
          \"unit_of_meas\":\"V\",\
          \"device\":{\"identifiers\":\"ESP32 MEP Interface @ Dabbler.dk\",\
            \"manufacturer\":\"%s\",\
            \"model\":\"%s\",\
            \"name\":\"dab_%s\",\
            \"sw_version\":\"%s\"\
            }\
          }", deviceId, deviceId, mqtt_topic, ESP_SW_By, ESP_SW, deviceId, ESP_SW_Version);
    mqtt_sensor_array[i] = mqtt_payload_hass_create;
    i++;
    sprintf(mqtt_payload_hass_create, "{\
          \"uniq_id\":\"%s_W_Reverse_L1\",\
          \"name\":\"%s W Reverse L1\",\
          \"icon\":\"mdi:sine-wave\",\
          \"stat_t\":\"%s/sensors\",\
          \"val_tpl\":\"{{ value_json.L1_Rev_W}}\",\
          \"dev_cla\":\"voltage\",\
          \"unit_of_meas\":\"V\",\
          \"device\":{\"identifiers\":\"ESP32 MEP Interface @ Dabbler.dk\",\
            \"manufacturer\":\"%s\",\
            \"model\":\"%s\",\
            \"name\":\"dab_%s\",\
            \"sw_version\":\"%s\"\
            }\
          }", deviceId, deviceId, mqtt_topic, ESP_SW_By, ESP_SW, deviceId, ESP_SW_Version);
    mqtt_sensor_array[i] = mqtt_payload_hass_create;
    i++;
    sprintf(mqtt_payload_hass_create, "{\
          \"uniq_id\":\"%s_W_Reverse_L2\",\
          \"name\":\"%s W Reverse L2\",\
          \"icon\":\"mdi:sine-wave\",\
          \"stat_t\":\"%s/sensors\",\
          \"val_tpl\":\"{{ value_json.L2_Rev_W}}\",\
          \"dev_cla\":\"voltage\",\
          \"unit_of_meas\":\"V\",\
          \"device\":{\"identifiers\":\"ESP32 MEP Interface @ Dabbler.dk\",\
            \"manufacturer\":\"%s\",\
            \"model\":\"%s\",\
            \"name\":\"dab_%s\",\
            \"sw_version\":\"%s\"\
            }\
          }", deviceId, deviceId, mqtt_topic, ESP_SW_By, ESP_SW, deviceId, ESP_SW_Version);
    mqtt_sensor_array[i] = mqtt_payload_hass_create;
    i++;
    sprintf(mqtt_payload_hass_create, "{\
          \"uniq_id\":\"%s_W_Reverse_L3\",\
          \"name\":\"%s W Reverse L3\",\
          \"icon\":\"mdi:sine-wave\",\
          \"stat_t\":\"%s/sensors\",\
          \"val_tpl\":\"{{ value_json.L3_Rev_W}}\",\
          \"dev_cla\":\"voltage\",\
          \"unit_of_meas\":\"V\",\
          \"device\":{\"identifiers\":\"ESP32 MEP Interface @ Dabbler.dk\",\
            \"manufacturer\":\"%s\",\
            \"model\":\"%s\",\
            \"name\":\"dab_%s\",\
            \"sw_version\":\"%s\"\
            }\
          }", deviceId, deviceId, mqtt_topic, ESP_SW_By, ESP_SW, deviceId, ESP_SW_Version);
    mqtt_sensor_array[i] = mqtt_payload_hass_create;
/*
add "hw" to string - and use abbreviations from https://www.home-assistant.io/docs/mqtt/discovery/
// exp_aft: expire_after fx 300 sekunder
// pl_avail: payload_available lave available topic der viser online / offline
*/


  for (int e = 0; e < i ; e++) { // Loop through above sensors and send via MQTT broker to autodiscover in HomeAssistant
    sprintf(mqtt_topic_hass_create, "homeassistant/sensor/dabbler_sensor_%s_%i/config", mqtt_topic, e);
    mqttclient.publish(mqtt_topic_hass_create, mqtt_sensor_array[e].c_str(), true); //hver sensor oprettes med et retain flag, så HASS altid ser sensorerne ved genstart af HASS.sensor_array[e][1].c_str()
  }
}

void MqttReadSendSensorData() {
  
  char mqtt_topic_data[100];
  char mqtt_payload_data[300];

  Serial.printf("Is connected to mqtt: %d\r\n", mqttclient.connected());

  sprintf(mqtt_topic_data, "%s/sensors/mydatajson", mqtt_topic); //CREATE TOPIC FOR CURRENT DEVICENAME (Can use another variable than deviceId, thats already in use in program)
  sprintf(mqtt_payload_data, "{\
\"L1_RMS_A\":%lu,\"L2_RMS_A\":%lu,\"L3_RMS_A\":%lu,\
\"L1_RMS_V\":%lu,\"L2_RMS_V\":%lu,\"L3_RMS_V\":%lu,\
\"L1_PF_Fac\":%lu,\"L2_PF_Fac\":%lu,\"L3_PF_Fac\":%lu,\
\"ExportReactive_VAr\":%lu,\"ImportReactive_VAr\":%lu\
}",\
      ConsumptionData.BT28_RMS_mA_L1, ConsumptionData.BT28_RMS_mA_L2, ConsumptionData.BT28_RMS_mA_L3,
      ConsumptionData.BT28_RMS_mV_L1, ConsumptionData.BT28_RMS_mV_L2, ConsumptionData.BT28_RMS_mV_L3,
      ConsumptionData.BT28_PowerFactor_Fac_L1, ConsumptionData.BT28_PowerFactor_Fac_L2, ConsumptionData.BT28_PowerFactor_Fac_L3,
      ConsumptionData.BT28_ExportReactive_VAr, ConsumptionData.BT28_ImportReactive_VAr
  );

  Serial.printf("Is connected to mqtt: %d\r\n", mqttclient.connected());
  Serial.printf("mqtt data \r\ntopic: %s\r\npayload: %s\r\n", mqtt_topic_data, mqtt_payload_data);

  if (mqttclient.publish(mqtt_topic_data, mqtt_payload_data)) {
    Serial.printf("Sent OK\r\n");
  }
  else {
        Serial.printf("Sent NOK\r\n");
  }

  sprintf(mqtt_topic_data, "%s/meterConsumptionTotal/mydatajson", mqtt_topic); //CREATE TOPIC FOR CURRENT DEVICENAME (Can use another variable than deviceId, thats already in use in program)
  sprintf(mqtt_payload_data, "{\
\"Fwd_Act_Wh\":%lu,\"Rev_Act_Wh\":%lu\
}",\
      ConsumptionData.BT23_Fwd_Act_Wh, ConsumptionData.BT23_Rev_Act_Wh
  );

  Serial.printf("mqtt data \r\ntopic: %s\r\npayload: %s\r\n", mqtt_topic_data, mqtt_payload_data);
  if (mqttclient.publish(mqtt_topic_data, mqtt_payload_data)) {
    Serial.printf("Sent OK\r\n");
  }
  else {
        Serial.printf("Sent NOK\r\n");
  }

  sprintf(mqtt_topic_data, "%s/meterConsumptionFwd/mydatajson", mqtt_topic); //CREATE TOPIC FOR CURRENT DEVICENAME (Can use another variable than deviceId, thats already in use in program)
  sprintf(mqtt_payload_data, "{\
\"Fwd_W\":%lu,\
\"Fwd_Avg_W\":%lu,\
\"L1_Fwd_W\":%lu,\"L2_Fwd_W\":%lu,\"L3_Fwd_W\":%lu,\
\"L1_Fwd_Avg_W\":%lu,\"L2_Fwd_Avg_W\":%lu,\"L3_Fwd_Avg_W\":%lu\
}",\
  ConsumptionData.BT28_Fwd_W,
  ConsumptionData.BT28_Fwd_Avg_W,
  ConsumptionData.BT28_Fwd_W_L1, ConsumptionData.BT28_Fwd_W_L2, ConsumptionData.BT28_Fwd_W_L3,
  ConsumptionData.BT28_Fwd_Avg_W_L1, ConsumptionData.BT28_Fwd_Avg_W_L2, ConsumptionData.BT28_Fwd_Avg_W_L3
  );

  Serial.printf("mqtt data \r\ntopic: %s\r\npayload: %s\r\n", mqtt_topic_data, mqtt_payload_data);
  if (mqttclient.publish(mqtt_topic_data, mqtt_payload_data)) {
    Serial.printf("Sent OK\r\n");
  }
  else {
        Serial.printf("Sent NOK\r\n");
  }

  sprintf(mqtt_topic_data, "%s/meterConsumptionRev/mydatajson", mqtt_topic); //CREATE TOPIC FOR CURRENT DEVICENAME (Can use another variable than deviceId, thats already in use in program)
  sprintf(mqtt_payload_data, "{\
\"Rev_W\":%lu,\
\"Rev_Avg_W\":%lu,\
\"L1_Rev_W\":%lu,\"L2_Rev_W\":%lu,\"L3_Rev_W\":%lu,\
\"L1_Rev_Avg_W\":%lu,\"L2_Rev_Avg_W\":%lu,\"L3_Rev_Avg_W\":%lu\
}",\
  ConsumptionData.BT28_Rev_W,
  ConsumptionData.BT28_Rev_Avg_W,
  ConsumptionData.BT28_Rev_W_L1, ConsumptionData.BT28_Rev_W_L2, ConsumptionData.BT28_Rev_W_L3,
  ConsumptionData.BT28_Rev_Avg_W_L1, ConsumptionData.BT28_Rev_Avg_W_L2, ConsumptionData.BT28_Rev_Avg_W_L3
  );

  Serial.printf("mqtt data \r\ntopic: %s\r\npayload: %s\r\n", mqtt_topic_data, mqtt_payload_data);
  if (mqttclient.publish(mqtt_topic_data, mqtt_payload_data)) {
    Serial.printf("Sent OK\r\n");
  }
  else {
        Serial.printf("Sent NOK\r\n");
  }


  sprintf(mqtt_topic_data, "%s/frequency/mydatajson", mqtt_topic); //CREATE TOPIC FOR CURRENT DEVICENAME (Can use another variable than deviceId, thats already in use in program)
  sprintf(mqtt_payload_data, "{\
\"Freq_mHz\":%lu\
}",\
     ConsumptionData.BT28_Freq_mHz
  );

  Serial.printf("mqtt data \r\ntopic: %s\r\npayload: %s\r\n", mqtt_topic_data, mqtt_payload_data);
  if (mqttclient.publish(mqtt_topic_data, mqtt_payload_data)) {
    Serial.printf("Sent OK\r\n");
  }
  else {
        Serial.printf("Sent NOK\r\n");
  }


}