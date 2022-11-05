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

const char* ssid = "REPLACE_WITH_YOUR_SSID";
const char* password = "REPLACE_WITH_YOUR_PASSWORD";

AsyncWebServer server ( 80 );

void setup ( void ) {
  Serial.begin ( 115200 );

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  server.on ( "/", HTTP_GET, []( AsyncWebServerRequest *request ) {
      request->redirect("/update");
  } );

  server.onNotFound ( []( AsyncWebServerRequest * request ) {
      request->redirect("/update");
  } );

  // Demarrage du serveur AsyncElegantOTA
  AsyncElegantOTA.begin ( &server ); 

  server.begin ( );
  
  Serial.print ( "HTTP server started, connect to http://" );
  Serial.print (WiFi.localIP())
  Serial.println ("/update")
}

void loop ( void ) { }
