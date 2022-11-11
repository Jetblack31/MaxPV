/*
  MaxPV_ESP.ino - ESP8266 program that provides a web interface and a API for EcoPV 3+
  Copyright (C) 2022 - Bernard Legrand.

  https://github.com/Jetblack31/

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published
  by the Free Software Foundation, either version 2.1 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

/*************************************************************************************
**                                                                                  **
**        Ce programme fonctionne sur ESP8266 de tye Wemos avec 4 Mo de mémoire     **
**        La compilation s'effectue avec l'IDE Arduino                              **
**        Site Arduino : https://www.arduino.cc                                     **
**                                                                                  **
**************************************************************************************/

// ***********************************************************************************
// ******************            OPTIONS DE COMPILATION                ***************
// ***********************************************************************************

// Pas de debug série pour MQTT
#define _ASYNC_MQTT_LOGLEVEL_               0

// ***********************************************************************************
// ******************        FIN DES OPTIONS DE COMPILATION            ***************
// ***********************************************************************************

// ***********************************************************************************
// ******************                   LIBRAIRIES                     ***************
// ***********************************************************************************

#include <LittleFS.h>
#include <ArduinoJson.h>
#include <TickerScheduler.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h>
#include <AsyncMqtt_Generic.h>
#include <AsyncElegantOTA.h>
#include <DNSServer.h>
#include <NTPClient.h>
#include <SimpleFTPServer.h>

// ***      ATTENTION : NE PAS ACTIVER LE DEBUG SERIAL SUR AUCUNE LIBRAIRIE        ***

// ***********************************************************************************
// ******************               FIN DES LIBRAIRIES                 ***************
// ***********************************************************************************

// ***********************************************************************************
// ************************    DEFINITIONS ET DECLARATIONS     ***********************
// ***********************************************************************************

// ***********************************************************************************
// ****************************   Définitions générales   ****************************
// ***********************************************************************************

#define MAXPV_VERSION "3.32"
#define MAXPV_VERSION_FULL "MaxPV! 3.32"

// Heure solaire
#define GMT_OFFSET 0 

// SSID pour le Config Portal
#define SSID_CP "MaxPV"

// Login et password pour le service FTP
#define LOGIN_FTP "maxpv"
#define PWD_FTP "maxpv"

// Communications
#define TELNET_PORT 23     // Port Telnet

#define SERIAL_BAUD 500000 // Vitesse de la liaison port série pour la connexion avec l'arduino
#define SERIALTIMEOUT 100  // Timeout pour les interrogations sur liaison série en ms
#define SERIAL_BUFFER 256  // Taille du buffer RX pour la connexion avec l'arduino (256 max)

// Historisation des index
#define HISTORY_INTERVAL 30  // Périodicité en minutes de l'enregistrement des index d'énergie pour l'historisation
// Valeurs permises : 1, 2, 3, 4, 5, 6, 10, 12, 15, 20, 30, 60, 120, 180, 240, 480
#define HISTORY_RECORD  193  // Nombre de points dans l'historique
// Attention à la taille totale de l'historique en mémoire
// Et la capacité d'export en CSV

#define OFF 0
#define ON 1
#define STOP 0
#define FORCE 1
#define AUTOM 9

// définition de l'ordre des paramètres de configuration de EcoPV tels que transmis
// et de l'index de rangement dans le tableau de stockage (début à la position 1)
// on utilise la position 0 pour stocker la version
#define NB_PARAM 17 // Nombre de paramètres transmis par EcoPV (17 = 16 + VERSION)
#define ECOPV_VERSION 0
#define V_CALIB 1
#define P_CALIB 2
#define PHASE_CALIB 3
#define P_OFFSET 4
#define P_RESISTANCE 5
#define P_MARGIN 6
#define GAIN_P 7
#define GAIN_I 8
#define E_RESERVE 9
#define P_DIV2_ACTIVE 10
#define P_DIV2_IDLE 11
#define T_DIV2_ON 12
#define T_DIV2_OFF 13
#define T_DIV2_TC 14
#define CNT_CALIB 15
#define P_INSTALLPV 16

// définition de l'ordre des informations statistiques transmises par EcoPV
// et de l'index de rangement dans le tableau de stockage (début à la position 1)
// on utilise la position 0 pour stocker la version
// ATTENTION : dans le reste du programme les 4 index de début de journée sont ajoutés à la suite
// pour les informations disponibles par l'API
// ils doivent toujours être situés en toute fin de tableu
#define NB_STATS 23     // Nombre d'informations statistiques transmis par EcoPV (23 = 22 + VERSION)
#define NB_STATS_SUPP 4 // Nombre d'informations statistiques supplémentaires
//#define ECOPV_VERSION 0
#define V_RMS 1
#define I_RMS 2
#define P_ACT 3
#define P_APP 4
#define P_ROUTED 5
#define P_IMP 6
#define P_EXP 7
#define COS_PHI 8
#define INDEX_ROUTED 9
#define INDEX_IMPORT 10
#define INDEX_EXPORT 11
#define INDEX_IMPULSION 12
#define P_IMPULSION 13
#define TRIAC_MODE 14
#define RELAY_MODE 15
#define DELAY_MIN 16
#define DELAY_AVG 17
#define DELAY_MAX 18
#define BIAS_OFFSET 19
#define STATUS_BYTE 20
#define ONTIME 21
#define SAMPLES 22
#define INDEX_ROUTED_J 23
#define INDEX_IMPORT_J 24
#define INDEX_EXPORT_J 25
#define INDEX_IMPULSION_J 26

// Définition des channels MQTT
#define MQTT_STATE         "maxpv/state"
#define MQTT_V_RMS         "maxpv/vrms"
#define MQTT_I_RMS         "maxpv/irms"
#define MQTT_P_ACT         "maxpv/pact"
#define MQTT_P_APP         "maxpv/papp"
#define MQTT_P_ROUTED      "maxpv/prouted"
#define MQTT_P_IMPULSION   "maxpv/pimpulsion"
#define MQTT_COS_PHI       "maxpv/cosphi"
#define MQTT_INDEX_ROUTED       "maxpv/indexrouted"
#define MQTT_INDEX_IMPORT       "maxpv/indeximport"
#define MQTT_INDEX_EXPORT       "maxpv/indexexport"
#define MQTT_INDEX_IMPULSION    "maxpv/indeximpulsion"
#define MQTT_TRIAC_MODE    "maxpv/triacmode"
#define MQTT_SET_TRIAC_MODE    "maxpv/triacmode/set"
#define MQTT_RELAY_MODE    "maxpv/relaymode"
#define MQTT_SET_RELAY_MODE    "maxpv/relaymode/set"
#define MQTT_BOOST_MODE      "maxpv/boost"
#define MQTT_SET_BOOST_MODE  "maxpv/boost/set"
#define MQTT_STATUS_BYTE   "maxpv/statusbyte"

// ***********************************************************************************
// ******************* Variables globales de configuration MaxPV! ********************
// ***********************************************************************************

// Configuration IP statique mode STA
char static_ip[16] = "192.168.1.250";
char static_gw[16] = "192.168.1.1";
char static_sn[16] = "255.255.255.0";
char static_dns1[16] = "192.168.1.1";
char static_dns2[16] = "8.8.8.8";

// Port HTTP                  
// Attention, le choix du port est inopérant dans cette version
uint16_t httpPort = 80;

// Définition des paramètres du mode BOOST
byte boostRatio = 100;        // En % de la puissance max
int boostDuration = 120;      // En minutes
int boostTimerHour = 4;       // Heure Timer Boost
int boostTimerMinute = 0;     // Minute Timer Boost
int boostTimerActive = OFF;   // BOOST timer actif (=ON) ou non (=OFF)

// Configuration de MQTT
char mqttIP[16] = "192.168.1.100";  // IP du serveur MQTT
uint16_t mqttPort = 1883;           // Port du serveur MQTT
int mqttPeriod = 10;                // Période de transmission en secondes
char mqttUser[40] = "";             // Utilisateur du serveur MQTT
                                    // Optionnel : si vide, pas d'authentification
char mqttPass[40] = "";             // Mot de passe du serveur MQTT
int mqttActive = OFF;               // MQTT actif (= ON) ou non (= OFF)

// ***********************************************************************************
// *************** Fin des variables globales de configuration MaxPV! ****************
// ***********************************************************************************


// ***********************************************************************************
// ************************ Déclaration des variables globales ***********************
// ***********************************************************************************

// Stockage des informations en provenance de EcoPV - Arduino Nano
String ecoPVConfig[NB_PARAM];
String ecoPVStats[NB_STATS + NB_STATS_SUPP];
String ecoPVConfigAll;
String ecoPVStatsAll;

// Définition du nombre de tâches de Ticker
TickerScheduler ts(11);
// Compteur général à la seconde
unsigned long generalCounterSecond = 0;
// Flag indiquant la nécessité de sauvegarder la configuration de MaxPV!
bool shouldSaveConfig = false;
// Flag indiquant la nécessité de lire les paramètres de routage EcoPV
bool shouldReadParams = false;
// Variables pour surveiller que l'on garde le contact avec EcoPV dans l'Arduino Nano
unsigned long refTimeContactEcoPV = millis();
bool contactEcoPV = false;

// Variables pour l'historisation
struct historicData
{ // Structure pour le stockage des données historiques
  unsigned long time; // epoch Time
  float eRouted;      // index de l'énergie routée en kWh stocké en float
  float eImport;      // index de l'énergie importée en kWh stocké en float
  float eExport;      // index de l'énergie exportée en kWh stocké en float
  float eImpulsion;   // index de l'énergie produite (compteur impulsion) en kWh stocké en float
};
historicData energyIndexHistoric[HISTORY_RECORD];
int historyCounter = 0; // position courante dans le tableau de l'historique
                        // = position du plus ancien enregistrement
                        // = position du prochain enregistrement à réaliser

// Variables pour la gestion du mode BOOST
#define BURST_PERIOD 300    // Période des bursts SSR pour le mode BOOST en secondes
int boostTime = -1;         // Temps restant pour le mode BOOST, en secondes (-1 = arrêt)
int burstCnt = 0;           // Compteur de la PWM software pour la gestion du mode BOOST entre 0 et BURST_PERIOD

// buffer pour manipuler le fichier de configuration de MaxPV! (ajuster la taille en fonction des besoins)
StaticJsonDocument<1024> jsonConfig;
// Variables pour la manipulation des adresses IP
IPAddress _ip, _gw, _sn, _dns1, _dns2, _ipmqtt;

// ***********************************************************************************
// ************************ DECLARATION DES SERVEUR ET CLIENTS ***********************
// ***********************************************************************************

AsyncWebServer webServer(80);
DNSServer dnsServer;
WiFiServer telnetServer(TELNET_PORT);
WiFiClient tcpClient;
AsyncMqttClient mqttClient;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 3600 * GMT_OFFSET, 600000);
FtpServer ftpSrv;

// ***********************************************************************************
// **********************  FIN DES DEFINITIONS ET DECLARATIONS  **********************
// ***********************************************************************************



// ***********************************************************************************
// ***************************   FONCTIONS ET PROCEDURES   ***************************
// ***********************************************************************************

/////////////////////////////////////////////////////////
// setup                                               //
// Routine d'initialisation générale                   //
/////////////////////////////////////////////////////////
void setup()
{
  unsigned long refTime = millis();
  boolean APmode = true;

  // Début du debug sur liaison série
  Serial.begin(115200);
  Serial.println(F("\nMaxPV! par Bernard Legrand (2022)."));
  Serial.print(F("Version : "));
  Serial.println(MAXPV_VERSION);
  Serial.println();

  // On teste l'existence du système de fichier
  // et sinon on formatte le système de fichier
  if (!LittleFS.begin())
  {
    Serial.println(F("Système de fichier absent, formatage..."));
    LittleFS.format();
    if (LittleFS.begin())
      Serial.println(F("Système de fichier prêt et monté !"));
    else
    {
      Serial.println(F("Erreur de préparation du système de fichier, redémarrage..."));
      delay(1000);
      ESP.restart();
    }
  }
  else
    Serial.println(F("Système de fichier prêt et monté !"));

  Serial.println();

  // On teste l'existence du fichier de configuration de MaxPV!
  if (LittleFS.exists(F("/config.json")))
  {
    Serial.println(F("Fichier de configuration présent, lecture de la configuration..."));
    if (configRead())
    {
      Serial.println(F("Configuration lue et appliquée !"));
      APmode = false;
    }
    else
    {
      Serial.println(F("Fichier de configuration incorrect, effacement du fichier et redémarrage..."));
      LittleFS.remove(F("/config.json"));
      delay(1000);
      ESP.restart();
    }
  }
  else
    Serial.println(F("Fichier de configuration absent, démarrage en mode point d'accès pour la configuration réseau..."));

  Serial.println();

  _ip.fromString(static_ip);
  _gw.fromString(static_gw);
  _sn.fromString(static_sn);
  _dns1.fromString(static_dns1);
  _dns2.fromString(static_dns2);

  AsyncWiFiManager wifiManager(&webServer, &dnsServer);

  wifiManager.setAPCallback(configModeCallback);
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  wifiManager.setDebugOutput(true);
  wifiManager.setSTAStaticIPConfig(_ip, _gw, _sn, _dns1, _dns2);
  // wifiManager.setAPStaticIPConfig ( IPAddress ( 192, 168, 4, 1 ), IPAddress ( 192, 168, 4, 1 ), IPAddress ( 255, 255, 255, 0 ) );

  if (APmode)
  { // Si on démarre en mode point d'accès / on efface le dernier réseau wifi connu pour forcer le mode AP
    wifiManager.resetSettings();
  }

  else
  { // on devrait se connecter au réseau local avec les paramètres connus
    Serial.print(F("Tentative de connexion au dernier réseau connu..."));
    Serial.println(F("Configuration IP, GW, SN, DNS1, DNS2 :"));
    Serial.println(_ip.toString());
    Serial.println(_gw.toString());
    Serial.println(_sn.toString());
    Serial.println(_dns1.toString());
    Serial.println(_dns2.toString());
  }

  wifiManager.autoConnect(SSID_CP);

  Serial.println();
  Serial.println(F("Connecté au réseau local en utilisant les paramètres IP, GW, SN, DNS1, DNS2 :"));
  Serial.println(WiFi.localIP());
  Serial.println(WiFi.gatewayIP());
  Serial.println(WiFi.subnetMask());
  Serial.println(WiFi.dnsIP(0));
  Serial.println(WiFi.dnsIP(1));

  // Mise à jour des variables globales de configuration IP (systématique même si pas de changement)
  WiFi.localIP().toString().toCharArray(static_ip, 16);
  WiFi.gatewayIP().toString().toCharArray(static_gw, 16);
  WiFi.subnetMask().toString().toCharArray(static_sn, 16);
  WiFi.dnsIP(0).toString().toCharArray(static_dns1, 16);
  WiFi.dnsIP(1).toString().toCharArray(static_dns2, 16);

  // Sauvegarde de la configuration si nécessaire
  if (shouldSaveConfig)
  {
    configWrite();
    Serial.println(F("\nConfiguration sauvegardée !"));
  }

  Serial.println(F("\n\n***** Le debug se poursuit en connexion telnet *****"));
  Serial.print(F("Dans un terminal : nc "));
  Serial.print(static_ip);
  Serial.print(F(" "));
  Serial.println(TELNET_PORT);

  // Démarrage du service TELNET
  telnetServer.begin();
  telnetServer.setNoDelay(true);

  // Attente de 5 secondes pour permettre la connexion TELNET
  refTime = millis();
  while ((!telnetDiscoverClient()) && ((millis() - refTime) < 5000))
  {
    delay(200);
    Serial.print(F("."));
  }

  // Fermeture du debug serial et fermeture de la liaison série
  wifiManager.setDebugOutput(false);
  Serial.println(F("\nFermeture de la connexion série de debug et poursuite du démarrage..."));
  Serial.println(F("Bye bye !\n"));
  delay(100);
  Serial.end();

  tcpClient.println(F("\n***** Reprise de la transmission du debug *****\n"));
  tcpClient.println(F("Connexion au réseau wifi réussie !"));

  // ***********************************************************************
  // ********      DECLARATIONS DES HANDLERS DU SERVEUR WEB         ********
  // ***********************************************************************

  webServer.onNotFound([](AsyncWebServerRequest * request)
  {
    request->redirect("/");
  });

  webServer.on("/", HTTP_ANY, [](AsyncWebServerRequest * request)
  {
    String response = "";
    if ( LittleFS.exists ( F("/main.html") ) ) {
      request->send ( LittleFS, F("/main.html") );
    }
    else {
      response = F("Site Web non trouvé. Filesystem non chargé. Allez à : http://");
      response += WiFi.localIP().toString();
      response += F("/update pour uploader le filesystem.");
      request->send ( 200, "text/plain", response );
    }
  });

  webServer.on("/index.html", HTTP_ANY, [](AsyncWebServerRequest * request)
  {
    String response = "";
    if ( LittleFS.exists ( F("/index.html") ) ) {
      request->send ( LittleFS, F("/index.html") );
    }
    else {
      response = F("Site Web non trouvé. Filesystem non chargé. Allez à : http://");
      response += WiFi.localIP().toString();
      response += F("/update pour uploader le filesystem.");
      request->send ( 200, "text/plain", response );
    }
  });

  webServer.on("/configuration.html", HTTP_ANY, [](AsyncWebServerRequest * request)
  {
    request->send(LittleFS, F("/configuration.html"));
  });

  webServer.on("/main.html", HTTP_ANY, [](AsyncWebServerRequest * request)
  {
    request->send(LittleFS, F("/main.html"));
  });

  webServer.on("/admin.html", HTTP_ANY, [](AsyncWebServerRequest * request)
  {
    request->send(LittleFS, F("/admin.html"));
  });

  webServer.on("/credits.html", HTTP_ANY, [](AsyncWebServerRequest * request)
  {
    request->send(LittleFS, F("/credits.html"));
  });

  webServer.on("/wizard.html", HTTP_ANY, [](AsyncWebServerRequest * request)
  {
    request->send(LittleFS, F("/wizard.html"));
  });

  webServer.on("/maj.html", HTTP_ANY, [](AsyncWebServerRequest * request)
  {
    //request->send ( LittleFS, F("/maj") );
    request->redirect("/update");
  });

  webServer.on("/graph.html", HTTP_ANY, [](AsyncWebServerRequest * request)
  {
    request->send(LittleFS, F("/graph.html"));
  });

  webServer.on("/maxpv.css", HTTP_ANY, [](AsyncWebServerRequest * request)
  {
    request->send(LittleFS, F("/maxpv.css"), "text/css");
  });

  webServer.on("/favicon.ico", HTTP_ANY, [](AsyncWebServerRequest * request)
  {
    request->send(LittleFS, F("/favicon.ico"), "image/png");
  });

  // Download le fichier de configuration de MaxPV!
  webServer.on("/DLconfig", HTTP_ANY, [](AsyncWebServerRequest * request)
  {
    request->send(LittleFS, F("/config.json"), String(), true);
  });

  // ***********************************************************************
  // ********                  HANDLERS DE L'API                    ********
  // ***********************************************************************

  webServer.on("/api/action", HTTP_GET, [](AsyncWebServerRequest * request)
  {
    String response = F("Request successfully processed");
    if ( request->hasParam ( "restart" ) )
      restartEcoPV ( );
    else if ( request->hasParam ( "resetindex" ) )
      resetIndexEcoPV ( );
    else if ( request->hasParam ( "saveindex" ) )
      saveIndexEcoPV ( );
    else if ( request->hasParam ( "saveparam" ) )
      saveConfigEcoPV ( );
    else if ( request->hasParam ( "loadparam" ) )
      loadConfigEcoPV ( );
    else if ( request->hasParam ( "format" ) )
      formatEepromEcoPV ( );
    else if ( request->hasParam ( "eraseconfigesp" ) )
      LittleFS.remove ( "/config.json" );
    else if ( request->hasParam ( "rebootesp" ) )
      rebootESP ( );
    else if ( request->hasParam ( "booston" ) )
      boostON ( );
    else if ( request->hasParam ( "boostoff" ) )
      boostOFF ( );
    else response = F("Unknown request");
    request->send ( 200, "text/plain", response );
  });

  webServer.on("/api/get", HTTP_GET, [](AsyncWebServerRequest * request)
  {
    String response = "";
    if ( request->hasParam ( "configmaxpv" ) )
      request->send ( LittleFS, F("/config.json"), "text/plain" );
    else if ( request->hasParam ( "versionweb" ) )
      request->send ( LittleFS, F("/versionweb.txt"), "text/plain" );
    else {
      if ( ( request->hasParam ( "param" ) ) && ( request->getParam("param")->value().toInt() > 0 ) && ( request->getParam("param")->value().toInt() <= ( NB_PARAM - 1 ) ) )
        response = ecoPVConfig [ request->getParam("param")->value().toInt() ];
      else if ( ( request->hasParam ( "data" ) ) && ( request->getParam("data")->value().toInt() > 0 ) && ( request->getParam("data")->value().toInt() <= ( NB_STATS + NB_STATS_SUPP - 1 ) ) )
        response = ecoPVStats [ request->getParam("data")->value().toInt() ];
      else if ( request->hasParam ( "allparam" ) )
        response = ecoPVConfigAll;
      else if ( request->hasParam ( "alldata" ) )
        response = ecoPVStatsAll;
      else if ( request->hasParam ( "version" ) )
        response = ecoPVConfig [ 0 ];
      else if ( request->hasParam ( "versionmaxpv" ) )
        response = MAXPV_VERSION;
      else if ( request->hasParam ( "relaystate" ) ) {
        if ( ecoPVStats[RELAY_MODE].toInt() == STOP ) response = F("STOP");
        else if ( ecoPVStats[RELAY_MODE].toInt() == FORCE ) response = F("FORCE");
        else if ( ecoPVStats[STATUS_BYTE].toInt() & B00000100 ) response = F("ON");
        else response = F("OFF");
      }
      else if ( request->hasParam ( "ssrstate" ) ) {
        if ( ecoPVStats[TRIAC_MODE].toInt() == STOP ) response = F("STOP");
        else if ( ecoPVStats[TRIAC_MODE].toInt() == FORCE ) response = F("FORCE");
        else if ( ecoPVStats[TRIAC_MODE].toInt() == AUTOM ) {
          if ( ecoPVStats[STATUS_BYTE].toInt() & B00000010 ) response = F("MAX");
          else if ( ecoPVStats[STATUS_BYTE].toInt() & B00000001 ) response = F("ON");
          else response = F("OFF");
        }
      }
      else if ( request->hasParam ( "ping" ) )
        if ( contactEcoPV ) response = F("running");
        else response = F("offline");
      else if ( request->hasParam ( "time" ) )
        response = timeClient.getFormattedTime ( );
      else response = F("Unknown request");
      request->send ( 200, "text/plain", response );
    }
  });

  webServer.on("/api/set", HTTP_GET, [](AsyncWebServerRequest * request)
  {
    String response = F("Request successfully processed");
    String mystring = "";
    if ( ( request->hasParam ( "param" ) ) && ( request->hasParam ( "value" ) )
         && ( request->getParam("param")->value().toInt() > 0 ) && ( request->getParam("param")->value().toInt() <= ( NB_PARAM - 1 ) ) ) {
      mystring = request->getParam("value")->value();
      mystring.replace( ",", "." );
      mystring.trim();
      setParamEcoPV ( request->getParam("param")->value(), mystring );
      // Note : la mise à jour de la base interne de MaxPV se fera de manière asynchrone
      // setParamEcoPV() demande à EcoPV de renvoyer tous les paramètres à MaxPV
    }
    else if ( ( request->hasParam ( "relaymode" ) ) && ( request->hasParam ( "value" ) ) ) {
      mystring = request->getParam("value")->value();
      mystring.trim();
      if ( mystring == F("stop") ) relayModeEcoPV ( STOP );
      else if ( mystring == F("force") ) relayModeEcoPV ( FORCE );
      else if ( mystring == F("auto") ) relayModeEcoPV ( AUTOM );
      else response = F("Bad request");
    }
    else if ( ( request->hasParam ( "ssrmode" ) ) && ( request->hasParam ( "value" ) ) ) {
      mystring = request->getParam("value")->value();
      mystring.trim();
      if ( mystring == F("stop") ) SSRModeEcoPV ( STOP );
      else if ( mystring == F("force") ) SSRModeEcoPV ( FORCE );
      else if ( mystring == F("auto") ) SSRModeEcoPV ( AUTOM );
      else response = F("Bad request");
    }
    else if ( ( request->hasParam ( "configmaxpv" ) ) && ( request->hasParam ( "value" ) ) ) {
      mystring = request->getParam("value")->value();
      deserializeJson ( jsonConfig, mystring );
      strlcpy ( static_ip,
                jsonConfig ["ip"],
                16);
      strlcpy ( static_gw,
                jsonConfig ["gateway"],
                16);
      strlcpy ( static_sn,
                jsonConfig ["subnet"],
                16);
      strlcpy ( static_dns1,
                jsonConfig ["dns1"],
                16);
      strlcpy ( static_dns2,
                jsonConfig ["dns2"],
                16);
      httpPort = jsonConfig["http_port"];
      boostDuration = jsonConfig["boost_duration"];
      boostRatio = jsonConfig["boost_ratio"];
      strlcpy ( mqttIP,
                jsonConfig ["mqtt_ip"],
                16);
      mqttPort = jsonConfig["mqtt_port"];
      mqttPeriod = jsonConfig["mqtt_period"];
      strlcpy ( mqttUser,
                 jsonConfig["mqtt_user"],
                 40);
      strlcpy ( mqttPass,
                 jsonConfig["mqtt_pass"],
                 40);
      mqttActive = jsonConfig["mqtt_active"];
      boostTimerHour = jsonConfig["boost_timer_hour"];
      boostTimerMinute = jsonConfig["boost_timer_minute"];
      boostTimerActive = jsonConfig["boost_timer_active"];
 
      configWrite ( );
    }
    else response = F("Bad request or request unknown");
    request->send ( 200, "text/plain", response );
  });

  // ***********************************************************************
  // ********             HANDLERS DES DATAS HISTORIQUES            ********
  // ***********************************************************************

  webServer.on("/api/history", HTTP_GET, [](AsyncWebServerRequest * request)
  {
    AsyncResponseStream *response = request->beginResponseStream("text/csv");
    String timeStamp;
    float pRouted;
    float pImport;
    float pExport;
    float pImpuls;
    int localCounter;
    int lastLocalCounter;
    unsigned long timePowerFactor = 60000UL / HISTORY_INTERVAL;   // attention : HISTORY_INTERVAL sous-multiple de 60000
    // 60000 = 60 minutes par heure * 1000 Wh par kWh

    if ( request->hasParam ( "power" ) )
    {
      response->print(F("Time,Import réseau,Export réseau,Production PV,Routage ECS\r\n"));
      for (int i = 1; i < HISTORY_RECORD; i++) {
        localCounter = (historyCounter + i) % HISTORY_RECORD;
        lastLocalCounter = ((localCounter + HISTORY_RECORD - 1) % HISTORY_RECORD);
        if ( ( energyIndexHistoric[localCounter].time != 0 ) && ( (energyIndexHistoric[lastLocalCounter].time) != 0 ) ) {
          timeStamp = String (energyIndexHistoric[localCounter].time) + "000";
          pRouted = (energyIndexHistoric[localCounter].eRouted - energyIndexHistoric[lastLocalCounter].eRouted) * timePowerFactor;
          pImport = (energyIndexHistoric[localCounter].eImport - energyIndexHistoric[lastLocalCounter].eImport) * timePowerFactor;
          pExport = -(energyIndexHistoric[localCounter].eExport - energyIndexHistoric[lastLocalCounter].eExport) * timePowerFactor;
          pImpuls = (energyIndexHistoric[localCounter].eImpulsion - energyIndexHistoric[lastLocalCounter].eImpulsion) * timePowerFactor;
          response->printf("%s,%.0f,%.0f,%.0f,%.0f\r\n", timeStamp.c_str(), pImport, pExport, pImpuls, pRouted);
        }
      }
      request->send(response);
    }
    else
    {
      request->send ( 200, "text/plain", F("Unknown request") );
    }
  });

  // ***********************************************************************
  // ********             HANDLERS DES EVENEMENTS MQTT              ********
  // ***********************************************************************
  
  mqttClient.onConnect(onMqttConnect); // Appel de la fonction lors d'une connection MQTT établie
  mqttClient.onMessage(onMqttMessage); // Appel de la fonction lors de la réception d'un message MQTT

  // ***********************************************************************
  // ********                   FIN DES HANDLERS                    ********
  // ***********************************************************************

  // Démarrage des service réseau
  tcpClient.println(F("\nConfiguration des services web..."));
  AsyncElegantOTA.setID(MAXPV_VERSION_FULL);
  AsyncElegantOTA.begin(&webServer);
  webServer.begin();
  timeClient.begin();
  tcpClient.println(F("Services web configurés et démarrés !"));
  tcpClient.print(F("Port web : "));
  tcpClient.println(httpPort);
  if (mqttActive = ON) {
    _ipmqtt.fromString(mqttIP);
    if (strlen(mqttUser) != 0) {
       mqttClient.setCredentials(mqttUser,mqttPass);
    }
    mqttClient.setServer(_ipmqtt, mqttPort);
    mqttClient.setWill(MQTT_STATE, 0, true, "disconnected");
    mqttClient.connect();
    tcpClient.println(F("Services MQTT configuré et démarré !"));
  }

  // Démarrage du service FTP
  ftpSrv.begin(LOGIN_FTP, PWD_FTP);
  tcpClient.println(F("Service FTP configuré et démarré !"));
  tcpClient.println(F("Port FTP : 21"));
  tcpClient.print(F("Login FTP : "));
  tcpClient.print(LOGIN_FTP);
  tcpClient.print(F("  password FTP : "));
  tcpClient.println(PWD_FTP);

  tcpClient.println(F("\nDémarrage de la connexion à l'Arduino..."));
  Serial.setRxBufferSize(SERIAL_BUFFER);
  Serial.begin(SERIAL_BAUD);
  Serial.setTimeout(SERIALTIMEOUT);
  clearSerialInputCache();
  tcpClient.println(F("Liaison série configurée pour la communication avec l'Arduino, en attente d'un contact..."));

  while (!serialProcess()) { tcpClient.print(F(".")); }
  tcpClient.println(F("\nContact établi !\n"));
  contactEcoPV = true;

  // Premier peuplement des informations de configuration de EcoPV
  clearSerialInputCache();
  getAllParamEcoPV();
  delay(100);
  serialProcess();
  clearSerialInputCache();
  getVersionEcoPV();
  delay(100);
  serialProcess();
  // Récupération des informations de fonctionnement
  // En moins d'une seconde EcoPV devrait envoyer les informations
  while (!serialProcess()) { }
  tcpClient.println(F("\nDonnées récupérées de l'Arduino !\n"));

  // Peuplement des références des index journaliers
  setRefIndexJour();
  // Initialisation de l'historique
  initHistoric();
  tcpClient.println(F("Historiques initialisés.\n\n"));

  tcpClient.println(MAXPV_VERSION_FULL);
  tcpClient.print(F("EcoPV version "));
  tcpClient.println(ecoPVConfig[ECOPV_VERSION]);
  if (String(MAXPV_VERSION) != ecoPVConfig[ECOPV_VERSION])
  {
    tcpClient.println(F("\n*****                !! ATTENTION !!                  *****"));
    tcpClient.println(F("\n***** Version ECOPV et version MaxPV! différentes !!! *****"));
  };
  tcpClient.println(F("\n*** Fin du Setup ***\n"));


  // ***********************************************************************
  // ********      DEFINITION DES TACHES RECURRENTES DE TICKER      ********
  // ***********************************************************************

  // Découverte d'une connexion TELNET
  ts.add(
    0, 533, [&](void *)
  {
    telnetDiscoverClient();
  },
  nullptr, true);

  // Lecture et traitement des messages de l'Arduino sur port série
  ts.add(
    1, 70, [&](void *)
  {
    if ( Serial.available ( ) ) serialProcess ( );
  },
  nullptr, true);

  // Forcage de l'envoi des paramètres du routeur EcoPV
  ts.add(
    2, 89234, [&](void *)
  {
    shouldReadParams = true;
  },
  nullptr, true);

  // Mise à jour de l'horloge par NTP
  ts.add(
    3, 20003, [&](void *)
  {
    timeClient.update();
  },
  nullptr, true);

  // Surveillance du fonctionnement du routeur et de l'Arduino
  ts.add(
    4, 617, [&](void *)
  {
    watchDogContactEcoPV();
  },
  nullptr, true);

  // Traitement des tâches FTP et DNS
  ts.add(
    5, 97, [&](void *)
  {
    ftpSrv.handleFTP();
    dnsServer.processNextRequest();
  },
  nullptr, true);

  // Traitement des demandes de lecture des paramètres du routeur EcoPV
  ts.add(
    6, 2679, [&](void *)
  {
    if ( shouldReadParams ) getAllParamEcoPV ( );
  },
  nullptr, true);

  // Affichage périodique d'informations de debug sur TELNET
  ts.add(
    7, 31234, [&](void *)
  {
    tcpClient.print ( F("Heap disponible : ") );
    tcpClient.print ( ESP.getFreeHeap ( ) );
    tcpClient.println ( F(" bytes") );
    tcpClient.print ( F("Heap fragmentation : ") );
    tcpClient.print ( ESP.getHeapFragmentation ( ) );
    tcpClient.println ( F(" %") );
  },
  nullptr, true);

  // Appel chaque minute le scheduler pour les tâches planifiées à la minute près
  // La périodicité est légèrement inférieure à la minute pour être sûr de ne pas rater
  // de minute. C'est à la fonction à gérer les fait qu'il puisse y avoir 2 appels à la même minute.
  ts.add(
    8, 59654, [&](void *)
  {
    timeScheduler();
  },
  nullptr, true);

  // Enregistrement de l'historique
  ts.add(
    9, HISTORY_INTERVAL * 60000UL, [&](void *)
  {
    recordHistoricData();
  },
  nullptr, true);

  // Traitement des tâches à la seconde
  ts.add(
    10, 1000, [&](void *)
  {
    generalCounterSecond++;

    // traitement du mode BOOST
    if (boostTime > 0) {
      if ( burstCnt <= ( ( BURST_PERIOD * int ( boostRatio ) ) / 100 ) ) SSRModeEcoPV(FORCE);
      else SSRModeEcoPV(STOP);
      boostTime--;
      burstCnt++;
      if ( burstCnt >= BURST_PERIOD ) burstCnt = 0;
    }
    else if (boostTime == 0) {
      SSRModeEcoPV(AUTOM);
      boostTime--;
    }
    // fin du traitement du mode BOOST

    // traitement MQTT
    if ((mqttActive == ON) && (generalCounterSecond % mqttPeriod == 0) ){
        mqttTransmit();
    }
    // fin de traitement MQTT

    
  },
  nullptr, true);
  delay(1000);
}



///////////////////////////////////////////////////////////////////
// loop                                                          //
// Loop routine exécutée en boucle                               //
///////////////////////////////////////////////////////////////////
void loop()
{

  // Exécution des tâches récurrentes Ticker
  ts.update();
}



///////////////////////////////////////////////////////////////////
// Fonctions                                                     //
// et Procédures                                                 //
///////////////////////////////////////////////////////////////////
bool configRead(void)
{
  // Note ici on utilise un debug sur liaison série car la fonction n'est appelé qu'au début du SETUP
  Serial.println(F("Lecture du fichier de configuration..."));
  File configFile = LittleFS.open(F("/config.json"), "r");
  if (configFile)
  {
    Serial.println(F("Configuration lue !"));
    Serial.println(F("Analyse..."));
    DeserializationError error = deserializeJson(jsonConfig, configFile);
    if (!error)
    {
      Serial.println(F("\nparsed json:"));
      serializeJsonPretty(jsonConfig, Serial);
      if (jsonConfig["ip"])
      {
        //Serial.println(F("\n\nRestauration de la configuration IP..."));
        strlcpy(static_ip,
                jsonConfig["ip"] | "192.168.1.250",
                16);
        strlcpy(static_gw,
                jsonConfig["gateway"] | "192.168.1.1",
                16);
        strlcpy(static_sn,
                jsonConfig["subnet"] | "255.255.255.0",
                16);
        strlcpy(static_dns1,
                jsonConfig["dns1"] | "192.168.1.1",
                16);
        strlcpy(static_dns2,
                jsonConfig["dns2"] | "8.8.8.8",
                16);
        httpPort = jsonConfig["http_port"] | 80;
        //Serial.println(F("\nRestauration des paramètres du mode boost..."));
        boostDuration = jsonConfig["boost_duration"] | 120;
        boostRatio = jsonConfig["boost_ratio"] | 100;
        //Serial.println(F("\nRestauration des paramètres MQTT..."));
        strlcpy(mqttIP,
                jsonConfig["mqtt_ip"] | "192.168.1.100",
                16);
        mqttPort = jsonConfig["mqtt_port"] | 1883;
        mqttPeriod = jsonConfig["mqtt_period"] | 10;
        strlcpy(mqttUser,
                jsonConfig["mqtt_user"] | "",
                40);
        strlcpy(mqttPass,
                jsonConfig["mqtt_pass"] | "",
                40);
        mqttActive = jsonConfig["mqtt_active"] | OFF;
        boostTimerHour = jsonConfig["boost_timer_hour"] | 4;
        boostTimerMinute = jsonConfig["boost_timer_minute"] | 0;
        boostTimerActive = jsonConfig["boost_timer_active"] | OFF;

     }
      else
      {
        Serial.println(F("\n\nPas d'adresse IP dans le fichier de configuration !"));
        return false;
      }
    }
    else
    {
      Serial.println(F("Erreur durant l'analyse du fichier !"));
      return false;
    }
  }
  else
  {
    Serial.println(F("Erreur de lecture du fichier !"));
    return false;
  }
  return true;
}

void configWrite(void)
{
  jsonConfig["ip"] = static_ip;
  jsonConfig["gateway"] = static_gw;
  jsonConfig["subnet"] = static_sn;
  jsonConfig["dns1"] = static_dns1;
  jsonConfig["dns2"] = static_dns2;
  jsonConfig["http_port"] = httpPort;
  jsonConfig["boost_duration"] = boostDuration;
  jsonConfig["boost_ratio"] = boostRatio;
  jsonConfig["mqtt_ip"] = mqttIP;
  jsonConfig["mqtt_port"] = mqttPort;
  jsonConfig["mqtt_period"] = mqttPeriod;
  jsonConfig["mqtt_user"] = mqttUser;
  jsonConfig["mqtt_pass"] = mqttPass;
  jsonConfig["mqtt_active"] = mqttActive;
  jsonConfig["boost_timer_hour"] = boostTimerHour;
  jsonConfig["boost_timer_minute"] = boostTimerMinute;
  jsonConfig["boost_timer_active"] = boostTimerActive; 

  File configFile = LittleFS.open(F("/config.json"), "w");
  serializeJson(jsonConfig, configFile);
  configFile.close();
}

void rebootESP(void)
{
  delay(100);
  ESP.reset();
  delay(1000);
}

bool telnetDiscoverClient(void)
{
  if (telnetServer.hasClient())
  {
    tcpClient = telnetServer.available();
    clearScreen();
    tcpClient.println(F("\nMaxPV! par Bernard Legrand (2022)."));
    tcpClient.print(F("Version : "));
    tcpClient.println(MAXPV_VERSION);
    tcpClient.println();
    tcpClient.println(F("Configuration IP : "));
    tcpClient.print(F("Adresse IP : "));
    tcpClient.println(WiFi.localIP());
    tcpClient.print(F("Passerelle : "));
    tcpClient.println(WiFi.gatewayIP());
    tcpClient.print(F("Masque SR  : "));
    tcpClient.println(WiFi.subnetMask());
    tcpClient.print(F("IP DNS 1   : "));
    tcpClient.println(WiFi.dnsIP(0));
    tcpClient.print(F("IP DNS 2   : "));
    tcpClient.println(WiFi.dnsIP(1));
    tcpClient.print(F("Port HTTP : "));
    tcpClient.println(httpPort);
    tcpClient.println(F("Port FTP : 21"));
    tcpClient.println();
    tcpClient.println(F("Configuration du mode BOOST : "));
    tcpClient.print(F("Durée du mode BOOST : "));
    tcpClient.print(boostDuration);
    tcpClient.println(F(" minutes"));
    tcpClient.print(F("Puissance pour le mode BOOST : "));
    tcpClient.print(boostRatio);
    tcpClient.println(F(" %"));
    return true;
  }
  return false;
}

void saveConfigCallback()
{
  Serial.println(F("La configuration sera sauvegardée !"));
  shouldSaveConfig = true;
}

void configModeCallback(AsyncWiFiManager *myWiFiManager)
{
  Serial.print(F("Démarrage du mode point d'accès : "));
  Serial.println(myWiFiManager->getConfigPortalSSID());
  Serial.println(F("Adresse du portail : "));
  Serial.println(WiFi.softAPIP());
}

void clearScreen(void)
{
  tcpClient.write(27);       // ESC
  tcpClient.print(F("[2J")); // clear screen
  tcpClient.write(27);       // ESC
  tcpClient.print(F("[H"));  // cursor to home
}

void clearSerialInputCache(void)
{
  while (Serial.available() > 0)
  {
    Serial.read();
  }
}

bool serialProcess(void)
{
#define END_OF_TRANSMIT '#'
  int stringCounter = 0;
  int index = 0;

  // Les chaînes valides envoyées par l'arduino se terminent toujours par #
  String incomingData = Serial.readStringUntil(END_OF_TRANSMIT);

  // On teste la validité de la chaîne qui doit contenir 'END' à la fin
  if (incomingData.endsWith(F("END")))
  {
    tcpClient.print(F("Réception de : "));
    tcpClient.println(incomingData);
    incomingData.replace(F(",END"), "");
    contactEcoPV = true;
    refTimeContactEcoPV = millis();

    if (incomingData.startsWith(F("STATS")))
    {
      incomingData.replace(F("STATS,"), "");
      stringCounter++; // on incrémente pour placer la première valeur à l'index 1
      while ((incomingData.length() > 0) && (stringCounter < NB_STATS))
      {
        index = incomingData.indexOf(',');
        if (index == -1)
        {
          ecoPVStats[stringCounter++] = incomingData;
          break;
        }
        else
        {
          ecoPVStats[stringCounter++] = incomingData.substring(0, index);
          incomingData = incomingData.substring(index + 1);
        }
      }
      // Conversion des index en kWh
      ecoPVStats[INDEX_ROUTED] = String((ecoPVStats[INDEX_ROUTED].toFloat() / 1000.0), 3);
      ecoPVStats[INDEX_IMPORT] = String((ecoPVStats[INDEX_IMPORT].toFloat() / 1000.0), 3);
      ecoPVStats[INDEX_EXPORT] = String((ecoPVStats[INDEX_EXPORT].toFloat() / 1000.0), 3);
      ecoPVStats[INDEX_IMPULSION] = String((ecoPVStats[INDEX_IMPULSION].toFloat() / 1000.0), 3);

      ecoPVStatsAll = "";
      // on génère la chaîne complète des STATS pour l'API en y incluant les index de début de journée
      for (int i = 1; i < (NB_STATS + NB_STATS_SUPP); i++)
      {
        ecoPVStatsAll += ecoPVStats[i];
        if (i < (NB_STATS + NB_STATS_SUPP - 1))
          ecoPVStatsAll += F(",");
      }
    }

    else if (incomingData.startsWith(F("PARAM")))
    {
      incomingData.replace(F("PARAM,"), "");
      ecoPVConfigAll = incomingData;
      stringCounter++; // on incrémente pour placer la première valeur Vrms à l'index 1
      while ((incomingData.length() > 0) && (stringCounter < NB_PARAM))
      {
        index = incomingData.indexOf(',');
        if (index == -1)
        {
          ecoPVConfig[stringCounter++] = incomingData;
          break;
        }
        else
        {
          ecoPVConfig[stringCounter++] = incomingData.substring(0, index);
          incomingData = incomingData.substring(index + 1);
        }
      }
    }

    else if (incomingData.startsWith(F("VERSION")))
    {
      incomingData.replace(F("VERSION,"), "");
      index = incomingData.indexOf(',');
      if (index != -1)
      {
        ecoPVConfig[ECOPV_VERSION] = incomingData.substring(0, index);
        ecoPVStats[ECOPV_VERSION] = ecoPVConfig[ECOPV_VERSION];
      }
    }
    return true;
  }
  else
  {
    return false;
  }
}

void formatEepromEcoPV(void)
{
  Serial.print(F("FORMAT,END#"));
}

void getAllParamEcoPV(void)
{
  Serial.print(F("PARAM,END#"));
  shouldReadParams = false;
}

void setParamEcoPV(String param, String value)
{
  String command = "";
  if ((param.toInt() < 10) && (!param.startsWith("0")))
    param = "0" + param;
  command = F("SETPARAM,") + param + F(",") + value + F(",END#");
  Serial.print(command);
  shouldReadParams = true; // on demande la lecture des paramètres contenus dans EcoPV
  // pour mettre à jour dans MaxPV
  // ce qui permet de vérifier la prise en compte de la commande
  // C'est par ce seul moyen de MaxPV met à jour sa base interne des paramètres
}

void getVersionEcoPV(void)
{
  Serial.print(F("VERSION,END#"));
}

void saveConfigEcoPV(void)
{
  Serial.print(F("SAVECFG,END#"));
}

void loadConfigEcoPV(void)
{
  Serial.print(F("LOADCFG,END#"));
  shouldReadParams = true; // on demande la lecture des paramètres contenus dans EcoPV
  // pour mettre à jour dans MaxPV
  // ce qui permet de vérifier la prise en compte de la commande
  // C'est par ce seul moyen de MaxPV met à jour sa base interne des paramètres
}

void saveIndexEcoPV(void)
{
  Serial.print(F("SAVEINDX,END#"));
}

void resetIndexEcoPV(void)
{
  Serial.print(F("INDX0,END#"));
}

void restartEcoPV(void)
{
  Serial.print(F("RESET,END#"));
}

void relayModeEcoPV(byte opMode)
{
  char buff[2];
  String str;
  String command = F("SETRELAY,");
  if (opMode == STOP)
    command += F("STOP");
  if (opMode == FORCE)
    command += F("FORCE");
  if (opMode == AUTOM)
    command += F("AUTO");
  command += F(",END#");
  Serial.print(command);

  // Envoi du status via MQTT
  str = String(opMode);
  str.toCharArray(buff,2);
  if (mqttClient.connected ()) mqttClient.publish(MQTT_RELAY_MODE, 0, true, buff);

}

void SSRModeEcoPV(byte opMode)
{
  char buff[2];
  String str;
  String command = F("SETSSR,");
  if (opMode == STOP)
    command += F("STOP");
  if (opMode == FORCE)
    command += F("FORCE");
  if (opMode == AUTOM)
    command += F("AUTO");
  command += F(",END#");
  Serial.print(command);
  // Envoi du status via MQTT
  str = String(opMode);
  str.toCharArray(buff,2);
  if (mqttClient.connected ()) mqttClient.publish(MQTT_TRIAC_MODE, 0, true, buff);
}

void watchDogContactEcoPV(void)
{
  if ((millis() - refTimeContactEcoPV) > 1500)
    contactEcoPV = false;
}

void setRefIndexJour(void)
{
  ecoPVStats[INDEX_ROUTED_J] = ecoPVStats[INDEX_ROUTED];
  ecoPVStats[INDEX_IMPORT_J] = ecoPVStats[INDEX_IMPORT];
  ecoPVStats[INDEX_EXPORT_J] = ecoPVStats[INDEX_EXPORT];
  ecoPVStats[INDEX_IMPULSION_J] = ecoPVStats[INDEX_IMPULSION];
}

void initHistoric(void)
{
  for (int i = 0; i < HISTORY_RECORD; i++)
  {
    energyIndexHistoric[i].time = 0;
    energyIndexHistoric[i].eRouted = 0;
    energyIndexHistoric[i].eImport = 0;
    energyIndexHistoric[i].eExport = 0;
    energyIndexHistoric[i].eImpulsion = 0;
  }
  historyCounter = 0;
}

void recordHistoricData(void)
{
  energyIndexHistoric[historyCounter].time = timeClient.getEpochTime();
  energyIndexHistoric[historyCounter].eRouted = ecoPVStats[INDEX_ROUTED].toFloat();
  energyIndexHistoric[historyCounter].eImport = ecoPVStats[INDEX_IMPORT].toFloat();
  energyIndexHistoric[historyCounter].eExport = ecoPVStats[INDEX_EXPORT].toFloat();
  energyIndexHistoric[historyCounter].eImpulsion = ecoPVStats[INDEX_IMPULSION].toFloat();
  historyCounter++;
  historyCounter %= HISTORY_RECORD;   // Création d'un tableau circulaire
}

void boostON(void)
{
  if ( boostRatio != 0 ) {
    boostTime = ( 60 * boostDuration );   // conversion en secondes
    burstCnt = 0;
    if (mqttClient.connected ()) mqttClient.publish(MQTT_BOOST_MODE, 0, true, "on");
  }
}

void boostOFF(void)
{
  boostTime = 0;
  if (mqttClient.connected ()) mqttClient.publish(MQTT_BOOST_MODE, 0, true, "off");
}

void mqttTransmit(void)
{
  char buf[16];
  if (mqttClient.connected ()) {          // On vérifie si on est connecté
    ecoPVStats[V_RMS].toCharArray(buf, 16);
    mqttClient.publish(MQTT_V_RMS, 0, true, buf);
    ecoPVStats[I_RMS].toCharArray(buf, 16);
    mqttClient.publish(MQTT_I_RMS, 0, true, buf);
    ecoPVStats[P_APP].toCharArray(buf, 16);
    mqttClient.publish(MQTT_P_APP, 0, true, buf);
    ecoPVStats[COS_PHI].toCharArray(buf, 16);
    mqttClient.publish(MQTT_COS_PHI, 0, true, buf);
    ecoPVStats[P_ACT].toCharArray(buf, 16);
    mqttClient.publish(MQTT_P_ACT, 0, true, buf);
    ecoPVStats[P_ROUTED].toCharArray(buf, 16);
    mqttClient.publish(MQTT_P_ROUTED, 0, true, buf);
    ecoPVStats[P_IMPULSION].toCharArray(buf, 16);
    mqttClient.publish(MQTT_P_IMPULSION, 0, true, buf);
    ecoPVStats[INDEX_ROUTED].toCharArray(buf, 16);
    mqttClient.publish(MQTT_INDEX_ROUTED, 0, true, buf);
    ecoPVStats[INDEX_IMPORT].toCharArray(buf, 16);
    mqttClient.publish(MQTT_INDEX_IMPORT, 0, true, buf);
    ecoPVStats[INDEX_EXPORT].toCharArray(buf, 16);
    mqttClient.publish(MQTT_INDEX_EXPORT, 0, true, buf);
    ecoPVStats[INDEX_IMPULSION].toCharArray(buf, 16);
    mqttClient.publish(MQTT_INDEX_IMPULSION, 0, true, buf);
    ecoPVStats[TRIAC_MODE].toCharArray(buf, 16);
    mqttClient.publish(MQTT_TRIAC_MODE, 0, true, buf);
    ecoPVStats[RELAY_MODE].toCharArray(buf, 16);
    mqttClient.publish(MQTT_RELAY_MODE, 0, true, buf);
    ecoPVStats[STATUS_BYTE].toCharArray(buf, 16);
    mqttClient.publish(MQTT_STATUS_BYTE, 0, true, buf);
    if (boostTime == -1) mqttClient.publish(MQTT_BOOST_MODE, 0, true, "off");
    else mqttClient.publish(MQTT_BOOST_MODE, 0, true, "on");
  }
  else mqttClient.connect();        // Sinon on ne transmet pas mais on tente la reconnexion
}

void timeScheduler(void)
{
  // Le scheduler est exécuté toutes les minutes
  int day = timeClient.getDay();
  int hour = timeClient.getHours();
  int minute = timeClient.getMinutes();

  // Mise à jour des index de début de journée en début de journée solaire à 00H00
  if ( ( hour == 0 ) && ( minute == 0 ) ) setRefIndexJour ( );

  // Déclenchement du mode BOOST sur Timer
  if ( ( hour == boostTimerHour ) && ( minute == boostTimerMinute ) && ( boostTimerActive == ON ) ) {
    boostON ( );
  };
}

void onMqttConnect(bool sessionPresent) 
{
  // Souscriptions aux topics pour gérer les états relais, SSR et boost
  mqttClient.subscribe(MQTT_SET_RELAY_MODE,0);
  mqttClient.subscribe(MQTT_SET_TRIAC_MODE,0);
  mqttClient.subscribe(MQTT_SET_BOOST_MODE,0);

  // Publication du status
  mqttClient.publish(MQTT_STATE, 0, true, "connected");

  // On crée les informations pour le Discovery HomeAssistant
  // On crée un identifiant unique
  String deviceID = "maxpv";
  deviceID += ESP.getChipId();

  // On récupère l'URL d'accès
  String ip_url = "http://" + WiFi.localIP().toString();

  // On crée les templates du topic et du Payload
  String configTopicTemplate = String(F("homeassistant/#COMPONENT#/#DEVICEID#/#DEVICEID##SENSORID#/config"));
  configTopicTemplate.replace(F("#DEVICEID#"), deviceID);

  // Capteurs
  String configPayloadTemplate = String(F(
    "{"
    "\"dev\":{"
    "\"ids\":\"#DEVICEID#\","
    "\"name\":\"MaxPV\","
    "\"mdl\":\"MaxPV!\","
    "\"mf\":\"JetBlack\","
    "\"sw\":\"#VERSION#\","
    "\"cu\":\"#IP#\""
    "},"
    "\"avty_t\":\"maxpv/state\","
    "\"pl_avail\":\"connected\","
    "\"pl_not_avail\":\"disconnected\","
    "\"uniq_id\":\"#DEVICEID##SENSORID#\","
    "\"dev_cla\":\"#CLASS#\","
    "\"name\":\"#SENSORNAME#\","
    "\"stat_t\":\"#STATETOPIC#\","
    "\"unit_of_meas\":\"#UNIT#\""
    "}"));
  configPayloadTemplate.replace(" ", "");
  configPayloadTemplate.replace(F("#DEVICEID#"), deviceID);
  configPayloadTemplate.replace(F("#VERSION#"), MAXPV_VERSION);
  configPayloadTemplate.replace(F("#IP#"), ip_url);

  String topic;
  String payload;

  // On publie la config pour chaque sensor
  // V_RMS
  topic = configTopicTemplate;
  topic.replace(F("#COMPONENT#"), F("sensor"));
  topic.replace(F("#SENSORID#"), F("Tension"));

  payload = configPayloadTemplate;
  payload.replace(F("#SENSORID#"), F("Tension"));
  payload.replace(F("#SENSORNAME#"), F("Tension"));
  payload.replace(F("#CLASS#"), F("voltage"));
  payload.replace(F("#STATETOPIC#"), F(MQTT_V_RMS));
  payload.replace(F("#UNIT#"), "V");
  mqttClient.publish(topic.c_str(), 0, true, payload.c_str());

  // I_RMS
  topic = configTopicTemplate;
  topic.replace(F("#COMPONENT#"), F("sensor"));
  topic.replace(F("#SENSORID#"), F("Courant"));

  payload = configPayloadTemplate;
  payload.replace(F("#SENSORID#"), F("Courant"));
  payload.replace(F("#SENSORNAME#"), F("Courant"));
  payload.replace(F("#CLASS#"), F("current"));
  payload.replace(F("#STATETOPIC#"), F(MQTT_V_RMS));
  payload.replace(F("#UNIT#"), "A");
  mqttClient.publish(topic.c_str(), 0, true, payload.c_str());

  // P_APP
  topic = configTopicTemplate;
  topic.replace(F("#COMPONENT#"), F("sensor"));
  topic.replace(F("#SENSORID#"), F("PuissanceApparente"));

  payload = configPayloadTemplate;
  payload.replace(F("#SENSORID#"), F("PuissanceApparente"));
  payload.replace(F("#SENSORNAME#"), F("Puissance apparente"));
  payload.replace(F("#CLASS#"), F("apparent_power"));
  payload.replace(F("#STATETOPIC#"), F(MQTT_P_APP));
  payload.replace(F("#UNIT#"), "VA");
  mqttClient.publish(topic.c_str(), 0, true, payload.c_str());

  // MQTT_COS_PHI
  topic = configTopicTemplate;
  topic.replace(F("#COMPONENT#"), F("sensor"));
  topic.replace(F("#SENSORID#"), F("FacteurPuissance"));

  payload = configPayloadTemplate;
  payload.replace(F("#SENSORID#"), F("FacteurPuissance"));
  payload.replace(F("#SENSORNAME#"), F("Facteur de puissance"));
  payload.replace(F("\"dev_cla\":\"#CLASS#\","), F(""));
  payload.replace(F("#STATETOPIC#"), F(MQTT_COS_PHI));
  payload.replace(F("#UNIT#"), "");
  mqttClient.publish(topic.c_str(), 0, true, payload.c_str());

  // MQTT_P_ACT
  topic = configTopicTemplate;
  topic.replace(F("#COMPONENT#"), F("sensor"));
  topic.replace(F("#SENSORID#"), F("PuissanceActive"));

  payload = configPayloadTemplate;
  payload.replace(F("#SENSORID#"), F("PuissanceActive"));
  payload.replace(F("#SENSORNAME#"), F("Puissance active"));
  payload.replace(F("#CLASS#"), F("power"));
  payload.replace(F("#STATETOPIC#"), F(MQTT_P_ACT));
  payload.replace(F("#UNIT#"), "W");
  mqttClient.publish(topic.c_str(), 0, true, payload.c_str());

  // MQTT_P_ROUTED
  topic = configTopicTemplate;
  topic.replace(F("#COMPONENT#"), F("sensor"));
  topic.replace(F("#SENSORID#"), F("PuissanceRoutee"));

  payload = configPayloadTemplate;
  payload.replace(F("#SENSORID#"), F("PuissanceRoutee"));
  payload.replace(F("#SENSORNAME#"), F("Puissance routée"));
  payload.replace(F("#CLASS#"), F("power"));
  payload.replace(F("#STATETOPIC#"), F(MQTT_P_ROUTED));
  payload.replace(F("#UNIT#"), "W");
  mqttClient.publish(topic.c_str(), 0, true, payload.c_str());
  
  // MQTT_P_IMPULSION
  topic = configTopicTemplate;
  topic.replace(F("#COMPONENT#"), F("sensor"));
  topic.replace(F("#SENSORID#"), F("PuissanceProduite"));

  payload = configPayloadTemplate;
  payload.replace(F("#SENSORID#"), F("PuissanceProduite"));
  payload.replace(F("#SENSORNAME#"), F("Puissance produite"));
  payload.replace(F("#CLASS#"), F("power"));
  payload.replace(F("#STATETOPIC#"), F(MQTT_P_IMPULSION));
  payload.replace(F("#UNIT#"), "W");
  mqttClient.publish(topic.c_str(), 0, true, payload.c_str());

  // MQTT_INDEX_ROUTED
  topic = configTopicTemplate;
  topic.replace(F("#COMPONENT#"), F("sensor"));
  topic.replace(F("#SENSORID#"), F("EnergieRoutee"));

  payload = configPayloadTemplate;
  payload.replace(F("#SENSORID#"), F("EnergieRoutee"));
  payload.replace(F("#SENSORNAME#"), F("Energie routée"));
  payload.replace(F("#CLASS#"), F("energy"));
  payload.replace(F("#STATETOPIC#"), F(MQTT_INDEX_ROUTED));
  payload.replace(F("#UNIT#"), "KWh");
  mqttClient.publish(topic.c_str(), 0, true, payload.c_str());

  // MQTT_INDEX_IMPORT
  topic = configTopicTemplate;
  topic.replace(F("#COMPONENT#"), F("sensor"));
  topic.replace(F("#SENSORID#"), F("EnergieImportee"));

  payload = configPayloadTemplate;
  payload.replace(F("#SENSORID#"), F("EnergieImportee"));
  payload.replace(F("#SENSORNAME#"), F("Energie importée"));
  payload.replace(F("#CLASS#"), F("energy"));
  payload.replace(F("#STATETOPIC#"), F(MQTT_INDEX_IMPORT));
  payload.replace(F("#UNIT#"), "KWh");
  mqttClient.publish(topic.c_str(), 0, true, payload.c_str());
  
  // MQTT_INDEX_EXPORT
  topic = configTopicTemplate;
  topic.replace(F("#COMPONENT#"), F("sensor"));
  topic.replace(F("#SENSORID#"), F("EnergieExportee"));

  payload = configPayloadTemplate;
  payload.replace(F("#SENSORID#"), F("EnergieExportee"));
  payload.replace(F("#SENSORNAME#"), F("Energie exportée"));
  payload.replace(F("#CLASS#"), F("energy"));
  payload.replace(F("#STATETOPIC#"), F(MQTT_INDEX_EXPORT));
  payload.replace(F("#UNIT#"), "KWh");
  mqttClient.publish(topic.c_str(), 0, true, payload.c_str());
  
  // MQTT_INDEX_IMPULSION
  topic = configTopicTemplate;
  topic.replace(F("#COMPONENT#"), F("sensor"));
  topic.replace(F("#SENSORID#"), F("EnergieProduite"));

  payload = configPayloadTemplate;
  payload.replace(F("#SENSORID#"), F("EnergieProduite"));
  payload.replace(F("#SENSORNAME#"), F("Energie produite"));
  payload.replace(F("#CLASS#"), F("energy"));
  payload.replace(F("#STATETOPIC#"), F(MQTT_INDEX_IMPULSION));
  payload.replace(F("#UNIT#"), "KWh");
  mqttClient.publish(topic.c_str(), 0, true, payload.c_str());

  // MQTT_TRIAC_MODE
  topic = configTopicTemplate;
  topic.replace(F("#COMPONENT#"), F("select"));
  topic.replace(F("#SENSORID#"), F("SSR"));

  payload = configPayloadTemplate;
  payload.replace(F("#SENSORID#"), F("SSR"));
  payload.replace(F("#SENSORNAME#"), F("SSR"));
  payload.replace(F("\"dev_cla\":\"#CLASS#\","), F(""));
  payload.replace(F("#STATETOPIC#"), F(MQTT_TRIAC_MODE));
  payload.replace(F("\"unit_of_meas\":\"#UNIT#\""),
                  F("\"val_tpl\":\"{% if value == '0' %}stop{% elif value == '1' %}force{% else %}auto{% endif %}\","
                    "\"cmd_t\":\"#CMDTOPIC#\","
                    "\"options\":[\"force\",\"auto\",\"stop\"]"));
  payload.replace(F("#CMDTOPIC#"), F(MQTT_SET_TRIAC_MODE));
  mqttClient.publish(topic.c_str(), 0, true, payload.c_str());

  // MQTT_RELAY_MODE
  topic = configTopicTemplate;
  topic.replace(F("#COMPONENT#"), F("select"));
  topic.replace(F("#SENSORID#"), F("Relais"));

  payload = configPayloadTemplate;
  payload.replace(F("#SENSORID#"), F("Relais"));
  payload.replace(F("#SENSORNAME#"), F("Relais"));
  payload.replace(F("\"dev_cla\":\"#CLASS#\","), F(""));
  payload.replace(F("#STATETOPIC#"), F(MQTT_RELAY_MODE));
  payload.replace(F("\"unit_of_meas\":\"#UNIT#\""),
                  F("\"val_tpl\":\"{% if value == '0' %}stop{% elif value == '1' %}force{% else %}auto{% endif %}\","
                    "\"cmd_t\":\"#CMDTOPIC#\","
                    "\"options\":[\"force\",\"auto\",\"stop\"]"));
  payload.replace(F("#CMDTOPIC#"), F(MQTT_SET_RELAY_MODE));
  mqttClient.publish(topic.c_str(), 0, true, payload.c_str());

  // MQTT_BOOST_MODE
  topic = configTopicTemplate;
  topic.replace(F("#COMPONENT#"), F("switch"));
  topic.replace(F("#SENSORID#"), F("Boost"));

  payload = configPayloadTemplate;
  payload.replace(F("#SENSORID#"), F("Boost"));
  payload.replace(F("#SENSORNAME#"), F("Boost"));
  payload.replace(F("\"dev_cla\":\"#CLASS#\","), F(""));
  payload.replace(F("#STATETOPIC#"), F(MQTT_BOOST_MODE));
  payload.replace(F("\"unit_of_meas\":\"#UNIT#\""),
                  F("\"cmd_t\":\"#CMDTOPIC#\","
                    "\"payload_on\":\"on\","
                    "\"payload_off\":\"off\""));
  payload.replace(F("#CMDTOPIC#"), F(MQTT_SET_BOOST_MODE));
  mqttClient.publish(topic.c_str(), 0, true, payload.c_str());

}

void onMqttMessage(char* topic, char* payload, const AsyncMqttClientMessageProperties& properties, 
                   const size_t& len, const size_t& index, const size_t& total)
{
  // A la réception d'un message sur un des topics en écoute
  // on vérifie le topic concerné et la commande reçue
  if (String(topic).startsWith(F(MQTT_SET_RELAY_MODE))) {
    if ( String(payload).startsWith(F("stop")) ) relayModeEcoPV ( STOP );
    else if ( String(payload).startsWith(F("force")) ) relayModeEcoPV ( FORCE );
    else if ( String(payload).startsWith(F("auto")) ) relayModeEcoPV ( AUTOM );
  }
  else if (String(topic).startsWith(F(MQTT_SET_TRIAC_MODE))) {
    if ( String(payload).startsWith(F("stop")) ) SSRModeEcoPV ( STOP );
    else if ( String(payload).startsWith(F("force")) ) SSRModeEcoPV ( FORCE );
    else if ( String(payload).startsWith(F("auto")) ) SSRModeEcoPV ( AUTOM );
  }
  else if (String(topic).startsWith(F(MQTT_SET_BOOST_MODE))) {
    if ( String(payload).startsWith(F("on")) ) boostON ( );
    else if ( String(payload).startsWith(F("off")) ) boostOFF ( );
  }
}
