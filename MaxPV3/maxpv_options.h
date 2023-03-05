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
// ******************            OPTIONS DE COMPILATION                ***************
// ***********************************************************************************

// ***********************************************************************************
// ******************     Dé-commenter pour activer le serveur FTP     ***************
// ***********************************************************************************

// ATTENTION : le serveur FTP n'est pas asynchrone

//#define MAXPV_FTP_SERVER

// Voir maxpv_defines.h pour la configuration du serveur FTP



// ***********************************************************************************
// ******************         AUTRES OPTIONS DE COMPILATION            ***************
// ******************             !! NE PAS MODIFIER !!                ***************
// ***********************************************************************************

// Pas de debug série pour MQTT
#define _ASYNC_MQTT_LOGLEVEL_       0  
// Pas de debug série pour HTTP
#define _ASYNC_HTTP_LOGLEVEL_       0


// Permet l'utilisation de connexions TCP sécurisées
#define ASYNC_TCP_SSL_ENABLED       false
