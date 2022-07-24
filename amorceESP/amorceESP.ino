 /*
  Programme d'amorce pour permettre le téléchargement des firmware et filesystem dans le Wemos 
  */

#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>

const char* ssid = "MaxPV";
const char* password = "MaxPVMaxPV";

IPAddress local_IP ( 192, 168, 4, 1 );
IPAddress gateway ( 192, 168, 4, 1 );
IPAddress subnet ( 255, 255, 255, 0 );

AsyncWebServer server ( 80 );

void setup ( void ) {
  Serial.begin ( 115200 );

  WiFi.softAPConfig ( local_IP, gateway, subnet );
  WiFi.softAP ( ssid, password );

  server.on ( "/", HTTP_GET, []( AsyncWebServerRequest *request ) {
    request->send ( 200, "text/plain", "Connect to http://192.168.4.1/update" );
  } );

  AsyncElegantOTA.begin ( &server );    // Start AsyncElegantOTA

  server.begin ( );
  
  Serial.println ( "HTTP server started, connect to http://192.168.4.1/update" );
}

void loop ( void ) { }
