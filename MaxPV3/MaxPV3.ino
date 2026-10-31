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

#include <memory>
#include <LittleFS.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <DNSServer.h>
#include <TZ.h>
#include <WiFiUdp.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h>
#include <AsyncMqtt_Generic.h>
#include <AsyncHTTPRequest_Generic.h> 
#include <Updater.h>
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

// Importation des fichiers du site Web intégrés au firmware
// Généré à la compilation par extra_script.py à partir de MaxPV3/data
#include "web_files.h"

// Définitions utiles pour le programme
#define OFF         0
#define ON          1
#define STOP        0
#define FORCE       1
#define AUTOM       9

// Types d'événements du journal
#define EVT_BOOT           1    // Démarrage de MaxPV!, detail = cause
#define EVT_BOOST_ON       2    // Démarrage boost, detail = origine, value = durée en minutes
#define EVT_BOOST_OFF      3    // Arrêt boost, detail = raison, value = durée effective en minutes (-1 = inconnue)
#define EVT_BOOST_REFUSED  4    // Boost refusé, detail = origine, value = température ECS
#define EVT_SSR_MODE       5    // Changement de mode SSR, detail = origine, value = nouveau mode
#define EVT_WIFI_LOST      6    // Perte Wi-Fi, detail = code raison du SDK
#define EVT_WIFI_OK        7    // Reprise Wi-Fi, value = durée de coupure en secondes
#define EVT_WIFI_WATCHDOG  8    // Redémarrage de la connexion par le watchdog Wifi
#define EVT_MQTT_LOST      9    // Perte MQTT, detail = AsyncMqttClientDisconnectReason
#define EVT_MQTT_OK        10   // Connexion MQTT, value = durée de coupure en secondes (-1 = première connexion)

// Origines et raisons des événements boost et SSR
#define EVT_SRC_WEB        1    // Interface web ou API HTTP
#define EVT_SRC_MQTT       2    // Commande MQTT
#define EVT_SRC_TIMER      3    // Programmation horaire
#define EVT_SRC_TEMP       4    // Température ECS atteinte
#define EVT_SRC_END        5    // Durée écoulée
#define EVT_SRC_ECOPV      6    // Fin signalée par EcoPV sans que la durée soit écoulée
#define EVT_SRC_SSR        7    // Changement de mode SSR

// Causes de démarrage : 0 à 6 = rst_info.reason du SDK, au-delà redémarrages logiciels demandés par MaxPV!
#define EVT_BOOT_WEB       10   // Redémarrage demandé via l'API
#define EVT_BOOT_CONFIG    11   // Fichier de configuration incorrect
#define EVT_BOOT_FS        12   // Erreur de filesystem
#define EVT_BOOT_OTA       13   // Mise à jour du firmware


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

int wtdWifiActive    = OFF;                       // WatchDog actif (= ON) ou non (= OFF)

bool isTimeTrue      = false;

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

// Définition des paramètres RelayPlus
int relayPlusMin    = DEFAULT_RELAYPLUS_MIN;         // Temps minimum de fonctionnement journalier
int relayPlusMax    = DEFAULT_RELAYPLUS_MAX;         // Temps maximum de fonctionnement journalier
int relayPlusHour   = DEFAULT_RELAYPLUS_HOUR;        // Heure de référence pour les calculs
int relayPlusActive = OFF;                           // RelayPlus actif (= ON) ou non (= OFF)

// Définition des paramètres de contrôle de température
int maxTemp         = DEFAULT_MAX_TEMP;
int tempLimitActive = OFF;


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
// Cause du redémarrage demandé par shouldReboot, mémorisée pour le journal des événements
byte rebootCause = EVT_BOOT_WEB;
// Flag indiquant que le firmware reçu par OTA a été écrit avec succès
bool otaSuccess = false;
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
long boostStartUptime = -1;  // Instant de démarrage du BOOST en secondes depuis le boot (-1 = inconnu)
bool boostStopPending = false;  // Arrêt du BOOST demandé par MaxPV!, en attente de la confirmation d'EcoPV

// Variables pour le journal des événements
// Structure pour le stockage d'un événement
struct eventRecord
{
  uint32_t id;        // numéro d'ordre de l'événement, 0 = emplacement vide
  uint32_t time;      // epoch Time, 0 si l'heure NTP n'était pas encore valide
  uint32_t uptime;    // secondes depuis le démarrage
  int32_t  value;     // valeur associée, dépend du type
  byte     type;      // type d'événement EVT_xxx
  byte     detail;    // origine, raison ou cause, dépend du type
};
eventRecord eventLog[EVENT_LOG_SIZE];
uint32_t eventCounter = 0;      // id du dernier événement enregistré
                                // l'événement d'id n est rangé à la position (n - 1) % EVENT_LOG_SIZE
uint32_t eventPublishedId = 0;  // id du dernier événement publié en MQTT

// Suivi des connexions Wifi et MQTT pour ne journaliser que les changements d'état
WiFiEventHandler wifiDisconnectHandler;
WiFiEventHandler wifiGotIPHandler;
bool wifiLinkUp = true;
uint32_t wifiDownSince = 0;
byte mqttLinkState = 0;         // 0 = jamais connecté, 1 = connecté, 2 = déconnecté
uint32_t mqttDownSince = 0;

// Variables pour la gestion du relais distant
int relaisDistantPreviousState = OFF;

// Variables pour la gestion du mode relayPlus
long relaisPlusIndex = -1;      // Index du temps de fontionnement du relais comme référence journalière
                                // -1 : non initialisé
int relaisPlusCountDown = -1;   // compte à rebours de la durée d'une journée moins 10 minutes

// ***********************************************************************************
// ********************** Déclaration des serveurs et des clients ********************
// ***********************************************************************************

AsyncWebServer    webServer(HTTP_PORT);
AsyncMqttClient   mqttClient;
AsyncHTTPRequest  remoteRelayRequest;
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
DNSServer         dnsServer;
AsyncWiFiManager  wifiManager(&webServer, &dnsServer);

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

  // Premier événement du journal : démarrage et sa cause
  logBootEvent();

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
      saveRebootCause(EVT_BOOT_FS);
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
      saveRebootCause(EVT_BOOT_CONFIG);
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

  // Journalisation des pertes et reprises de la connexion Wifi
  setWifiEventHandlers();

  // Sauvegarde de la configuration MaxPV! si nécessaire
  if (shouldSaveConfig) {
    configWrite();
    logSerial ( F("[ESP]"), F("Configuration sauvegardée") );
    shouldSaveConfig = false;
  }
  delay(50);

  logSerial ( F("[ESP]"), F("Initialisation") );

  // Démarrage du client NTP
  logSerial ( F("[ESP]"), F("Configuration client NTP") ); 
  configTzTime(TIME_ZONE, NTP_SERVER);
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
  
  if ((shouldCheckWifi) && (wtdWifiActive==ON)) {
    watchDogWifi();
    shouldCheckWifi = false;
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

  publishPendingEvents();

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
        if (jsonConfig.containsKey("watchdogwifi_active"))  wtdWifiActive = jsonConfig["watchdogwifi_active"]; else shouldSaveConfig = true;
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
        if (jsonConfig.containsKey("RelayPlus_min"))      relayPlusMin = jsonConfig["RelayPlus_min"]; else shouldSaveConfig = true;
        if (jsonConfig.containsKey("RelayPlus_max"))      relayPlusMax = jsonConfig["RelayPlus_max"]; else shouldSaveConfig = true;
        if (jsonConfig.containsKey("RelayPlus_hour"))     relayPlusHour = jsonConfig["RelayPlus_hour"]; else shouldSaveConfig = true;
        if (jsonConfig.containsKey("RelayPlus_active"))   relayPlusActive = jsonConfig["RelayPlus_active"]; else shouldSaveConfig = true;
        if (jsonConfig.containsKey("max_temp"))           maxTemp = jsonConfig["max_temp"]; else shouldSaveConfig = true;
        if (jsonConfig.containsKey("temp_limit_active"))  tempLimitActive = jsonConfig["temp_limit_active"]; else shouldSaveConfig = true;

        // traitement des durées min et max du mode RelayPlus
        if ( relayPlusMin <= 5)  relayPlusMin = 5;
        if ( relayPlusMax <= relayPlusMin)  relayPlusMax = relayPlusMin + 5;
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
  jsonConfig["watchdogwifi_active"] = wtdWifiActive;
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
  jsonConfig["RelayPlus_min"] = relayPlusMin;
  jsonConfig["RelayPlus_max"] = relayPlusMax;
  jsonConfig["RelayPlus_hour"] = relayPlusHour;
  jsonConfig["RelayPlus_active"] = relayPlusActive;
  jsonConfig["max_temp"] = maxTemp;
  jsonConfig["temp_limit_active"] = tempLimitActive;


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
  saveRebootCause(rebootCause);
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
    time_t timestamp = time(NULL);
    char buffer[32];
    struct tm *pTime = localtime(&timestamp );
    strftime(buffer, 32, "%H:%M:%S", pTime);
    msg = String(buffer);
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

    time_t timestamp = time(NULL);
    char buffer[32];
    struct tm *pTime = localtime(&timestamp );
    strftime(buffer, 32, "%d/%m/%Y %H:%M", pTime);

    msg = String(buffer);;
    msg += F("  ");
    msg += logger;
    msg += F("  ");
    msg += message;
    mqttClient.publish(MQTT_LOGGER, 0, true, msg.c_str());
  }
}


///////////////////////////////////////////////////////////////////
// Journal des événements                                        //
// Tampon circulaire en RAM, consultable en JSON (/api/events)   //
// et publié événement par événement en MQTT (MQTT_EVENT)        //
///////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////
// Temps écoulé depuis le démarrage en secondes                  //
// micros64() ne reboucle pas, contrairement à millis()          //
///////////////////////////////////////////////////////////////////

uint32_t uptimeSeconds(void)
{
  return uint32_t ( micros64() / 1000000ULL );
}

///////////////////////////////////////////////////////////////////
// Enregistrement d'un événement dans le tampon circulaire       //
// Sans allocation : utilisable dans les callbacks Wifi, MQTT    //
// et les handlers Web. La publication MQTT est faite par loop() //
///////////////////////////////////////////////////////////////////

void logEvent(byte type, byte detail, long value)
{
  eventRecord& e = eventLog[eventCounter % EVENT_LOG_SIZE];
  time_t timestamp = time(NULL);
  eventCounter++;
  e.id      = eventCounter;
  e.time    = ( timestamp > TIME_REF ) ? uint32_t (timestamp) : 0;
  e.uptime  = uptimeSeconds();
  e.value   = value;
  e.type    = type;
  e.detail  = detail;
}

///////////////////////////////////////////////////////////////////
// Mémorise en mémoire RTC la cause d'un redémarrage logiciel    //
// à appeler juste avant ESP.restart()                           //
///////////////////////////////////////////////////////////////////

void saveRebootCause(byte cause)
{
  uint32_t rtcData[2] = { RTC_REBOOT_MAGIC, cause };
  ESP.rtcUserMemoryWrite(RTC_REBOOT_OFFSET, rtcData, sizeof(rtcData));
}

///////////////////////////////////////////////////////////////////
// Journalise le démarrage avec sa cause : raison matérielle     //
// du SDK, précisée par la cause mémorisée en RTC si le          //
// redémarrage logiciel a été demandé par MaxPV!                 //
///////////////////////////////////////////////////////////////////

void logBootEvent(void)
{
  uint32_t rtcData[2];
  const rst_info* resetInfo = ESP.getResetInfoPtr();
  byte cause = resetInfo->reason;
  long value = ( cause == REASON_EXCEPTION_RST ) ? long (resetInfo->exccause) : 0;

  if ( ESP.rtcUserMemoryRead(RTC_REBOOT_OFFSET, rtcData, sizeof(rtcData)) && ( rtcData[0] == RTC_REBOOT_MAGIC ) ) {
    if ( cause == REASON_SOFT_RESTART ) cause = byte (rtcData[1]);
    rtcData[0] = 0;
    ESP.rtcUserMemoryWrite(RTC_REBOOT_OFFSET, rtcData, sizeof(rtcData));
  }
  logEvent ( EVT_BOOT, cause, value );
}

///////////////////////////////////////////////////////////////////
// Callbacks de perte et de reprise de la connexion Wifi         //
// La perte est signalée à chaque tentative de reconnexion :     //
// on ne journalise que les changements d'état                   //
///////////////////////////////////////////////////////////////////

void setWifiEventHandlers(void)
{
  wifiLinkUp = WiFi.isConnected();
  wifiDisconnectHandler = WiFi.onStationModeDisconnected([](const WiFiEventStationModeDisconnected& evt)
  {
    if (wifiLinkUp) {
      wifiLinkUp = false;
      wifiDownSince = uptimeSeconds();
      logEvent ( EVT_WIFI_LOST, byte (evt.reason), 0 );
    }
  });
  wifiGotIPHandler = WiFi.onStationModeGotIP([](const WiFiEventStationModeGotIP&)
  {
    if (!wifiLinkUp) {
      wifiLinkUp = true;
      logEvent ( EVT_WIFI_OK, 0, long ( uptimeSeconds() - wifiDownSince ) );
    }
  });
}

///////////////////////////////////////////////////////////////////
// Libellés des événements                                       //
///////////////////////////////////////////////////////////////////

const __FlashStringHelper* eventTypeKey(byte type)
{
  switch (type) {
    case EVT_BOOT:          return F("boot");
    case EVT_BOOST_ON:      return F("boost_on");
    case EVT_BOOST_OFF:     return F("boost_off");
    case EVT_BOOST_REFUSED: return F("boost_refused");
    case EVT_SSR_MODE:      return F("ssr_mode");
    case EVT_WIFI_LOST:     return F("wifi_lost");
    case EVT_WIFI_OK:       return F("wifi_ok");
    case EVT_WIFI_WATCHDOG: return F("wifi_watchdog");
    case EVT_MQTT_LOST:     return F("mqtt_lost");
    case EVT_MQTT_OK:       return F("mqtt_ok");
    default:                return F("unknown");
  }
}

const __FlashStringHelper* eventSourceKey(byte source)
{
  switch (source) {
    case EVT_SRC_WEB:   return F("web");
    case EVT_SRC_MQTT:  return F("mqtt");
    case EVT_SRC_TIMER: return F("timer");
    case EVT_SRC_TEMP:  return F("temperature");
    case EVT_SRC_END:   return F("end");
    case EVT_SRC_ECOPV: return F("ecopv");
    case EVT_SRC_SSR:   return F("ssr");
    default:            return F("unknown");
  }
}

const __FlashStringHelper* eventSourceText(byte source)
{
  switch (source) {
    case EVT_SRC_WEB:   return F("web / API");
    case EVT_SRC_MQTT:  return F("MQTT");
    case EVT_SRC_TIMER: return F("programmation horaire");
    case EVT_SRC_TEMP:  return F("température ECS atteinte");
    case EVT_SRC_END:   return F("durée écoulée");
    case EVT_SRC_ECOPV: return F("fin signalée par EcoPV");
    case EVT_SRC_SSR:   return F("changement de mode SSR");
    default:            return F("inconnue");
  }
}

const __FlashStringHelper* bootCauseKey(byte cause)
{
  switch (cause) {
    case REASON_DEFAULT_RST:      return F("power_on");
    case REASON_WDT_RST:          return F("hw_watchdog");
    case REASON_EXCEPTION_RST:    return F("exception");
    case REASON_SOFT_WDT_RST:     return F("soft_watchdog");
    case REASON_SOFT_RESTART:     return F("soft_restart");
    case REASON_DEEP_SLEEP_AWAKE: return F("deep_sleep");
    case REASON_EXT_SYS_RST:      return F("external_reset");
    case EVT_BOOT_WEB:            return F("web_request");
    case EVT_BOOT_CONFIG:         return F("config_error");
    case EVT_BOOT_FS:             return F("filesystem_error");
    case EVT_BOOT_OTA:            return F("ota_update");
    default:                      return F("unknown");
  }
}

const __FlashStringHelper* bootCauseText(byte cause)
{
  switch (cause) {
    case REASON_DEFAULT_RST:      return F("mise sous tension");
    case REASON_WDT_RST:          return F("watchdog matériel");
    case REASON_EXCEPTION_RST:    return F("exception logicielle");
    case REASON_SOFT_WDT_RST:     return F("watchdog logiciel");
    case REASON_SOFT_RESTART:     return F("redémarrage logiciel");
    case REASON_DEEP_SLEEP_AWAKE: return F("réveil de veille profonde");
    case REASON_EXT_SYS_RST:      return F("reset externe");
    case EVT_BOOT_WEB:            return F("redémarrage demandé via l'API");
    case EVT_BOOT_CONFIG:         return F("fichier de configuration incorrect");
    case EVT_BOOT_FS:             return F("erreur de filesystem");
    case EVT_BOOT_OTA:            return F("mise à jour du firmware");
    default:                      return F("inconnue");
  }
}

const __FlashStringHelper* mqttReasonKey(byte reason)
{
  switch (reason) {
    case 0:  return F("tcp_disconnected");
    case 1:  return F("unacceptable_protocol");
    case 2:  return F("identifier_rejected");
    case 3:  return F("server_unavailable");
    case 4:  return F("malformed_credentials");
    case 5:  return F("not_authorized");
    case 6:  return F("not_enough_space");
    case 7:  return F("tls_bad_fingerprint");
    default: return F("unknown");
  }
}

const __FlashStringHelper* mqttReasonText(byte reason)
{
  switch (reason) {
    case 0:  return F("connexion TCP perdue");
    case 1:  return F("version de protocole refusée");
    case 2:  return F("identifiant refusé");
    case 3:  return F("serveur indisponible");
    case 4:  return F("identifiants mal formés");
    case 5:  return F("non autorisé");
    case 6:  return F("mémoire insuffisante");
    case 7:  return F("empreinte TLS invalide");
    default: return F("raison inconnue");
  }
}

const __FlashStringHelper* wifiReasonText(byte reason)
{
  switch (reason) {
    case WIFI_DISCONNECT_REASON_AUTH_EXPIRE:            return F("authentification expirée");
    case WIFI_DISCONNECT_REASON_ASSOC_EXPIRE:           return F("association expirée");
    case WIFI_DISCONNECT_REASON_ASSOC_LEAVE:            return F("déconnexion demandée");
    case WIFI_DISCONNECT_REASON_4WAY_HANDSHAKE_TIMEOUT: return F("échec de la négociation WPA");
    case WIFI_DISCONNECT_REASON_BEACON_TIMEOUT:         return F("perte du signal");
    case WIFI_DISCONNECT_REASON_NO_AP_FOUND:            return F("point d'accès introuvable");
    case WIFI_DISCONNECT_REASON_AUTH_FAIL:              return F("échec d'authentification");
    case WIFI_DISCONNECT_REASON_ASSOC_FAIL:             return F("échec d'association");
    case WIFI_DISCONNECT_REASON_HANDSHAKE_TIMEOUT:      return F("délai de négociation dépassé");
    default:                                            return nullptr;
  }
}

///////////////////////////////////////////////////////////////////
// Message lisible d'un événement                                //
///////////////////////////////////////////////////////////////////

String eventMessage(const eventRecord& e)
{
  String msg;
  msg.reserve(96);
  switch (e.type) {
    case EVT_BOOT:
      msg = F("Démarrage de MaxPV! : ");
      msg += bootCauseText(e.detail);
      if ( e.detail == REASON_EXCEPTION_RST ) {
        msg += F(" (exception ");
        msg += e.value;
        msg += F(")");
      }
      break;
    case EVT_BOOST_ON:
      msg = F("Boost démarré (");
      msg += eventSourceText(e.detail);
      msg += F(") pour ");
      msg += e.value;
      msg += F(" min");
      break;
    case EVT_BOOST_OFF:
      msg = F("Boost arrêté : ");
      if ( ( e.detail == EVT_SRC_WEB ) || ( e.detail == EVT_SRC_MQTT ) ) msg += F("arrêt manuel ");
      msg += eventSourceText(e.detail);
      if ( e.value >= 0 ) {
        msg += F(", après ");
        msg += e.value;
        msg += F(" min");
      }
      break;
    case EVT_BOOST_REFUSED:
      msg = F("Boost refusé (");
      msg += eventSourceText(e.detail);
      msg += F(") : température ECS ");
      msg += e.value;
      msg += F(" °C, limite ");
      msg += maxTemp;
      msg += F(" °C");
      break;
    case EVT_SSR_MODE:
      msg = F("Mode SSR : ");
      if ( e.value == STOP )        msg += F("STOP");
      else if ( e.value == FORCE )  msg += F("FORCE");
      else                          msg += F("AUTO");
      msg += F(" (");
      msg += eventSourceText(e.detail);
      msg += F(")");
      break;
    case EVT_WIFI_LOST:
      msg = F("Perte de la connexion Wifi : ");
      if ( wifiReasonText(e.detail) != nullptr ) msg += wifiReasonText(e.detail);
      else {
        msg += F("code ");
        msg += e.detail;
      }
      break;
    case EVT_WIFI_OK:
      msg = F("Connexion Wifi rétablie après ");
      msg += e.value;
      msg += F(" s");
      break;
    case EVT_WIFI_WATCHDOG:
      msg = F("Watchdog Wifi : redémarrage de la connexion");
      break;
    case EVT_MQTT_LOST:
      msg = F("Perte de la connexion MQTT : ");
      msg += mqttReasonText(e.detail);
      break;
    case EVT_MQTT_OK:
      if ( e.value < 0 ) msg = F("Connexion MQTT établie");
      else {
        msg = F("Connexion MQTT rétablie après ");
        msg += e.value;
        msg += F(" s");
      }
      break;
    default:
      msg = F("Evénement inconnu");
  }
  return msg;
}

///////////////////////////////////////////////////////////////////
// Sérialisation JSON d'un événement                             //
// Si l'heure NTP n'était pas valide au moment de l'événement,   //
// l'heure est reconstituée à partir de l'uptime                 //
///////////////////////////////////////////////////////////////////

String eventToJson(const eventRecord& e)
{
  String json;
  json.reserve(256);
  uint32_t eventTime = e.time;
  time_t timestamp = time(NULL);
  if ( ( eventTime == 0 ) && ( timestamp > TIME_REF ) ) eventTime = uint32_t (timestamp) - ( uptimeSeconds() - e.uptime );

  json = F("{\"id\":");
  json += e.id;
  json += F(",\"time\":");
  json += eventTime;
  json += F(",\"uptime\":");
  json += e.uptime;
  json += F(",\"type\":\"");
  json += eventTypeKey(e.type);
  json += F("\",\"detail\":\"");
  switch (e.type) {
    case EVT_BOOT:          json += bootCauseKey(e.detail); break;
    case EVT_BOOST_ON:
    case EVT_BOOST_OFF:
    case EVT_BOOST_REFUSED:
    case EVT_SSR_MODE:      json += eventSourceKey(e.detail); break;
    case EVT_WIFI_LOST:     json += e.detail; break;
    case EVT_MQTT_LOST:     json += mqttReasonKey(e.detail); break;
    default:                break;
  }
  json += F("\",\"value\":");
  json += e.value;
  json += F(",\"msg\":\"");
  json += eventMessage(e);
  json += F("\"}");
  return json;
}

///////////////////////////////////////////////////////////////////
// Publication MQTT des événements pas encore publiés            //
// Appelé par loop() : les événements survenus pendant une       //
// coupure Wifi ou MQTT sont publiés à la reconnexion            //
///////////////////////////////////////////////////////////////////

void publishPendingEvents(void)
{
  if ( ( eventPublishedId == eventCounter ) || ( !mqttClient.connected() ) ) return;
  // Les événements écrasés dans le tampon avant d'avoir été publiés sont perdus
  if ( ( eventCounter - eventPublishedId ) > EVENT_LOG_SIZE ) eventPublishedId = eventCounter - EVENT_LOG_SIZE;
  for ( int n = 0; ( n < EVENT_MQTT_BURST ) && ( eventPublishedId < eventCounter ); n++ ) {
    // l'événement suivant (id = eventPublishedId + 1) est rangé à la position eventPublishedId % EVENT_LOG_SIZE
    const eventRecord& e = eventLog[eventPublishedId % EVENT_LOG_SIZE];
    if ( mqttClient.publish(MQTT_EVENT, 0, true, eventToJson(e).c_str()) == 0 ) break;
    eventPublishedId++;
    yield();
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

      // Arrêt du Boost si la température ECS est atteinte
      if ( ( tempLimitActive == ON ) && ( ecoPVStats[TEMP_ECS].toInt() > maxTemp ) && ( boostTime > 0 ) ) boostOFF ( EVT_SRC_TEMP );
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
      long newBoostTime = incomingData.toInt();
      // Après un arrêt demandé par MaxPV!, déjà journalisé avec sa raison, le minuteur est désactivé :
      // EcoPV peut encore transmettre un temps restant (0 ou valeur en transit) avant de confirmer l'arrêt par -1,
      // ce qui relancerait le suivi du boost et produirait une fausse fin
      if ( boostStopPending ) {
        if ( newBoostTime == -1 ) boostStopPending = false;
        return true;
      }
      if ( ( boostTime >= 0 ) && ( newBoostTime == -1 ) ) {
        // On ignore un -1 émis par EcoPV avant la prise en compte d'un boost tout juste lancé
        if ( ( boostStartUptime >= 0 ) && ( long (uptimeSeconds()) - boostStartUptime < 3 ) ) return true;
        // EcoPV transmet le temps restant toutes les 30 secondes :
        // une fin annoncée alors qu'il restait plus d'une minute n'est pas une fin de durée
        boostStopped ( ( boostTime <= 60 ) ? EVT_SRC_END : EVT_SRC_ECOPV );
        if (mqttClient.connected ()) mqttClient.publish(MQTT_BOOST_MODE, 0, true, "off");
      }
      boostTime = newBoostTime;
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

  // Desactivation de la fonction RelayPlus pour la journée dès que l'on envoie un ordre manuel au relais
  // réalisé en mettant relaisPlusIndex à -1, ca bloque l'entrée dans les fonctions à la minute
  // relaisPlusIndex = -1;
}

///////////////////////////////////////////////////////////////////
// Change le mode de fonctionnement du SSR                       //
///////////////////////////////////////////////////////////////////

void SSRModeEcoPV(byte opMode, byte source)
{
  String str;
  String command;
  str.reserve(16);
  command.reserve(32);

  // Pendant un boost, EcoPV alterne FORCE et STOP : toute commande est alors un changement
  if ( ( boostTime >= 0 ) || ( opMode != ecoPVStats[TRIAC_MODE].toInt() ) ) logEvent ( EVT_SSR_MODE, source, opMode );
  // EcoPV arrête le boost en cours lors d'un changement de mode SSR
  if ( boostTime >= 0 ) {
    boostStopped ( EVT_SRC_SSR );
    boostTime = -1;
    boostStopPending = true;
    if (mqttClient.connected ()) mqttClient.publish(MQTT_BOOST_MODE, 0, true, "off");
  }

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
// Active ou arrête le mode Relay+ dans EcoPV                    //
// qui est en fait un Timer sur le relais                        //
///////////////////////////////////////////////////////////////////

void relayPlusEcoPV(long bT)
{
  String command;
  command.reserve(32);
  command = F("SETRELPLUS,");
  command += bT;
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

void boostON(byte source)
{
  // Pas de boost si la température ECS est déjà atteinte
  if ( ( tempLimitActive == ON ) && ( ecoPVStats[TEMP_ECS].toInt() >= maxTemp ) ) {
    logEvent ( EVT_BOOST_REFUSED, source, ecoPVStats[TEMP_ECS].toInt() );
    return;
  }
  if ( boostRatio != 0 ) {
    long bT = ( 60 * boostDuration );   // conversion en secondes
    //burstCnt = 0;
    boostModeEcoPV(bT,boostRatio);
    boostTime = bT;
    boostStartUptime = uptimeSeconds();
    boostStopPending = false;
    logEvent ( EVT_BOOST_ON, source, boostDuration );
    logMqtt ( F("[ESP]"), F("Mode Boost --> ON") );
    if (mqttClient.connected ()) mqttClient.publish(MQTT_BOOST_MODE, 0, true, "on");
  }
}


void boostOFF(byte reason)
{
  boostModeEcoPV(0,boostRatio);
  if ( boostTime >= 0 ) boostStopped ( reason );
  boostTime = -1;
  boostStopPending = true;    // EcoPV confirmera l'arrêt par BOOSTTIME,-1
  logMqtt ( F("[ESP]"), F("Mode Boost --> OFF") );
  if (mqttClient.connected ()) mqttClient.publish(MQTT_BOOST_MODE, 0, true, "off");
}


// Journalise la fin d'un boost en cours, avec sa durée effective si elle est connue
void boostStopped(byte reason)
{
  long elapsed = -1;
  if ( boostStartUptime >= 0 ) elapsed = ( long (uptimeSeconds()) - boostStartUptime ) / 60;
  logEvent ( EVT_BOOST_OFF, reason, elapsed );
  boostStartUptime = -1;
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
  if (isTimeTrue) {
    time_t timestamp = time(NULL);
    energyIndexHistoric[historyCounter].time      = long (timestamp);
    energyIndexHistoric[historyCounter].eRouted   = ecoPVStats[INDEX_ROUTED].toFloat();
    energyIndexHistoric[historyCounter].eImport   = ecoPVStats[INDEX_IMPORT].toFloat();
    energyIndexHistoric[historyCounter].eExport   = ecoPVStats[INDEX_EXPORT].toFloat();
    energyIndexHistoric[historyCounter].eImpulsion = ecoPVStats[INDEX_IMPULSION].toFloat();
    energyIndexHistoric[historyCounter].tRelayOn  = long (ecoPVStats[INDEX_RELAY].toInt());
    historyCounter++;
    historyCounter %= HISTORY_RECORD;   // Création d'un tableau circulaire
    logMqtt ( F("[ESP]"), F("Relève des index effectuée") ); 
  }
  else logMqtt ( F("[ESP]"), F("Index non relevés, heure NTP incorrecte") ); 
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

  // Toutes les 119 secondes on demande la version ecoPV
  if ( generalCounterSecond % 119 == 63 )  getVersionEcoPV();
  
  yield();

  // Toutes les 273 secondes on vérifie la validité de l'heure NTP
  if ( generalCounterSecond % 273 == 17 )  checkTimeSet();
  
  yield();

  // Enregistrement des historiques
  if (generalCounterSecond % (HISTORY_INTERVAL * 60) == 0) recordHistoricData();;

  yield();

  // traitement MQTT
  if ((mqttActive == ON) && (generalCounterSecond % mqttPeriod == 0) ) mqttTransmit();

  yield();

  // On ping le dns1 périodiquement, ici tous les 30 secondes, 2 fois maxi, timeout 1000 ms, sir le watchdog wifi est actif
  if ((generalCounterSecond % 30 == 7) && (wtdWifiActive==ON)) {
    IPAddress _dns1;
    _dns1.fromString(static_dns1);
    ping.begin (_dns1, 2, 1000);
  }

  yield();

  // Traitement RelaisPlus toutes les minutes
  if ( ( relayPlusActive == ON ) && ( generalCounterSecond % 60 == 43 ) && ( relaisPlusIndex >=0 ) && ( relaisPlusCountDown >=0 ) )  {
    relaisPlusCountDown--;
    if ( ( long (ecoPVStats[INDEX_RELAY].toInt()) - relaisPlusIndex ) >= relayPlusMax ) {
      logMqtt ( F("[RelayPlus]"), F("Temps journalier maximum atteint") ); 
      relayModeEcoPV(STOP);
      relaisPlusCountDown = -1;
    }
    else if ( relaisPlusCountDown <= ( relayPlusMin - ( long (ecoPVStats[INDEX_RELAY].toInt()) - relaisPlusIndex ) ) ) {
      logMqtt ( F("[RelayPlus]"), F("Relais ON pour atteindre la durée minimale") ); 
      relayPlusEcoPV( 60 * (relayPlusMin - ( long (ecoPVStats[INDEX_RELAY].toInt()) - relaisPlusIndex )) );
      relaisPlusCountDown = -1;
    }
  }
  yield();  
}


void timeScheduler(void)
{
  // Le scheduler est exécuté toutes les minutes
  time_t timestamp = time(NULL);
  struct tm *pTime = localtime(&timestamp );
  
  int day     = pTime->tm_mday;
  int hour    = pTime->tm_hour;
  int minute  = pTime->tm_min;

  // Mise à jour des index de début de journée en début de journée solaire à 00H00
  if ( ( hour == 0 ) && ( minute == 0 ) ) {
    setRefIndexJour ( );
    logMqtt ( F("[ESP]"), F("Initialisation des index de début de journée") ); 
  };

  // Déclenchement du mode BOOST sur Timer
  // Le scheduler pouvant être appelé 2 fois dans la même minute, on ne déclenche qu'une fois
  // La limite de température est vérifiée par boostON()
  static time_t lastBoostTimer = 0;
  if ( ( hour == boostTimerHour ) && ( minute == boostTimerMinute )
        && ( boostTimerActive == ON ) && ( ( timestamp - lastBoostTimer ) > 120 ) ) {
    lastBoostTimer = timestamp;
    boostON ( EVT_SRC_TIMER );
  }

  // Initialisation du mode RelayPlus pour la nouvelle journée
  if ( ( hour == relayPlusHour ) && ( minute == 1 ) && ( relayPlusActive == ON ) )  {
    logMqtt ( F("[RelayPlus]"), F("Initialisation du jour") ); 
    relaisPlusIndex = long (ecoPVStats[INDEX_RELAY].toInt());
    relaisPlusCountDown = 1430; // correspond à 23h50min
    relayModeEcoPV(AUTOM);      // on met le relais en mode automatique
  }
}



///////////////////////////////////////////////////////////////////
// Fonctions de gestion de la connexion Mqtt                     //
// et de l'autodiscovery Home Assisant                           //
///////////////////////////////////////////////////////////////////

void startMqtt (void) 
{
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
  logEvent ( EVT_MQTT_OK, 0, ( mqttLinkState == 2 ) ? long ( uptimeSeconds() - mqttDownSince ) : -1 );
  mqttLinkState = 1;
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
  payload.replace(F("#DEVICECLASS#"), F("power_factor"));
  payload.replace(F("#STATECLASS#"), F("measurement"));
  payload.replace(F("#STATETOPIC#"), F(MQTT_COS_PHI));
  payload.replace(F(",\"unit_of_meas\":\"#UNIT#\""), "");
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

  // MQTT_P_IMP
  topic = configTopicTemplate;
  topic.replace(F("#COMPONENT#"), F("sensor"));
  topic.replace(F("#SENSORID#"), F("PuissanceImportee"));

  payload = configPayloadTemplate;
  payload.replace(F("#SENSORID#"), F("PuissanceImportee"));
  payload.replace(F("#SENSORNAME#"), F("Puissance importée"));
  payload.replace(F("#DEVICECLASS#"), F("power"));
  payload.replace(F("#STATECLASS#"), F("measurement"));
  payload.replace(F("#STATETOPIC#"), F(MQTT_P_IMP));
  payload.replace(F("#UNIT#"), "W");
  mqttClient.publish(topic.c_str(), 0, true, payload.c_str());
  
  // MQTT_P_EXP
  topic = configTopicTemplate;
  topic.replace(F("#COMPONENT#"), F("sensor"));
  topic.replace(F("#SENSORID#"), F("PuissanceExportee"));

  payload = configPayloadTemplate;
  payload.replace(F("#SENSORID#"), F("PuissanceExportee"));
  payload.replace(F("#SENSORNAME#"), F("Puissance exportée"));
  payload.replace(F("#DEVICECLASS#"), F("power"));
  payload.replace(F("#STATECLASS#"), F("measurement"));
  payload.replace(F("#STATETOPIC#"), F(MQTT_P_EXP));
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

  // MQTT_TEMP_ECS
  topic = configTopicTemplate;
  topic.replace(F("#COMPONENT#"), F("sensor"));
  topic.replace(F("#SENSORID#"), F("Temperature"));

  payload = configPayloadTemplate;
  payload.replace(F("#SENSORID#"), F("Temperature"));
  payload.replace(F("#SENSORNAME#"), F("Température ECS"));
  payload.replace(F("#DEVICECLASS#"), F("temperature"));
  payload.replace(F("#STATECLASS#"), F("measurement"));
  payload.replace(F("#STATETOPIC#"), F(MQTT_TEMP_ECS));
  payload.replace(F("#UNIT#"), "°C");
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

  // MQTT_EVENT
  topic = configTopicTemplate;
  topic.replace(F("#COMPONENT#"), F("sensor"));
  topic.replace(F("#SENSORID#"), F("Evenement"));

  payload = configPayloadTemplate;
  payload.replace(F("#SENSORID#"), F("Evenement"));
  payload.replace(F("#SENSORNAME#"), F("Dernier événement"));
  payload.replace(F("\"dev_cla\":\"#DEVICECLASS#\","), F(""));
  payload.replace(F("\"stat_cla\":\"#STATECLASS#\","), F(""));
  payload.replace(F("#STATETOPIC#"), F(MQTT_EVENT));
  payload.replace(F("\"unit_of_meas\":\"#UNIT#\""),
                  F("\"val_tpl\":\"{{ value_json.msg }}\","
                    "\"json_attr_t\":\"" MQTT_EVENT "\""));
  mqttClient.publish(topic.c_str(), 0, true, payload.c_str());
}


void onMqttDisconnect(AsyncMqttClientDisconnectReason reason)
{
  // Appelé à chaque échec de reconnexion : on ne journalise que le passage à l'état déconnecté
  if ( mqttLinkState != 2 ) {
    logEvent ( EVT_MQTT_LOST, byte (reason), 0 );
    mqttLinkState = 2;
    mqttDownSince = uptimeSeconds();
  }
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
    // on déacive relay+ s'il était activé
    relaisPlusIndex = -1;
  }
  else if (String(topic).startsWith(F(MQTT_SET_TRIAC_MODE))) {
    if ( String(payload).startsWith(F("stop")) )        SSRModeEcoPV ( STOP, EVT_SRC_MQTT );
    else if ( String(payload).startsWith(F("force")) )  SSRModeEcoPV ( FORCE, EVT_SRC_MQTT );
    else if ( String(payload).startsWith(F("auto")) )   SSRModeEcoPV ( AUTOM, EVT_SRC_MQTT );
  }
  else if (String(topic).startsWith(F(MQTT_SET_BOOST_MODE))) {
    if ( String(payload).startsWith(F("on")) )          boostON ( EVT_SRC_MQTT );
    else if ( String(payload).startsWith(F("off")) )    boostOFF ( EVT_SRC_MQTT );
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
    mqttClient.publish(MQTT_P_IMP, 0, true, ecoPVStats[P_IMP].c_str());
    yield();
    mqttClient.publish(MQTT_P_EXP, 0, true, ecoPVStats[P_EXP].c_str());
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
    mqttClient.publish(MQTT_TEMP_ECS, 0, true, ecoPVStats[TEMP_ECS].c_str());
    yield();
    if (boostTime == -1) mqttClient.publish(MQTT_BOOST_MODE, 0, true, "off");
    else mqttClient.publish(MQTT_BOOST_MODE, 0, true, "on");
    yield();


/*
    // test pour le fonctionnement relaisPlus
    mqttClient.publish("maxpv/relaisPlusIndex", 0, true, String(relaisPlusIndex).c_str());
    yield();
    mqttClient.publish("maxpv/relayPlusMin", 0, true, String(relayPlusMin).c_str());
    yield();
    mqttClient.publish("maxpv/relayPlusMax", 0, true, String(relayPlusMax).c_str());
    yield();
    mqttClient.publish("maxpv/relayPlusCountDown", 0, true, String(relaisPlusCountDown).c_str());
    yield();
    long condition = ( relayPlusMin - ( long (ecoPVStats[INDEX_RELAY].toInt()) - relaisPlusIndex ) );
    mqttClient.publish("maxpv/relaisPlusCondition", 0, true, String(condition).c_str());
    yield();
*/

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
      logMqtt ( F("[ESP]"), F("Succès ping DNS1") );
      refTimePingWifi = millis();
      return true; // on s'arrête
    }
    else {
      // le ping n'est pas revenu
      logMqtt ( F("[ESP]"), F("Echec ping DNS1") );
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
    logMqtt ( F("[ESP]"), F("Watchdog Wifi activé") );
    logEvent ( EVT_WIFI_WATCHDOG, 0, 0 );
    WiFi.mode(WIFI_OFF);
    delay(20);
    WiFi.mode(WIFI_STA);
    delay(20);
    WiFi.reconnect();
    delay(50);
    refTimePingWifi = millis();
    logMqtt ( F("[ESP]"), F("Connexion Wifi redémarrée") );
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

///////////////////////////////////////////////////////////////////
// Envoi d'un fichier du site Web intégré au firmware            //
// Les fichiers sont stockés compressés en gzip                  //
///////////////////////////////////////////////////////////////////

const webFile* findWebFile (const char* path)
{
  for (size_t i = 0; i < WEB_FILES_COUNT; i++)
    if ( strcmp ( webFiles[i].path, path ) == 0 ) return &webFiles[i];
  return nullptr;
}


void sendWebFile (AsyncWebServerRequest * request, const webFile* file)
{
  if ( file == nullptr ) {
    request->send ( 404, "text/plain", F("Not found") );
    return;
  }
  AsyncWebServerResponse *response = request->beginResponse_P ( 200, file->mime, file->data, file->len );
  response->addHeader ( F("Content-Encoding"), F("gzip") );
  request->send ( response );
}


void startWeb (void)
{
  setWebHandlers();
  delay(100);
  webServer.begin();
  delay(2000);
}


///////////////////////////////////////////////////////////////////
// Vérification de la validité de l'heure                        //
///////////////////////////////////////////////////////////////////

void checkTimeSet (void) 
{
  time_t timestamp = time( NULL );
  isTimeTrue = (timestamp > TIME_REF) ? true : false;
}


void setWebHandlers (void) {

  // ***********************************************************************
  // ********      DECLARATIONS DES HANDLERS DU SERVEUR WEB         ********
  // ***********************************************************************

  webServer.onNotFound([](AsyncWebServerRequest * request)
  {
    request->redirect( F("/") );
  });

  // Les pages du site Web sont intégrées au firmware (web_files.h)
  // Le filesystem ne contient plus que la configuration de MaxPV!
  webServer.on("/", HTTP_ANY, [](AsyncWebServerRequest * request)
  {
    sendWebFile ( request, findWebFile ( "/main.html" ) );
  });

  for (size_t i = 0; i < WEB_FILES_COUNT; i++) {
    const webFile* file = &webFiles[i];
    webServer.on(file->path, HTTP_ANY, [file](AsyncWebServerRequest * request)
    {
      sendWebFile ( request, file );
    });
  }

  // ***********************************************************************
  // ********          HANDLERS DE LA MISE A JOUR DU FIRMWARE       ********
  // ***********************************************************************

  // Seul le firmware peut être mis à jour : le filesystem, qui contient
  // la configuration de MaxPV!, n'est jamais réécrit.
  // Une image filesystem est refusée par Update qui vérifie l'en-tête du firmware
  webServer.on("/update", HTTP_GET, [](AsyncWebServerRequest * request)
  {
    sendWebFile ( request, findWebFile ( "/maj.html" ) );
  });

  webServer.on("/update", HTTP_POST, [](AsyncWebServerRequest * request)
  {
    // Appelé une fois l'envoi du fichier terminé
    AsyncWebServerResponse *response;
    if ( otaSuccess ) {
      response = request->beginResponse ( 200, "text/plain", F("OK") );
      logMqtt ( F("[ESP]"), F("Mise à jour du firmware réussie") );
      rebootCause = EVT_BOOT_OTA;
      shouldReboot = true;
    }
    else {
      String msg = Update.hasError() ? Update.getErrorString() : String ( F("envoi incomplet") );
      response = request->beginResponse ( 500, "text/plain", msg );
      logMqtt ( F("[ESP]"), F("Echec de la mise à jour du firmware") );
    }
    response->addHeader ( F("Connection"), F("close") );
    request->send ( response );
  },
  [](AsyncWebServerRequest * request, String filename, size_t index, uint8_t *data, size_t len, bool final)
  {
    // Réception du firmware par morceaux
    if ( index == 0 ) {
      otaSuccess = false;
      if ( Update.isRunning() ) Update.end();   // envoi précédent interrompu
      Update.runAsync ( true );
      uint32_t maxSketchSpace = ( ESP.getFreeSketchSpace() - 0x1000 ) & 0xFFFFF000;
      if ( Update.begin ( maxSketchSpace, U_FLASH ) ) {
        // Le MD5 doit être fourni après begin() qui le réinitialise
        if ( request->hasParam ( F("MD5"), true ) ) Update.setMD5 ( request->getParam ( F("MD5"), true )->value().c_str() );
        logMqtt ( F("[ESP]"), F("Mise à jour du firmware en cours") );
      }
    }
    if ( Update.isRunning() && len ) Update.write ( data, len );
    if ( final && Update.isRunning() ) otaSuccess = Update.end ( true );
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
    else if ( request->hasParam ( F("rebootesp") ) ) {
      rebootCause = EVT_BOOT_WEB;
      shouldReboot = true;
    }
    else if ( request->hasParam ( F("booston") ) )  boostON ( EVT_SRC_WEB );
    else if ( request->hasParam ( F("boostoff") ) ) boostOFF ( EVT_SRC_WEB );
    else response = F("Unknown request");
    request->send ( 200, "text/plain", response );
  });

  webServer.on("/api/get", HTTP_GET, [](AsyncWebServerRequest * request)
  {
    String response = "";
    if ( request->hasParam ( F("configmaxpv") ) )
      request->send ( LittleFS, F("/config.json"), "text/plain" );
    else if ( request->hasParam ( F("versionweb") ) )
      request->send ( 200, "text/plain", F(WEB_VERSION) );
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
      else if ( request->hasParam ( F("relayplusstate") ) ) {
        if (relaisPlusIndex == -1) response = F("OFF");
          else response = F("ON");
      }
      else if ( request->hasParam ( F("ping") ) )
        if ( contactEcoPV ) response = F("running");
        else response = F("offline");
      else if ( request->hasParam ( F("time") ) ) {
        time_t timestamp = time(NULL);
        char buffer[32];
        struct tm *pTime = localtime(&timestamp );
        strftime(buffer, 32, "%H:%M:%S", pTime);
        response = String(buffer);
      }
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
      // on déacive relay+ s'il était activé
      relaisPlusIndex = -1;
    }
    else if ( ( request->hasParam ( F("ssrmode") ) ) && ( request->hasParam ( F("value") ) ) ) {
      mystring = request->getParam(F("value"))->value();
      mystring.trim();
      if ( mystring == F("stop") )        SSRModeEcoPV ( STOP, EVT_SRC_WEB );
      else if ( mystring == F("force") )  SSRModeEcoPV ( FORCE, EVT_SRC_WEB );
      else if ( mystring == F("auto") )   SSRModeEcoPV ( AUTOM, EVT_SRC_WEB );
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
    float perCentRelay;
    int localCounter;
    int lastLocalCounter;
    unsigned long timePowerFactor = 60000UL / HISTORY_INTERVAL;   // attention : HISTORY_INTERVAL sous-multiple de 60 en minutes
    // 60000 = 60 minutes par heure * 1000 Wh par kWh
    float timeRelayFactor = 100.0 / HISTORY_INTERVAL;   // attention : HISTORY_INTERVAL sous-multiple de 60 en minutes
    // 100 = 100% de temps de fonctionnement

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
    else if ( request->hasParam ( F("relay") ) )
    {
      response->print(F("Time,Relais secondaire\r\n"));
      for (int i = 1; i < HISTORY_RECORD; i++) {
        localCounter = (historyCounter + i) % HISTORY_RECORD;
        lastLocalCounter = ((localCounter + HISTORY_RECORD - 1) % HISTORY_RECORD);
        if ( ( energyIndexHistoric[localCounter].time != 0 ) && ( (energyIndexHistoric[lastLocalCounter].time) != 0 ) ) {
          timeStamp = String (energyIndexHistoric[localCounter].time);
          timeStamp += F("000");
          perCentRelay = (energyIndexHistoric[localCounter].tRelayOn - energyIndexHistoric[lastLocalCounter].tRelayOn) * timeRelayFactor;
          response->printf("%s,%.1f\r\n", timeStamp.c_str(), perCentRelay);
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
  // ********             HANDLER DU JOURNAL DES EVENEMENTS         ********
  // ***********************************************************************

  // Evénements du plus récent au plus ancien
  // La réponse est générée par morceaux, un événement à la fois, pour ne pas
  // construire tout le journal en mémoire
  webServer.on("/api/events", HTTP_GET, [](AsyncWebServerRequest * request)
  {
    struct eventsStream
    {
      uint32_t lastId;    // id du plus récent événement au début de la requête
      uint32_t count;     // nombre d'événements à transmettre
      uint32_t next;      // nombre d'événements déjà traités
      bool     first;     // aucun événement encore transmis
      bool     done;      // fin du JSON transmise
      String   pending;   // morceau en cours de transmission
      size_t   offset;    // position dans le morceau en cours
    };
    std::shared_ptr<eventsStream> state = std::make_shared<eventsStream>();
    time_t timestamp = time(NULL);

    state->lastId = eventCounter;
    state->count  = ( eventCounter < EVENT_LOG_SIZE ) ? eventCounter : EVENT_LOG_SIZE;
    state->next   = 0;
    state->first  = true;
    state->done   = false;
    state->offset = 0;
    state->pending = F("{\"time\":");
    state->pending += ( timestamp > TIME_REF ) ? uint32_t (timestamp) : 0;
    state->pending += F(",\"uptime\":");
    state->pending += uptimeSeconds();
    state->pending += F(",\"total\":");
    state->pending += eventCounter;
    state->pending += F(",\"size\":");
    state->pending += EVENT_LOG_SIZE;
    state->pending += F(",\"events\":[");

    AsyncWebServerResponse *response = request->beginChunkedResponse("application/json",
      [state](uint8_t *buffer, size_t maxLen, size_t index) -> size_t
    {
      size_t written = 0;
      while ( written < maxLen ) {
        if ( state->offset >= state->pending.length() ) {
          // Préparation du morceau suivant
          state->pending = "";
          state->offset = 0;
          if ( state->next < state->count ) {
            uint32_t id = state->lastId - state->next;
            const eventRecord& e = eventLog[(id - 1) % EVENT_LOG_SIZE];
            state->next++;
            // un événement écrasé pendant la transmission est ignoré
            if ( e.id != id ) continue;
            if ( !state->first ) state->pending = F(",");
            state->pending += eventToJson(e);
            state->first = false;
          }
          else if ( !state->done ) {
            state->pending = F("]}");
            state->done = true;
          }
          else break;
        }
        size_t n = std::min ( maxLen - written, size_t ( state->pending.length() - state->offset ) );
        memcpy ( buffer + written, state->pending.c_str() + state->offset, n );
        written += n;
        state->offset += n;
      }
      return written;
    });
    request->send(response);
  });
}
