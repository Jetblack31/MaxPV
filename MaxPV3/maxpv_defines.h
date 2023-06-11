/*
  This file is a part of MaxPV_ESP.ino - ESP8266 program that provides a web interface and a API for EcoPV 3+
  Copyright (C) 2022 - Bernard Legrand.
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

// ***********************************************************************************
// ****************************   Définitions générales   ****************************
// ***********************************************************************************

// Version MaxPV!
#define MAXPV_VERSION      "3.57"
#define MAXPV_VERSION_FULL "MaxPV! 3.57"

// SSID pour le Config Portal
#define SSID_CP            "MaxPV"

// Communication série de débug pendant la phase initiale de démarrage
#define SERIAL_LOG_BAUD   115200 // Vitesse de la liaison port série pour les logs de démarrage
#define SERIAL_LOG_ENABLE false   // Log sur liaison série : true ou false

#define MQTT_LOG_ENABLE   true    // Log sur la connexion MQTT : true ou false

// Communication série entre l'arduino et l'ESP 8266
#define SERIAL_BAUD     500000 // Vitesse de la liaison port série pour la connexion avec l'arduino
#define SERIALTIMEOUT   100    // Timeout pour les interrogations sur liaison série en ms
#define SERIAL_BUFFER   256    // Taille du buffer RX pour la connexion avec l'arduino (256 max)
#define END_OF_TRANSMIT '#'    // Caractère de fin de transmission de l'Arduino

// Communications TCP, ports 
#define MAXPV_MDNS      "maxpv"         // mDNS pour accès local
#define HTTP_PORT       80              // Port serveur web MaxPV!

// Taille de la configuration JSON MaxPV! pour la manipulation
#define JSON_CONFIG_SIZE    1100        // en caractères

// Valeurs par défaut de la configuration TCP MaxPV!
#define DEFAULT_IP          "192.168.1.250"
#define DEFAULT_GW          "192.168.1.1"
#define DEFAULT_SN          "255.255.255.0"
#define DEFAULT_DNS1        "192.168.1.1"
#define DEFAULT_DNS2        "8.8.8.8"

#define PING_WIFI_TIMEOUT    600000     // Délai en ms où on considère un problème de 
                                        // connexion wifi qui active le watchdog wifi

// Login et password pour le service FTP si activé
#define LOGIN_FTP           "maxpv"
#define PWD_FTP             "maxpv"

#define WELCOME_MESSAGE     "MaxPV! par Bernard Legrand (2022)"

// NTP
// Décalage de fuseau horaire par rapport à UTC / GMT. 0 = heure solaire française
#define GMT_OFFSET          0 
#define NTP_SERVER          "europe.pool.ntp.org"
#define NTP_UPDATE_INTERVAL 1800000

#define DEFAULT_MQTT_SERVER     "192.168.1.100" // Serveur MQTT par défaut
#define DEFAULT_MQTT_PORT       1883            // Port serveur MQTT
#define RECONNECT_TIME          5               // Délai de reconnexion en secondes suite à perte de connexion du serveur mqtt

#define DEFAULT_REMOTE_RELAY_SERVER     "192.168.1.200"   // Serveur du relais distant
#define DEFAULT_REMOTE_RELAY_PORT       80                // Port serveur relais distant
#define DEFAULT_REMOTE_RELAY_CMD_ON     "/relay0/cmd/1"   // Chemin de requête pour relais on
#define DEFAULT_REMOTE_RELAY_CMD_OFF    "/relay0/cmd/0"   // Chemin de requête pour relais off
#define REMOTE_RELAY_MIRRORING_PERIOD   20                // Période de vérification de l'état du relais et de recopie de l'état
                                                          // minimum 15 secondes

#define DEFAUT_EMPTY_USER  ""
#define DEFAUT_EMPTY_PWD   ""
#define DEFAUT_USER_LENGTH 39
#define DEFAUT_PWD_LENGTH  39
#define DEFAUT_PATH_CMD    64

// Mode BOOST
#define DEFAULT_BOOST_RATIO      100 // Ratio des burst 0..100
#define DEFAULT_BOOST_DURATION   120 // Durée mode boost en minutes
#define DEFAULT_BOOST_HOUR       4   // Heure de déclenchement mode boost programmé
#define DEFAULT_BOOST_MINUTE     0   // Minute de déclenchement mode boost programmé

// Fonction RelayPlus
#define DEFAULT_RELAYPLUS_MIN     60  // Temps minimum de fonctionnement
#define DEFAULT_RELAYPLUS_MAX    480  // Temps maximum de fonctionnement
#define DEFAULT_RELAYPLUS_HOUR    21  // Heure de référence pour les calculs

// Historisation des index
#define HISTORY_INTERVAL  30  // Périodicité en minutes de l'enregistrement des index d'énergie pour l'historisation
                              // Valeurs permises : 1, 2, 3, 4, 5, 6, 10, 12, 15, 20, 30, 60, 120, 180, 240, 480
#define HISTORY_RECORD    193 // Nombre de points dans l'historique
                              // Attention à la taille totale de l'historique en mémoire
                              // Et la capacité d'export en CSV


// définition de l'ordre des paramètres de configuration de EcoPV tels que transmis
// et de l'index de rangement dans le tableau de stockage (début à la position 1)
// on utilise la position 0 pour stocker la version
#define NB_PARAM      17    // Nombre de paramètres transmis par EcoPV (17 = 16 + VERSION)
#define ECOPV_VERSION 0
#define V_CALIB       1
#define P_CALIB       2
#define PHASE_CALIB   3
#define P_OFFSET      4
#define P_RESISTANCE  5
#define P_MARGIN      6
#define GAIN_P        7
#define GAIN_I        8
#define E_RESERVE     9
#define P_DIV2_ACTIVE 10
#define P_DIV2_IDLE   11
#define T_DIV2_ON     12
#define T_DIV2_OFF    13
#define T_DIV2_TC     14
#define CNT_CALIB     15
#define P_INSTALLPV   16


// définition de l'ordre des informations statistiques transmises par EcoPV
// et de l'index de rangement dans le tableau de stockage (début à la position 1)
// on utilise la position 0 pour stocker la version
// ATTENTION : dans le reste du programme les 4 index de début de journée sont ajoutés à la suite
// pour les informations disponibles par l'API
// ils doivent toujours être situés en toute fin de tableau
#define NB_STATS        24     // Nombre d'informations statistiques transmis par EcoPV (23 = 22 + VERSION)
#define NB_STATS_SUPP   5      // Nombre d'informations statistiques supplémentaires
//#define ECOPV_VERSION 0
#define V_RMS           1
#define I_RMS           2
#define P_ACT           3
#define P_APP           4
#define P_ROUTED        5
#define P_IMP           6
#define P_EXP           7
#define COS_PHI         8
#define INDEX_ROUTED    9
#define INDEX_IMPORT    10
#define INDEX_EXPORT    11
#define INDEX_IMPULSION 12
#define P_IMPULSION     13
#define TRIAC_MODE      14
#define RELAY_MODE      15
#define DELAY_MIN       16
#define DELAY_AVG       17
#define DELAY_MAX       18
#define BIAS_OFFSET     19
#define STATUS_BYTE     20
#define ONTIME          21
#define SAMPLES         22
#define INDEX_RELAY     23
// Informations supplémentaires ajoutées
#define INDEX_ROUTED_J  24
#define INDEX_IMPORT_J  25
#define INDEX_EXPORT_J  26
#define INDEX_IMPULSION_J 27
#define INDEX_RELAY_J     28


// Définition des topics MQTT
#define DEFAULT_MQTT_PUBLISH_PERIOD   10     // en secondes, intervalle de publication MQTT

// Topics utilisés pour HomeAssistant
#define MQTT_STATE          "maxpv/state"
#define MQTT_V_RMS          "maxpv/vrms"
#define MQTT_I_RMS          "maxpv/irms"
#define MQTT_P_ACT          "maxpv/pact"
#define MQTT_P_APP          "maxpv/papp"
#define MQTT_P_ROUTED       "maxpv/prouted"
#define MQTT_P_IMPULSION    "maxpv/pimpulsion"
#define MQTT_COS_PHI        "maxpv/cosphi"
#define MQTT_INDEX_ROUTED   "maxpv/indexrouted"
#define MQTT_INDEX_IMPORT   "maxpv/indeximport"
#define MQTT_INDEX_EXPORT   "maxpv/indexexport"
#define MQTT_INDEX_IMPULSION "maxpv/indeximpulsion"
#define MQTT_INDEX_RELAY     "maxpv/indexrelay"
#define MQTT_TRIAC_MODE     "maxpv/triacmode"
#define MQTT_SET_TRIAC_MODE "maxpv/triacmode/set"
#define MQTT_RELAY_MODE     "maxpv/relaymode"
#define MQTT_SET_RELAY_MODE "maxpv/relaymode/set"
#define MQTT_BOOST_MODE     "maxpv/boost"
#define MQTT_SET_BOOST_MODE "maxpv/boost/set"
#define MQTT_STATUS_BYTE    "maxpv/statusbyte"

// Topics utilisés pour la transmission des informations du système et le debug en fonctionnement
#define MQTT_SYS_FREE_HEAP      "maxpv/SYS/freeHeap"
#define MQTT_SYS_FRAG           "maxpv/SYS/heapFragmentation"
#define MQTT_SYS_MAX_FREE       "maxpv/SYS/maxFreeBlockSize"
#define MQTT_SYS_CPU_FREQ       "maxpv/SYS/cpuFrequency"
#define MQTT_SYS_RESET_REASON   "maxpv/SYS/lastResetReason"
#define MQTT_LOGGER             "maxpv/SYS/logger"
