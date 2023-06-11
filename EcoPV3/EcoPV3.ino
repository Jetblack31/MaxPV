/*
  EcoPV3.ino - Arduino program that maximizes the use of home photovoltaïc production
  by monitoring energy consumption and diverting power to a resistive charge
  when needed.
  Copyright (C) 2019 - Bernard Legrand and Mickaël Lefebvre.
  Copyright (C) 2023 - Bernard Legrand.
  maxpv@bernard-legrand.net
  https://github.com/Jetblack31/EcoPV
  https://github.com/Jetblack31/MaxPV

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

  Version 3.x: management interface removed from Arduino and integrated to ESP
  via a Serial Protocol to exchange data

*/

/*************************************************************************************
**                                                                                  **
**        Ce programme fonctionne sur ATMEGA 328P @ VCC = 5 V et clock 16 MHz       **
**        comme l'Arduino Uno et l'Arduino Nano                                     **
**        La compilation s'effectue avec l'IDE Arduino                              **
**        Site Arduino : https://www.arduino.cc                                     **
**                                                                                  **
**************************************************************************************/

// ***********************************************************************************
// ******************            OPTIONS DE COMPILATION                ***************
// ***********************************************************************************

// ***********************************************************************************
// ****************** Dé-commenter pour activer l'affichage            ***************
// ****************** des données sur écran oled 128 x 64  I2C         ***************
// ***********************************************************************************

//#define OLED_128X64

// *** Note : l'écran utilise la connexion I2C
// *** sur les pins A4 (SDA) et A5 (SCK)
// *** Ces pins ne doivent pas être utilisées comme entrées analogiques
// *** si OLED_128X64 est activé.
// *** La bibliothèque SSD1306Ascii doit être installée dans l'IDE Arduino.
// *** Voir les définitions de configuration OLED_128X64
// *** dans la suite des déclarations, en particulier l'adresse de l'écran
// *** Note : ne pas oublier de placer un résistance de pull-up de 10 kohms
// *** sur les lignes SDA et SCK


// ***********************************************************************************
// ****************** Configuration de l'accumulation                  ***************
// ****************** de l'énergie routée                              ***************
// ***********************************************************************************

#define ACC_FORCE_ENERGY    true

// *** ACC_FORCE_ENERGY peut être true ou false 
//     false : le compteur d'énergie routée accumule uniquement l'énergie routée en cas
//     de surplus de production PV
//     true : le compteur d'énergie routée accumule l'énergie délivrée par le SSR
//     en cas de routage d'excédent PV, en mode FORCE et en mode BOOST


// ***********************************************************************************
// ******************        FIN DES OPTIONS DE COMPILATION            ***************
// ***********************************************************************************


// ***********************************************************************************
// ************************    DEFINITIONS ET DECLARATIONS     ***********************
// ***********************************************************************************

// ***********************************************************************************
// ****************************   Définitions générales   ****************************
// ***********************************************************************************

#define VERSION          "3.57"       // Version logicielle
#define SERIAL_BAUD      500000       // Vitesse de la liaison port série
#define SERIALTIMEOUT       100       // Timeout pour les interrogations sur liaison série en ms

#define V_SECTEUR           230.0     // Tension secteur nominale
#define NB_CYCLES            50       // Nombre de cycles / s du secteur AC (50 ou 60 Hz) : 50 ou 60
#define SAMP_PER_CYCLE      166       // Nombre d'échantillons I,V attendu par cycle : 166 sur ATMEGA 328P @ VCC = 5 V et clock 16 MHz
// Dépend de la configuration de l'ADC dans le programme
#define WH_PER_INC            5       // Nombre de Wh par incrément pour le stockage 3 compteurs d'énergie (Rout, Imp, Exp)

#define BOOST_BURST_PERIOD  300       // Durée de la période du cycle PWM mode BOOST en secondes

// ***********************************************************************************
// ***************************   Définitions utilitaires   ***************************
// ***********************************************************************************

#define OFF                   0
#define ON                    1
#define POSITIVE           true
#define STOP                  0
#define FORCE                 1
#define AUTOM                 9

// ***********************************************************************************
// *********************   Définition des modes au démarrage   ***********************
// ***********************************************************************************

//                ***   Mode de fonctionnement au démarrage du SSR / TRIAC   ***
#define triacModePowerON      AUTOM     // AUTOM, STOP, FORCE
//             ***   Mode de fonctionnement au démarrage du relais secondaire   ***
#define relayModePowerON      AUTOM    // AUTOM, STOP, FORCE

// ***********************************************************************************
// ********************   Définition des pins d'entrée/sortie    *********************
// ***********************************************************************************

//                          ***   I/O analogiques   ***
#define voltageSensorMUX       3    // IN ANALOG   = PIN A3, lecture tension V sur ADMUX 3
#define currentSensorMUX       0    // IN ANALOG   = PIN A0, lecture courant I sur ADMUX 0
// ATTENTION PIN A4 et A5 incompatibles avec activation de OLED_128X64

//                          ***   I/O digitales     ***
#define pulseExternalPin       2    // IN DIGITAL  = PIN D2, Interrupt sur INT0, signal d'impulsion externe (falling)
// NON MODIFIABLE
#define synchroACPin           3    // IN DIGITAL  = PIN D3, Interrupt sur INT1, signal de détection du passage par zéro (toggle)
// NON MODIFIBALE

#define synchroOutPin          4    // OUT DIGITAL = PIN D4, indique un passage par zéro détecté par l'ADC (toggle)
// PORT D EXCLUSIVEMENT
#define pulseTriacPin          5    // OUT DIGITAL = PIN D5, commande Triac/Solid State Relay
// PORT D EXCLUSIVEMENT

#define ledPinStatus           6    // OUT DIGITAL = PIN D6, LED de signalisation fonctionnement / erreur
#define ledPinRouting          7    // OUT DIGITAL = PIN D7, LED de signalisation routage de puissance
#define relayPin               8    // OUT DIGITAL = PIN D8, commande On/Off du relais de délestage secondaire

//                   **************************************************
//                   **********   A  T  T  E  N  T  I  O  N   *********
//                   **************************************************
// A4 et A5 constituent le port I2C qui est utilisé pour l'affichage écran
// Si OLED_128X64 est activé, ne pas utiliser A4 et A5 comme entrées analogiques ADC pour I ou V

// D0 et D1 sont utilisés par la liaison série de l'Arduino et pour sa programmation >> ne pas affecter à un autre usage
// D2 est l'entrée d'impulsion externe INT0 et doit être absolument affecté à pulseExternalPin
// D3 est l'entrée d'interruption INT1 et doit être absolument affecté à synchroACPin

// !! Choisir impérativement synchroOutPin et pulseTriacPin parmi D4, D5, D6, D7 (port D) !!

//                   **************************************************
//                   **********          N  O  T  E           *********
//                   **************************************************
// *** Fonction d'autotrigger de la détection du passage par zéro
// *** en reliant physiquement synchroACPin IN et synchroOutPin OUT
// *** PAR DEFAUT : RELIER ELECTRIQUEMENT D3 ET D4


// ***********************************************************************************
// ***************   Définition de macros de manipulation des pins   *****************
// ***************   pour la modification rapide des états           *****************
// ***********************************************************************************
// *** Définitions valables pour le PORTD, pins possibles :
// *** 0 = PIN D0 à 7 = PIN D7
// *** Cela concerne les pins pulseTriacPin et synchroOutPin
// *** Utilisation dans les interruptions pour réduire le temps de traitement
#define TRIAC_ON               PORTD |=  ( 1 << pulseTriacPin )
#define TRIAC_OFF              PORTD &= ~( 1 << pulseTriacPin )
#define SYNC_ON                PORTD |=  ( 1 << synchroOutPin )
#define SYNC_OFF               PORTD &= ~( 1 << synchroOutPin )


// ***********************************************************************************
// ************************ DECLARATION DES VARIABLES GLOBALES ***********************
// ***********************************************************************************

// ***********************************************************************************
// ************* Définition des paramètres du système (valeurs par défaut) ***********
// ************* NOTE : Ces valeurs seront remplacées automatiquement      ***********
// ************* par les valeurs lues en EEPROM si celles-ci sont valides  ***********
// ***********************************************************************************

// ************* Calibrage du système
float V_CALIB      =    0.800;        // Valeur de calibration de la tension du secteur lue (Volt par bit)
// 0.800 = valeur par défaut pour Vcc = 5 V
float P_CALIB      =    0.111;        // Valeur de calibration de la puissance (VA par bit)
// Implicitement I_CALIB = P_CALIB / V _CALIB
int   PHASE_CALIB  =       13;        // Valeur de correction de la phase (retard) de l'acquisition de la tension
// Entre 0 et 32 :
// 16 = pas de correction
// 0  = application d'un retard = temps de conversion ADC
// 32 = application d'une avance = temps de conversion ADC
int   P_OFFSET     =      -15;        // Correction d'offset de la lecture de Pactive en Watt
int   P_RESISTANCE =     2000;        // Valeur en Watt de la résistance contrôlée

// ************* Paramètres de régulation du routeur de puissance (valeurs par défaut)
int   P_MARGIN     =       10;        // Cible de puissance importée en Watt
int   GAIN_P       =        8;        // Gain proportionnel du correcteur
int   GAIN_I       =       45;        // Gain intégral du correcteur
byte  E_RESERVE    =        5;        // Réserve d'énergie en Joule avant régulation

// ************* Paramètres de délestage secondaire de puissance (DIV2)
// ************* Fonctionnement pour un relais en tout ou rien (valeurs par défaut)
int   P_DIV2_ACTIVE =    1000;         // Valeur de puissance routée en Watt qui déclenche le relais de délestage
int   P_DIV2_IDLE   =     200;         // Puissance active importée en Watt qui désactive le relais de délestage
byte  T_DIV2_ON     =       5;         // Durée minimale d'activation du délestage en minutes
byte  T_DIV2_OFF    =       5;         // Durée minimale d'arrêt du délestage en minutes
byte  T_DIV2_TC     =       1;         // Constante de temps de moyennage des puissance routées et active en minutes
// NOTE : Il faut une condition d'hystérésis pour un bon fonctionnement :
// P_DIV2_ACTIVE + P_DIV2_IDLE > à la puissance de la charge de délestage secondaire

// ************* Calibration du compteur à impulsion | conversion nombre de Wh par impulsion (valeur par défaut)
float CNT_CALIB     =       1.0;       // En Wh par impulsion externe

// ************* Puissance de l'installation PV (valeurs par défaut)
int   P_INSTALLPV   =    3000;         // Valeur en Wc de la puissance de l'installation PV | production max (valeur par défaut)

// ***********************************************************************************************************************
// energyToDelay [ ] = Tableau du retard de déclenchement du SSR/TRIAC en fonction du routage de puissance désiré.
// 256 valeurs codées pour envoyer linéairement 0 (0) à 100 % (255) de puissance vers la résistance.
// Délais par pas de 64 us (= pas de comptage de CNT1) calculés pour une tension secteur 50 Hz.
// Les valeurs de DELAY_MIN (8) et de DELAY_MAX (140) sont fixées :
// Soit pour la fiabilité du déclenchement, soit parce que l'energie routée sera trop petite.
// La pente de la montée de la tension secteur à l'origine est de 72 V / ms, une demi-alternance fait 10 ms.
// Pour DELAY_MIN = 8 par pas de 64 us, soit 512 us, le secteur a atteint 36 V, le triac peut se déclencher correctement.
// On ne déclenche plus au delà de DELAY_MAX = 144 par pas de 64 us, soit 9.2 ms, pour la fiabilité du déclenchement
// DELAY_MAX doit être inférieur à PULSE_END qui correspond à l'instant où on arrêtera le pulse de déclenchement SSR/TRIAC
// ***********************************************************************************************************************
byte energyToDelay [ ] = {
  144, 143, 141, 140, 139, 137, 136, 135, 134, 133, 132, 131, 130, 129, 128, 128,
  127, 127, 126, 125, 124, 123, 123, 122, 122, 121, 121, 120, 119, 119, 118, 118,
  117, 117, 116, 116, 115, 115, 114, 114, 114, 113, 113, 112, 112, 112, 111, 111,
  110, 110, 109, 109, 109, 108, 108, 108, 107, 107, 107, 106, 106, 106, 105, 105,
  104, 104, 104, 103, 103, 103, 102, 102, 102, 101, 101, 101, 100, 100, 100,  99,
  99,  99,  99,  98,  98,  98,  97,  97,  97,  96,  96,  95,  95,  95,  95,  94,
  94,  94,  93,  93,  93,  93,  92,  92,  91,  91,  91,  91,  90,  90,  90,  89,
  89,  89,  88,  88,  88,  87,  87,  87,  86,  86,  86,  85,  85,  84,  84,  84,
  84,  83,  83,  83,  82,  82,  82,  82,  81,  81,  80,  80,  80,  80,  79,  79,
  79,  78,  78,  78,  77,  77,  77,  76,  76,  76,  75,  75,  74,  74,  74,  73,
  73,  73,  72,  72,  72,  71,  71,  71,  70,  70,  70,  69,  69,  69,  69,  68,
  68,  67,  67,  67,  66,  66,  65,  65,  65,  64,  64,  63,  63,  63,  62,  62,
  62,  61,  61,  61,  60,  60,  60,  59,  59,  58,  58,  58,  57,  57,  56,  56,
  56,  55,  55,  54,  54,  53,  53,  52,  52,  52,  51,  51,  50,  50,  50,  49,
  49,  48,  48,  47,  46,  45,  44,  44,  43,  43,  42,  42,  41,  41,  40,  39,
  39,  38,  37,  36,  35,  33,  32,  31,  27,  25,  20,  15,  12,  10,   9,   8
};

#define PULSE_END          148          // Instant d'arrêt du pulse triac après le passage à 0
// par pas de 64 us (9.5 ms = 148). Valeur pour secteur 50 Hz


// ***********************************************************************************
// *********  Définition de la structure de la configuration du routeur     **********
// ***********************************************************************************
#define NB_PARAM            16          // Nombre de paramètres dans la configuration, 14 en EEPROM V1, 16 en V2

struct paramInConfig {                  // Structure pour la manipulation des données de configuration
  byte dataType;                        // 0 : int, 1 : float, 4 : byte
  int  minValue;                        // valeur minimale que peut prendre le paramètre (attention au type !)
  int  maxValue;                        // valeur maximale que peut prendre le paramètre (attention au type !)
  void *adr;                            // pointeur vers le paramètre
};

const paramInConfig pvrParamConfig [ ] = {
  // Tableau utilisé pour la manipulation des données de configuration
  // dataType min      max     adr
  { 1,        0,       2,   &V_CALIB        },       // V_CALIB
  { 1,        0,       1,   &P_CALIB        },       // P_CALIB
  { 0,      -16,      48,   &PHASE_CALIB    },       // PHASE_CALIB
  { 0,     -100,     100,   &P_OFFSET       },       // P_OFFSET
  { 0,      100,   10000,   &P_RESISTANCE   },       // P_RESISTANCE
  { 0,    -2000,    2000,   &P_MARGIN       },       // P_MARGIN
  { 0,        0,    1000,   &GAIN_P         },       // GAIN_P
  { 0,        0,    1000,   &GAIN_I         },       // GAIN_I
  { 4,        0,     200,   &E_RESERVE      },       // E_RESERVE
  { 0,        0,    9999,   &P_DIV2_ACTIVE  },       // P_DIV2_ACTIVE
  { 0,        0,    9999,   &P_DIV2_IDLE    },       // P_DIV2_IDLE
  { 4,        0,     240,   &T_DIV2_ON      },       // T_DIV2_ON
  { 4,        0,     240,   &T_DIV2_OFF     },       // T_DIV2_OFF
  { 4,        0,      60,   &T_DIV2_TC      },       // T_DIV2_TC
  { 1,        0,    1000,   &CNT_CALIB      },       // CNT_CALIB
  { 0,      100,   30000,   &P_INSTALLPV    }        // P_INSTALLPV
};


// ***********************************************************************************
// ************* Variables globales pour le fonctionnement du régulateur   ***********
// ************* Ces variables permettent de communiquer les informations  ***********
// ************* entre les routines d'interruption                         ***********
// ***********************************************************************************
volatile int     biasOffset    = 511;       // pour réguler le point milieu des acquisitions ADC,
// attendu autour de 511
volatile long    periodP       =   0;       // Samples de puissance accumulés
// sur un demi-cycle secteur (période de la puissance)
#define          NCSTART           5
volatile byte    coldStart     =   NCSTART; // Indicateur de passage à 0 après le démarrage
// Atteint la valeur 0 une fois le warm-up terminé
// Attente de NCSTART passage à 0 avant de démarrer la régulation

// ***********************************************************************************
// ************* Variables globales utilisées pour les calcul              ***********
// ************* des statistiques de fonctionnement                        ***********
// ************* Ces variables permettent de communiquer les informations  ***********
// ************* entre les routines d'interruption                         ***********
// ***********************************************************************************
volatile long          sumP           = 0;
volatile unsigned long PVRClock       = 0;  // horloge interne par pas de 20 ms
volatile unsigned long sumV           = 0;
volatile unsigned long sumI           = 0;
volatile unsigned long sumVsqr        = 0;
volatile unsigned long sumIsqr        = 0;
volatile unsigned int  routed_power   = 0;
volatile unsigned int  samples        = 0;
volatile byte          error_status   = 0;
// Signification des bits du byte error_status
// bits 0..3 : informations
// bit 0 (1)   : Routage en cours
// bit 1 (2)   : Commande de routage à 100 %
// bit 2 (4)   : Relais secondaire de délestage activé
// bit 3 (8)   : Exportation d'énergie
// bits 4..7 : erreurs
// bit 4 (16)  : Anomalie signaux analogiques : ADC I/V overflow, biasOffset
// bit 5 (32)  : Anomalie taux d'acquisition
// bit 6 (64)  : Anomalie furtive Détection passage à 0 (bruit sur le signal)
// bit 7 (128) : Anomalie majeure Détection passage à 0 (sur 2 secondes de comptage)

// ***********************************************************************************
// ************* Variables utilisées pour le transfert des statistiques  *************
// ************* de fonctionnement                                       *************
// ************* des routines d'interruption vers le traitement          *************
// ************* dans la LOOP tous les NB_CYCLES                         *************
// ***********************************************************************************
volatile long          stats_sumP         = 0;   // Somme des échantillons de puissance
volatile unsigned long stats_sumVsqr      = 0;   // Somme des échantillons de tension au carré
volatile unsigned long stats_sumIsqr      = 0;   // Somme des échantillons de courant au carré
volatile long          stats_sumV         = 0;   // Somme des échantillons de tension
volatile long          stats_sumI         = 0;   // Somme des échantillons de courant
volatile unsigned int  stats_routed_power = 0;   // Evaluation de la puissance routée vers la charge
volatile unsigned int  stats_samples      = 0;   // Nombre d'échantillons total
volatile byte          stats_error_status = 0;
volatile int           stats_biasOffset   = 0;   // Valeur de la correction d'offset de lecture ADC
volatile byte          stats_ready_flag   = 0;
// 0 = Données traitées par la loop (), en attente de nouvelles données
// 1 = Nouvelles données disponibles ou en cours de traitement par la loop ()
// 9 = Données statistiques en cours de transfert

// ***********************************************************************************
// ************* Variables globales utilisées pour les statistiques      *************
// ************* de fonctionnement et d'information                      *************
// ************* Ces variables n'ont pas d'utilité directe               *************
// ************* pour la régulation du PV routeur                        *************
// ************* Elles ne sont pas utilisées dans les interruptions      *************
// ***********************************************************************************
float                  VCC_1BIT         = 0.0049;   // valeur du bit de l'ADC sous Vcc = 5 V
long                   indexRouted      = 0;        // compteur d'énergie en incrément
long                   indexImported    = 0;        // compteur d'énergie en incrément
long                   indexExported    = 0;        // compteur d'énergie en incrément
long                   indexImpulsion   = 0;        // compteur d'impulsions externes
long                   indexRelayOn     = 0;        // compteur de temps de fonctionnement du relais en minutes
volatile unsigned long deltaTimeImpulsion = 0;      // temps entre 2 impulsions externes

float                  Prouted          = 0;        // puissance routée en Watts
float                  Vrms             = 0;        // tension rms en V
float                  Irms             = 0;        // courant rms en A
float                  Papp             = 0;        // puissance apparente en VA
float                  Pact             = 0;        // puissance active en Watts
int                    Pimpulsion       = 0;        // puissance compteur impulsion en impulsion par heure
float                  cos_phi          = 0;        // cosinus phi

byte                   secondsOnline    = 0;        // horloge interne
byte                   minutesOnline    = 0;        // basée sur la période secteur
byte                   hoursOnline      = 0;        // et mise à jour à chaque fois que les données
byte                   daysOnline       = 0;        // statistiques sont disponibles

byte                   ledBlink         = 0;        // séquenceur de clignotement pour les LEDs, période T
// bit 0 (1)   : T = 40 ms
// bit 1 (2)   : T = 80 ms
// bit 2 (4)   : T = 160 ms
// bit 3 (8)   : T = 320 ms
// bit 4 (16)  : T = 640 ms
// bit 5 (32)  : T = 1280 ms
// bit 6 (64)  : T = 2560 ms
// bit 7 (128) : T = 5120 ms

byte                   triacMode        = triacModePowerON;    // mode de fonctionnement du triac/SSR
byte                   relayMode        = relayModePowerON;    // mode de fonctionnement du relais secondaire de délestage

bool                   hasRestarted     = false;    // flag indiquant que le routeur a redémarré                

// ***********************************************************************************
// ************* Variables pour la gestion du mode BOOST                 *************
// ***********************************************************************************

long                   boostTime = -1;         // Temps restant pour le mode BOOST, en secondes (-1 = arrêt)
int                    burstCnt  = 0;          // Compteur de la PWM software pour la gestion du mode BOOST entre 0 et BOOST_BURST_PERIOD
int                    burstThreshold  = 10;   // Instant de mise en route SSR dans la période burst pour le mode BOOST entre 0 et BOOST_BURST_PERIOD

// ***********************************************************************************
// ************* Variables pour la gestion du relais en mode Timer       *************
// ***********************************************************************************

long                   relayplusTime = -1;     // Temps restant pour le mode relay+, en secondes (-1 = arrêt)

// ***********************************************************************************
// ********  Définitions pour manipuler les données en EEPROM               **********
// ***********************************************************************************
#include <EEPROM.h>
#define          PVR_EEPROM_START             600   // Adresse de base des données PVR
#define          PVR_EEPROM_INDEX_ADR        1000   // Adresse de base des compteurs de kWh
#define          DATAEEPROM_MAGIC     384021670UL   // UID de signature de la configuration
#define          DATAEEPROM_VERSION             2   // Version du type de configuration

struct dataEeprom {                         // Structure des données pour le stockage en EEPROM
  unsigned long         magic;              // Magic Number
  byte                  struct_version;     // Structure version :DATAEEPROM_VERSION
  float                 v_calib;
  float                 p_calib;
  int                   phase_calib;
  int                   p_offset;
  int                   p_resistance;
  int                   p_margin;
  int                   gain_p;
  int                   gain_i;
  byte                  e_reserve;
  int                   p_div2_active;
  int                   p_div2_idle;
  byte                  t_div2_on;
  byte                  t_div2_off;
  byte                  t_div2_tc;
  // fin des données eeprom V1
  float                 cnt_calib;
  int                   p_installpv;
  // fin des données eeprom V2
  // taille totale : 39 bytes (byte = 1 byte, int = 2 bytes, long = float = 4 bytes)
};

struct indexEeprom {
  long                  routed;
  long                  imported;
  long                  exported;
  long                  impulsion;
  long                  relayon;
  // taille totale : 20 bytes
};

// ***********************************************************************************
// ****************** Définitions pour la communication OLED_128X64    ***************
// ***********************************************************************************
// *** OLED_128X64 utilise les pins A4 et A5
// *** pour la connexion I2C de l'écran
#if defined (OLED_128X64)
#include "SSD1306Ascii.h"
#include "SSD1306AsciiAvrI2c.h"

#define I2C_ADDRESS                   0x3C       // adresse I2C de l'écran oled
#define OLED_128X64_REFRESH_PERIOD       6       // période de raffraichissement des données à l'écran en secondes

SSD1306AsciiAvrI2c      oled;
#endif


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
void setup ( ) {

  // Initialisation des pins
  pinMode      ( pulseExternalPin, INPUT_PULLUP );  // Entrée de comptage impulsion externe
  pinMode      ( synchroACPin,  INPUT  );           // Entrée de synchronisation secteur
  pinMode      ( pulseTriacPin, OUTPUT );           // Commande Triac
  pinMode      ( synchroOutPin, OUTPUT );           // Détection passage par zéro émis par l'ADC
  pinMode      ( ledPinStatus,  OUTPUT );           // LED Statut
  pinMode      ( ledPinRouting, OUTPUT );           // LED Routage puissance
  pinMode      ( relayPin,      OUTPUT );           // Commande relais de délestage tout ou rien

  digitalWrite ( pulseTriacPin, OFF    );
  digitalWrite ( synchroOutPin, OFF    );
  digitalWrite ( ledPinStatus,  ON     );
  digitalWrite ( ledPinRouting, ON     );
  digitalWrite ( relayPin,      OFF    );

  // Le délai suivant de 1500 ms est important lors du reboot après programmation
  // pour éviter des problèmes d'écriture en EEPROM
  // + Clignotement des leds (power on)
  delay ( 500 );
  digitalWrite ( ledPinStatus,  OFF    );
  digitalWrite ( ledPinRouting, OFF    );
  delay ( 500 );
  digitalWrite ( ledPinStatus,  ON     );
  digitalWrite ( ledPinRouting, ON     );
  delay ( 500 );
  digitalWrite ( ledPinStatus,  OFF    );
  digitalWrite ( ledPinRouting, OFF    );
  // Fin du délai de 1500 ms

  // Activation de l'écran oled si défini
#if defined (OLED_128X64)
  oled.begin( &Adafruit128x64, I2C_ADDRESS );
  oled.setFont ( System5x7 );
  oLedPrint ( 9 );
#endif

  // Chargement de la configuration EEPROM si existante
  // Sinon écriture des valeurs par défaut en EEPROM
  if ( eeConfigRead ( ) == false ) eeConfigWrite ( );

  // Chargement des l'index d'énergie kWh
  indexRead ( );

  // Activation de la liaison série
  Serial.begin ( SERIAL_BAUD );
  while ( !Serial ) { };
  Serial.setTimeout ( SERIALTIMEOUT );

  // Séquence de démarrage du PV routeur
  startPVR ( );

#if defined (OLED_128X64)
  oLedPrint ( 10 );
#endif
}


///////////////////////////////////////////////////////////////////
// loop                                                          //
// Loop routine exécutée en boucle                               //
///////////////////////////////////////////////////////////////////
void loop ( ) {

#define BIASOFFSET_TOL       20       // Tolérance en bits sur la valeur de biasOffset par rapport
  // au point milieu 511 pour déclencher une remontée d'erreur
  static unsigned long refTime        = millis ( );
  static float         routedEnergy   = ( -3600.0 * WH_PER_INC ); // initialisation à valeur négative
  static float         exportedEnergy = ( -3600.0 * WH_PER_INC ); // Ca permet ensuite de faire la comparaison
  static float         importedEnergy = ( -3600.0 * WH_PER_INC ); // au passage à 0
  static int           relayOnSeconds = -60;

  static float         inv_NB_CYCLES  = 1 / float ( NB_CYCLES );
  static float         inv_255        = 1 / float ( 255 );
  static float         inv_V_SECTEUR_SQUARE  = 1 / float ( V_SECTEUR * V_SECTEUR );
  float                inv_stats_samples;

  float                Filter_param;
  float                Psurplus = 0;
  float                PestimatedProd = 0;
  static float         Pact_filtered = 0;
  static float         Prouted_filtered = 0;
  static unsigned int  Div2_On_cnt = 0;
  static unsigned int  Div2_Off_cnt = 0;

  unsigned int         OCR1A_tmp;
  byte                 OCR1A_byte;
  static byte          OCR1A_min = 255;
  static byte          OCR1A_max = 0;
  static unsigned long OCR1A_avg = 0;
  static unsigned long OCR1A_cnt = 0;

  long                 indexImpulsionTemp = 0;
  byte                 i = 0;

  // *** Vérification perte longue de synchronisation secteur
  if ( ( millis ( ) - refTime ) > 2010 ) {
    // Absence de remontée d'informations depuis plus de 2 secondes = absence de synchronisation secteur
    refTime = millis ( );
    noInterrupts ( );
    error_status |= B10000000;            // On reporte l'erreur majeure au système de régulation
    stats_error_status |= error_status;   // On transfère tous les bits de statut et d'erreur
    interrupts ( );
  }

  // *** Statistiques du délai de déclenchement du TRIAC / SSR
  // Note : information approximative basée sur la lecture du registre OCR1A
  noInterrupts ( );
  OCR1A_tmp = OCR1A;
  interrupts ( );
  if ( OCR1A_tmp < 256 ) {
    OCR1A_byte = byte ( OCR1A_tmp );
    OCR1A_avg += OCR1A_byte;
    OCR1A_cnt++;
    if ( OCR1A_byte > OCR1A_max ) OCR1A_max = OCR1A_byte;
    if ( OCR1A_byte < OCR1A_min ) OCR1A_min = OCR1A_byte;
  }

  // *** Traitement des informations statistiques lorsqu'elles sont disponibles
  // *** tous les NB_CYCLES sur flag, soit toutes les secondes pour NB_CYCLES = 50 @ 50 Hz
  if ( stats_ready_flag == 1 ) {          // Les données statistiques sont disponibles après NB_CYCLES
    refTime = millis ( );                 // Mise à jour du temps de passage dans la boucle

    // *** Vérification du routage SSR                                    ***
    if ( stats_routed_power > 0 )
      stats_error_status |= B00000001;
    else
      stats_error_status &= B11111110;

    // *** Vérification du routage pleine puissance                       ***
    if ( ( stats_routed_power / ( 2 * NB_CYCLES ) ) >= 245 )
      // Note : on teste la pleine puissance si on atteint 245 alors que le max possible est 255.
      // Ceci pour plus de fiabilité de la détection de la pleine puissance routée.
      stats_error_status |= B00000010;
    else
      stats_error_status &= B11111101;

    // *** Vérification du nombre de samples à +/- 2 %                    ***
    if ( ( stats_samples >= int ( NB_CYCLES + 1 ) * SAMP_PER_CYCLE )
         || ( stats_samples <= int ( NB_CYCLES - 1 ) * SAMP_PER_CYCLE ) )
      stats_error_status |= B00100000;

    // *** Calcul des valeurs statistiques en unités réelles              ***
    inv_stats_samples = 1 / float ( stats_samples );
    Vrms    = V_CALIB * sqrt ( stats_sumVsqr * inv_stats_samples );
    Papp    = P_CALIB * sqrt ( ( stats_sumVsqr * inv_stats_samples )
                               * ( stats_sumIsqr * inv_stats_samples ) );
    Pact    = - ( P_CALIB * stats_sumP * inv_stats_samples + P_OFFSET );
    // le signe - sur le calcul de Pact permet d'avoir Pact < 0 en exportation
    Prouted = float ( P_RESISTANCE ) * float ( stats_routed_power )
              * 0.5 * inv_NB_CYCLES * inv_255 * ( Vrms * Vrms * inv_V_SECTEUR_SQUARE );
    // Prouted prend en compte une correction en fonction de la tension secteur réelle (P % U^2/R)
    Irms    = Papp / Vrms;
    cos_phi = Pact / Papp;

    // *** Vérification de l'exportation d'énergie                        ***
    if ( Pact < 0 )
      stats_error_status |= B00001000;
    else
      stats_error_status &= B11110111;

    // *** Calcul des valeurs filtrées de Pact et Prouted                 ***
    // *** Usage : déclenchement du relais secondaire de délestage        ***
    Filter_param = 1 / float ( 1 + ( int ( T_DIV2_TC ) * 60 ) );
    Pact_filtered = Pact_filtered + Filter_param * ( Pact - Pact_filtered );
    Prouted_filtered = Prouted_filtered + Filter_param * ( Prouted - Prouted_filtered );

    // En mode TRIAC/SSR AUTOM, le surplus PV correspond à la puissance routée
    if ( triacMode == AUTOM ) {
      Psurplus = Prouted_filtered;
    }
    // En mode TRIAC/SSR ON ou OFF, le surplus PV correspond à - la puissance active
    else {
      Psurplus = -Pact_filtered;
      Prouted_filtered = 0;
    }

    // *** Déclenchement et gestion du relais secondaire de délestage     ***
    if ( relayMode == AUTOM ) {
      if ( ( Psurplus >= float ( P_DIV2_ACTIVE ) ) && ( Div2_Off_cnt == 0 ) && ( digitalRead ( relayPin ) == OFF ) ) {
        digitalWrite ( relayPin, ON );    // Activation du relais de délestage
        Div2_On_cnt = 60 * T_DIV2_ON;     // Initialisation de la durée de fonctionnement minimale en sevondes
      }
      else if ( Div2_On_cnt > 0 ) {
        Div2_On_cnt --;                   // décrément d'une seconde
      }
      else if ( ( Pact_filtered >= float ( P_DIV2_IDLE ) ) && ( digitalRead ( relayPin ) == ON ) ) {
        digitalWrite ( relayPin, OFF );   // Arrêt du délestage
        Div2_Off_cnt = 60 * T_DIV2_OFF;   // Initialisation de la durée d'arrêt minimale en secondes
      }
      else if ( Div2_Off_cnt > 0 ) {
        Div2_Off_cnt --;                  // décrément d'une seconde
      }
    }
    else {
      Div2_On_cnt = 0;
      Div2_Off_cnt = 0;
      digitalWrite ( relayPin, relayMode ); // Cas où le relais est en mode forcé STOP ou FORCE
    }                                                          

    // *** Vérification de l'état du relais de délestage                  ***
    if ( digitalRead ( relayPin ) == ON ) {
      stats_error_status |= B00000100;
      relayOnSeconds++;
    }
    else stats_error_status &= B11111011;


    // *** Correction de l'artefact de puissance routée maximale          ***
    // *** obtenue si la charge pilotée par le SSR se déconnecte.         ***
    // *** But : amélioration du comptage de l'énergie routée.            ***
    // *** Remarque 1, Ca ne couvre pas tous les cas de figure.           ***
    // *** Remarque 2, on ne fait pas la correction pour le pilotage du   ***
    // *** relais de délestage pour éviter l'exportation.                 ***
    
    if (Pimpulsion > 0) PestimatedProd = Pimpulsion * CNT_CALIB;
    else                PestimatedProd = P_INSTALLPV;
 
    if ( ( stats_error_status & B00000010 )            // Routage maximum vers la résistance
      && ( PestimatedProd + Pact <= P_RESISTANCE ) )   // Cas impossible : charge déconnectée
      Prouted = 0;

    // *** Accumulation des énergies routée, importée, exportée           ***
    // *** Les calculs sont faits toutes les secondes                     ***
    // *** La puissance x 1s = énergie en Joule                           ***
    if ( ( ACC_FORCE_ENERGY ) || ( triacMode == AUTOM ) ) routedEnergy += Prouted;
    if ( Pact < 0 )     exportedEnergy -= Pact;
    else                importedEnergy += Pact;

    if ( routedEnergy >= 0 )   { // On a accumulé (3600 * WH_PER_INC) J = WH_PER_INC Wh
      routedEnergy -= ( 3600.0 * WH_PER_INC );
      indexRouted ++;
    }
    if ( exportedEnergy >= 0 ) {
      exportedEnergy -= ( 3600.0 * WH_PER_INC );
      indexExported ++;
    }
    if ( importedEnergy >= 0 ) {
      importedEnergy -= ( 3600.0 * WH_PER_INC );
      indexImported ++;
    }
    if ( relayOnSeconds >= 0 ) {
      relayOnSeconds -= 60;
      indexRelayOn ++;
    }
    

    // *** Calcul des statistiques du déclenchement du TRIAC              ***
    OCR1A_avg /= OCR1A_cnt;

    // *** Obtention des données du compteur à impulsions                 ***
    noInterrupts ( );
    indexImpulsionTemp = indexImpulsion;
    interrupts ( );

    // *** Transmission des donnéees statistiques                         ***
    // *** L'ordre de transmission est important !!!                      ***
    // *** MaxPV! attend tous ces paramètres dans cet ordre               ***
    // *** à chaque transmission.                                         ***
    Serial.print ( F("STATS,") );
    Serial.print ( Vrms, 1 );
    Serial.print ( F(",") );
    Serial.print ( Irms, 3 );
    Serial.print ( F(",") );
    Serial.print ( Pact, 1 );
    Serial.print ( F(",") );
    Serial.print ( Papp, 1 );
    Serial.print ( F(",") );
    Serial.print ( Prouted, 1 );
    Serial.print ( F(",") );
    Serial.print ( ( ( Pact >= 0 ) ? Pact : 0 ), 1 );
    Serial.print ( F(",") );
    Serial.print ( ( ( Pact <= 0 ) ? -Pact : 0 ), 1 );
    Serial.print ( F(",") );
    Serial.print ( ( Pact / Papp ), 4 );
    Serial.print ( F(",") );
    Serial.print ( indexRouted * WH_PER_INC );    // entier
    Serial.print ( F(",") );
    Serial.print ( indexImported * WH_PER_INC );  // entier
    Serial.print ( F(",") );
    Serial.print ( indexExported * WH_PER_INC );  // entier
    Serial.print ( F(",") );
    Serial.print ( float ( indexImpulsionTemp * CNT_CALIB ), 0 );  // float car CNT_CALIB est float
    Serial.print ( F(",") );
    Serial.print ( float ( Pimpulsion * CNT_CALIB ), 1 );
    Serial.print ( F(",") );
    Serial.print ( triacMode );
    Serial.print ( F(",") );
    Serial.print ( relayMode );
    Serial.print ( F(",") );
    if ( ( OCR1A_min < 255 ) && ( triacMode == AUTOM ) ) {   
      // Affichage si on est en mode SSR automatique et régime de régulation (exécent PV)
      Serial.print ( float ( OCR1A_min * 0.064 ), 2 );
      Serial.print ( F(",") );
      Serial.print ( float ( OCR1A_avg * 0.064 ), 2 );
      Serial.print ( F(",") );
      Serial.print ( float ( OCR1A_max * 0.064 ), 2 );
    }
    else {
      Serial.print ( F("N/A,N/A,N/A") );
    }
    Serial.print ( F(",") );
    Serial.print ( float ( VCC_1BIT * stats_biasOffset ), 3 );
    Serial.print ( F(",") );
    if ( stats_error_status <= B00000001 ) Serial.print ( F("0000000") );
    else if ( stats_error_status <= B00000011 ) Serial.print ( F("000000") );
    else if ( stats_error_status <= B00000111 ) Serial.print ( F("00000") );
    else if ( stats_error_status <= B00001111 ) Serial.print ( F("0000") );
    else if ( stats_error_status <= B00011111 ) Serial.print ( F("000") );
    else if ( stats_error_status <= B00011111 ) Serial.print ( F("000") );
    else if ( stats_error_status <= B00111111 ) Serial.print ( F("00") );
    else if ( stats_error_status <= B01111111 ) Serial.print ( F("0") );
    Serial.print ( stats_error_status, BIN );
    Serial.print ( F(",") );
    Serial.print ( daysOnline );
    Serial.print ( F(":") );
    if ( hoursOnline < 10 ) Serial.print ( F("0") );
    Serial.print ( hoursOnline );
    Serial.print ( F(":") );
    if ( minutesOnline < 10 ) Serial.print ( F("0") );
    Serial.print ( minutesOnline );
    Serial.print ( F(":") );
    if ( secondsOnline < 10 ) Serial.print ( F("0") );
    Serial.print ( secondsOnline );
    Serial.print ( F(",") );
    Serial.print ( stats_samples );
    Serial.print ( F(",") );
    Serial.print ( indexRelayOn );
    Serial.print ( F(",END#") );

    // *** Reset du Flag pour indiquer que les données ont été traitées   ***
    stats_ready_flag = 0;

    // *** Initialisation des statistique OCR1A                           ***
    OCR1A_avg = 0;
    OCR1A_max = 0;
    OCR1A_min = 255;
    OCR1A_cnt = 0;

    // *** Mise à jour du temps de fonctionnement UpTime                  ***
    upTime ( );

    // *** Appel du scheduler                                             ***
    // *** Gestion des actions régulières et tâches planifiées            ***
    PVRScheduler ( );
  }

  // *** Fin du Traitement des informations statistiques                  ***
  // *** Et des actions cadencées à la seconde NB_CYCLES = 50 @ 50 Hz     ***

  // *** La suite est exécutée à chaque passage dans loop                 ***

  // *** Mise à jour de l'état des LEDs de signalisation                  ***
  PVRLed ( );

  hasRestarted = false;

  // *** Traitement des requêtes par liaison série                        ***
  // *** On fait 5 traitements de suite au cas où il y a une salve        ***
  // *** de données communiquées                                          ***
  while ( i < 5 ) {
    if ( Serial.available ( ) > 0 ) serialRequest ( );
    i++;
  }

  // *** Traitement de la perte longue de synchronisation, erreur majeure ***
  if ( stats_error_status >= B10000000 ) fatalError ( );

  if ( hasRestarted ) {
    clearSerialInputCache ( );
    refTime = millis ( );
  }

}


///////////////////////////////////////////////////////////////////////////////////////
// startPVR                                                                          //
// Fonction de démarrage                                                             //
// Premier démarrage ou re-démarrage                                                 //
///////////////////////////////////////////////////////////////////////////////////////
void startPVR ( void ) {

  delay ( 10 );
  noInterrupts ( );
  // Lecture de la tension d'alimentation de l'arduino via la référence interne 1.1 V
  // A chaque démarrage du routeur, on met à jour la tension d'alimentation
  analogReadReference ( );                       // Première lecture à vide de la référence de tension interne
  delay ( 10 );
  VCC_1BIT = ( 1.1 / analogReadReference ( ) );  // Calcul de la valeur en V d'un bit ADC
  delay ( 10 );

  // arrêt du SSR par sécurité
  TRIAC_OFF;
  // arrêt du relais secondaire de délestage  par sécurité
  digitalWrite ( relayPin, OFF );
  // Configuration du triac/SSR en automatique
  triacMode = triacModePowerON;
  // Configuration du relais de délestage secondaire en automatique
  relayMode = relayModePowerON;

  // Configuration du convertisseur ADC pour travailler sur interruptions
  configADC ( );
  // Configuration des Timer1 et Rimer2
  configTimer1 ( );
  configTimer2 ( );
  // Configuration de l'entrée d'interruption synchroACPin
  attachInterrupt ( digitalPinToInterrupt ( synchroACPin ), zeroCrossingInterrupt, CHANGE );
  // Configuration de l'entrée d'interruption pulseExternalPin
  attachInterrupt ( digitalPinToInterrupt ( pulseExternalPin ), pulseExternalInterrupt, FALLING );

  // Initialisations pour le premier cycle
  coldStart = NCSTART;
  error_status = 0;
  stats_error_status = 0;
  stats_ready_flag = 0;
  interrupts ( );
  delay ( 50 );

  // Démarrage de la première conversion ADC = démarrage du routeur
  ADCSRA |= B01000000;

  // *** Si période de warm-up, on attend
  // *** avant de redonner la main au programme
  while ( coldStart > 0 ) {
    delay ( 10 );
  };
  hasRestarted = true;
}


///////////////////////////////////////////////////////////////////////////////////////
// stopPVR                                                                           //
// Fonction d'arrêt                                                                  //
///////////////////////////////////////////////////////////////////////////////////////
void stopPVR ( void ) {

  // arrêt ADC et arrêt interruption ADC
  ADCSRA = 0x00;
  // arrêt interruption détection passage par zéro
  detachInterrupt ( digitalPinToInterrupt ( synchroACPin ) );
  // arrêt interruption comptage des impulsions externes
  detachInterrupt ( digitalPinToInterrupt ( pulseExternalPin ) );
  // arrêt du Timer 1
  TCCR1B = 0x00;
  // arrêt du SSR
  TRIAC_OFF;
  // arrêt du relais secondaire de délestage
  digitalWrite ( relayPin, OFF );
}


///////////////////////////////////////////////////////////////////////////////////////
// serialRequest                                                                     //
// Fonction de traitement des requêtes par liaison série                             //
///////////////////////////////////////////////////////////////////////////////////////
void serialRequest ( void ) {

  // Les chaînes valides envoyées par l'ESP se terminent toujours par #
  // !! envoyer les châines avec un Serial.print et non pas un Serialprintln !
  String incomingCommand;
  incomingCommand.reserve(32);
  incomingCommand = Serial.readStringUntil ( '#' );
  incomingCommand.trim();
  // On teste la validité de la chaîne qui doit contenir 'END' à la fin
  if ( incomingCommand.endsWith ( F("END") ) ) {
    if ( incomingCommand.startsWith ( F("PARAM") ) ) {
      int i = 0;
      Serial.print ( F("PARAM,") );
      while ( i < NB_PARAM ) {
        switch ( pvrParamConfig [i].dataType ) {
          case 0: {
              int *tmp_int = (int *) pvrParamConfig [i].adr;
              Serial.print ( *tmp_int );
              break;
            }
          case 1: {
              float *tmp_float = (float *) pvrParamConfig [i].adr;
              Serial.print ( *tmp_float, 3 );
              break;
            }
          case 4: {
              byte *tmp_byte = (byte *) pvrParamConfig [i].adr;
              Serial.print ( *tmp_byte );
              break;
            }
        }
        Serial.print ( F(",") );
        i++;
      }
    }

    else if ( incomingCommand.startsWith ( F("FORMAT") ) ) {
      // Demande de formatage de l'EEPROM
      for ( int i = 0 ; i < int ( EEPROM.length ( ) ) ; i++ ) {
        EEPROM.write ( i, 0 );
      }
      delay (50);
      Serial.print ( F("DONE:FORMAT,OK,") );
    }

    else if ( incomingCommand.startsWith ( F("SAVECFG") ) ) {
      // Demande de sauvegarde de la configuration
      eeConfigWrite ( );
      Serial.print ( F("DONE:SAVECFG,OK,") );
    }

    else if ( incomingCommand.startsWith ( F("LOADCFG") ) ) {
      // Demande de lecture de la configuration
      if ( eeConfigRead ( ) == false ) eeConfigWrite ( );
      Serial.print ( F("DONE:LOADCFG,OK,") );
    }

    else if ( incomingCommand.startsWith ( F("SAVEINDX") ) ) {
      // Demande de sauvegarde des index
      indexWrite ( );
      Serial.print ( F("DONE:SAVEINDX,OK,") );
    }

    else if ( incomingCommand.startsWith ( F("VERSION") ) ) {
      // Demande des versions
      Serial.print ( F("VERSION,") );
      Serial.print ( F(VERSION) );
      Serial.print ( F(",") );
      Serial.print ( DATAEEPROM_VERSION );
      Serial.print ( F(",") );
    }

    else if ( incomingCommand.startsWith ( F("INDX0") ) ) {
      // Demande de mise à 0 des index
      indexRouted = 0;
      indexExported = 0;
      indexImported = 0;
      noInterrupts ( );
      indexImpulsion = 0;
      interrupts ( );
      indexRelayOn = 0;
      indexWrite ( );
      Serial.print ( F("DONE:INDX0,OK,") );
    }

    else if ( incomingCommand.startsWith ( F("RESET") ) ) {
      // Demande de redémarrage du routeur
      if ( coldStart == 0 ) {  // On ne redémarre pas si on est encore dans le SETUP
        // ou en phase de démarrage
        indexWrite ( );
        delay ( 100 );
        stopPVR ( );
        delay ( 100 );
        startPVR ( );
        Serial.print ( F("DONE:RESET,OK,") );
      }
      else Serial.print ( F("NOK:RESET,OK,") );
    }

    else if ( incomingCommand.startsWith ( F("SETPARAM") ) ) {
      // Modification d'un paramètre
      incomingCommand.replace ( F("SETPARAM,"), "" );
      incomingCommand.replace ( F(",END"), "" );

      int paramNum = incomingCommand.substring ( 0, 2 ).toInt ( );

      if ( ( paramNum > 0) && ( paramNum <= NB_PARAM ) ) {
        int index = paramNum - 1;
        byte dataType = pvrParamConfig [index].dataType;
        int minValue = pvrParamConfig [index].minValue;
        int maxValue = pvrParamConfig [index].maxValue;
        float valueFloat;
        int valueInt;
        if ( dataType == 1 ) {
          valueFloat = incomingCommand.substring ( 3 ).toFloat ( );
          valueFloat = constrain ( valueFloat, float ( minValue ), float ( maxValue ) );
        }
        else {
          valueInt = incomingCommand.substring ( 3 ).toInt ( );
          valueInt = constrain ( valueInt, minValue, maxValue );
        }

        switch ( dataType ) {
          case 0: {
              int *tmp_int = (int *) pvrParamConfig [index].adr;
              noInterrupts ( );
              *tmp_int = valueInt;
              interrupts ( );
              break;
            }
          case 1: {
              float *tmp_float = (float *) pvrParamConfig [index].adr;
              noInterrupts ( );
              *tmp_float = float ( valueFloat );
              interrupts ( );
              break;
            }
          case 4: {
              byte *tmp_byte = (byte *) pvrParamConfig [index].adr;
              noInterrupts ( );
              *tmp_byte = byte ( valueInt );
              interrupts ( );
              break;
            }
        }
        Serial.print ( F("DONE:SETPARAM,OK,") );
      }
      else Serial.print ( F("NOK:SETPARAM,OK,") );
    }

    else if ( incomingCommand.startsWith ( F("SETRELAY") ) ) {
      // Modification du mode relais secondaire de délestage
      incomingCommand.replace ( F("SETRELAY,"), "" );
      incomingCommand.replace ( F(",END"), "" );

      if ( incomingCommand == F("STOP") )       relayMode = STOP;
      else if ( incomingCommand == F("FORCE") ) relayMode = FORCE;
      else if ( incomingCommand == F("AUTO") )  relayMode = AUTOM;
      // Arrêt du mode relayPlus si il est en cours pour donner la priorité 
      // au changement de mode relais
      if ( relayplusTime >= 0 ) {
          relayplusTime = -1;
          Serial.print ( F("RELAYPLUSTIME,-1,END") );
      }
      Serial.print ( F("DONE:SETRELAY,OK,") );
    }

    else if ( incomingCommand.startsWith ( F("SETSSR") ) ) {
      // Modification du mode triac/SSR
      incomingCommand.replace ( F("SETSSR,"), "" );
      incomingCommand.replace ( F(",END"), "" );

      if ( incomingCommand == F("STOP") )       triacMode = STOP;
      else if ( incomingCommand == F("FORCE") ) triacMode = FORCE;
      else if ( incomingCommand == F("AUTO") )  triacMode = AUTOM;
      // Arrêt du mode BOOST si il est en cours pour donner la priorité 
      // au changement de mode SSR
      if ( boostTime >= 0 ) {
          boostTime = -1;
          Serial.print ( F("BOOSTTIME,-1,END") );
      }
      Serial.print ( F("DONE:SETSSR,OK,") );
    }

    else if ( incomingCommand.startsWith ( F("SETBOOST") ) ) {
      int index = 0;
      long bT = 0;
      int burstRatio = 100;  // rapport PWM par défaut : 100%

      incomingCommand.replace ( F("SETBOOST,"), "" );
      incomingCommand.replace ( F(",END"), "" );

      bT = incomingCommand.toInt ( );
      bT = constrain ( bT, 0, 86400 );
      if ( ( bT != 0 ) || ( boostTime != -1 ) ) boostTime = bT;
      index = incomingCommand.indexOf(',');
      if ( index != -1 ) burstRatio = incomingCommand.substring ( index + 1 ).toInt ( );
      burstRatio = constrain ( burstRatio, 10, 100 );
      burstThreshold = int ( ( long ( BOOST_BURST_PERIOD ) * long ( burstRatio ) ) / 100 );
      burstCnt = 0; // reset du cycle de la PWM burst
      Serial.print ( F("DONE:SETBOOST,OK,") );
    }

    else if ( incomingCommand.startsWith ( F("SETRELPLUS") ) ) {
      long rT = 0;

      incomingCommand.replace ( F("SETRELPLUS,"), "" );
      incomingCommand.replace ( F(",END"), "" );

      rT = incomingCommand.toInt ( );
      relayplusTime = constrain ( rT, 0, 86400 );
      Serial.print ( F("DONE:SETRELPLUS,OK,") );
    }

    else Serial.print ( F("UNKNOWN COMMAND,") );
  }
  else Serial.print ( F("ERROR?,") );
  Serial.print ( F("END#") );
}


///////////////////////////////////////////////////////////////////////////////////////
// fatalError                                                                        //
// Fonction de gestion d'erreur majeure                                              //
///////////////////////////////////////////////////////////////////////////////////////
void fatalError ( void ) {

  stopPVR ( );
  // Le système est mis en sécurité
  indexWrite ( );
  // Sauvegarde des index par sécurité

#if defined (OLED_128X64)
  oLedPrint ( 99 );
#endif

  for ( int k = 0; k <= 100; k++ ) {
    digitalWrite ( pulseTriacPin, OFF ); //arrêt du SSR par sécurité
    digitalWrite ( ledPinStatus,  OFF );
    digitalWrite ( ledPinRouting, ON );
    delay ( 100 );
    digitalWrite ( ledPinStatus,  ON );
    digitalWrite ( ledPinRouting, OFF );
    delay ( 100 );
  }
  // le système redémarre au bout de 20 secondes environ
  startPVR ( );
}


///////////////////////////////////////////////////////////////////////////////////////
// eeConfigRead                                                                      //
// Fonction de lecture de la configuration EEPROM                                    //
///////////////////////////////////////////////////////////////////////////////////////
bool eeConfigRead ( void ) {

  dataEeprom pvrConfig;
  EEPROM.get ( PVR_EEPROM_START, pvrConfig );
  if ( pvrConfig.magic != DATAEEPROM_MAGIC ) return false;
  else {
    noInterrupts ( );
    V_CALIB       = pvrConfig.v_calib;
    P_CALIB       = pvrConfig.p_calib;
    PHASE_CALIB   = pvrConfig.phase_calib;
    P_OFFSET      = pvrConfig.p_offset;
    P_RESISTANCE  = pvrConfig.p_resistance;
    P_MARGIN      = pvrConfig.p_margin;
    GAIN_P        = pvrConfig.gain_p;
    GAIN_I        = pvrConfig.gain_i;
    E_RESERVE     = pvrConfig.e_reserve;
    interrupts ( );
    P_DIV2_ACTIVE = pvrConfig.p_div2_active;
    P_DIV2_IDLE   = pvrConfig.p_div2_idle;
    T_DIV2_ON     = pvrConfig.t_div2_on;
    T_DIV2_OFF    = pvrConfig.t_div2_off;
    T_DIV2_TC     = pvrConfig.t_div2_tc;
    CNT_CALIB     = pvrConfig.cnt_calib;
    P_INSTALLPV   = pvrConfig.p_installpv;
    return true;
  }
}


///////////////////////////////////////////////////////////////////////////////////////
// eeConfigWrite                                                                     //
// Fonction d'écriture de la configuration EEPROM                                    //
///////////////////////////////////////////////////////////////////////////////////////
void eeConfigWrite ( void ) {

  dataEeprom pvrConfig;
  pvrConfig.magic           = DATAEEPROM_MAGIC;
  pvrConfig.struct_version  = DATAEEPROM_VERSION;
  pvrConfig.v_calib         = V_CALIB;
  pvrConfig.p_calib         = P_CALIB;
  pvrConfig.phase_calib     = PHASE_CALIB;
  pvrConfig.p_offset        = P_OFFSET;
  pvrConfig.p_resistance    = P_RESISTANCE;
  pvrConfig.p_margin        = P_MARGIN;
  pvrConfig.gain_p          = GAIN_P;
  pvrConfig.gain_i          = GAIN_I;
  pvrConfig.e_reserve       = E_RESERVE;
  pvrConfig.p_div2_active   = P_DIV2_ACTIVE;
  pvrConfig.p_div2_idle     = P_DIV2_IDLE;
  pvrConfig.t_div2_on       = T_DIV2_ON;
  pvrConfig.t_div2_off      = T_DIV2_OFF;
  pvrConfig.t_div2_tc       = T_DIV2_TC;
  pvrConfig.cnt_calib       = CNT_CALIB;
  pvrConfig.p_installpv     = P_INSTALLPV;
  EEPROM.put ( PVR_EEPROM_START, pvrConfig );
}


///////////////////////////////////////////////////////////////////////////////////////
// indexRead                                                                         //
// Fonction de lecture des index en EEPROM                                           //
///////////////////////////////////////////////////////////////////////////////////////
void indexRead ( void ) {

  indexEeprom indexTable;
  EEPROM.get ( PVR_EEPROM_INDEX_ADR, indexTable );
  indexRouted = indexTable.routed;
  indexImported = indexTable.imported;
  indexExported = indexTable.exported;
  indexRelayOn = indexTable.relayon;
  noInterrupts ( );
  indexImpulsion = indexTable.impulsion;
  interrupts ( );
}


///////////////////////////////////////////////////////////////////////////////////////
// indexWrite                                                                        //
// Fonction d'écriture des index en EEPROM                                           //
///////////////////////////////////////////////////////////////////////////////////////
void indexWrite ( void ) {

  indexEeprom indexTable;
  indexTable.routed = indexRouted;
  indexTable.imported = indexImported;
  indexTable.exported = indexExported;
  indexTable.relayon = indexRelayOn; 
  noInterrupts ( );
  indexTable.impulsion = indexImpulsion;
  interrupts ( );
  EEPROM.put ( PVR_EEPROM_INDEX_ADR, indexTable );
  delay (10);
}


///////////////////////////////////////////////////////////////////////////////////////
// upTime                                                                            //
// Fonction de lecture de mise à jour des données de l'horloge interne               //
///////////////////////////////////////////////////////////////////////////////////////
void upTime ( void ) {

  unsigned long stats_PVRClock;

  noInterrupts ( );
  stats_PVRClock = PVRClock;
  interrupts ( );
  stats_PVRClock /= NB_CYCLES;
  secondsOnline = ( stats_PVRClock         ) % 60;
  minutesOnline = ( stats_PVRClock / 60    ) % 60;
  hoursOnline   = ( stats_PVRClock / 3600  ) % 24;
  daysOnline    = ( stats_PVRClock / 86400 );
  // daysOnline est limité à 994 jours modulo 256, puis repassera à 0
}


///////////////////////////////////////////////////////////////////////////////////////
// PVRScheduler                                                                      //
// Fonction Scheduler appelée chque seconde                                          //
///////////////////////////////////////////////////////////////////////////////////////
void PVRScheduler ( void ) {
  static unsigned long  lastDeltaTimeImpulsion = 0;
  static unsigned long  lastMinuteDeltaTimeImpulsion = 0;
  static bool           impulsionFlag = false;
  unsigned long         deltaTimeImpulsion_tmp = 0;

  //*** Toutes les heures : Enregistrement des index en mémoire EEPROM  ***
  if ( ( minutesOnline == 0 ) && ( secondsOnline == 0 ) ) indexWrite ( );

  // *** Affichage des donnéees statistiques sur écran oled si activé   ***
  // *** Période d'envoi définie par OLED_128X64_REFRESH_PERIOD         ***
  // *** Envoi page 0 à la 2ème seconde de l'intervalle de temps        ***
  // *** et page 1 à la 5ème seconde                                    ***
#if ( ( defined (OLED_128X64) ) && ( defined (OLED_128X64_REFRESH_PERIOD) ) )
  if ( OLED_128X64_REFRESH_PERIOD > 0 ) {
    if ( ( secondsOnline % OLED_128X64_REFRESH_PERIOD ) == 2 ) oLedPrint ( 0 );
    if ( ( secondsOnline % OLED_128X64_REFRESH_PERIOD ) == 5 ) oLedPrint ( 1 );
  }
#endif

  //*** Toutes les 30 secondes , envoi du temps BOOST restant                            ***
  if ( ( secondsOnline % 30 )  == 17 ) {
    Serial.print ( F("BOOSTTIME,") );
    Serial.print ( boostTime );
    Serial.print ( F(",END#") );
  }

  //*** Toutes les 30 secondes , envoi du temps relayPlus restant                        ***
  if ( ( secondsOnline % 30 )  == 19 ) {
    Serial.print ( F("RELAYPLUSTIME,") );
    Serial.print ( relayplusTime );
    Serial.print ( F(",END#") );
  }

  //*** Toutes les 2 secondes : Calcul de la puissance relative à la mesure d'impulsion  ***
  //*** Calcul en 'impulsion par heure'                                                  ***
  if ( ( secondsOnline % 2 ) == 0 ) {
    noInterrupts ( );
    deltaTimeImpulsion_tmp = deltaTimeImpulsion;
    interrupts ( );
    if ( secondsOnline == 8 ) {    // une fois par minute, on teste l'absence d'impulsions
      if ( deltaTimeImpulsion_tmp == lastMinuteDeltaTimeImpulsion ) {
        Pimpulsion = 0;
        impulsionFlag = false;
      }
      lastMinuteDeltaTimeImpulsion = deltaTimeImpulsion_tmp;
    }
    if ( deltaTimeImpulsion_tmp != lastDeltaTimeImpulsion ) {
      lastDeltaTimeImpulsion = deltaTimeImpulsion_tmp;
      if ( impulsionFlag ) Pimpulsion = ( 3600000.0 / float ( deltaTimeImpulsion_tmp ) );
      else impulsionFlag = true;
    }
  }

  //*** Toutes les secondes : Traitement du mode BOOST
  if (boostTime == 0) {
    boostTime--;
    triacMode = AUTOM;
    // Envoi immédiat de la fin du mode Boost
    Serial.print ( F("BOOSTTIME,-1,END") );
  }
  else if (boostTime > 0) {
    if ( burstCnt <= burstThreshold ) triacMode = FORCE;
    else triacMode = STOP;
    boostTime--;
    burstCnt++;
    burstCnt %= BOOST_BURST_PERIOD;
  }

  //*** Toutes les secondes : Traitement du mode relayPlus
  if (relayplusTime == 0) {
    relayplusTime--;
    relayMode = STOP;
    // Envoi immédiat de la fin du mode relayPlus
    Serial.print ( F("RELAYPLUSTIME,-1,END") );
  }
  else if (relayplusTime > 0) {
    relayplusTime--;
    relayMode = FORCE;
  }
}


///////////////////////////////////////////////////////////////////////////////////////
// PVRLed                                                                            //
// Fonction gestion allumage des leds de signalisation                               //
///////////////////////////////////////////////////////////////////////////////////////
void PVRLed ( void ) {

  byte routingByte = stats_error_status & B00001011;
  byte errorByte = stats_error_status & B11110000;

  if ( routingByte == 0 ) {         // pas de routage
    digitalWrite ( ledPinRouting, OFF );      // led éteinte
  }
  else if ( routingByte == 1 ) {    // routage en régulation
    digitalWrite ( ledPinRouting, ON  );      // allumage fixe
  }
  else {                            // autre cas : routage à 100 % voire exportation
    digitalWrite ( ledPinRouting, ( ( ledBlink & B00001000 ) == 0 ) ? 0 : 1 ); // T = 320 ms
  }

  if ( errorByte == 0 ) {          // pas d'ereur
    digitalWrite ( ledPinStatus, ( ( ledBlink & B01000000 ) == 0 ) ? 0 : 1 ); // T = 2560 ms
  }
  else if ( errorByte < 64 ) {     // anomalie sur les signaux analogiques ou le taux d'acquisition
    digitalWrite ( ledPinStatus, ( ( ledBlink & B00010000 ) == 0 ) ? 0 : 1 ); // T = 640 ms
  }
  else {                           // autre cas : anomalie furtive voire grave de détection du passage à 0
    digitalWrite ( ledPinStatus, ( ( ledBlink & B00000100 ) == 0 ) ? 0 : 1 ); // T = 160 ms
  }
}


///////////////////////////////////////////////////////////////////////////////////////
// clearSerialInputCache                                                             //
// Fonction de vidage du buffer de réception de la liaison série                     //
///////////////////////////////////////////////////////////////////////////////////////
void clearSerialInputCache ( void ) {

  while ( Serial.available ( ) > 0 ) Serial.read ( );
}


///////////////////////////////////////////////////////////////////////////////////////
// analogReadReference                                                               //
// Fonction de lecture de la référence interne de tension                            //
// Adapté de : https://www.carnetdumaker.net/snippets/77/                            //
///////////////////////////////////////////////////////////////////////////////////////
unsigned int analogReadReference ( void ) {

  ADCSRA &= B00000000;
  ADCSRA |= B10000000;
  ADCSRA |= B00000111;
  /* Elimine toutes charges résiduelles */
  ADMUX = 0x4F;
  delayMicroseconds ( 500 );
  /* Sélectionne la référence interne à 1.1 volts comme point de mesure, avec comme limite haute VCC */
  ADMUX = 0x4E;
  delayMicroseconds ( 1500 );

  /* Lance une conversion analogique -> numérique */
  ADCSRA |= ( 1 << ADSC );
  /* Attend la fin de la conversion */
  while ( ( ADCSRA & ( 1 << ADSC ) ) );
  /* Récupère le résultat de la conversion */
  return ADCL | ( ADCH << 8 );

  /* Note : Idéalement VCC = 5 volts = 1023 'bits' en conversion ADC
    1 'bit' vaut VCC / 1023
    Référence interne à 1.1 volts = ( 1023 * 1.1 ) / VCC = 225 'bits'
    En mesurant la référence à 1.1 volts, on peut déduire la tension d'alimentation réelle du microcontrôleur
    vccSupply = ( 1023 * VCC_1BIT )
    avec   VCC_1BIT = ( 1.1 / analogReadReference ( ) )
  */
}


///////////////////////////////////////////////////////////////////////////////////////
// oLedPrint                                                                         //
// Fonction d'affichage écran oled                                                   //
///////////////////////////////////////////////////////////////////////////////////////
#if defined (OLED_128X64)
void oLedPrint ( int page ) {

  oled.clear ( );
  oled.set2X ( );

  switch ( page ) {

    case 0 : {
        oled.println ( F(" Running") );
        if ( Pact < 0 )
          oled.print ( F("Expt ") );
        else
          oled.print ( F("Impt ") );
        if ( abs ( Pact ) < 10 ) oled.print ( F("   ") );
        else if ( abs ( Pact ) < 100 ) oled.print ( F("  ") );
        else if ( abs ( Pact ) < 1000 ) oled.print ( F(" ") );
        oled.print ( abs ( Pact ), 0 );
        oled.println ( F("W") );
        oled.print ( F("Rout ") );
        if ( Prouted < 10 ) oled.print ( F("   ") );
        else if ( Prouted < 100 ) oled.print ( F("  ") );
        else if ( Prouted < 1000 ) oled.print ( F(" ") );
        oled.print ( Prouted, 0 );
        oled.println ( F("W") );
        oled.print ( F("Relay ") );
        if ( digitalRead ( relayPin ) == ON ) oled.println ( F("  On") );
        else oled.println ( F(" Off") );
        break;
      }

    case 1 : {
        if ( stats_error_status > 15 ) oled.println ( F("! Check !") );
        else oled.println ( F("  Normal") );
        oled.print ( F(" ") );
        oled.print ( Vrms, 0 );
        oled.println ( F(" Volts") );
        if ( Irms < 10 ) oled.print ( F(" ") );
        oled.print ( Irms, 1 );
        oled.println ( F(" Amps") );
        oled.print ( F(" ") );
        oled.print ( abs ( cos_phi ), 3 );
        oled.println ( F(" PF ") );
        break;
      }

    case 9 : {
        oled.print ( F("MaxPV! ") );
        oled.println ( F(VERSION) );
        oled.println ( );
        oled.println ( F(" Demarrage") );
        oled.println ( F("en cours...") );
        break;
      }

    case 10 : {
        oled.print ( F("MaxPV! ") );
        oled.println ( F(VERSION) );
        oled.println ( );
        oled.println ( F("En fonction") );
        break;
      }

    case 99 : {
        oled.println ( F("***********") );
        oled.println ( F("* ANOMALIE*") );
        oled.println ( F("* MAJEURE *") );
        oled.println ( F("***********") );
        break;
      }
  }
}
#endif


///////////////////////////////////////////////////////////////////////////////////////
// configADC                                                                         //
// Fonction de configuration du convertisseur ADC                                    //
///////////////////////////////////////////////////////////////////////////////////////
void configADC ( void ) {

  // Configuration : voir documention ATMEGA328P
  // Fonctionnement sur interruption et choix de la référence
  ADMUX &= B11011111;
  ADMUX &= B00111111;
  ADMUX |= B01000000;
  ADCSRA &= B00000000;
  ADCSRA |= B10000000;
  ADCSRA |= B00001000;
  ADCSRA |= B00000110; // Prescaler to 64 (==> 166 échantillons par cycle)

  // désactivaton des I/O digitales branchées sur les ports ADC utilisés
  DIDR0 |= ( B00000001 << voltageSensorMUX );
  DIDR0 |= ( B00000001 << currentSensorMUX );

  // Sélection du port analogique correspondant au courant
  ADMUX &= B11110000;
  ADMUX |= currentSensorMUX;

  // La conversion démarrera en mettant à 1 le bit ADSC de ADCSRA
  // après avoir configuré ADMUX pour le choix de l'entrée analogique
  // Pour démarrer : ADCSRA |= B01000000;
}


///////////////////////////////////////////////////////////////////////////////////////
// configTimer1 (16 bits)                                                                     //
// Fonction de configuration du Timer 1, gestion du pulse SSR/TRIAC                  //
///////////////////////////////////////////////////////////////////////////////////////
void configTimer1 ( void ) {

  TIMSK1 = 0x03;     // activation des interruptions sur comparateur A et overflow
  TCCR1A = 0x00;     // fonctionnement normal,
  TCCR1B = 0x00;     // timer arrêté
  OCR1A  = 30000;    // comparateur initialisé à 30000
  TCNT1  = 0;        // compteur initialisé à 0

  /*******************  Pour information : *****************************
    //TCCR1B=0x05; // démarrage du compteur par pas de 64 us
    //TCCR1B=0x00; // arrêt du compteur
  *********************************************************************/
}


///////////////////////////////////////////////////////////////////////////////////////
// configTimer2 (8 bits)                                                                     //
// Fonction de configuration du Timer 2, gestion de l'antirebond des pulses externes //
///////////////////////////////////////////////////////////////////////////////////////
void configTimer2 ( void ) {

  TIMSK2 = 0x03;     // activation des interruptions sur comparateur A et overflow
  TCCR2A = 0x00;     // fonctionnement normal,
  TCCR2B = 0x00;     // timer arrêté
  OCR2A  = 127;      // comparateur initialisé à 63 (soit 4 ms à 64 us par pas)
  TCNT2  = 0;        // compteur initialisé à 0

  /*******************  Pour information : *****************************
    //TCCR2B=0x07; // démarrage du compteur par pas de 64 us
    //TCCR2B=0x00; // arrêt du compteur
  *********************************************************************/
}


// ***********************************************************************************
// ***************************   ROUTINES D'INTERRUPTION   ***************************
// ***********************************************************************************
// Quand une interruption est appelée, les autres sont désactivées
// mais gardées en mémoire.
// Elles seront appelées à la fin de l'exécution de l'interruption en cours
// dans un ordre de priorité : INT0, INT1, COUNT1, ADC.



///////////////////////////////////////////////////////////////////////
// zeroCrossingInterrupt                                             //
// Interrupt service routine de passage à zéro du secteur            //
// Temps mesuré de traitement de l'interruption : entre 36 et 40 us  //
///////////////////////////////////////////////////////////////////////
void zeroCrossingInterrupt ( void ) {

#define ERROR_BIT_SHIFT        6        // Valeur de décimation des données pour le calcul de la régulation PI
#define COMMAND_BIT_SHIFT     14        // Valeur de décimation pour la commande de puissance

  static bool           periodParity   = POSITIVE;
  static unsigned long  last_time;
  static byte           numberOfCycle  = 0;
  static long           P_SETPOINT     = long ( float ( ( P_MARGIN + P_OFFSET ) )
                                         * float ( SAMP_PER_CYCLE ) * ( 0.5 / P_CALIB ) );
  // Setpoint du régulateur : Valeur de P_MARGIN transféré dans le système d'unité du régulateur
  // Prise en compte de l'offset de mesure P_OFFSET sur la puissance active
  static long           controlError        = P_SETPOINT >> ERROR_BIT_SHIFT;
  static long           lastControlError    = P_SETPOINT >> ERROR_BIT_SHIFT;
  static long           controlIntegral     = 0;
  static long           controlIntegralMinValue = - ( long ( ( long ( NB_CYCLES ) * long ( SAMP_PER_CYCLE ) * long ( E_RESERVE ) )
      / P_CALIB ) >> ERROR_BIT_SHIFT );
  static long           controlCommand      = 0;
  unsigned long         present_time;

  present_time = micros ( );

  if ( coldStart > 0 ) {
    // Phase de Warm-up : initialisation jusque coldStart = 0;
    coldStart --;                               // on décrémente coldStart

    TRIAC_OFF;
    TCCR1B           = 0x00;
    TCNT1            = 0x00;
    OCR1A            = 30000;                   // on charge à un délai inatteignable

    periodParity     = POSITIVE;                // signe arbitraire de l'alternance
    periodP          = 0;
    P_SETPOINT       = long ( float ( ( P_MARGIN + P_OFFSET ) )
                              * float ( SAMP_PER_CYCLE ) * ( 0.5 / P_CALIB ) );
    controlError     = P_SETPOINT >> ERROR_BIT_SHIFT;
    lastControlError = P_SETPOINT >> ERROR_BIT_SHIFT;
    controlIntegral  = 0;
    controlIntegralMinValue = - ( long ( ( long ( NB_CYCLES ) * long ( SAMP_PER_CYCLE ) * long ( E_RESERVE ) )
                                         / P_CALIB ) >> ERROR_BIT_SHIFT );
    controlCommand   = 0;
    numberOfCycle    = 0;
    last_time        = present_time;

    // Initialisation pour les statistiques
    samples          = 0;
    sumVsqr          = 0;
    sumIsqr          = 0;
    sumP             = 0;
    routed_power     = 0;
    stats_ready_flag = 0;
    error_status     = 0;
  }

  else if ( ( present_time - last_time ) > 8000 ) {
    // *** PASSAGE PAR ZERO DETECTE - ON A TERMINE UNE DEMI PERIODE ***
    // gestion de l'antirebond du passage à 0 en calculant de temps
    // entre 2 passages à 0 qui doivent être séparés de 8 ms au moins

    // Si on est en mode SSR FORCE, on active immédiatement le SSR
    // Sinon on arrête le SSR (mode AUTOM ou STOP)
    if ( triacMode == FORCE ) TRIAC_ON;
    else TRIAC_OFF;

    TCCR1B = 0x00;                        // arrêt du Timer par sécurité
    TCNT1  = 0x00;                        // on remet le compteur à 0 par sécurité
    TCCR1B = 0x05;                        // on démarre le Timer par pas de 64 us
    OCR1A  = 30000;                       // on charge à un délai inatteignable en attendant les calculs
    // ATTENTION, le Timer1 commence à compter ici !!

    // *** Calculs de fin de période (demi-cycle secteur)
    // *** Convention : si injection sur le réseau (PV excédentaire), periodP est positif

    controlError = ( periodP + P_SETPOINT ) >> ERROR_BIT_SHIFT;
    // calcul de l'erreur du régulateur
    // signe + lié à la définition de P_SETPOINT
    // P_SETPOINT prend en compte la correction de l'offset P_OFFSET de lecture de Pact
    // réduction de résolution de ERROR_BIT_SHIFT bits
    // = division par 2 puissance ERROR_BIT_SHIFT
    // pour éviter de dépasser la capacité des long dans les calculs
    // et donner de la finesse au réglage du gain

    controlIntegral += controlError;
    // Note : l'erreur ne sera intégrée que si on est en régime linéaire de régulation
    // pour éviter le problème d'integral windup et de débordement de controlCommand
    // Le régime linéaire de régulation est observé sur la dernière commande SSR controlCommand
    // Voir le traitement fait plus bas

    // on calcule la commande à appliquer (correction PI)
    // note : lissage de l'erreur sur 2 périodes pour l'action proportionnelle pour corriger le bruit systématique
    controlCommand = long ( GAIN_I ) * controlIntegral + long ( GAIN_P ) * ( controlError + lastControlError );

    // application du gain fixe de normalisation : réduction de COMMAND_BIT_SHIFT bits
    controlCommand = controlCommand >> COMMAND_BIT_SHIFT;

    if ( controlCommand <= 0 ) {                                // équilibre ou importation, donc pas de routage de puissance
      TCCR1B = 0;                                               // arrêt du Timer = inhibition du déclenchement du triac pour cette période
      TCNT1  = 0;                                               // compteur à 0
      controlCommand = 0;
      if ( controlIntegral <= controlIntegralMinValue ) {       // fonction anti integral windup
        controlIntegral = controlIntegralMinValue;
      }
    }

    else {                                                      // controlCommand est strictement positif
      if ( controlCommand > 255 ) {                             // Saturation de la commande en pleine puissance
        controlCommand = 255;                                   // Pleine puissance
        controlIntegral -= controlError;                        // gel de l'accumulation de l'intégrale, fonction anti integral windup
      }
      // *** Régime linéaire de régulation (ou SSR max) : initialisation du comparateur de CNT1
      // *** pour le déclenchement du SSR/TRIAC géré par interruptions Timer1
      // par conséquent, si OCR1A garde sa valeur d'initialisation à 30000 : on n'est pas en régulation linéaire
      OCR1A = energyToDelay [ byte ( controlCommand ) ];
    }

    // si on n'est pas en mode automatique, on gèle la commande routage par arrêt du Timer
    // mais on a quand même fait auparavant les calculs de régulation.
    // on réaffecte controlCommand pour l'accumulation de la puissance délivrée réellement par le SSR
    // qui servira à l'affichage de la puissance routée et de l'index de routage
    if ( triacMode == STOP ) {
      TCCR1B = 0;
      TCNT1  = 0;
      controlCommand = 0;
    }
    else if ( triacMode == FORCE ) {
      TCCR1B = 0;
      TCNT1  = 0;
      controlCommand = 255;
      // Note : en mode FORCE, le triac a été déclenché précédemment avant le calcul de la régulation
    }

    // Calcul pour les statistiques
    routed_power += controlCommand;
    sumP += periodP;

    // Initialisation pour la période suivante
    periodP = 0;
    lastControlError = controlError;

    // changement de parité de la période (pour le demi-cycle suivant)
    periodParity = !periodParity;

    // opérations réalisées toutes les 2 périodes (à chaque cycle secteur)
    if ( periodParity ) {   // La demi-période suivante est positive

      // incrément du nombre de cycles
      numberOfCycle ++;

      // Opérations réalisées tous les NB_CYCLES cycles soit toutes les secondes pour NB_CYCLES = 50
      if ( numberOfCycle == NB_CYCLES ) {

        numberOfCycle = 0;

        if ( stats_ready_flag == 0 ) {        // La LOOP a traité les données précédentes
          // Transfert des données statistiques pour utilisation par la LOOP (Partie 1)
          stats_routed_power = routed_power;  // Evaluation de la puissance routée vers la charge
          routed_power = 0;
          stats_biasOffset = biasOffset;      // Dernière valeur de la correction d'offset de lecture ADC
          stats_error_status &= B00001111;    // RAZ des bits d'ERREUR 4..7
          stats_error_status |= error_status; // Transfert des bits d'ERREUR 4..7 uniquement
          error_status = 0;                   // Les bits signalant les erreurs sont remis à 0
          // à chaque traitement statistique

          stats_ready_flag = 9;               // Flag pour le prochain appel de l'interruption ADC
          // qui poursuivra le transfert des statistiques (Partie 2)

        }
        else {                                // La LOOP n'a pas (encore) traité les données précédentes :
          // Les données courantes sont perdues.
          routed_power = 0;
          error_status = 0;
          sumP = 0;
          sumVsqr = 0;
          sumIsqr = 0;
          sumV    = 0;
          sumI    = 0;
          samples = 0;
        }
      }
    }

    else {   // La demi-période suivante est négative
      // Incrément de l'horloge interne de temps de fonctionnement par pas de 20 ms
      PVRClock ++;
      // incrément du séquenceur pour le clignotement des leds
      ledBlink ++;

      // Détection des erreurs de biasOffset
      if ( abs ( biasOffset - 511 ) >= BIASOFFSET_TOL ) {
        error_status |= B00010000;
      }
    }

    last_time = present_time;
  }
  else error_status |= B01000000;  // Fausse détection d'un passage à 0, on signale l'évènement
}

//////////////////////////////////////////////////////////////////////
// ISR ( ADC_vect )                                                 //
// Interrupt service routine pour la conversion ADC de I et V       //
// Temps mesuré de traitement de l'interruption : 20 à 24 us        //
//////////////////////////////////////////////////////////////////////
ISR ( ADC_vect ) {
  // Note : pour des raisons de retard de lecture analogique I et V, on convertit d'abord I, puis V
  // La charge de calcul est répartie entre les 2 phases de conversion pour optimiser les temps d'interruption
  // caractéristique du filtrage de détermination de biasOffset
#define FILTERSHIFT           15  // constante de temps de 4s
#define FILTERROUNDING        0b100000000000000

  static byte   readFlagADC = 0;
  // readFlagADC = 0 pour la conversion du courant
  // readFlagADC = 9 pour la conversion de la tension
  // Variable locale. Ne peut pas prendre d'autres valeurs que 0 ou 9
  static long   fBiasOffset = ( 511L << FILTERSHIFT );
  // pré-chargement du filtre passe-bas du biasOffset au point milieu de l'ADC
  static int    lastSampleVcorr = 0;
  static int    lastSampleIcorr = 0;
  int           analogVoltage;
  int           analogCurrent;
  int           sampleVcorr;
  int           sampleVcorrDelayed;
  int           sampleIcorr;
  static int    sampleIcorrDelayed;
  long          sampleP;

  // conversion du courant disponible
  if ( readFlagADC == 0 ) {

    // Configuration pour l'acquisition de la tension
    ADMUX &= B11110000;
    ADMUX |= voltageSensorMUX;
    // Démarrage de la conversion
    ADCSRA |= B01000000;
    //Flag pour indiquer une conversion de tension
    readFlagADC = 9;

    // Si fin d'un cycle secteur, transfert de la suite des données statistiques
    if ( stats_ready_flag == 9 ) {
      stats_sumP = sumP;                  // Somme des échantillons de puissance
      sumP = 0;
      stats_sumVsqr = sumVsqr;            // Somme des échantillons de tension au carré
      sumVsqr = 0;
      stats_sumIsqr = sumIsqr;            // Somme des échantillons de courant au carré
      sumIsqr = 0;
      stats_sumV = sumV;                  // Somme des échantillons de tension
      sumV = 0;
      stats_sumI = sumI;                  // Somme des échantillons de courant
      sumI = 0;
      stats_samples = samples;            // Nombre d'échantillons total
      samples = 0;
      // Flag : toutes les données statistiques ont été transférées
      stats_ready_flag = 1;
    }

    analogCurrent = ADCL | ( ADCH << 8 );

    // Calculs après la conversion du courant
    sampleIcorr = analogCurrent - biasOffset;
    // Echantillon de courant sur lequel on applique un délai
    // pour corriger la phase en fonction d'une estimation linéaire d'évolution
    sampleIcorrDelayed = int ( ( 16 - PHASE_CALIB ) * ( sampleIcorr - lastSampleIcorr ) + ( sampleIcorr << 4  ) ) >> 4;
    // Calcul pour les statistiques
    sumIsqr += long ( sampleIcorr ) * long ( sampleIcorr );
    sumI += long ( sampleIcorr );

    // Calcul pour la mise à jour de biasOffset
    biasOffset = int ( ( fBiasOffset + FILTERROUNDING ) >> FILTERSHIFT );

    // détection des erreurs
    if ( ( analogCurrent == 0 ) || ( analogCurrent == 1023 ) ) error_status |= B00010000;

    lastSampleIcorr = sampleIcorr;
  }

  // Sinon conversion de tension disponible
  else if ( readFlagADC == 9 ) {

    //Configuration pour l'acquisition du courant
    ADMUX &= B11110000;
    ADMUX |= currentSensorMUX;
    //Démarrage de la conversion du courant
    ADCSRA |= B01000000;
    //Flag pour indiquer une conversion de courant
    readFlagADC = 0;

    // Si fin d'un cycle secteur, transfert de la suite des données statistiques
    if ( stats_ready_flag == 9 ) {
      stats_sumP = sumP;                  // Somme des échantillons de puissance
      sumP = 0;
      stats_sumVsqr = sumVsqr;            // Somme des échantillons de tension au carré
      sumVsqr = 0;
      stats_sumIsqr = sumIsqr;            // Somme des échantillons de courant au carré
      sumIsqr = 0;
      stats_sumV = sumV;                  // Somme des échantillons de tension
      sumV = 0;
      stats_sumI = sumI;                  // Somme des échantillons de courant
      sumI = 0;
      stats_samples = samples;            // Nombre d'échantillons total
      samples = 0;
      // Flag : toutes les données statistiques ont été transférées
      stats_ready_flag = 1;
    }

    analogVoltage = ADCL | ( ADCH << 8 );

    // Calculs après la conversion de tension
    sampleVcorr = analogVoltage - biasOffset;
    // Calcul de l'échantillon de tension sur lequel on applique un délai
    // pour corriger la phase en fonction d'une estimation linéaire d'évolution
    sampleVcorrDelayed = int ( ( PHASE_CALIB - 16 ) * ( sampleVcorr - lastSampleVcorr ) + ( sampleVcorr << 4 ) ) >> 4;
    // somme des échantillons de puissance pour le calcul de la puissance active
    sampleP = long ( sampleVcorrDelayed ) * long ( sampleIcorrDelayed );
    periodP += sampleP;

    // détection du passage à 0 et génération du signal de synchronisation (changement d'état)
    if ( sampleVcorr >= 0 )  SYNC_ON;         //digitalWrite ( synchroOutPin, ON  );
    else                     SYNC_OFF;        //digitalWrite ( synchroOutPin, OFF );

    // détection des erreurs
    if ( ( analogVoltage == 0 ) || ( analogVoltage == 1023 ) ) error_status |= B00010000;

    // Calcul pour les statistiques
    sumVsqr += long ( sampleVcorr ) * long ( sampleVcorr );
    sumV += long ( sampleVcorr );

    // Calcul pour la mise à jour de biasOffset
    fBiasOffset += sampleVcorr;

    lastSampleVcorr = sampleVcorr;
    samples ++;
  }
}


//////////////////////////////////////////////////////////////////////////////////////
// ISR ( TIMER1_COMPA_vect )  et  ISR ( TIMER1_OVF_vect )                           //
// Interrupt service routine Timer1 pour générer le pulse de déclenchement du TRIAC //
//////////////////////////////////////////////////////////////////////////////////////
ISR ( TIMER1_COMPA_vect ) {   // TCNT1 = OCR1A : instant de déclenchement du SSR/TRIAC

  TRIAC_ON;
  // chargement du compteur pour que le pulse SSR/TRIAC s'arrête à l'instant PULSE_END
  // relativement au passage à 0 (nécessite PULSE_END > OCR1A)
  TCNT1 = 65535 - ( PULSE_END - OCR1A );
}


ISR ( TIMER1_OVF_vect ) {     // TCNT1 overflow, instant PULSE_END

  TRIAC_OFF;
  TCCR1B = 0x00;              // arrêt du Timer
  TCNT1 = 0;                  // on remet le compteur à 0 par sécurité
}


//////////////////////////////////////////////////////////////////////////////////////
// ISR ( TIMER2_COMPA_vect )  et  ISR ( TIMER2_OVF_vect )                           //
// Interrupt service routine Timer2 pour l'antirebond de la détection des pulses    //
//////////////////////////////////////////////////////////////////////////////////////
ISR ( TIMER2_COMPA_vect ) {   // TCNT2 = OCR2A : 4 ms après le front descendant du pulse externe

  if ( digitalRead ( pulseExternalPin ) == ON ) { // le pulse n'est pas maintenu à l'état bas, fausse détection
    // on arrête le timer
    TCCR2B = 0x00;              // arrêt du Timer
    TCNT2 = 0;                  // on remet le compteur à 0
  }
}


ISR ( TIMER2_OVF_vect ) {     // TCNT2 overflow, 16 ms après le front descendant du pulse externe

  static unsigned long  refTime = 0;
  TCCR2B = 0x00;              // arrêt du Timer
  TCNT2 = 0;                  // on remet le compteur à 0
  if ( digitalRead ( pulseExternalPin ) == OFF ) {  // le pulse est toujours à l'état bas, c'est un vrai pulse
    indexImpulsion ++;                              // on incrémente le compteur de pulse
    deltaTimeImpulsion = millis ( ) - refTime;      // calcul du temps depuis la dernière impulsion
    refTime = millis ( );
  }
}


///////////////////////////////////////////////////////////////////////
// pulseExternalInterrupt                                            //
// Interrupt service routine de comptage de pulse externe INT0       //
///////////////////////////////////////////////////////////////////////
void pulseExternalInterrupt ( void ) {

  if ( ( TCNT2 == 0 ) && ( TCCR2B == 0 ) ) TCCR2B = 0x07;        // si Timer2 est arrêté on lance le Timer2 pour la vérification de la validité du pulse
  else {                                                         // on stoppe la vérification en cours car de manière évidente le précédent pulse n'était pas valide
                                                                 // et on redémarre la vérification pour la nouvelle détection
    TCCR2B = 0x00;              // arrêt du Timer
    TCNT2 = 0;                  // on remet le compteur à 0
    TCCR2B = 0x07;              // on redémarre le Timer
  }
}
