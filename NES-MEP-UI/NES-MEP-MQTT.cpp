#include <Arduino.h>
#include <PubSubClient.h> 
#include "NES-MEP.h"

extern MeterInfoStruct MeterInfo;
extern ConsumptionDataStruct ConsumptionData;

extern const char* deviceId;
extern char mqtt_server[];
extern char mqtt_user[];
extern char mqtt_password[];
extern char TopicMqttHomeassistantCreate[];
extern char PayloadMqttHomeassistantCreate[];
extern char TopicData[];
extern char PayloadData[]; 
extern String MqttSensorArray[];
extern PubSubClient mqttclient;

//WANT TO HAVE ALL MQTT RELATED FUNCTIONS HERE..
