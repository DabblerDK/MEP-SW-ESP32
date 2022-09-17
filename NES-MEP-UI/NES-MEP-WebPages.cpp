#include <Arduino.h>
#include "NES-MEP.h"
#include "SPIFFS.h"
#include <WebServer.h>
#include <Preferences.h>
#include <Update.h>
#include "NES-MEP-Tools.h"
#include "NES-MEP-WebPages.h"
#include "NES-MEP-RecoveryWebPages.h"

const char compile_date[] = __DATE__ " " __TIME__;

extern IPAddress IP;
extern WebServer MyWebServer;
extern Preferences preferences;
extern long UserLoginSession;
extern long SecretLoginSession;
extern MEPQueueStruct MEPQueue[];
extern char wifi_ssid[];
extern char wifi_password[];
extern char user_login[];
extern char user_password[];
extern char mep_key[];
extern byte MEPQueueNextIndex;
extern ConsumptionDataStruct ConsumptionData;
extern MeterInfoStruct MeterInfo;

String GetESP32DateTime(boolean JsonFormat)
{
  struct tm tm;
  
  getLocalTime(&tm);
  return DateTime2String(tm.tm_year-100,tm.tm_mon+1,tm.tm_mday,tm.tm_hour,tm.tm_min,tm.tm_sec,tm.tm_wday+1,JsonFormat);
}

void HandleFileInclude(String *pageBuffer, String tag, String filename)
{
  File IncludeFile;
  if (pageBuffer->indexOf(tag) > 0) {
    IncludeFile = SPIFFS.open(filename, FILE_READ);
    if (IncludeFile) {
      Serial.printf("Replacing tag %s with contents of file %s\r\n", tag.c_str(), filename.c_str());
      pageBuffer->replace(tag, IncludeFile.readString());
      IncludeFile.close();
    }
  }
}

String MEPQueue2HTML()
{
  String htmlBuffer = "";
  for (unsigned long i = 0; i < MaxMEPBuffer; i++)
  {
    htmlBuffer +=
      "<tr>"
      "<td>" + String(i) + "</td>"
      "<td>" + UnsignedLong2String(MEPQueue[i].Millis, true) + "</td>"
      "<td><small>" + bytesToHexString(MEPQueue[i].Request, MEPQueue[i].RequestLength, true) + "</small></td>"
      "<td>" + UnsignedLong2String(MEPQueue[i].SendAttempts, true) + "</td>"
      "<td><small>" + bytesToHexString(MEPQueue[i].Reply, MEPQueue[i].ReplyLength, true) + "</small></td>"
      "<td>" + ReplyPackageToMessage(MEPQueue, i) + "</td>"
      "<td>" + UnsignedLong2String(MEPQueue[i].NextAction, true) + "</td>"
      "</tr>";
  }
  return htmlBuffer;
}

void RedirectWebRequest(String URL)
{
  MyWebServer.sendHeader("Connection", "close");
  MyWebServer.send(200, "text/html", "<meta http-equiv='refresh' content='0; URL="+URL+"?LoginSession="+String(UserLoginSession)+"'/>");
}

void HandleGetDashDataWS()
{
  const char json[] PROGMEM = R"rawliteral(
    {
      "ESP_SW":"ESP32 MEP",
      "ESP_SW_By":"dabbler.dk",
      "ESP_SW_Version":"%s",
      "Meter_Manufacturer":"%s",
      "Meter_Model":"%s",
      "Meter_SW_Version":"%i.%i",
      "Utility_SN":"%s",
      "CurrentDateTimeString":"%s",
      "CurrentDateTime":"%s",
      "Fwd_Act_Wh":%lu,
      "Rev_Act_Wh":%lu,
      "Fwd_W":%lu,
      "Rev_W":%lu,
      "Freq_mHz":%lu,
      "L1_RMS_A":%lu,
      "L2_RMS_A":%lu,
      "L3_RMS_A":%lu,
      "L1_RMS_V":%lu,
      "L2_RMS_V":%lu,
      "L3_RMS_V":%lu,
      "L1_Fwd_W":%lu,
      "L2_Fwd_W":%lu,
      "L3_Fwd_W":%lu,
      "L1_Rev_W":%lu,
      "L2_Rev_W":%lu,
      "L3_Rev_W":%lu,
      "Fwd_Avg_W":%lu,
      "Rev_Avg_W":%lu,
      "L1_Fwd_Avg_W":%lu,
      "L2_Fwd_Avg_W":%lu,
      "L3_Fwd_Avg_W":%lu,
      "L1_Rev_Avg_W":%lu,
      "L2_Rev_Avg_W":%lu,
      "L3_Rev_Avg_W":%lu
    }
  )rawliteral";
  char pageBuffer[1024];
  
  MyWebServer.sendHeader("Connection", "close");
  sprintf(pageBuffer,json,
          compile_date,
          MeterInfo.BT01_Manufacturer,
          MeterInfo.BT01_Model,
          MeterInfo.BT01_MainFirmwareVersionNumber,
          MeterInfo.BT01_FirmwareRevisionNumber,
          MeterInfo.ET03_UtilitySerialNumber,
          GetESP32DateTime(false).c_str(),
          GetESP32DateTime(true).c_str(),
          ConsumptionData.BT23_Fwd_Act_Wh,ConsumptionData.BT23_Rev_Act_Wh,
          ConsumptionData.BT28_Fwd_W,ConsumptionData.BT28_Rev_W,
          ConsumptionData.BT28_Freq_mHz,
          ConsumptionData.BT28_RMS_mA_L1,ConsumptionData.BT28_RMS_mA_L2,ConsumptionData.BT28_RMS_mA_L3,
          ConsumptionData.BT28_RMS_mV_L1,ConsumptionData.BT28_RMS_mV_L2,ConsumptionData.BT28_RMS_mV_L3,
          ConsumptionData.BT28_Fwd_W_L1,ConsumptionData.BT28_Fwd_W_L2,ConsumptionData.BT28_Fwd_W_L3,
          ConsumptionData.BT28_Rev_W_L1,ConsumptionData.BT28_Rev_W_L2,ConsumptionData.BT28_Rev_W_L3,
          ConsumptionData.BT28_Fwd_Avg_W,ConsumptionData.BT28_Rev_Avg_W,
          ConsumptionData.BT28_Fwd_Avg_W_L1,ConsumptionData.BT28_Fwd_Avg_W_L2,ConsumptionData.BT28_Fwd_Avg_W_L3,
          ConsumptionData.BT28_Rev_Avg_W_L1,ConsumptionData.BT28_Rev_Avg_W_L2,ConsumptionData.BT28_Rev_Avg_W_L3);
  MyWebServer.send(200, "application/json", pageBuffer);
  Serial.printf("Serving web request: GetDashDataWS with Fwd_Act_Wh=%lu and Rev_Act_Wh=%lu\r\n",ConsumptionData.BT23_Fwd_Act_Wh,ConsumptionData.BT23_Rev_Act_Wh);
}

boolean CheckLogin()
{
  if (MyWebServer.arg("LoginSession") == String(SecretLoginSession))
    return true;

  RedirectWebRequest("/loginIndex");
  return false;
}

void HandleWebRequest(String URL, String Filename, String ContentType, boolean VerifyLogin)
{
  String pageBuffer;
  File WebFile;
  MyWebServer.sendHeader("Connection", "close");
  if (VerifyLogin && !CheckLogin())
    return;

  if (!SPIFFS.exists(Filename))
  {
    MyWebServer.send(404);
    Serial.printf("File %s was not found. Returning 404 as response to %s\r\n", Filename.c_str(), URL.c_str());
    return;
  }

  WebFile = SPIFFS.open(Filename, FILE_READ);
  if (WebFile)
  {
    pageBuffer = WebFile.readString();
    WebFile.close();
    HandleFileInclude(&pageBuffer, "###FILE:/menu-include.html###", "/www_menu-include.html");
    pageBuffer.replace("###CurDateTime###", GetESP32DateTime(false));
    pageBuffer.replace("###MEPQueue###", MEPQueue2HTML());
    pageBuffer.replace("###LoginSession###", String(UserLoginSession));
    pageBuffer.replace("###IP###", IP.toString());
    pageBuffer.replace("###Version###", String(compile_date));
    pageBuffer.replace("###wifi_ssid###", wifi_ssid);
    pageBuffer.replace("###wifi_pwd###", wifi_password);
    pageBuffer.replace("###user_login###", user_login);
    pageBuffer.replace("###user_pwd###", user_password);
    pageBuffer.replace("###mep_key###", mep_key);
    pageBuffer.replace("###MaxMEPReplyLengthAsHex###", MaxMEPReplyLengthAsHex());
    MyWebServer.send(200, ContentType, pageBuffer);
    Serial.printf("File %s sent as response to %s\r\n", Filename.c_str(), URL.c_str());
  }
  else
  {
    MyWebServer.send(404);
    Serial.printf("File %s could not be opened. Returning 404 as response to %s\r\n", Filename.c_str(), URL.c_str());
  }
}

void SendMEPRequest(String PredefinedMEPPackage, String RawMEPPackage)
{
  if((PredefinedMEPPackage == "") || (PredefinedMEPPackage == "None"))
  {
    if (RawMEPPackage != "")
    {
      Serial.printf("Send raw MEP request: %s\r\n",RawMEPPackage.c_str());
      queueRequest(RawMEPPackage,mep_key,MEPQueue,&MEPQueueNextIndex,None);
    }
  }
  else
  {
    Serial.printf("Send predefined MEP request: %s\r\n",PredefinedMEPPackage.c_str());
    queueRequest(PredefinedMEPPackage,mep_key,MEPQueue,&MEPQueueNextIndex,None);
  }
}

boolean HandleLogin(String Login, String Password)
{
  boolean LoggedIn;
  if(((Login == String(wifi_ssid))  && (Password == String(wifi_password))) ||
     ((Login == String(user_login)) && (Password == String(user_password))) ||
     (String(user_password) == ""))
  {
    SecretLoginSession = random(LONG_MAX);
    UserLoginSession = SecretLoginSession;
    Serial.printf("Login successfull:\r\n");
    LoggedIn = true;
  }
  else
  {
    Serial.printf("Login FAILED with this info.:\r\n");
    LoggedIn = false;
  }
  Serial.printf("Login: '%s'\r\n",Login.c_str());
  Serial.printf("Password: '%s'\r\n",Password.c_str());
  return LoggedIn;
}

void StoreNewConfig(String ssid, String pwd, String userlogin, String userpwd, String mepkey)
{
  
  Serial.printf("Storing new config in preferences:\r\n");
  Serial.printf("SSID: %s\r\n",ssid.c_str());
  Serial.printf("Password: %s\r\n",pwd.c_str());
  preferences.putString("wifi_ssid",ssid);
  preferences.putString("wifi_password",pwd);
  
  Serial.printf("User login: %s\r\n",userlogin.c_str());
  Serial.printf("User Password: %s\r\n",userpwd.c_str());
  preferences.putString("user_login",userlogin);
  preferences.putString("user_password",userpwd);

  Serial.printf("MEP Key: %s\r\n",mepkey.c_str());
  preferences.putString("mep_key",mepkey);
}

void SetupWebPages()
{
  // SPIFFS pages that does not require authorization
  MyWebServer.on("/style.css",                 HTTP_GET,  []() {
    HandleWebRequest("/style.css",  "/www_style.css",        "text/css", false);
  });
  MyWebServer.on("/sevenSeg.js",               HTTP_GET,  []() {
    HandleWebRequest("/sevenSeg.js",  "/www_sevenSeg.js",    "text/javascript", false);
  });
  MyWebServer.on("/loginIndex",                HTTP_GET,  []() {
    SecretLoginSession = random(LONG_MAX);
    HandleWebRequest("/loginIndex", "/www_loginIndex.html",  "text/html", false);
  });
  MyWebServer.on("/dashIndex",                 HTTP_GET,  []() {
    HandleWebRequest("/",           "/www_dashIndex.html",    "text/html", false);
  });
  MyWebServer.on("/getDashDataWS",             HTTP_GET,  []() {
    HandleGetDashDataWS();
  });

  // Simple SPIFFS pages that requires authorization and only return pages (no parameters)
  MyWebServer.on("/",                          HTTP_GET,  []() {
    HandleWebRequest("/",           "/www_homeIndex.html",   "text/html", true);
  });
  MyWebServer.on("/setupIndex",                HTTP_GET,  []() {
    HandleWebRequest("/",           "/www_setupIndex.html",  "text/html", true);
  });
  MyWebServer.on("/otaIndex",                  HTTP_GET,  []() {
    HandleWebRequest("/",           "/www_otaIndex.html",    "text/html", true);
  });

  // Actions that does NOT require authorization
  MyWebServer.on("/Login",                     HTTP_GET, []() {
    if(HandleLogin(MyWebServer.arg("login"),MyWebServer.arg("pwd")))
      RedirectWebRequest("/");
    else
      RedirectWebRequest("/loginIndex");
  });

  // Actions that requires authorization
  MyWebServer.on("/SendMEP",                   HTTP_GET, []() {
    if(CheckLogin())
    {
      
      SendMEPRequest(MyWebServer.arg("PredefinedMEPPackage"), MyWebServer.arg("RawMEPPackage"));
      RedirectWebRequest("/");
    }
    else
      RedirectWebRequest("/loginIndex");    
  });

  MyWebServer.on("/SavePreferencesAndRestart", HTTP_GET, []() {
    if(CheckLogin())
    {
      StoreNewConfig(MyWebServer.arg("ssid"),MyWebServer.arg("pwd"),MyWebServer.arg("userlogin"),MyWebServer.arg("userpwd"),MyWebServer.arg("mepkey"));
      RedirectWebRequest("/");
      delay(5000);
      preferences.end();  
      ESP.restart();
    }
    else
      RedirectWebRequest("/loginIndex");    
  });
  
  MyWebServer.on("/updateFirmware",            HTTP_POST, []() {
    if(CheckLogin())
    {
      if(Update.hasError())
      {
        MyWebServer.sendHeader("Connection", "close");
        MyWebServer.send(200, "text/plain", "UPDATE FAILED!!");
      }
    }
    else
      RedirectWebRequest("/loginIndex");    
  },
  HandleFirmwareUpload); // Receive and save the file
}
