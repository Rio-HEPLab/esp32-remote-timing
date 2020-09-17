#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#include "secrets.h"

const unsigned int PACKET_SIZE = 8;
//unsigned char ReceivedPackage[256];
unsigned char ReceivedPackage[PACKET_SIZE];

//The udp library class
WiFiUDP udp;

void setup() {
  Serial.begin(115200);
  Serial.println("Booting");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  // Port defaults to 3232
  // ArduinoOTA.setPort(3232);

  // Hostname defaults to esp3232-[MAC]
  // ArduinoOTA.setHostname("myesp32");

   // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");
  pinMode(LED_BUILTIN, OUTPUT);
  udp.begin(3333);
  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
      ESP.restart();
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();

  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void loop() 
{
  ArduinoOTA.handle();
  if(udp.parsePacket())
  {
    char buffervar[4];
    //udp.read(ReceivedPackage,256);
    udp.read(ReceivedPackage,PACKET_SIZE);    
    if (ReceivedPackage[0]=='C' && ReceivedPackage[1]=='A' && ReceivedPackage[2]=='L' && ReceivedPackage[3]=='I' && ReceivedPackage[4]=='B')
    {
      Serial.println("Entering on Calibration Mode!");
      IPAddress remote = udp.remoteIP();
      uint16_t port = udp.remotePort();
      
      udp.beginPacket(remote,port);
      udp.printf("%lu", millis());
      udp.endPacket();
      
      int startTime = millis();
      Serial.print("Waiting Return Package from ");
      Serial.println(remote);
      while(!udp.parsePacket())
      {};
      int stopTime = millis();
      //udp.read(ReceivedPackage,256);
      udp.read(ReceivedPackage,PACKET_SIZE);
      unsigned long delayVar = (stopTime - startTime)/2;
      Serial.print("Delay calculated: ");
      Serial.println(delayVar);
      
      memset(ReceivedPackage, 0, PACKET_SIZE);
      memcpy(&ReceivedPackage[0], &delayVar, sizeof(unsigned long));
      udp.beginPacket(remote,port);
      //udp.printf("%lu", delayVar);
      //udp.write(ReceivedPackage, 256);
      udp.write(ReceivedPackage, PACKET_SIZE);
      udp.endPacket();
      Serial.println("Delay packet sent.");
    }
    else
    {
      memcpy(buffervar,ReceivedPackage,4);
      int state = ReceivedPackage[4];
      unsigned long timestamp = buffervar[0] | (buffervar[1] << 8) | (buffervar[2] << 16) | (buffervar[3] << 24);
      Serial.printf("TimeStamp: %d - State Found: %d\n",timestamp,state);
      digitalWrite(LED_BUILTIN,state);
    }
  }
}
