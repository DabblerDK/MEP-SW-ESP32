// MQTT TEST IMPLEMENTATION 
void MqttSetup() {
  client.setServer(MqttServer, 1883);
  while (!client.connected()) {
    Serial.print("Setting up MQTT connection... ");
    if (client.connect(deviceId, MqttUser, MqttPass)) { //CHANGE deviceId to program
      Serial.println("connected............... OK!"); //debug code can be removed
      MqttSensorHomeAssistantAutoDisoverySetup();
    } else {
      Serial.print("failed, rc="); //debug code can be removed
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds"); //debug code can be removed
      delay(5000);
    }
  }
}

void MqttSensorHomeAssistantAutoDisoverySetup() {
  //DATA NOT CURRENTLY BEING SENT - should only be initialized once if needed? Maybe enough just to pass information to the device class below
  sprintf(ESP_SW, "ESP32 MEP")
  sprintf(ESP_SW_By, "dabbler.dk")
  sprintf(ESP_SW_Version, "%s", MeterInfo.BT01_MainFirmwareVersionNumber)
  sprintf(Meter_Manufacturer, "%s", MeterInfo.BT01_Manufacturer)
  sprintf(Meter_Model, "%s", MeterInfo.BT01_Model)
  sprintf(Meter_SW_Version, "%i.%i", MeterInfo.BT01_FirmwareRevisionNumber)
  sprintf(Utility_SN, "%s", MeterInfo.ET03_UtilitySerialNumber)
//END OF DATA NOT CURRENTLY BEING SENT

  //maybe implement check for already existing sensors in HASS to ensure sensors are not wrongly duplicated?
  int i=0;
    sprintf(PayloadMqttHomeassistantCreate, "{
          \"uniq_id\":\"%s_time\",
          \"name\":\"%s time\",
          \"icon\":\"mdi:clock-outline\",
          \"stat_t\":\"ESP32MEP/%s/sensors\",
          \"val_tpl\":\"{{ value_json.CurrentDateTimeString}}\",
          \"dev_cla\":\"timestamp\",
          \"device\":{\"identifiers\":\"ESP32 MEP Interface @ Dabbler.dk\",
            \"manufacturer\":\"%s\",
            \"model\":\"%s\",
            \"name\":\"dab_%s\",
            \"sw_version\":\"%s\"
            },
          }", deviceId, deviceId, deviceId, ESP_SW_By, ESP_SW, deviceId, ESP_SW_Version);
    sensor_array[i] = TopicMqttHomeassistantCreate;
    i++;
    sprintf(PayloadMqttHomeassistantCreate, "{
          \"uniq_id\":\"%s_energy_actual_forward\",
          \"name\":\"%s energy forward\",
          \"icon\":\"mdi:sine-wave\",
          \"stat_t\":\"ESP32MEP/%s/sensors\",
          \"val_tpl\":\"{{ value_json.Fwd_Act_Wh}}\",
          \"dev_cla\":\"energy\",
          \"unit_of_meas\":\"Wh\",
          \"device\":{\"identifiers\":\"ESP32 MEP Interface @ Dabbler.dk\",
            \"manufacturer\":\"%s\",
            \"model\":\"%s\",
            \"name\":\"dab_%s\",
            \"sw_version\":\"%s\"
            },
          }", deviceId, deviceId, deviceId, ESP_SW_By, ESP_SW, deviceId, ESP_SW_Version);
    sensor_array[i] = TopicMqttHomeassistantCreate;
    i++;
    sprintf(PayloadMqttHomeassistantCreate, "{\
          \"uniq_id\":\"%s_energy_actual_reverse\",
          \"name\":\"%s energy reverse\",
          \"icon\":\"mdi:sine-wave\",
          \"stat_t\":\"ESP32MEP/%s/sensors\",
          \"val_tpl\":\"{{ value_json.Rev_Act_Wh}}\",
          \"dev_cla\":\"energy\",
          \"unit_of_meas\":\"Wh\",
          \"device\":{\"identifiers\":\"ESP32 MEP Interface @ Dabbler.dk\",
            \"manufacturer\":\"%s\",
            \"model\":\"%s\",
            \"name\":\"dab_%s\",
            \"sw_version\":\"%s\"
            },
          }", deviceId, deviceId, deviceId, ESP_SW_By, ESP_SW, deviceId, ESP_SW_Version);
    sensor_array[i] = TopicMqttHomeassistantCreate;
    i++;
    sprintf(PayloadMqttHomeassistantCreate, "{
          \"uniq_id\":\"%s_P_forward\",
          \"name\":\"%s P forward\",
          \"icon\":\"mdi:sine-wave\",
          \"stat_t\":\"ESP32MEP/%s/sensors\",
          \"val_tpl\":\"{{ value_json.Fwd_W}}\",
          \"dev_cla\":\"power\",
          \"unit_of_meas\":\"W\",
          \"device\":{\"identifiers\":\"ESP32 MEP Interface @ Dabbler.dk\",
            \"manufacturer\":\"%s\",
            \"model\":\"%s\",
            \"name\":\"dab_%s\",
            \"sw_version\":\"%s\"
            },
          }", deviceId, deviceId, deviceId, ESP_SW_By, ESP_SW, deviceId, ESP_SW_Version);
    sensor_array[i] = TopicMqttHomeassistantCreate;
    i++;
    sprintf(PayloadMqttHomeassistantCreate, "{\
          \"uniq_id\":\"%s_P_reverse\",
          \"name\":\"%s P reverse\",
          \"icon\":\"mdi:sine-wave\",
          \"stat_t\":\"ESP32MEP/%s/sensors\",
          \"val_tpl\":\"{{ value_json.Rev_W}}\",
          \"dev_cla\":\"power\",
          \"unit_of_meas\":\"W\",
          \"device\":{\"identifiers\":\"ESP32 MEP Interface @ Dabbler.dk\",
            \"manufacturer\":\"%s\",
            \"model\":\"%s\",
            \"name\":\"dab_%s\",
            \"sw_version\":\"%s\"
            },
          }", deviceId, deviceId, deviceId, ESP_SW_By, ESP_SW, deviceId, ESP_SW_Version);
    sensor_array[i] = TopicMqttHomeassistantCreate;
    i++;
    sprintf(PayloadMqttHomeassistantCreate, "{
          \"uniq_id\":\"%s_A_L1\",
          \"name\":\"%s A L1\",
          \"icon\":\"mdi:sine-wave\",
          \"stat_t\":\"ESP32MEP/%s/sensors\",
          \"val_tpl\":\"{{ value_json.L1_RMS_A}}\",
          \"dev_cla\":\"current\",
          \"unit_of_meas\":\"A\",
          \"device\":{\"identifiers\":\"ESP32 MEP Interface @ Dabbler.dk\",
            \"manufacturer\":\"%s\",
            \"model\":\"%s\",
            \"name\":\"dab_%s\",
            \"sw_version\":\"%s\"
            },
          }", deviceId, deviceId, deviceId, ESP_SW_By, ESP_SW, deviceId, ESP_SW_Version);
    sensor_array[i] = TopicMqttHomeassistantCreate;
    i++;
    sprintf(PayloadMqttHomeassistantCreate, "{\
          \"uniq_id\":\"%s_A_L2\",
          \"name\":\"%s A L2\",
          \"icon\":\"mdi:sine-wave\",
          \"stat_t\":\"ESP32MEP/%s/sensors\",
          \"val_tpl\":\"{{ value_json.L2_RMS_A}}\",
          \"dev_cla\":\"current\",
          \"unit_of_meas\":\"A\",
          \"device\":{\"identifiers\":\"ESP32 MEP Interface @ Dabbler.dk\",
            \"manufacturer\":\"%s\",
            \"model\":\"%s\",
            \"name\":\"dab_%s\",
            \"sw_version\":\"%s\"
            },
          }", deviceId, deviceId, deviceId, ESP_SW_By, ESP_SW, deviceId, ESP_SW_Version);
    sensor_array[i] = TopicMqttHomeassistantCreate;
    i++;
    sprintf(PayloadMqttHomeassistantCreate, "{\
          \"uniq_id\":\"%s_A_L3\",
          \"name\":\"%s A L3\",
          \"icon\":\"mdi:sine-wave\",
          \"stat_t\":\"ESP32MEP/%s/sensors\",
          \"val_tpl\":\"{{ value_json.L3_RMS_A}}\",
          \"dev_cla\":\"current\",
          \"unit_of_meas\":\"A\",
          \"device\":{\"identifiers\":\"ESP32 MEP Interface @ Dabbler.dk\",
            \"manufacturer\":\"%s\",
            \"model\":\"%s\",
            \"name\":\"dab_%s\",
            \"sw_version\":\"%s\"
            },
          }", deviceId, deviceId, deviceId, ESP_SW_By, ESP_SW, deviceId, ESP_SW_Version);
    sensor_array[i] = TopicMqttHomeassistantCreate;
    i++;
    sprintf(PayloadMqttHomeassistantCreate, "{
          \"uniq_id\":\"%s_V_L1\",
          \"name\":\"%s V L1\",
          \"icon\":\"mdi:sine-wave\",
          \"stat_t\":\"ESP32MEP/%s/sensors\",
          \"val_tpl\":\"{{ value_json.L1_RMS_V}}\",
          \"dev_cla\":\"voltage\",
          \"unit_of_meas\":\"V\",
          \"device\":{\"identifiers\":\"ESP32 MEP Interface @ Dabbler.dk\",
            \"manufacturer\":\"%s\",
            \"model\":\"%s\",
            \"name\":\"dab_%s\",
            \"sw_version\":\"%s\"
            },
          }", deviceId, deviceId, deviceId, ESP_SW_By, ESP_SW, deviceId, ESP_SW_Version);
    sensor_array[i] = TopicMqttHomeassistantCreate;
    i++;
    sprintf(PayloadMqttHomeassistantCreate, "{\
          \"uniq_id\":\"%s_V_L2\",
          \"name\":\"%s V L2\",
          \"icon\":\"mdi:sine-wave\",
          \"stat_t\":\"ESP32MEP/%s/sensors\",
          \"val_tpl\":\"{{ value_json.L2_RMS_V}}\",
          \"dev_cla\":\"voltage\",
          \"unit_of_meas\":\"V\",
          \"device\":{\"identifiers\":\"ESP32 MEP Interface @ Dabbler.dk\",
            \"manufacturer\":\"%s\",
            \"model\":\"%s\",
            \"name\":\"dab_%s\",
            \"sw_version\":\"%s\"
            },
          }", deviceId, deviceId, deviceId, ESP_SW_By, ESP_SW, deviceId, ESP_SW_Version);
    sensor_array[i] = TopicMqttHomeassistantCreate;
    i++;
    sprintf(PayloadMqttHomeassistantCreate, "{\
          \"uniq_id\":\"%s_V_L3\",
          \"name\":\"%s V L3\",
          \"icon\":\"mdi:sine-wave\",
          \"stat_t\":\"ESP32MEP/%s/sensors\",
          \"val_tpl\":\"{{ value_json.L3_RMS_V}}\",
          \"dev_cla\":\"voltage\",
          \"unit_of_meas\":\"V\",
          \"device\":{\"identifiers\":\"ESP32 MEP Interface @ Dabbler.dk\",
            \"manufacturer\":\"%s\",
            \"model\":\"%s\",
            \"name\":\"dab_%s\",
            \"sw_version\":\"%s\"
            },
          }", deviceId, deviceId, deviceId, ESP_SW_By, ESP_SW, deviceId, ESP_SW_Version);
    sensor_array[i] = TopicMqttHomeassistantCreate;
    i++;
    sprintf(PayloadMqttHomeassistantCreate, "{
          \"uniq_id\":\"%s_W_Forward_L1\",
          \"name\":\"%s W Forward L1\",
          \"icon\":\"mdi:sine-wave\",
          \"stat_t\":\"ESP32MEP/%s/sensors\",
          \"val_tpl\":\"{{ value_json.L1_Fwd_W}}\",
          \"dev_cla\":\"voltage\",
          \"unit_of_meas\":\"V\",
          \"device\":{\"identifiers\":\"ESP32 MEP Interface @ Dabbler.dk\",
            \"manufacturer\":\"%s\",
            \"model\":\"%s\",
            \"name\":\"dab_%s\",
            \"sw_version\":\"%s\"
            },
          }", deviceId, deviceId, deviceId, ESP_SW_By, ESP_SW, deviceId, ESP_SW_Version);
    sensor_array[i] = TopicMqttHomeassistantCreate;
    i++;
    sprintf(PayloadMqttHomeassistantCreate, "{\
          \"uniq_id\":\"%s_W_Forward_L2\",
          \"name\":\"%s W Forward L2\",
          \"icon\":\"mdi:sine-wave\",
          \"stat_t\":\"ESP32MEP/%s/sensors\",
          \"val_tpl\":\"{{ value_json.L2_Fwd_W}}\",
          \"dev_cla\":\"voltage\",
          \"unit_of_meas\":\"V\",
          \"device\":{\"identifiers\":\"ESP32 MEP Interface @ Dabbler.dk\",
            \"manufacturer\":\"%s\",
            \"model\":\"%s\",
            \"name\":\"dab_%s\",
            \"sw_version\":\"%s\"
            },
          }", deviceId, deviceId, deviceId, ESP_SW_By, ESP_SW, deviceId, ESP_SW_Version);
    sensor_array[i] = TopicMqttHomeassistantCreate;
    i++;
    sprintf(PayloadMqttHomeassistantCreate, "{\
          \"uniq_id\":\"%s_W_Forward_L3\",
          \"name\":\"%s W Forward L3\",
          \"icon\":\"mdi:sine-wave\",
          \"stat_t\":\"ESP32MEP/%s/sensors\",
          \"val_tpl\":\"{{ value_json.L3_Fwd_W}}\",
          \"dev_cla\":\"voltage\",
          \"unit_of_meas\":\"V\",
          \"device\":{\"identifiers\":\"ESP32 MEP Interface @ Dabbler.dk\",
            \"manufacturer\":\"%s\",
            \"model\":\"%s\",
            \"name\":\"dab_%s\",
            \"sw_version\":\"%s\"
            },
          }", deviceId, deviceId, deviceId, ESP_SW_By, ESP_SW, deviceId, ESP_SW_Version);
    sensor_array[i] = TopicMqttHomeassistantCreate;
    i++;
    sprintf(PayloadMqttHomeassistantCreate, "{
          \"uniq_id\":\"%s_W_Reverse_L1\",
          \"name\":\"%s W Reverse L1\",
          \"icon\":\"mdi:sine-wave\",
          \"stat_t\":\"ESP32MEP/%s/sensors\",
          \"val_tpl\":\"{{ value_json.L1_Rev_W}}\",
          \"dev_cla\":\"voltage\",
          \"unit_of_meas\":\"V\",
          \"device\":{\"identifiers\":\"ESP32 MEP Interface @ Dabbler.dk\",
            \"manufacturer\":\"%s\",
            \"model\":\"%s\",
            \"name\":\"dab_%s\",
            \"sw_version\":\"%s\"
            },
          }", deviceId, deviceId, deviceId, ESP_SW_By, ESP_SW, deviceId, ESP_SW_Version);
    sensor_array[i] = TopicMqttHomeassistantCreate;
    i++;
    sprintf(PayloadMqttHomeassistantCreate, "{\
          \"uniq_id\":\"%s_W_Reverse_L2\",
          \"name\":\"%s W Reverse L2\",
          \"icon\":\"mdi:sine-wave\",
          \"stat_t\":\"ESP32MEP/%s/sensors\",
          \"val_tpl\":\"{{ value_json.L2_Rev_W}}\",
          \"dev_cla\":\"voltage\",
          \"unit_of_meas\":\"V\",
          \"device\":{\"identifiers\":\"ESP32 MEP Interface @ Dabbler.dk\",
            \"manufacturer\":\"%s\",
            \"model\":\"%s\",
            \"name\":\"dab_%s\",
            \"sw_version\":\"%s\"
            },
          }", deviceId, deviceId, deviceId, ESP_SW_By, ESP_SW, deviceId, ESP_SW_Version);
    sensor_array[i] = TopicMqttHomeassistantCreate;
    i++;
    sprintf(PayloadMqttHomeassistantCreate, "{\
          \"uniq_id\":\"%s_W_Reverse_L3\",
          \"name\":\"%s W Reverse L3\",
          \"icon\":\"mdi:sine-wave\",
          \"stat_t\":\"ESP32MEP/%s/sensors\",
          \"val_tpl\":\"{{ value_json.L3_Rev_W}}\",
          \"dev_cla\":\"voltage\",
          \"unit_of_meas\":\"V\",
          \"device\":{\"identifiers\":\"ESP32 MEP Interface @ Dabbler.dk\",
            \"manufacturer\":\"%s\",
            \"model\":\"%s\",
            \"name\":\"dab_%s\",
            \"sw_version\":\"%s\"
            },
          }", deviceId, deviceId, deviceId, ESP_SW_By, ESP_SW, deviceId, ESP_SW_Version);
    sensor_array[i] = TopicMqttHomeassistantCreate;
 
  for (int e = 0; e < i ; e++) { // Loop through above sensors and send via MQTT broker to autodiscover in HomeAssistant
    sprintf(TopicMqttHomeassistantCreate, "homeassistant/sensor/dabbler_sensor_%s_%i/config", deviceId, e);
    client.publish(sensor_array[e].c_str(), TopicMqttHomeassistantCreate, true); //hver sensor oprettes med et retain flag, så HASS altid ser sensorerne ved genstart af HASS.sensor_array[e][1].c_str()
  }
}

void MqttReadSendSensorData() { //ESP_SW_Version
//DATA CURRENTLY BEING SENT
  sprintf(CurrentDateTimeString, "%s", GetESP32DateTime(false).c_str())
  sprintf(CurrentDateTime, "%s", GetESP32DateTime(true).c_str())
  sprintf(Fwd_Act_Wh, "%lu", ConsumptionData.BT23_Fwd_Act_Wh)
  sprintf(Rev_Act_Wh, "%lu", ConsumptionData.BT23_Rev_Act_Wh)
  sprintf(Fwd_W, "%lu", ConsumptionData.BT28_Fwd_W)
  sprintf(Rev_W, "%lu", ConsumptionData.BT28_Rev_W)
  sprintf(Freq_mHz, "%lu", ConsumptionData.BT28_Freq_mHz)
  sprintf(L1_RMS_A, "%lu", ConsumptionData.BT28_RMS_mA_L1)
  sprintf(L2_RMS_A, "%lu", ConsumptionData.BT28_RMS_mA_L2)
  sprintf(L3_RMS_A, "%lu", ConsumptionData.BT28_RMS_mA_L3)
  sprintf(L1_RMS_V, "%lu", ConsumptionData.BT28_RMS_mV_L1)
  sprintf(L2_RMS_V, "%lu", ConsumptionData.BT28_RMS_mV_L2)
  sprintf(L3_RMS_V, "%lu", ConsumptionData.BT28_RMS_mV_L3)
  sprintf(L1_Fwd_W, "%lu", ConsumptionData.BT28_Fwd_W_L1)
  sprintf(L2_Fwd_W, "%lu", ConsumptionData.BT28_Fwd_W_L2)
  sprintf(L3_Fwd_W, "%lu", ConsumptionData.BT28_Fwd_W_L3)
  sprintf(L1_Rev_W, "%lu", ConsumptionData.BT28_Rev_W_L1)
  sprintf(L2_Rev_W, "%lu", ConsumptionData.BT28_Rev_W_L2)
  sprintf(L3_Rev_W, "%lu", ConsumptionData.BT28_Rev_W_L3)
  //maybe pass data below to MQTT later?
/*  sprintf(Fwd_Avg_W, "%lu", ConsumptionData.BT28_Fwd_Avg_W) 
  sprintf(Rev_Avg_W, "%lu", ConsumptionData.BT28_Rev_Avg_W)
  sprintf(L1_Fwd_Avg_W, "%lu", ConsumptionData.BT28_Fwd_Avg_W_L1)
  sprintf(L2_Fwd_Avg_W, "%lu", ConsumptionData.BT28_Fwd_Avg_W_L2)
  sprintf(L3_Fwd_Avg_W, "%lu", ConsumptionData.BT28_Fwd_Avg_W_L3)
  sprintf(L1_Rev_Avg_W, "%lu", ConsumptionData.BT28_Rev_Avg_W_L1)
  sprintf(L2_Rev_Avg_W, "%lu", ConsumptionData.BT28_Rev_Avg_W_L2)
  sprintf(L3_Rev_Avg_W, "%lu", ConsumptionData.BT28_Rev_Avg_W_L3)
*/
//END OF DATA NOT CURRENTLY BEING SENT     
    /* inspiration hvis det er nødvendigt at afrunde..
      dtostrf(originalVariabel1, 4, 1, L1_RMS_V); //1 decimal, 4 karakterer, bruger variablen originalVariabel1 -> som danner V_L1
      dtostrf(originalVariabel2, 4, 1, L2_RMS_V);
      dtostrf(originalVariabel3, 4, 1, L3_RMS_V);
    */

  sprintf(TopicData, "ESP32MEP/%s/sensors", deviceId); //CREATE TOPIC FOR CURRENT DEVICENAME (Can use another variable than deviceId, thats already in use in program)
  sprintf(PayloadData, "{
    \"CurrentDateTimeString\":%s,\"CurrentDateTime\":%s,
    \"Fwd_Act_Wh\":%s,\"Rev_Act_Wh\":%s,\"Freq_mHz\":%s,
    \"L1_RMS_A\":%s,\"L2_RMS_A\":%s,\"L3_RMS_A\":%s,
    \"L1_RMS_V\":%s,\"L2_RMS_V\":%s,\"L3_RMS_V\":%s,
    \"L1_Fwd_W\":%s,\"L2_Fwd_W\":%s,\"L3_Fwd_W\":%s,
    \"L1_Rev_W\":%s,\"L2_Rev_W\":%s,\"L3_Rev_W\":%s,
    \"Fwd_Avg_W\":%s,\"Rev_Avg_W\":%s,
    \"L1_Fwd_Avg_W\":%s,\"L2_Fwd_Avg_W\":%s,\"L3_Fwd_Avg_W\":%s,
    \"L1_Rev_Avg_W\":%s,\"L2_Rev_Avg_W\":%s,\"L3_Rev_Avg_W\":%s\"
    }",
    CurrentDateTimeString, CurrentDateTime, 
    Fwd_Act_Wh, Rev_Act_Wh, Freq_mHz, 
    L1_RMS_A, L2_RMS_A, L3_RMS_A, 
    L1_RMS_V, L2_RMS_V, L3_RMS_V, 
    L1_Fwd_W, L2_Fwd_W, L3_Fwd_W, 
    L1_Rev_W, L2_Rev_W, L3_Rev_W, 
    Fwd_Avg_W, Rev_Avg_W
    //maybe pass data below to MQTT later?
  /*  , 
    L1_Fwd_Avg_W, L2_Fwd_Avg_W, L3_Fwd_Avg_W, 
    L1_Rev_Avg_W, L2_Rev_Avg_W, L3_Rev_Avg_W 
  */
    ); 
  client.publish(TopicData, PayloadData);
}

// MQTT TEST IMPLEMENTATION END