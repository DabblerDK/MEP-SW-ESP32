#include <Arduino.h>
#include "SPIFFS.h"
#include <WebServer.h>
#include <Update.h>
#include "NES-MEP.h"
#include "NES-MEP-RecoveryWebPages.h"

extern WebServer MyWebServer;

File RecoveryFile; // Need to be global so it can be used between calls to HandleFirmwareUpload()

void FormatSPIFFS()
{
  Serial.printf("Format SPIFF: %i\r\n",  SPIFFS.format());
  MyWebServer.sendHeader("Location", "/recovery"); // Redirect the client to the success page
  MyWebServer.send(303);
}

void RemoveAllWWWFiles()
{
  File root = SPIFFS.open("/");
  File DeleteFile = root.openNextFile();
  while (DeleteFile)
  {
    if (String(DeleteFile.name()).startsWith("/www_"))
    {
      Serial.printf("  Removing: %s\r\n", DeleteFile.name());
      SPIFFS.remove(DeleteFile.name());
    }
    DeleteFile = root.openNextFile();
  }
}

void SplitAllWWWFiles()
{
  String buffer;
  File outfile;
  File infile = SPIFFS.open("/update.wwws", FILE_READ);
  if (infile)
  {
    while (infile.available())
    {
      buffer = infile.readStringUntil('\n');
      if (buffer.endsWith("\r"))
      {
        buffer.remove(buffer.length()-1);
      }
      if (buffer.startsWith("/www_")) // New file
      {
        if (outfile)
        {
          outfile.close();
          Serial.printf("---EOF---\r\n");
        }
        outfile = SPIFFS.open(buffer, FILE_WRITE);
        Serial.printf("   Extracting file: %s\r\n", buffer.c_str());
        Serial.printf("---SOF---\r\n");
      }
      else
      {
        outfile.printf("%s\r\n", buffer.c_str());
        Serial.printf("%s\r\n", buffer.c_str());
      }
    }
    if (outfile)
    {
      outfile.close();
      Serial.printf("---EOF---\r\n");
    }
    infile.close();
  }
  Serial.printf("   Done extracting files!\r\n");
}

void HandleStaticRecovery()
{
  const char StaticRecoveryIndex[] PROGMEM = R"rawliteral(
    <html>
      <body>
        <H1>ESP32 MEP Recovery and update</H1>
        This upload supports both firmware files (.bin-files) and html files (.wwws-files).<br>
        Please always upload both in that order (.bin, then .wwws) when updating.<br><br>
        1. <a href='/formatSPIFFS'>Optional: Format SPIFFS (ESP32 SPI File System)</a><br><br>
        <form method='post' enctype='multipart/form-data' action='/recovery'>
          2. <input type='file' name='recoveryFile' accept='.wwws,.bin'><br><br>
          3. <input class='button' type='submit' value='Upload'>
        </form>
        <script>
          function sub(obj){
            var fileName = obj.value.split('\\\\');
            document.getElementById('file-input').innerHTML = '   '+ fileName[fileName.length-1];
          };
          
          $('form').submit(function(e){
            e.preventDefault();
            var form = $('#upload_form')[0];
            var data = new FormData(form);
            $.ajax({
              url: '/recovery',
              type: 'POST',
              data: data,
              contentType: false,
              processData:false,
              xhr:function(){
                var xhr = new window.XMLHttpRequest();
                xhr.upload.addEventListener('progress', function(evt){
                  if (evt.lengthComputable) {
                    var per = evt.loaded / evt.total;
                    $('#prg').html('progress: ' + Math.round(per*100) + '%');
                    $('#bar').css('width',Math.round(per*100) + '%');
                  }
                }, false);
                return xhr;
              },
              success:function(d, s){
                console.log('success!');
                window.location.replace("/");
              },
              error:function (a, b, c){
              }
            });
          });
        </script>
        <br><div id='prg'></div><br>
        4a. <a href='/recovery'>When the upload of the bin file is done, wait a few seconds and then click this line to reload!</a><br><br>
        4b. <a href='/'>When the upload of the wwws file is done, wait a few seconds and then click this line to reload!</a><br>
      <body>
    <html>
  )rawliteral";

  Serial.printf("Recovery page opened. Awaiting file upload...\r\n");
  MyWebServer.sendHeader("Connection", "close");
  MyWebServer.send(200, "text/html", StaticRecoveryIndex);
}

void HandleFirmwareUpload()
{
  HTTPUpload& upload = MyWebServer.upload();
  String filename = upload.filename;
  if (upload.status == UPLOAD_FILE_START)
  {
    if (filename.endsWith(String(".bin")))
    {
      filename = "/update.bin";
      Serial.printf("HandleFirmwareUpload start...\r\nFilename: %s\r\n", filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) //start with max available size
        Update.printError(Serial);
    }
    else if (filename.endsWith(String(".wwws")))
    {
      filename = "/update.wwws";
      Serial.printf("HandleFirmwareUpload start...\r\nFilename: %s\r\n", filename.c_str());
      RecoveryFile = SPIFFS.open(filename, FILE_WRITE); // Open the file for writing in SPIFFS (create if it doesn't exist)
    }
    else
    {
      Serial.printf("HandleFirmwareUpload/Could not identify file from filename: %s!!!\r\n", filename.c_str());
      MyWebServer.send(500, "text/plain", "500: Unknown file received!!!");
      return;
    }
  }
  else if (upload.status == UPLOAD_FILE_WRITE)
  {
    if (filename.endsWith(String(".bin")))
    {
      // flashing firmware to ESP
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
        Update.printError(Serial);
    }
    else if (filename.endsWith(String(".wwws")))
    {
      // Write the received bytes to the file
      if (RecoveryFile)
        if (RecoveryFile.write(upload.buf, upload.currentSize) != upload.currentSize)
          Serial.printf("Error while writing to: %s\r\n", filename.c_str());
    }
  }
  else if (upload.status == UPLOAD_FILE_END)
  {
    if (filename.endsWith(String(".bin")))
    {
      if (Update.end(true)) //true to set the size to the current progress
      {
        Serial.printf("Update Success: %u\r\nRebooting...\r\n", upload.totalSize);
        MyWebServer.sendHeader("Location", "/recovery"); // Redirect the client to the success page
        MyWebServer.send(303);
        delay(1000);
        ESP.restart();
      }
      else
        Update.printError(Serial);
    }
    else if (filename.endsWith(String(".wwws")))
    {
      if (RecoveryFile)
      {
        RecoveryFile.close(); // Close the file
        Serial.printf("HandleFirmwareUpload/File Size: %i\r\n", upload.totalSize);
        Serial.printf("File with wwws (.wwws-file) was received:\r\n1. Removing all www files from SPIFFS...\r\n");
        RemoveAllWWWFiles();
        Serial.printf("2. Extracting www files from the received file...\r\n");
        SplitAllWWWFiles();
        Serial.printf("3. Deleting update.wwws file...\r\n");
        SPIFFS.remove("/update.wwws");
        Serial.printf("4. Done...\r\n");
        MyWebServer.sendHeader("Location", "/"); // Redirect the client to the success page
        MyWebServer.send(303);
      }
    }
  }
  else
  {
    Serial.printf("HandleFirmwareUpload/Upload failed!!!\r\n");
    MyWebServer.send(500, "text/plain", "500: Upload failed!!!");
  }
}

// Recovery pages (works without SPIFFS pages - can be used to flash and initialize SPIFFS)
void SetupRecoveryWebPages()
{
  MyWebServer.on("/recovery",     HTTP_GET,  []() {
    HandleStaticRecovery();
  });
  
  MyWebServer.on("/recovery",     HTTP_POST, []() {
    MyWebServer.send(200);     // Send status 200 (OK) to tell the client we are ready to receive
  },
  HandleFirmwareUpload); // Receive and save the file
  
  MyWebServer.on("/formatSPIFFS", HTTP_GET,  []() {
    FormatSPIFFS();
  });
}
