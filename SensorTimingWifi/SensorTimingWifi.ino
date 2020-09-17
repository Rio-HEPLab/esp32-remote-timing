/*
  DigitalReadSerial

  Reads a digital input on pin 2, prints the result to the Serial Monitor

  This example code is in the public domain.

  http://www.arduino.cc/en/Tutorial/DigitalReadSerial
*/

#include <WiFi.h>
#include <WiFiUdp.h>
#include <time.h>
#include <math.h>

#include "secrets.h"

//int wifi_status = WL_IDLE_STATUS;
boolean connected = false;
boolean calib = false;

// digital pin 2 has a pushbutton attached to it. Give it a name:
//int pushButton = 2;
//int pushButton = 5;

//int sensor = 7;
int sensor = 13;

int currentState = 0;

const int GMT = 1;
const int daylightOffset = 1;

//const char* ntpServer = "pool.ntp.org";
const char* ntpServer = "time.nist.gov";
const long  gmtOffset_sec = 3600 * GMT;
const int   daylightOffset_sec = 3600 * daylightOffset;

// A UDP instance to let us send and receive packets over UDP
WiFiUDP udp;
//IPAddress ip_broadcast(192, 168, 1, 255);
IPAddress ip_broadcast(192, 168, 4, 255);
unsigned int localPort = 2390;      // local port to listen for UDP packets
unsigned int remotePort = 3333;
unsigned int udpPort = localPort;
const unsigned int PACKET_SIZE = 16;
//unsigned char packetBuffer[256]; // data buffer
unsigned char packetBuffer[PACKET_SIZE];
IPAddress masterAddress;
float delay_val = 0;
float delay_val2 = 0;
float std_dev_delay = 0;
unsigned long remote_counter = 0;
unsigned long local_counter_ref = 0;
boolean set_counter_ref = false;

// the setup routine runs once when you press reset:
void setup() {
  // initialize serial communication at 9600 bits per second:
  Serial.begin(115200);
  Serial.println("Booting");
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
    
  // make the pushbutton's pin an input:
  //pinMode(pushButton, INPUT);
  pinMode(sensor, INPUT);
  // initialize digital pin LED_BUILTIN as an output.
  pinMode(LED_BUILTIN, OUTPUT);

  connectToWiFi(ssid, pass);

  while ( !connected ) { delay(100); Serial.print("."); };
    
  Serial.println("CONNECTED");

  String str = "Setting UDP port to "; str += localPort;
  Serial.println( str );
  udp.begin(localPort);
  //init_ntp = -1; 
  
  //init and get the time
  //configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org", "time.nist.gov");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  //Serial.println("Waiting for time");
  Serial.print("Waiting for time");
  // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
  //const unsigned long seventyYears = 2208988800UL;
  /*time_t now;
  time( &now );
  Serial.println( now );*/
  while ( !time( nullptr ) ) {
  //while ( !time( &now ) ) {
    Serial.print(".");
    //Serial.print( now );
    delay(100);
  }
  Serial.println("\nTime set.");
  
  printLocalTime();
      
}

// the loop routine runs over and over again forever:
void loop() {

  if ( !calib && connected ) {
    set_counter_ref = false;
    local_counter_ref = 0;
    remote_counter = 0;
    delay_val = 0;
    delay_val2 = 0;
    std_dev_delay = 0;
    unsigned int numberOfRounds = 10;
    for (unsigned int round = 0; round < numberOfRounds; ++round) {
      unsigned int numberOfTries = 0, maxTries = 5;
      unsigned long counter = 0, max_counter = 10E6;
      boolean pass = false; 
      boolean calib_round = false;
      while ( !calib_round && numberOfTries < maxTries ) {

        String str = "\nCALIB attempt "; str += numberOfTries;
        Serial.println( str );
      
        // Send CALIB command
        udp.beginPacket( ip_broadcast, remotePort );
        udp.printf( "CALIB" );
        udp.endPacket();
        Serial.println("Sent CALIB packet.");

        Serial.println("Waiting return from CALIB command.");
        pass = false; counter = 0;
        while ( counter < max_counter ) {
          if ( udp.parsePacket() ) { pass = true; break; }
          ++counter;
        }
        if ( pass ) {
          // Remote time
          //udp.read( packetBuffer, 256);
          udp.read( packetBuffer, PACKET_SIZE );
          // Send reply
          masterAddress = udp.remoteIP();
          udp.beginPacket( masterAddress, remotePort );
          udp.printf( "ACK" );
          udp.endPacket();
          Serial.println("ACK sent.");
          // Parse remote time and set local counter only in the first CALIB cycle
          if ( !set_counter_ref ) {
            local_counter_ref = millis();
            String str = "Local time = "; str += local_counter_ref;
            Serial.println( str );
            
            unsigned char buff4[4];
            memcpy(buff4, packetBuffer, 4);
            unsigned long* remote_counter_ptr = (unsigned long*)buff4;
            remote_counter = *remote_counter_ptr;
            str = "Remote time = "; str += remote_counter;
            Serial.println( str );
            
            set_counter_ref = true;
          }
        } else {
          ++numberOfTries;
          continue;
        }

        Serial.println("Waiting return with delay value.");
        pass = false; counter = 0;
        while ( counter < max_counter ) {
          if ( udp.parsePacket() ) { pass = true; break; }
          ++counter;
        }
        if ( pass ) {
          Serial.println("Packet received.");
          //udp.read( packetBuffer, 256 );
          udp.read( packetBuffer, PACKET_SIZE );
          unsigned char buff4[4];
          memcpy(buff4, packetBuffer, 4);
          unsigned long* delay_val_ptr = (unsigned long*)buff4;
          unsigned long delay_val_tmp = *delay_val_ptr;
          // Sum delays for average
          delay_val += delay_val_tmp;
          delay_val2 += ( delay_val_tmp * delay_val_tmp );
          String str = "Delay = "; str += delay_val_tmp;
          Serial.println( str );

          calib_round = true;
          if ( !calib ) calib = true;      
        } else {
          ++numberOfTries;
          continue;
        }
      }      
    }
    if ( calib ) {
      delay_val /= numberOfRounds;
      delay_val2 /= numberOfRounds;
      std_dev_delay = ( ( (float)numberOfRounds )/( numberOfRounds - 1 ) ) * sqrt( delay_val2 - delay_val*delay_val );
      String str = "Average delay = "; str += delay_val;
      Serial.println( str );
      str = "Std. deviation = "; str += std_dev_delay;
      Serial.println( str ); 
      Serial.println("CALIB succeeded.");
    }
    else {
      set_counter_ref = false;
      local_counter_ref = 0;
      remote_counter = 0;
      delay_val = 0;
      delay_val2 = 0;
      calib = true;
      Serial.println("CALIB failed.");
    }
  }
  
  // read the input pin:
  //int read_pushButton = digitalRead(pushButton);
  int read_sensor = digitalRead(sensor);
  
  int nextState = read_sensor;

  if ( nextState != currentState ) {

    time_t epoch_now;
    time( &epoch_now );

    unsigned long counter_now = millis();
    
    digitalWrite(LED_BUILTIN, nextState);
    
    //Serial.print("Changed state from "); Serial.print(currentState); Serial.print(" to "); Serial.print(nextState); Serial.println("."); 
    String str = "Changed state from "; str += currentState; str += " to "; str += nextState; str += ".";
    Serial.println(str);

    // send a reply, to the IP address and port that sent us the packet we received
    udp.beginPacket(ip_broadcast, remotePort);

    counter_now = ( counter_now - local_counter_ref + remote_counter + delay_val );
    
    memset(packetBuffer, 0, PACKET_SIZE);
    size_t pos = 0;
    memcpy(&packetBuffer[pos], &epoch_now, sizeof(unsigned long));
    str = "Pos "; str += pos; str += ": "; str += epoch_now; str += "\n";
    pos += sizeof(unsigned long);
    memcpy(&packetBuffer[pos], &counter_now, sizeof(unsigned long));
    str = "Pos "; str += pos; str += ": "; str += counter_now; str += "\n";
    pos += sizeof(unsigned long);
    packetBuffer[pos] = nextState;
    str += "Pos "; str += pos; str += ": "; str += packetBuffer[pos];
    Serial.println(str);
     
    //udp.write(packetBuffer, 256);
    udp.write(packetBuffer, PACKET_SIZE);
    udp.endPacket();
    
    currentState = nextState;
  }
  
}

void printLocalTime()
{
  /*struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");*/
  time_t now;
  struct tm * timeinfo;
  time(&now);
  timeinfo = localtime(&now);
  Serial.println(timeinfo, "%A, %B %d %Y %H:%M:%S");
}

void printWiFiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}

void print2digits(int number) {
  if (number < 10) {
    Serial.print("0");
  }
  Serial.print(number);
}

void connectToWiFi(const char * ssid, const char * pwd){
  Serial.println("Connecting to WiFi network: " + String(ssid));

  // delete old config
  WiFi.disconnect(true);
  //register event handler
  WiFi.onEvent(WiFiEvent);
  
  //Initiate connection
  WiFi.begin(ssid, pwd);

  Serial.println("Waiting for WIFI connection...");
}

//wifi event handler
void WiFiEvent(WiFiEvent_t event){
    switch(event) {
      case SYSTEM_EVENT_STA_GOT_IP:
          //When connected set 
          Serial.print("WiFi connected! IP address: ");
          Serial.println(WiFi.localIP());  
          //initializes the UDP state
          //This initializes the transfer buffer
          udp.begin(WiFi.localIP(),udpPort);
          connected = true;
          break;
      case SYSTEM_EVENT_STA_DISCONNECTED:
          Serial.println("WiFi lost connection");
          connected = false;
          break;
      default: break;
    }
}

/*// send an NTP request to the time server at the given address
unsigned long sendNTPpacket(IPAddress& address) {
  //Serial.println("1");
  // set all bytes in the buffer to 0
  memset(packetBufferNTP, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  //Serial.println("2");
  packetBufferNTP[0] = 0b11100011;   // LI, Version, Mode
  packetBufferNTP[1] = 0;     // Stratum, or type of clock
  packetBufferNTP[2] = 6;     // Polling Interval
  packetBufferNTP[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersionp
  packetBufferNTP[13]  = 0x4E;
  packetBufferNTP[14]  = 49;
  packetBufferNTP[15]  = 52;

  //Serial.println("3");

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  //Serial.println("4");
  Udp.write(packetBufferNTP, NTP_PACKET_SIZE);
  //Serial.println("5");
  Udp.endPacket();
  //Serial.println("6");
}*/
