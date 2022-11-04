 /*
  Programme d'amorce OAT pour permettre le téléchargement des firmware et filesystem dans le Wemos(ESP8266) et ESP32 
  */


#if defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <ESPAsyncTCP.h>
#elif defined(ESP32)
  #include <WiFi.h>
  #include <AsyncTCP.h>
#endif

#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>

const char* ssid = "MaxPV";

IPAddress local_IP ( 192, 168, 4, 1 );
IPAddress gateway ( 192, 168, 4, 1 );
IPAddress subnet ( 255, 255, 255, 0 );

AsyncWebServer server ( 80 );

void setup ( void ) {
  Serial.begin ( 115200 );

  WiFi.softAPConfig ( local_IP, gateway, subnet );
  WiFi.softAP ( ssid );

  server.on ( "/", HTTP_GET, []( AsyncWebServerRequest *request ) {
      request->redirect("/update");
  } );

  server.onNotFound ( []( AsyncWebServerRequest * request ) {
      request->redirect("/update");
  } );

  // Demarrage du serveur AsyncElegantOTA
  AsyncElegantOTA.begin ( &server ); 

  server.begin ( );
  
  Serial.println ( "HTTP server started, connect to http://192.168.4.1/update" );
}

void loop ( void ) { }
