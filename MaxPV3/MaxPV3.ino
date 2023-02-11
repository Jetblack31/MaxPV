/*
  MaxPV_ESP.ino - ESP8266 program that provides a web interface and a API for EcoPV 3+
  Copyright (C) 2022 - Bernard Legrand 
  maxpv@bernard-legrand.net
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
**        Ce programme fonctionne sur ESP8266 de type Wemos avec 4 Mo de mémoire    **
**        La compilation s'effectue avec l'IDE Arduino                              **
**        Site Arduino : https://www.arduino.cc                                     **
**                                                                                  **
**************************************************************************************/

// ***********************************************************************************
// ******************             OPTIONS DE COMPILATION               ***************
// ***********************************************************************************

// Importation des options de compilation
#include "maxpv_options.h"

// ***********************************************************************************
// ******************         FIN DES OPTIONS DE COMPILATION           ***************
// ***********************************************************************************


// ***********************************************************************************
// ******************                   LIBRAIRIES                     ***************
// ***********************************************************************************

// ***      ATTENTION : NE PAS ACTIVER LE DEBUG SERIAL SUR AUCUNE LIBRAIRIE        ***

#include <LittleFS.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <DNSServer.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h>
#include <AsyncMqtt_Generic.h>
#include <AsyncHTTPRequest_Generic.h> 
#include <AsyncElegantOTA.h>
#include <Ticker.h>
#include <TickerScheduler.h>
#include <ArduinoJson.h>
#include "AsyncPing.h"

#if defined (MAXPV_FTP_SERVER)
#include <SimpleFTPServer.h>
#endif


// ***      ATTENTION : NE PAS ACTIVER LE DEBUG SERIAL SUR AUCUNE LIBRAIRIE        ***

// ***********************************************************************************
// ******************               FIN DES LIBRAIRIES                 ***************
// ***********************************************************************************

// A FAIRE : CHANGER LE NTP POUR NE PLUS ETRE BLOQUANT

// ***********************************************************************************
// ************************    DEFINITIONS ET DECLARATIONS     ***********************
// ***********************************************************************************

// Importation des définitions de configuration générale de MaxPV!
#include "maxpv_defines.h"

// Définitions utiles pour le programme
#define OFF         0
#define ON          1
#define STOP        0
#define FORCE       1
#define AUTOM       9


// ***********************************************************************************
// ************************         VARIABLES GLOBALES         ***********************
// ***********************************************************************************

// ***********************************************************************************
// ******************* Variables globales de configuration MaxPV! ********************
// ******************* et configuration par défaut                ********************
// ***********************************************************************************

// NOTE : L'INITIALISATION DES STRINGs SE FAIT DANS SETUP()

// Configuration IP statique pour le mode STA.
String static_ip;
String static_gw;
String static_sn;
String static_dns1;
String static_dns2;

// Port HTTP                  
// Attention, le choix du port dans l'interface Web est inopérant dans cette version
uint16_t httpPort    = HTTP_PORT;

// Définition des paramètres du mode BOOST
byte boostRatio      = DEFAULT_BOOST_RATIO;       // En % de la puissance max
int boostDuration    = DEFAULT_BOOST_DURATION;    // En minutes
int boostTimerHour   = DEFAULT_BOOST_HOUR;        // Heure Timer Boost
int boostTimerMinute = DEFAULT_BOOST_MINUTE;      // Minute Timer Boost
int boostTimerActive = OFF;                       // BOOST timer actif (= ON) ou non (= OFF)

// Configuration de MQTT
String mqttIP;                                      // IP du serveur MQTT
String mqttUser;                                    // Utilisateur du serveur MQTT - Optionnel : si vide, pas d'authentification
String mqttPass;                                    // Mot de passe du serveur MQTT
uint16_t mqttPort   = DEFAULT_MQTT_PORT;            // Port du serveur MQTT
int mqttPeriod      = DEFAULT_MQTT_PUBLISH_PERIOD;  // Période de transmission en secondes
int mqttActive      = OFF;                          // MQTT actif (= ON) ou non (= OFF)

// Configuration du relais distant
String relaisDistantIP;                                     // IP du relais distant       
uint16_t relaisDistantPort   = DEFAULT_REMOTE_RELAY_PORT;   // Port du relais distant
String relaisDistantOn;                                     // Chemin pour relais ON
String relaisDistantOff;                                    // Chemin pour relais OFF
int relaisDistantActive      = OFF;                         // Relais distant actif (= ON) ou non (= OFF)



// ***********************************************************************************
// *************** Déclaration des variables globales de fonctionnement **************
// ***********************************************************************************

// Stockage des informations en provenance de EcoPV - Arduino Nano
String ecoPVConfig[NB_PARAM];
String ecoPVStats[NB_STATS + NB_STATS_SUPP];
String ecoPVConfigAll;
String ecoPVStatsAll;

// Définition des tâches de Ticker
TickerScheduler ts(2);
Ticker mqttReconnectTimer;
Ticker safeBoostOffTimer;

// Compteur général à la seconde
unsigned long generalCounterSecond = 0;

// Flag indiquant la nécessité de rebooter MaxPV!
bool shouldReboot = false;
// Flag indiquant la nécessité de sauvegarder la configuration de MaxPV!
bool shouldSaveConfig = false;
// Flag indiquant la nécessité de lire les paramètres de routage EcoPV
bool shouldReadParams = false;
// Flag indiquant la nécessité de vérifier la connexion effective au Wifi
bool shouldCheckWifi = false;
// Flag indiquant la nécessité d'exécuter les traitements à la seconde
bool shouldExecuteEachSecondTasks = false;
// Flag indiquant la nécessité d'exécuter le Time Scheduler
bool shouldExecuteTimeScheduler = false;
// Flag indiquant la nécessité d'updater le timeClient
bool shouldExecuteTimeClientUpdate = false;

unsigned long refTimePingWifi = millis();
unsigned long refTimeContactEcoPV = millis();
bool contactEcoPV = false;

// Variables pour l'historisation
// Structure pour le stockage des données historiques
struct historicData
{ 
  unsigned long time; // epoch Time
  float eRouted;      // index de l'énergie routée en kWh stocké en float
  float eImport;      // index de l'énergie importée en kWh stocké en float
  float eExport;      // index de l'énergie exportée en kWh stocké en float
  float eImpulsion;   // index de l'énergie produite (compteur impulsion) en kWh stocké en float
  long  tRelayOn;     // index du temps de fonctionnement du relais secondaire en minutes stocké en long
};
historicData energyIndexHistoric[HISTORY_RECORD];
int historyCounter = 0; // position courante dans le tableau de l'historique
                        // = position du plus ancien enregistrement
                        // = position du prochain enregistrement à réaliser

// Variables pour la gestion du mode BOOST
long boostTime = -1;         // Temps restant pour le mode BOOST, en secondes (-1 = arrêt)

// Variables pour la gestion du relais distant
int relaisDistantPreviousState = OFF;


// ***********************************************************************************
// ********************** Déclaration des serveurs et des clients ********************
// ***********************************************************************************

DNSServer         dnsServer;
AsyncWebServer    webServer(HTTP_PORT);
AsyncWiFiManager  wifiManager(&webServer, &dnsServer);
AsyncMqttClient   mqttClient;
AsyncHTTPRequest  remoteRelayRequest;
WiFiUDP           ntpUDP;
NTPClient         timeClient(ntpUDP, NTP_SERVER, 3600 * GMT_OFFSET, NTP_UPDATE_INTERVAL);
AsyncPing ping;

#if defined (MAXPV_FTP_SERVER)
FtpServer         ftpSrv;
#endif

// ***********************************************************************************
// ************************     FIN DES VARIABLES GLOBALES     ***********************
// ***********************************************************************************

// ***********************************************************************************
// **********************  FIN DES DEFINITIONS ET DECLARATIONS  **********************
// ***********************************************************************************



// ***********************************************************************************
// ***************************   FONCTIONS ET PROCEDURES   ***************************
// ***********************************************************************************

// ***********************************************************************************
// ***************************            SETUP()          ***************************
// ***************************   INITIALISATION GENERALE   ***************************
// ***********************************************************************************

void setup()
{
// Déclaration des variables locales
  IPAddress         _ip, _gw, _sn, _dns1, _dns2;
  File              configFile;
  boolean           APmode = true;
  String            msg;
  unsigned long     refTime = millis();


  msg.reserve(32);

//Initialisation des variables globales de type String
  static_ip.reserve(15);
  static_gw.reserve(15);
  static_sn.reserve(15);
  static_dns1.reserve(15);
  static_dns2.reserve(15);
  static_ip =    DEFAULT_IP;
  static_gw =    DEFAULT_GW;
  static_sn =    DEFAULT_SN;
  static_dns1 =  DEFAULT_DNS1;
  static_dns2 =  DEFAULT_DNS2;

  mqttIP.reserve(15);
  mqttUser.reserve(DEFAUT_USER_LENGTH);
  mqttPass.reserve(DEFAUT_PWD_LENGTH);
  mqttIP      = DEFAULT_MQTT_SERVER;     
  mqttUser    = DEFAUT_EMPTY_USER;     
  mqttPass    = DEFAUT_EMPTY_PWD;

  relaisDistantIP.reserve(15);
  relaisDistantOn.reserve(DEFAUT_PATH_CMD);
  relaisDistantOff.reserve(DEFAUT_PATH_CMD);
  relaisDistantIP = DEFAULT_REMOTE_RELAY_SERVER;
  relaisDistantOn = DEFAULT_REMOTE_RELAY_CMD_ON;
  relaisDistantOff = DEFAULT_REMOTE_RELAY_CMD_OFF;

  ecoPVConfigAll.reserve(100);
  ecoPVStatsAll.reserve(200);

  // Début du debug sur liaison série
  // Attention : si SERIAL_LOG_ENABLE n'est pas définie à true, rien ne sera loggé sur la liaison série
  Serial.begin(SERIAL_LOG_BAUD);
  delay(50);
  logSerial ( F("[ESP]"), F(WELCOME_MESSAGE) ); 
  msg = F("Version : ");
  msg += MAXPV_VERSION_FULL;
  logSerial ( F("[ESP]"), msg ); 

  delay(5);

  // Montage du système de ficher et chargement de la configuration
  logSerial ( F("[ESP]"), F("Montage du filesystem") ); 
  // On teste l'existence du système de fichier
  // et sinon on formatte le système de fichier
  if (!LittleFS.begin()) {
    logSerial ( F("[ESP]"), F("Filesystem absent, formattage") ); 
    LittleFS.format();
    if (LittleFS.begin())
      logSerial ( F("[ESP]"), F("Filesystem prêt") ); 
    else {
      logSerial ( F("[ESP]"), F("Erreur de filesystem, redémarrage") ); 
      delay(100);
      ESP.restart();
    }
  }
  else logSerial ( F("[ESP]"), F("Filesystem prêt") ); 

  delay(5);

  // On teste l'existence du fichier de configuration de MaxPV!
  if (LittleFS.exists(F("/config.json"))) {
    String msg_tmp;
    msg_tmp.reserve(2048);
    configFile = LittleFS.open(F("/config.json"), "r");
    msg_tmp = configFile.readString();
    configFile.close();
    logSerial ( F("[ESP]"), F("Fichier de configuration présent") ); 
    if (configRead(msg_tmp)) {
      logSerial ( F("[ESP]"), F("Configuration lue") );
      APmode = false;
    }
    else {
      logSerial ( F("[ESP]"), F("Fichier de configuration incorrect, effacement et redémarrage") );
      LittleFS.remove(F("/config.json"));
      delay(50);
      ESP.restart();
    }
  }
  else
    logSerial ( F("[ESP]"), F("Fichier de configuration absent, démarrage en mode AP") );

  delay(5);

  // Connexion au réseau Wifi
  _ip.fromString(static_ip);
  _gw.fromString(static_gw);
  _sn.fromString(static_sn);
  _dns1.fromString(static_dns1);
  _dns2.fromString(static_dns2);

  wifiManager.setAPCallback(configModeCallback);
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  wifiManager.setTryConnectDuringConfigPortal(true);
  wifiManager.setDebugOutput(true);
  wifiManager.setSTAStaticIPConfig(_ip, _gw, _sn, _dns1, _dns2);
  // wifiManager.setAPStaticIPConfig ( IPAddress ( 192, 168, 4, 1 ), IPAddress ( 192, 168, 4, 1 ), IPAddress ( 255, 255, 255, 0 ) );

  // Si on démarre en mode point d'accès / on efface le dernier réseau wifi connu pour forcer le mode AP
  if (APmode) wifiManager.resetSettings();
  else { 
  // on devrait se connecter au réseau local avec les paramètres connus
    logSerial ( F("[ESP]"), F("Tentative de connexion au dernier réseau connu") );    
    msg = F("Adresse IP : ");
    msg += _ip.toString();
    logSerial ( F("[ESP]"), msg );
  }
  delay(5);

  wifiManager.autoConnect(SSID_CP);
  delay(500);

  msg = F("Connecté avec l'IP ");
  msg += _ip.toString();
  logSerial ( F("[ESP]"), msg );

  // Mise à jour des variables globales de configuration IP (systématique même si pas de changement)
  static_ip = WiFi.localIP().toString();
  static_gw = WiFi.gatewayIP().toString();
  static_sn = WiFi.subnetMask().toString();
  static_dns1 = WiFi.dnsIP(0).toString();
  static_dns2 = WiFi.dnsIP(1).toString();
      
  WiFi.setAutoReconnect(true);
  wifiManager.setDebugOutput(false);
  delay(5);

  // Sauvegarde de la configuration MaxPV! si nécessaire
  if (shouldSaveConfig) {
    configWrite();
    logSerial ( F("[ESP]"), F("Configuration sauvegardée") );
    shouldSaveConfig = false;
  }
  delay(50);

  logSerial ( F("[ESP]"), F("Initialisation") );

  // Démarrage du client NTP
  logSerial ( F("[ESP]"), F("Démarrage client NTP") ); 
  timeClient.begin();
  timeClient.update();
  delay(200);

#if defined (MAXPV_FTP_SERVER)
  // Démarrage du service FTP
  logSerial ( F("[ESP]"), F("Démarrage serveur FTP") ); 
  ftpSrv.begin(LOGIN_FTP, PWD_FTP);
  delay(100);
#endif

  // Démarrage du client MQTT si configuré
  if (mqttActive == ON) {
    logSerial ( F("[ESP]"), F("Démarrage client Mqtt") ); 
    startMqtt();
  }

  // Fermeture du debug serial et fermeture de la liaison série
  logSerial ( F("[ESP]"), F("Fermeture de la liaison série") );
  logSerial ( F("[ESP]"), F("Suite des logs sur la connexion MQTT") );
  delay(5);
  Serial.end();
  delay(50);
  
  // Transmission des informations de fonctionnement
  espTransmitInfos (); 
  delay (200);

  // Configuration et connexion à l'Arduino Nano
  Serial.setRxBufferSize(SERIAL_BUFFER);
  Serial.begin(SERIAL_BAUD);
  Serial.setTimeout(SERIALTIMEOUT);
  logMqtt ( F("[328]"), F("Connexion à l'Arduino configurée, en attente d'un contact") ); 
  delay(5); 
  clearSerialInputCache();
  refTime = millis();
  contactEcoPV = false;
  while (( (millis()-refTime) <= 3000 ) && (!contactEcoPV) ) serialProcess();
  delay(5);
  if (contactEcoPV) {
    logMqtt ( F("[328]"), F("Contact établi") ); 
    delay(5);
    // Premier peuplement des informations de configuration de EcoPV
    logMqtt ( F("[328]"), F("Récupération des informations") ); 
    clearSerialInputCache();
    getAllParamEcoPV();
    delay(50);
    while (!serialProcess()) { delay (10); }
    delay(5);
    clearSerialInputCache();
    delay(5);
    getVersionEcoPV();
    delay(50);
    while (!serialProcess()) { delay (10); }
    delay(5);
    clearSerialInputCache();
    // Récupération des informations de fonctionnement
    // En moins d'une seconde EcoPV devrait envoyer les informations
    while (!serialProcess()) { delay (10); }
    delay(5);
    logMqtt ( F("[328]"), F("Informations récupérées") ); 
    msg = F("Version EcoPV : ");
    msg += ecoPVConfig[ECOPV_VERSION];
    logMqtt ( F("[328]"), msg ); 
    delay(5);
  }
  
  else logMqtt ( F("[328]"), F("Pas de contact avec l'Arduino, poursuite du démarrage") ); 


/*
  // Configuration des sorties de l'Arduino Nano en mode AUTOM par défaut
  logMqtt ( F("[328]"), F("Configuration des sorties") ); 
  SSRModeEcoPV ( AUTOM );
  delay (100);
  relayModeEcoPV ( AUTOM );
  delay (100);
*/

  // Initialisation des historiques
  logMqtt ( F("[ESP]"), F("Initialisation des historiques") ); 
  // Peuplement des références des index journaliers
  setRefIndexJour();
  delay(5);
  // Initialisation de l'historique
  initHistoric();
  delay(5);
  recordHistoricData();
  delay(5);

  // Transmission des informations de fonctionnement
  espTransmitInfos (); 
  delay (200);

  // Configuration des tâches planifiées
  logMqtt ( F("[ESP]"), F("Configuration des tâches planifiées") );
  setTsTasks ();
  delay(50);

  // Configuration du CallBack Ping
  setPingCallback ();
  delay(50);

  // Configuration du client tcp
  remoteRelayRequest.setDebug(false);
  delay (50);
  
  // Démarrage du serveur web
  logMqtt ( F("[ESP]"), F("Démarrage serveur Web") );
  startWeb();
  delay(100);

  // Synchronisation du relais distant
  if (relaisDistantActive==ON) {
    relaisDistantPreviousState = (ecoPVStats[STATUS_BYTE].toInt() & B00000100) >> 2;
    remoteRelay (relaisDistantPreviousState);
    delay (200);
  }

  // Démarrage mDNS
  if (MDNS.begin(MAXPV_MDNS)) {
    delay (10);
    MDNS.addService("http", "tcp", HTTP_PORT);
    logMqtt ( F("[ESP]"), F("mDNS démarré") );
    delay (100);
  }

  // Transmission des informations de fonctionnement
  espTransmitInfos (); 
  delay (200);

  logMqtt ( F("[ESP]"), F("Initialisation terminée") );

  clearSerialInputCache();
}

// ***********************************************************************************
// ***************************            LOOP ()          ***************************
// ***********************   BOUCLE INIFINE DE FONCTIONNEMENT   **********************
// ***********************************************************************************

void loop()
{
  static long unsigned refTime;
  static int i = 0;

  refTime = millis ();
 
  // ***   Exécution des tâches récurrentes   ***

  i = 0;
  while ( i < 5 ) {
    if ( Serial.available ( ) ) serialProcess ( );
    yield();
    i++;
  }

  watchDogContactEcoPV();

  yield();

  ts.update();

  yield();

#if defined (MAXPV_FTP_SERVER)
  ftpSrv.handleFTP();
  yield(); 
#endif

  // ***   Actions sur Flag   ***

  if (shouldSaveConfig) {
    configWrite();
    shouldSaveConfig = false;
  }

  yield();

  if ((shouldReboot) && (!shouldSaveConfig)) {
    rebootESP();
    shouldReboot = false;
  }

  yield();
  
  if (shouldCheckWifi) {
    watchDogWifi();
    shouldCheckWifi = false;
  }
  
  yield();

  if (shouldExecuteTimeClientUpdate) {
    timeClient.update();
    shouldExecuteTimeClientUpdate = false;
  }

  yield();

  if (shouldExecuteEachSecondTasks) {
    eachSecondTasks();
    shouldExecuteEachSecondTasks = false;
  }

  yield();

  if (shouldExecuteTimeScheduler) {
    timeScheduler();
    shouldExecuteTimeScheduler = false;
  }

  yield();

  if ( (millis() - refTime) >= 100 ) logMqtt ( F("[WARNING LOOP TIME]"), String (millis() - refTime) );

}




// ***********************************************************************************
// ***********************       FONCTIONS ET PROCEDURES        **********************
// ***********************************************************************************

///////////////////////////////////////////////////////////////////
// Définition des tâches de TickerScheduler                      //
// Le nombre de tâches doit être en correspondance               //
// avec la définition de ts                                      //
///////////////////////////////////////////////////////////////////

void setTsTasks (void) {
  
  // Appel chaque minute le scheduler pour les tâches planifiées à la minute près par rapport au temps NTP
  // La périodicité est légèrement inférieure à la minute pour être sûr de ne pas rater
  // de minute. C'est à la fonction à gérer le fait qu'il puisse y avoir 2 appels à la même minute.
  ts.add(
    0, 59654, [&](void *)
  {
    shouldExecuteTimeScheduler = true;
  },
  nullptr, true);

  // Traitement des tâches à la seconde
  ts.add(
    1, 1000, [&](void *)
  {
    shouldExecuteEachSecondTasks = true;
  },
  nullptr, false);
}


///////////////////////////////////////////////////////////////////
// Gestion de la configuration de MaxPV!                         //
// bool configRead(const String& configString)                   //
// void configWrite(void)                                        //
///////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////
// Lecture de la configuration à partir d'une chaîne             //
// de caractères en JSON                                         //
// Fonction utilisée au démarrage initial et lorsque             //
// la configuration est modifiée via le site Web (cf. Handlers)  //
// Si une clef n'est pas présente dans le fichier,               //
// on garde la valeur présente dans la variable en mémoire       //
///////////////////////////////////////////////////////////////////

bool configRead(const String& configString)
{
  if (configString != "")
  {
    // buffer pour manipuler le fichier de configuration de MaxPV!
    DynamicJsonDocument jsonConfig(JSON_CONFIG_SIZE);
    
    DeserializationError error = deserializeJson(jsonConfig, configString);
    if (!error)
    {
      if (jsonConfig.containsKey("ip"))   // Il faut au moins que l'IP soit présente pour que le fichier soit considéré comme valide
      {
                                                          static_ip = jsonConfig["ip"].as<String>();
        if (jsonConfig.containsKey("gateway"))            static_gw = jsonConfig["gateway"].as<String>(); else shouldSaveConfig = true;
        if (jsonConfig.containsKey("subnet"))             static_sn = jsonConfig["subnet"].as<String>(); else shouldSaveConfig = true;
        if (jsonConfig.containsKey("dns1"))               static_dns1 = jsonConfig["dns1"].as<String>(); else shouldSaveConfig = true;
        if (jsonConfig.containsKey("dns2"))               static_dns2 = jsonConfig["dns2"].as<String>(); else shouldSaveConfig = true;
        if (jsonConfig.containsKey("http_port"))          httpPort = jsonConfig["http_port"]; else shouldSaveConfig = true;
        if (jsonConfig.containsKey("boost_duration"))     boostDuration = jsonConfig["boost_duration"]; else shouldSaveConfig = true;
        if (jsonConfig.containsKey("boost_ratio"))        boostRatio = jsonConfig["boost_ratio"]; else shouldSaveConfig = true;
        if (jsonConfig.containsKey("mqtt_ip"))            mqttIP = jsonConfig["mqtt_ip"].as<String>(); else shouldSaveConfig = true;
        if (jsonConfig.containsKey("mqtt_port"))          mqttPort = jsonConfig["mqtt_port"]; else shouldSaveConfig = true;
        if (jsonConfig.containsKey("mqtt_period"))        mqttPeriod = jsonConfig["mqtt_period"]; else shouldSaveConfig = true;
        if (jsonConfig.containsKey("mqtt_user"))          mqttUser = jsonConfig["mqtt_user"].as<String>(); else shouldSaveConfig = true;
        if (jsonConfig.containsKey("mqtt_user"))          mqttPass = jsonConfig["mqtt_pass"].as<String>(); else shouldSaveConfig = true;
        if (jsonConfig.containsKey("mqtt_active"))        mqttActive = jsonConfig["mqtt_active"]; else shouldSaveConfig = true;
        if (jsonConfig.containsKey("boost_timer_hour"))   boostTimerHour = jsonConfig["boost_timer_hour"]; else shouldSaveConfig = true;
        if (jsonConfig.containsKey("boost_timer_minute")) boostTimerMinute = jsonConfig["boost_timer_minute"]; else shouldSaveConfig = true;
        if (jsonConfig.containsKey("boost_timer_active")) boostTimerActive = jsonConfig["boost_timer_active"]; else shouldSaveConfig = true;
        if (jsonConfig.containsKey("remoterelay_active")) relaisDistantActive = jsonConfig["remoterelay_active"]; else shouldSaveConfig = true;
        if (jsonConfig.containsKey("remoterelay_ip"))     relaisDistantIP = jsonConfig["remoterelay_ip"].as<String>(); else shouldSaveConfig = true;
        if (jsonConfig.containsKey("remoterelay_port"))   relaisDistantPort = jsonConfig["remoterelay_port"]; else shouldSaveConfig = true;
        if (jsonConfig.containsKey("remoterelay_on"))     relaisDistantOn = jsonConfig["remoterelay_on"].as<String>(); else shouldSaveConfig = true;
        if (jsonConfig.containsKey("remoterelay_off"))    relaisDistantOff = jsonConfig["remoterelay_off"].as<String>(); else shouldSaveConfig = true;
      }
      else return false;
    }
    else return false;
  }
  else return false;
  return true;
}

///////////////////////////////////////////////////////////////////
// Ecriture de la configuration courante en JSON                 //
// directement dans le fichier config.json                       //
///////////////////////////////////////////////////////////////////

void configWrite(void)
{
  // buffer pour manipuler le fichier de configuration de MaxPV!
  DynamicJsonDocument jsonConfig(JSON_CONFIG_SIZE);

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
  jsonConfig["remoterelay_active"] = relaisDistantActive;
  jsonConfig["remoterelay_ip"] = relaisDistantIP;
  jsonConfig["remoterelay_port"] = relaisDistantPort;
  jsonConfig["remoterelay_on"] = relaisDistantOn;
  jsonConfig["remoterelay_off"] = relaisDistantOff;

  File configFile = LittleFS.open(F("/config.json"), "w");
  serializeJson(jsonConfig, configFile);
  configFile.close();
}



///////////////////////////////////////////////////////////////////
// Fonctions Callback du portail AP de configuration Wifi        //
// void configModeCallback(AsyncWiFiManager *myWiFiManager)      //
// void saveConfigCallback(void)                                 //
///////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////
// Log des informations à la mise en place du point d'accès      //
// et du portail de configuration                                //
///////////////////////////////////////////////////////////////////

void configModeCallback(AsyncWiFiManager *myWiFiManager)
{
  String msg;
  msg.reserve(128);
  msg = F("Mode AP, SSID : ");
  msg += myWiFiManager->getConfigPortalSSID();
  logSerial ( F("[ESP]"), msg );
  msg = F("IP du portail de configuration : ");
  msg += WiFi.softAPIP().toString();
  logSerial ( F("[ESP]"), msg );
}

///////////////////////////////////////////////////////////////////
// Callback pour la sauvegarde de la configuration               //
// Wifi et réseau                                                //
///////////////////////////////////////////////////////////////////

void saveConfigCallback(void)
{
  shouldSaveConfig = true;
}



///////////////////////////////////////////////////////////////////
// Fonctions utiles de gestion de l'ESP                          //
// void rebootESP(void)                                          //
// espTransmitInfos (void)                                       //
// void logSerial ( const String& logger, const String& message )//
// void logMqtt ( const String& logger, const String& message )  //
///////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////
// Redémarrage software de l'ESP                                 //
// avec arrêt des connexions clients                             //
///////////////////////////////////////////////////////////////////

void rebootESP(void)
{
  logMqtt (F("ESP"), F("!! Redémarrage !!"));
  delay (50);
  if (mqttClient.connected()) mqttClient.disconnect();
  delay(200);
  ESP.restart();
}

///////////////////////////////////////////////////////////////////
// Transmission MQTT des informations de l'ESP                   //
///////////////////////////////////////////////////////////////////

void espTransmitInfos (void) 
{
  // Uniquement appelé en dehors des handles et CB, on peut utiliser yield()
  if (mqttClient.connected ()) {          // On vérifie si on est connecté
    mqttClient.publish(MQTT_SYS_FREE_HEAP,    0, true, (String (ESP.getFreeHeap()).c_str()));
    yield();
    mqttClient.publish(MQTT_SYS_FRAG,         0, true, (String (ESP.getHeapFragmentation()).c_str()));
    yield();
    mqttClient.publish(MQTT_SYS_MAX_FREE,     0, true, (String (ESP.getMaxFreeBlockSize()).c_str()));
    yield();
    mqttClient.publish(MQTT_SYS_CPU_FREQ,     0, true, (String (ESP.getCpuFreqMHz()).c_str()));
    yield();
    mqttClient.publish(MQTT_SYS_RESET_REASON, 0, true, ESP.getResetReason().c_str());
    yield();
  }
}

///////////////////////////////////////////////////////////////////
// log des informations sur la liaison série                     //
// Fonction utilisée lors du boot initial de l'ESP               //
///////////////////////////////////////////////////////////////////

void logSerial ( const String& logger, const String& message )
// !! A N'UTILISER QU'AU DEBUT DU SETUP AVANT LA CONNEXION A L'ARDUINO NANO !!
{
  if ( SERIAL_LOG_ENABLE ) {
    String msg;
    msg.reserve(128);
    msg = timeClient.getFormattedTime ( );
    msg += F("  ");
    msg += logger;
    msg += F("  ");
    msg += message;
    Serial.println(msg);
   }
}

///////////////////////////////////////////////////////////////////
// log des informations sur le broker MQTT                       //
// Nécessite une connexion à un broker MQTT                      //
///////////////////////////////////////////////////////////////////

void logMqtt ( const String& logger, const String& message )
// !! NE FONCTIONNE QUE SI UN SERVEUR MQTT EST CONFIGURE, ACTIVE ET CONNECTE !!
{
  if ( (mqttClient.connected ()) && (MQTT_LOG_ENABLE) ) {          // On vérifie si on est connecté à un serveur mqtt
    String msg;
    msg.reserve(128);
    msg = timeClient.getFormattedTime ( );
    msg += F("  ");
    msg += logger;
    msg += F("  ");
    msg += message;
    mqttClient.publish(MQTT_LOGGER, 0, true, msg.c_str());
  }
}



///////////////////////////////////////////////////////////////////
// Fonctions de communication avec l'Arduino Nano                //
// par liaison série                                             //
// Traitement des données reçues et envoyées                     //
///////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////
// Vidage du buffer RX                                           //
///////////////////////////////////////////////////////////////////

void clearSerialInputCache(void)
{
  while (Serial.available() > 0)    Serial.read();
}

///////////////////////////////////////////////////////////////////
// Traitement des messages en provenance de l'Arduino Nano       //
///////////////////////////////////////////////////////////////////

bool serialProcess(void)
{
  int stringCounter = 0;
  int index = 0;
  String incomingData;

  incomingData.reserve(200);

  // Les chaînes valides envoyées par l'arduino se terminent toujours par END_OF_TRANSMIT = '#'
  incomingData = Serial.readStringUntil(END_OF_TRANSMIT);
  yield();

  if (incomingData.length() == 0) return false;

  // On teste la validité de la chaîne qui doit contenir 'END' à la fin
  else if (incomingData.endsWith(F("END")))
  {
    //logMqtt ( F("[328]"), F("Transmission reçue Ok") ); 
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

    else if (incomingData.startsWith(F("BOOSTTIME")))
      {
      incomingData.replace(F("BOOSTTIME,"), "");
      boostTime = incomingData.toInt();
    }
    return true;
  }
  else
  {
    //logMqtt ( F("[328]"), F("Transmission invalide") ); 
    return false;
  }
}

///////////////////////////////////////////////////////////////////
// Provoque le formattage de l'EEPROM de l'Arduino Nano          //
///////////////////////////////////////////////////////////////////

void formatEepromEcoPV(void)
{
  Serial.print(F("FORMAT,END#"));
}

///////////////////////////////////////////////////////////////////
// Provoque l'envoi des paramètres de configuration du routeur   //
///////////////////////////////////////////////////////////////////

void getAllParamEcoPV(void)
{
  Serial.print(F("PARAM,END#"));
  shouldReadParams = false;
}

///////////////////////////////////////////////////////////////////
// Modifie un paramètre de configuration du routeur              //
///////////////////////////////////////////////////////////////////

void setParamEcoPV(const String& param, const String& value)
{
  String command;
  command.reserve(32);
  command = F("SETPARAM,");
  if ((param.toInt() < 10) && (!param.startsWith("0")))
    command += "0";
  command += param;
  command += F(",");
  command += value;
  command += F(",END#");
  Serial.print(command);
  shouldReadParams = true; // on demande la lecture des paramètres contenus dans EcoPV
  // pour mettre à jour dans MaxPV
  // ce qui permet de vérifier la prise en compte de la commande
  // C'est par ce seul moyen de MaxPV met à jour sa base interne des paramètres
}

///////////////////////////////////////////////////////////////////
// Provoque l'envoi de la version du programme EcoPV             //
///////////////////////////////////////////////////////////////////

void getVersionEcoPV(void)
{
  Serial.print(F("VERSION,END#"));
}

///////////////////////////////////////////////////////////////////
// Provoque l'enregistrement de la configuration en EEPROM       //
///////////////////////////////////////////////////////////////////

void saveConfigEcoPV(void)
{
  Serial.print(F("SAVECFG,END#"));
}

///////////////////////////////////////////////////////////////////
// Provoque le chargement de la configuration depuis l'EEPROM    //
///////////////////////////////////////////////////////////////////

void loadConfigEcoPV(void)
{
  Serial.print(F("LOADCFG,END#"));
  shouldReadParams = true; // on demande la lecture des paramètres contenus dans EcoPV
  // pour mettre à jour dans MaxPV
  // ce qui permet de vérifier la prise en compte de la commande
  // C'est par ce seul moyen de MaxPV met à jour sa base interne des paramètres
}

///////////////////////////////////////////////////////////////////
// Provoque l'enregistrement des index en EEPROM                 //
///////////////////////////////////////////////////////////////////

void saveIndexEcoPV(void)
{
  Serial.print(F("SAVEINDX,END#"));
}

///////////////////////////////////////////////////////////////////
// Provoque la remise à zéro des index                           //
///////////////////////////////////////////////////////////////////

void resetIndexEcoPV(void)
{
  Serial.print(F("INDX0,END#"));
}

///////////////////////////////////////////////////////////////////
// Provoque le redémarrage software du routeur                   //
///////////////////////////////////////////////////////////////////

void restartEcoPV(void)
{
  Serial.print(F("RESET,END#"));
}

///////////////////////////////////////////////////////////////////
// Change le mode de fonctionnement du relais                    //
///////////////////////////////////////////////////////////////////

void relayModeEcoPV(byte opMode)
{
  String str;
  String command;
  str.reserve(16);
  command.reserve(32);
  command = F("SETRELAY,");
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
  if (mqttClient.connected ()) mqttClient.publish(MQTT_RELAY_MODE, 0, true, str.c_str());
}

///////////////////////////////////////////////////////////////////
// Change le mode de fonctionnement du SSR                       //
///////////////////////////////////////////////////////////////////

void SSRModeEcoPV(byte opMode)
{
  String str;
  String command;
  str.reserve(16);
  command.reserve(32);
  command = F("SETSSR,");
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
  if (mqttClient.connected ()) mqttClient.publish(MQTT_TRIAC_MODE, 0, true, str.c_str());
}

///////////////////////////////////////////////////////////////////
// Active ou arrête le mode BOOST                                //
// Doit être appelé par boostON() ou boostOFF()                  //
///////////////////////////////////////////////////////////////////

void boostModeEcoPV(long bT, int bR)
{
  String command;
  command.reserve(32);
  command = F("SETBOOST,");
  command += bT;
  command += F(",");
  command += bR;
  command += F(",END#");
  Serial.print(command);
}


///////////////////////////////////////////////////////////////////
// WatchDog de surveillance de fonctionnement du routeur         //
///////////////////////////////////////////////////////////////////

void watchDogContactEcoPV(void)
{
  if ((millis() - refTimeContactEcoPV) > 1500)
    contactEcoPV = false;
}


///////////////////////////////////////////////////////////////////
// Fonctions de gestion des tâches de haut niveau                //
// liées au routage et effectuées par l'ESP :                    //
// Mode BOOST, index journaliers, historique                     //
///////////////////////////////////////////////////////////////////

void boostON(void)
{
  if ( boostRatio != 0 ) {
    long bT = ( 60 * boostDuration );   // conversion en secondes
    //burstCnt = 0;
    boostModeEcoPV(bT,boostRatio);
    boostTime = bT;
    logMqtt ( F("[ESP]"), F("Mode Boost --> ON") ); 
    if (mqttClient.connected ()) mqttClient.publish(MQTT_BOOST_MODE, 0, true, "on");
  }
}


void boostOFF(void)
{
  boostModeEcoPV(0,boostRatio);
  boostTime = -1;
  logMqtt ( F("[ESP]"), F("Mode Boost --> OFF") ); 
  if (mqttClient.connected ()) mqttClient.publish(MQTT_BOOST_MODE, 0, true, "off");
}


void setRefIndexJour(void)
{
  ecoPVStats[INDEX_ROUTED_J] = ecoPVStats[INDEX_ROUTED];
  ecoPVStats[INDEX_IMPORT_J] = ecoPVStats[INDEX_IMPORT];
  ecoPVStats[INDEX_EXPORT_J] = ecoPVStats[INDEX_EXPORT];
  ecoPVStats[INDEX_IMPULSION_J] = ecoPVStats[INDEX_IMPULSION];
  ecoPVStats[INDEX_RELAY_J]     = ecoPVStats[INDEX_RELAY];
}


void initHistoric(void)
{
  for (int i = 0; i < HISTORY_RECORD; i++)
  {
    energyIndexHistoric[i].time     = 0;
    energyIndexHistoric[i].eRouted  = 0;
    energyIndexHistoric[i].eImport  = 0;
    energyIndexHistoric[i].eExport  = 0;
    energyIndexHistoric[i].eImpulsion = 0;
    energyIndexHistoric[i].tRelayOn = 0;
  }
  historyCounter = 0;
}


void recordHistoricData(void)
{
  energyIndexHistoric[historyCounter].time      = timeClient.getEpochTime();
  energyIndexHistoric[historyCounter].eRouted   = ecoPVStats[INDEX_ROUTED].toFloat();
  energyIndexHistoric[historyCounter].eImport   = ecoPVStats[INDEX_IMPORT].toFloat();
  energyIndexHistoric[historyCounter].eExport   = ecoPVStats[INDEX_EXPORT].toFloat();
  energyIndexHistoric[historyCounter].eImpulsion = ecoPVStats[INDEX_IMPULSION].toFloat();
  energyIndexHistoric[historyCounter].tRelayOn  = ecoPVStats[INDEX_RELAY].toInt();
  historyCounter++;
  historyCounter %= HISTORY_RECORD;   // Création d'un tableau circulaire
  logMqtt ( F("[ESP]"), F("Relève des index effectuée") ); 
}



///////////////////////////////////////////////////////////////////
// Fonctions de gestion des tâches récurrentes                   //
// à la seconde, et les tâches horaires                          //
///////////////////////////////////////////////////////////////////

void eachSecondTasks(void)
  // Fonction appelée uniquement par la loop. On peut utiliser yield()
{  
  generalCounterSecond++;

  // Update de mDNS 
  MDNS.update();

  yield();

  // Toutes les REMOTE_RELAY_MIRRORING_PERIOD secondes, si activé, on recopie l'état du relais physique MaxPV vers le relais miroir
  if ( ( generalCounterSecond % REMOTE_RELAY_MIRRORING_PERIOD == 13 ) && (relaisDistantActive==ON) ) {
    int state_tmp = (ecoPVStats[STATUS_BYTE].toInt() & B00000100) >> 2;
    if ( relaisDistantPreviousState != state_tmp) {
          remoteRelay (state_tmp);
          relaisDistantPreviousState = state_tmp;
    }
  }

  yield();

  // Transmission des infos ESP toutes les 2 secondes
  if (generalCounterSecond % 2 == 1) espTransmitInfos();

  yield();

  // Toutes les 2 secondes on vérifie s'il faut aller récupérer les infos de config ecoPV
  if ( ( generalCounterSecond % 2 == 0 ) && ( shouldReadParams ) )  getAllParamEcoPV ( );
  // Le flag est remis à false dans la procédure getAllParamEcoPV ()
  
  yield();

  // Toutes les 119 secondes on demande les infos de config ecoPV
  if ( generalCounterSecond % 119 == 47 )  shouldReadParams = true;
  
  yield();

  // Enregistrement des historiques
  if (generalCounterSecond % (HISTORY_INTERVAL * 60) == 0) recordHistoricData();;

  yield();

  // traitement MQTT
  if ((mqttActive == ON) && (generalCounterSecond % mqttPeriod == 0) ) mqttTransmit();

  yield();

  // On ping la Gateway périodiquement, ici tous les 30 secondes, 2 fois maxi, timeout 1000 ms
  if (generalCounterSecond % 30 == 7) {
    IPAddress _gw;
    _gw.fromString(static_gw);
    ping.begin (_gw, 2, 1000);
    // Si le Wifi est déconnecté, on lance également la procédure de vérification
    if (!WiFi.isConnected()) shouldCheckWifi = true;  
  }

  yield();
  
}


void timeScheduler(void)
{
  // Le scheduler est exécuté toutes les minutes
  int day     = timeClient.getDay();
  int hour    = timeClient.getHours();
  int minute  = timeClient.getMinutes();

  // Mise à jour des index de début de journée en début de journée solaire à 00H00
  if ( ( hour == 0 ) && ( minute == 0 ) ) {
    setRefIndexJour ( );
    logMqtt ( F("[ESP]"), F("Initialisation des index de début de journée") ); 
  };

  // Déclenchement du mode BOOST sur Timer
  if ( ( hour == boostTimerHour ) && ( minute == boostTimerMinute ) && ( boostTimerActive == ON ) )  boostON ( );

  // Mise à jour NTP toutes les heures
  if ( minute == 47 ) shouldExecuteTimeClientUpdate = true;
}



///////////////////////////////////////////////////////////////////
// Fonctions de gestion de la connexion Mqtt                     //
// et de l'autodiscovery Home Assisant                           //
///////////////////////////////////////////////////////////////////

void startMqtt (void) {
  IPAddress _ipmqtt;
  mqttClient.onConnect(onMqttConnect);        // Appel de la fonction lors d'une connexion MQTT établie
  mqttClient.onDisconnect(onMqttDisconnect);  // Appel de la fonction lors d'une déconnexion MQTT
  mqttClient.onMessage(onMqttMessage);        // Appel de la fonction lors de la réception d'un message MQTT  
  _ipmqtt.fromString(mqttIP);
  if ( mqttUser.length() != 0 ) {
     mqttClient.setCredentials(mqttUser.c_str(), mqttPass.c_str());
  }
  mqttClient.setServer(_ipmqtt, mqttPort);
  mqttClient.setWill(MQTT_STATE, 0, true, "disconnected");
  mqttConnect();
  delay(3000);  
}


void mqttConnect (void)
{ 
  mqttClient.connect();  
};


void onMqttConnect(bool sessionPresent) 
{
  logMqtt ( F("[ESP]"), F("Service MQTT connecté") ); 
  // Souscriptions aux topics pour gérer les états relais, SSR et boost
  mqttClient.subscribe(MQTT_SET_RELAY_MODE,0);
  mqttClient.subscribe(MQTT_SET_TRIAC_MODE,0);
  mqttClient.subscribe(MQTT_SET_BOOST_MODE,0);

  // Publication du status
  mqttClient.publish(MQTT_STATE, 0, true, "connected");

  // On crée les informations pour le Discovery HomeAssistant
  // On crée un identifiant unique
  String deviceID = F("maxpv");
  deviceID += ESP.getChipId();

  // On récupère l'URL d'accès
  String ip_url = F("http://");
  ip_url += WiFi.localIP().toString();

  // On crée les templates du topic et du Payload
  String configTopicTemplate = F("homeassistant/#COMPONENT#/#DEVICEID#/#DEVICEID##SENSORID#/config");
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
    "\"dev_cla\":\"#DEVICECLASS#\","
    "\"stat_cla\":\"#STATECLASS#\","
    "\"name\":\"#SENSORNAME#\","
    "\"stat_t\":\"#STATETOPIC#\","
    "\"unit_of_meas\":\"#UNIT#\""
    "}"));
  configPayloadTemplate.replace(" ", "");
  configPayloadTemplate.replace(F("#DEVICEID#"), deviceID);
  configPayloadTemplate.replace(F("#VERSION#"), F(MAXPV_VERSION));
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
  payload.replace(F("#DEVICECLASS#"), F("voltage"));
  payload.replace(F("#STATECLASS#"), F("measurement"));
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
  payload.replace(F("#DEVICECLASS#"), F("current"));
  payload.replace(F("#STATECLASS#"), F("measurement"));
  payload.replace(F("#STATETOPIC#"), F(MQTT_I_RMS));
  payload.replace(F("#UNIT#"), "A");
  mqttClient.publish(topic.c_str(), 0, true, payload.c_str());

  // P_APP
  topic = configTopicTemplate;
  topic.replace(F("#COMPONENT#"), F("sensor"));
  topic.replace(F("#SENSORID#"), F("PuissanceApparente"));

  payload = configPayloadTemplate;
  payload.replace(F("#SENSORID#"), F("PuissanceApparente"));
  payload.replace(F("#SENSORNAME#"), F("Puissance apparente"));
  payload.replace(F("#DEVICECLASS#"), F("apparent_power"));
  payload.replace(F("#STATECLASS#"), F("measurement"));
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
  payload.replace(F("\"dev_cla\":\"#DEVICECLASS#\","), F(""));
  payload.replace(F("#STATECLASS#"), F("measurement"));
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
  payload.replace(F("#DEVICECLASS#"), F("power"));
  payload.replace(F("#STATECLASS#"), F("measurement"));
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
  payload.replace(F("#DEVICECLASS#"), F("power"));
  payload.replace(F("#STATECLASS#"), F("measurement"));
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
  payload.replace(F("#DEVICECLASS#"), F("power"));
  payload.replace(F("#STATECLASS#"), F("measurement"));
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
  payload.replace(F("#DEVICECLASS#"), F("energy"));
  payload.replace(F("#STATECLASS#"), F("total_increasing"));
  payload.replace(F("#STATETOPIC#"), F(MQTT_INDEX_ROUTED));
  payload.replace(F("#UNIT#"), "kWh");
  mqttClient.publish(topic.c_str(), 0, true, payload.c_str());

  // MQTT_INDEX_IMPORT
  topic = configTopicTemplate;
  topic.replace(F("#COMPONENT#"), F("sensor"));
  topic.replace(F("#SENSORID#"), F("EnergieImportee"));

  payload = configPayloadTemplate;
  payload.replace(F("#SENSORID#"), F("EnergieImportee"));
  payload.replace(F("#SENSORNAME#"), F("Energie importée"));
  payload.replace(F("#DEVICECLASS#"), F("energy"));
  payload.replace(F("#STATECLASS#"), F("total_increasing"));
  payload.replace(F("#STATETOPIC#"), F(MQTT_INDEX_IMPORT));
  payload.replace(F("#UNIT#"), "kWh");
  mqttClient.publish(topic.c_str(), 0, true, payload.c_str());
  
  // MQTT_INDEX_EXPORT
  topic = configTopicTemplate;
  topic.replace(F("#COMPONENT#"), F("sensor"));
  topic.replace(F("#SENSORID#"), F("EnergieExportee"));

  payload = configPayloadTemplate;
  payload.replace(F("#SENSORID#"), F("EnergieExportee"));
  payload.replace(F("#SENSORNAME#"), F("Energie exportée"));
  payload.replace(F("#DEVICECLASS#"), F("energy"));
  payload.replace(F("#STATECLASS#"), F("total_increasing"));
  payload.replace(F("#STATETOPIC#"), F(MQTT_INDEX_EXPORT));
  payload.replace(F("#UNIT#"), "kWh");
  mqttClient.publish(topic.c_str(), 0, true, payload.c_str());
  
  // MQTT_INDEX_IMPULSION
  topic = configTopicTemplate;
  topic.replace(F("#COMPONENT#"), F("sensor"));
  topic.replace(F("#SENSORID#"), F("EnergieProduite"));

  payload = configPayloadTemplate;
  payload.replace(F("#SENSORID#"), F("EnergieProduite"));
  payload.replace(F("#SENSORNAME#"), F("Energie produite"));
  payload.replace(F("#DEVICECLASS#"), F("energy"));
  payload.replace(F("#STATECLASS#"), F("total_increasing"));
  payload.replace(F("#STATETOPIC#"), F(MQTT_INDEX_IMPULSION));
  payload.replace(F("#UNIT#"), "kWh");
  mqttClient.publish(topic.c_str(), 0, true, payload.c_str());

  // MQTT_INDEX_RELAY
  topic = configTopicTemplate;
  topic.replace(F("#COMPONENT#"), F("sensor"));
  topic.replace(F("#SENSORID#"), F("TempsFonctionnementRelais"));

  payload = configPayloadTemplate;
  payload.replace(F("#SENSORID#"), F("TempsFonctionnementRelais"));
  payload.replace(F("#SENSORNAME#"), F("Temps de fonctionnement relais"));
  payload.replace(F("#DEVICECLASS#"), F("duration"));
  payload.replace(F("#STATECLASS#"), F("total_increasing"));
  payload.replace(F("#STATETOPIC#"), F(MQTT_INDEX_RELAY));
  payload.replace(F("#UNIT#"), "min");
  mqttClient.publish(topic.c_str(), 0, true, payload.c_str());

  // MQTT_TRIAC_MODE
  topic = configTopicTemplate;
  topic.replace(F("#COMPONENT#"), F("select"));
  topic.replace(F("#SENSORID#"), F("SSR"));

  payload = configPayloadTemplate;
  payload.replace(F("#SENSORID#"), F("SSR"));
  payload.replace(F("#SENSORNAME#"), F("SSR"));
  payload.replace(F("\"dev_cla\":\"#DEVICECLASS#\","), F(""));
  payload.replace(F("#STATECLASS#"), F(""));
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
  payload.replace(F("\"dev_cla\":\"#DEVICECLASS#\","), F(""));
  payload.replace(F("#STATECLASS#"), F(""));
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
  payload.replace(F("\"dev_cla\":\"#DEVICECLASS#\","), F(""));
  payload.replace(F("#STATECLASS#"), F(""));
  payload.replace(F("#STATETOPIC#"), F(MQTT_BOOST_MODE));
  payload.replace(F("\"unit_of_meas\":\"#UNIT#\""),
                  F("\"cmd_t\":\"#CMDTOPIC#\","
                    "\"payload_on\":\"on\","
                    "\"payload_off\":\"off\""));
  payload.replace(F("#CMDTOPIC#"), F(MQTT_SET_BOOST_MODE));
  mqttClient.publish(topic.c_str(), 0, true, payload.c_str());
}


void onMqttDisconnect(AsyncMqttClientDisconnectReason reason)
{
  mqttClient.clearQueue();
  mqttReconnectTimer.once(RECONNECT_TIME, mqttConnect);
}


void onMqttMessage(char* topic, char* payload, const AsyncMqttClientMessageProperties& properties, 
                   const size_t& len, const size_t& index, const size_t& total)
{
  // A la réception d'un message sur un des topics en écoute
  // on vérifie le topic concerné et la commande reçue
  if (String(topic).startsWith(F(MQTT_SET_RELAY_MODE))) {
    if ( String(payload).startsWith(F("stop")) )        relayModeEcoPV ( STOP );
    else if ( String(payload).startsWith(F("force")) )  relayModeEcoPV ( FORCE );
    else if ( String(payload).startsWith(F("auto")) )   relayModeEcoPV ( AUTOM );
  }
  else if (String(topic).startsWith(F(MQTT_SET_TRIAC_MODE))) {
    if ( String(payload).startsWith(F("stop")) )        SSRModeEcoPV ( STOP );
    else if ( String(payload).startsWith(F("force")) )  SSRModeEcoPV ( FORCE );
    else if ( String(payload).startsWith(F("auto")) )   SSRModeEcoPV ( AUTOM );
  }
  else if (String(topic).startsWith(F(MQTT_SET_BOOST_MODE))) {
    if ( String(payload).startsWith(F("on")) )          boostON ( );
    else if ( String(payload).startsWith(F("off")) )    boostOFF ( );
  }
}


void mqttTransmit(void)
{
  if (mqttClient.connected ()) {          // On vérifie si on est connecté

    mqttClient.publish(MQTT_V_RMS, 0, true, ecoPVStats[V_RMS].c_str());
    yield();
    mqttClient.publish(MQTT_I_RMS, 0, true, ecoPVStats[I_RMS].c_str());
    yield();
    mqttClient.publish(MQTT_P_APP, 0, true, ecoPVStats[P_APP].c_str());
    yield();
    mqttClient.publish(MQTT_COS_PHI, 0, true, ecoPVStats[COS_PHI].c_str());
    yield();
    mqttClient.publish(MQTT_P_ACT, 0, true, ecoPVStats[P_ACT].c_str());
    yield();
    mqttClient.publish(MQTT_P_ROUTED, 0, true, ecoPVStats[P_ROUTED].c_str());
    yield();
    mqttClient.publish(MQTT_P_IMPULSION, 0, true, ecoPVStats[P_IMPULSION].c_str());
    yield();
    mqttClient.publish(MQTT_INDEX_ROUTED, 0, true, ecoPVStats[INDEX_ROUTED].c_str());
    yield();
    mqttClient.publish(MQTT_INDEX_IMPORT, 0, true, ecoPVStats[INDEX_IMPORT].c_str());
    yield();
    mqttClient.publish(MQTT_INDEX_EXPORT, 0, true, ecoPVStats[INDEX_EXPORT].c_str());
    yield();
    mqttClient.publish(MQTT_INDEX_IMPULSION, 0, true, ecoPVStats[INDEX_IMPULSION].c_str());
    yield();
    mqttClient.publish(MQTT_INDEX_RELAY, 0, true, ecoPVStats[INDEX_RELAY].c_str());
    yield();
    mqttClient.publish(MQTT_TRIAC_MODE, 0, true, ecoPVStats[TRIAC_MODE].c_str());
    yield();
    mqttClient.publish(MQTT_RELAY_MODE, 0, true, ecoPVStats[RELAY_MODE].c_str());
    yield();
    mqttClient.publish(MQTT_STATUS_BYTE, 0, true, ecoPVStats[STATUS_BYTE].c_str());
    yield();
    if (boostTime == -1) mqttClient.publish(MQTT_BOOST_MODE, 0, true, "off");
    else mqttClient.publish(MQTT_BOOST_MODE, 0, true, "on");
    yield();
  }
}

///////////////////////////////////////////////////////////////////
// Fonctions Callback Ping                                       //
///////////////////////////////////////////////////////////////////

void setPingCallback ( void )
{
  // callback for each answer/timeout of ping
  ping.on(true,[](const AsyncPingResponse& response){
    if (response.answer) {
      // le ping est revenu valide, on ré-initialise le WD
      refTimePingWifi = millis();
      return true; // on s'arrête
    }
    else {
      // le ping n'est pas revenu
      logMqtt ( F("[ESP]"), F("Pas de réponse au ping") ); 
      shouldCheckWifi = true;
      return false; 
    }
  });
}


///////////////////////////////////////////////////////////////////
// Fonctions du WatchDog Wifi                                    //
///////////////////////////////////////////////////////////////////

void watchDogWifi ( void )
{
  if ( ( millis() - refTimePingWifi ) > PING_WIFI_TIMEOUT ) {    
    // On force la déconnexion du service MQTT
    // Si les services sont déjà déconnectés, soit ils n'ont pas été configurés
    // soit les tentatives de reconnexion sont déjà en cours

    if (mqttClient.connected ()) mqttClient.disconnect(true);
    delay (50);

    wifiManager.autoConnect(SSID_CP);
    delay (50);

    if (mqttActive == ON) startMqtt();

    yield ( );

    refTimePingWifi = millis();
  }   
}



///////////////////////////////////////////////////////////////////
// Fonction de pilotage du relais distant                        //
///////////////////////////////////////////////////////////////////

void remoteRelay ( int state )
{
  bool requestOpenResult;
  String urlCommand;
  urlCommand.reserve(128);

  urlCommand = F("http://");
  urlCommand += relaisDistantIP;
  urlCommand += F(":");
  urlCommand += relaisDistantPort;
  if (state==1)   urlCommand += relaisDistantOn;
  else urlCommand += relaisDistantOff;
  
  logMqtt ( F("[ESP]"), F("Synchronisation du relais distant") );
  
  if (remoteRelayRequest.readyState() == readyStateUnsent || remoteRelayRequest.readyState() == readyStateDone)
  {
    requestOpenResult = remoteRelayRequest.open("GET", urlCommand.c_str());
    if (requestOpenResult)  {
      remoteRelayRequest.send();
    }
    else  logMqtt ( F("[ESP]"), F("Erreur envoi requête Remote Relay") ); 
  }
  else  logMqtt ( F("[ESP]"), F("Erreur envoi requête Remote Relay") ); 
}


///////////////////////////////////////////////////////////////////
// Fonctions de gestion du serveur Web                           //
///////////////////////////////////////////////////////////////////

void startWeb (void) {
  AsyncElegantOTA.setID(MAXPV_VERSION_FULL);
  AsyncElegantOTA.begin(&webServer);
  delay(100);
  setWebHandlers();
  delay(100);
  webServer.begin();
  delay(2000);
}


void setWebHandlers (void) {

  // ***********************************************************************
  // ********      DECLARATIONS DES HANDLERS DU SERVEUR WEB         ********
  // ***********************************************************************

  webServer.onNotFound([](AsyncWebServerRequest * request)
  {
    request->redirect( F("/") );
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
    request->redirect(F("/update"));
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

  delay(5);

  // ***********************************************************************
  // ********                  HANDLERS DE L'API                    ********
  // ***********************************************************************

  webServer.on("/api/action", HTTP_GET, [](AsyncWebServerRequest * request)
  {
    String response = F("Request successfully processed");
    if ( request->hasParam ( F("restart") ) )
      restartEcoPV ( );
    else if ( request->hasParam ( F("resetindex") ) )
      resetIndexEcoPV ( );
    else if ( request->hasParam ( F("saveindex") ) )
      saveIndexEcoPV ( );
    else if ( request->hasParam ( F("saveparam") ) )
      saveConfigEcoPV ( );
    else if ( request->hasParam ( F("loadparam") ) )
      loadConfigEcoPV ( );
    else if ( request->hasParam ( F("format") ) )
      formatEepromEcoPV ( );
    else if ( request->hasParam ( F("eraseconfigesp") ) )
      LittleFS.remove ( F("/config.json") );
    else if ( request->hasParam ( F("rebootesp") ) )
      shouldReboot = true;
    else if ( request->hasParam ( F("booston") ) )
      boostON ( );
    else if ( request->hasParam ( F("boostoff") ) )
      boostOFF ( );
    else response = F("Unknown request");
    request->send ( 200, "text/plain", response );
  });

  webServer.on("/api/get", HTTP_GET, [](AsyncWebServerRequest * request)
  {
    String response = "";
    if ( request->hasParam ( F("configmaxpv") ) )
      request->send ( LittleFS, F("/config.json"), "text/plain" );
    else if ( request->hasParam ( F("versionweb") ) )
      request->send ( LittleFS, F("/versionweb.txt"), "text/plain" );
    else {
      if ( ( request->hasParam ( F("param") ) ) && ( request->getParam(F("param"))->value().toInt() > 0 ) && ( request->getParam(F("param"))->value().toInt() <= ( NB_PARAM - 1 ) ) )
        response = ecoPVConfig [ request->getParam(F("param"))->value().toInt() ];
      else if ( ( request->hasParam ( F("data") ) ) && ( request->getParam(F("data"))->value().toInt() > 0 ) && ( request->getParam(F("data"))->value().toInt() <= ( NB_STATS + NB_STATS_SUPP - 1 ) ) )
        response = ecoPVStats [ request->getParam(F("data"))->value().toInt() ];
      else if ( request->hasParam ( F("allparam") ) )
        response = ecoPVConfigAll;
      else if ( request->hasParam ( F("alldata") ) )
        response = ecoPVStatsAll;
      else if ( request->hasParam ( F("version") ) )
        response = ecoPVConfig [0];
      else if ( request->hasParam ( F("versionmaxpv") ) )
        response = F(MAXPV_VERSION);
      else if ( request->hasParam ( F("relaystate") ) ) {
        if ( ecoPVStats[RELAY_MODE].toInt() == STOP )           response = F("STOP");
        else if ( ecoPVStats[RELAY_MODE].toInt() == FORCE )     response = F("FORCE");
        else if ( ecoPVStats[STATUS_BYTE].toInt() & B00000100 ) response = F("ON");
        else response = F("OFF");
      }
      else if ( request->hasParam ( F("ssrstate") ) ) {
        if ( ecoPVStats[TRIAC_MODE].toInt() == STOP )             response = F("STOP");
        else if ( ecoPVStats[TRIAC_MODE].toInt() == FORCE )       response = F("FORCE");
        else if ( ecoPVStats[TRIAC_MODE].toInt() == AUTOM ) {
          if ( ecoPVStats[STATUS_BYTE].toInt() & B00000010 )      response = F("MAX");
          else if ( ecoPVStats[STATUS_BYTE].toInt() & B00000001 ) response = F("ON");
          else response = F("OFF");
        }
      }
      else if ( request->hasParam ( F("booststate") ) ) {
        if (boostTime == -1) response = F("OFF");
          else response = F("ON");
      }
      else if ( request->hasParam ( F("ping") ) )
        if ( contactEcoPV ) response = F("running");
        else response = F("offline");
      else if ( request->hasParam ( F("time") ) )
        response = timeClient.getFormattedTime ( );
      else response = F("Unknown request");
      request->send ( 200, "text/plain", response );
    }
  });

  webServer.on("/api/set", HTTP_GET, [](AsyncWebServerRequest * request)
  {
    String response = F("Request successfully processed");
    String mystring = "";
    if ( ( request->hasParam ( F("param") ) ) && ( request->hasParam ( F("value") ) )
         && ( request->getParam(F("param"))->value().toInt() > 0 ) && ( request->getParam(F("param"))->value().toInt() <= ( NB_PARAM - 1 ) ) ) {
      mystring = request->getParam(F("value"))->value();
      mystring.replace( F(","), F(".") );
      mystring.trim();
      setParamEcoPV ( request->getParam(F("param"))->value(), mystring );
      // Note : la mise à jour de la base interne de MaxPV se fera de manière asynchrone
      // setParamEcoPV() demande à EcoPV de renvoyer tous les paramètres à MaxPV
    }
    else if ( ( request->hasParam ( F("relaymode") ) ) && ( request->hasParam ( F("value") ) ) ) {
      mystring = request->getParam("value")->value();
      mystring.trim();
      if ( mystring == F("stop") )        relayModeEcoPV ( STOP );
      else if ( mystring == F("force") )  relayModeEcoPV ( FORCE );
      else if ( mystring == F("auto") )   relayModeEcoPV ( AUTOM );
      else response = F("Bad request");
    }
    else if ( ( request->hasParam ( F("ssrmode") ) ) && ( request->hasParam ( F("value") ) ) ) {
      mystring = request->getParam(F("value"))->value();
      mystring.trim();
      if ( mystring == F("stop") )        SSRModeEcoPV ( STOP );
      else if ( mystring == F("force") )  SSRModeEcoPV ( FORCE );
      else if ( mystring == F("auto") )   SSRModeEcoPV ( AUTOM );
      else response = F("Bad request");
    }
    else if ( ( request->hasParam ( F("configmaxpv") ) ) && ( request->hasParam ( F("value") ) ) ) {
      mystring = request->getParam(F("value"))->value();
      if (configRead(mystring)) shouldSaveConfig = true;
    }
    else response = F("Bad request or request unknown");
    request->send ( 200, "text/plain", response );
  });

  delay(5);

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

    if ( request->hasParam ( F("power") ) )
    {
      response->print(F("Time,Import réseau,Export réseau,Production PV,Routage ECS\r\n"));
      for (int i = 1; i < HISTORY_RECORD; i++) {
        localCounter = (historyCounter + i) % HISTORY_RECORD;
        lastLocalCounter = ((localCounter + HISTORY_RECORD - 1) % HISTORY_RECORD);
        if ( ( energyIndexHistoric[localCounter].time != 0 ) && ( (energyIndexHistoric[lastLocalCounter].time) != 0 ) ) {
          timeStamp = String (energyIndexHistoric[localCounter].time);
          timeStamp += F("000");
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
}
