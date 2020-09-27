
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <LiquidCrystal.h>
#include <time.h>

#include "secrets.h"

// initialize the library by associating any needed LCD interface pin
// with the arduino pin number it is connected to
//const int rs = 12, en = 11, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
//LiquidCrystal lcd(rs, en, d4, d5, d6, d7);
LiquidCrystal lcd(22, 23, 5, 18, 19, 21);

const unsigned int PACKET_SIZE = 16;
//unsigned char ReceivedPackage[256];
unsigned char ReceivedPackage[PACKET_SIZE];
unsigned char buffervar[4];
unsigned char macAdd[6];

//The udp library class
WiFiUDP udp;
unsigned int localPort = 3333;      // local port to listen for UDP packets

//WiFiServer server(80);

unsigned long event_count = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("Booting");
  
  /*WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }*/

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

  Serial.println("Configuring access point...");

  // You can remove the password parameter if you want the AP to be open.
  WiFi.softAP(ssid, pass);
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);
  //server.begin();
  //Serial.println("Server started");

  String str = "Setting UDP port to "; str += localPort;
  Serial.println( str );
  udp.begin( localPort );

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

  /*Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());*/

  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  // Print a message to the LCD.
  lcd.print("READY");
}

void loop() 
{
  
  ArduinoOTA.handle();

  //WiFiClient client = server.available();   // listen for incoming clients
  
  if(udp.parsePacket())
  {
    //udp.read(ReceivedPackage,256);
    udp.read(ReceivedPackage,PACKET_SIZE);    
    if (ReceivedPackage[0]=='C' && ReceivedPackage[1]=='A' && ReceivedPackage[2]=='L' && ReceivedPackage[3]=='I' && ReceivedPackage[4]=='B')
    {
      Serial.println("Entering on Calibration Mode");
      IPAddress remote = udp.remoteIP();
      uint16_t port = udp.remotePort();

      unsigned long refTime = millis();
      memset(ReceivedPackage, 0, PACKET_SIZE);
      memcpy(&ReceivedPackage[0], &refTime, sizeof(unsigned long));
      udp.beginPacket(remote,port);
      udp.write(ReceivedPackage, PACKET_SIZE);
      udp.endPacket();
      /*udp.beginPacket(remote,port);
      udp.printf("%lu", millis());
      udp.endPacket();*/
      Serial.print("Ref. time: "); Serial.println(refTime);
      
      unsigned long startTime = millis();
      Serial.print("Start time: "); Serial.println(startTime);
            
      Serial.print("Waiting Return Package from ");
      Serial.println(remote);
      //while (!udp.parsePacket()) {};
      // Check that packet has remote client as sender
      while (1) {
        if ( udp.parsePacket() && udp.remoteIP() == remote ) break;
      };
      unsigned long stopTime = millis();
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
      size_t pos = 0;
      memcpy(macAdd,&ReceivedPackage[pos],6);
      pos += 6;      
      memcpy(buffervar,&ReceivedPackage[pos],sizeof(unsigned long));
      pos += sizeof(unsigned long);
      //unsigned long timestamp = buffervar[0] | (buffervar[1] << 8) | (buffervar[2] << 16) | (buffervar[3] << 24);
      unsigned long* timestamp_ptr = (unsigned long*)buffervar;
      unsigned long timestamp = *timestamp_ptr;
      memcpy(buffervar,&ReceivedPackage[pos],sizeof(unsigned long));
      pos += sizeof(unsigned long);
      unsigned long* counter_ptr = (unsigned long*)buffervar;
      unsigned long counter = *counter_ptr;
      int state = ReceivedPackage[pos];

      Serial.println( "DATA BEGIN" );
      Serial.printf ( "MAC: %x:%x:%x:%x:%x:%x TimeStamp: %d Counter: %d State Found: %d",
                       macAdd[5],macAdd[4],macAdd[3],macAdd[2],macAdd[1],macAdd[0],timestamp,counter,state);
      Serial.println();
      Serial.println( "DATA END" );
      digitalWrite(LED_BUILTIN,state);
      /*digitalWrite(LED_BUILTIN,HIGH);
      delay(100);
      digitalWrite(LED_BUILTIN,LOW);*/

      ++event_count;
      lcd.setCursor(0,0);
      lcd.print("Count=");lcd.print(event_count);
      lcd.print(" St.="); lcd.print(state);
      lcd.setCursor(0,1);
      lcd.print("Time="); lcd.print(counter);
    }
  }
}
