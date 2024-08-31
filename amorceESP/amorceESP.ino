 /*
  Programme d'amorce pour permettre le téléchargement des firmware et filesystem dans le Wemos 
  */

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ElegantOTA.h>

const char* ssid = "MaxPV";

IPAddress local_IP ( 192, 168, 4, 1 );
IPAddress gateway ( 192, 168, 4, 1 );
IPAddress subnet ( 255, 255, 255, 0 );

ESP8266WebServer server ( 80 );

void setup ( void ) {
  Serial.begin ( 115200 );

  WiFi.softAPConfig ( local_IP, gateway, subnet );
  WiFi.softAP ( ssid );

  server.on ( "/", HTTP_GET, []( ) {
        server.sendHeader("Location", "/update",true);     
        server.send(302, "text/plane","");
  } );

  server.onNotFound ( []( ) {
        server.sendHeader("Location", "/update",true);     
        server.send(302, "text/plane","");
   } );

  ElegantOTA.begin ( &server ); 

  server.begin ( );
  
  Serial.println ( "HTTP server started, connect to http://192.168.4.1/update" );
}

void loop ( void ) {   
  server.handleClient();
  ElegantOTA.loop();
}
