Vous aimez ce projet ? Vous pouvez faire une donation pour m'encourager grâce au bouton **Sponsor** dans la barre ci-dessus !
Merci à tous pour vos messages et votre soutien au projet !

# MaxPV!
MaxPV! est une nouvelle interface pour EcoPV, compatible avec les montages EcoPV possédant une liaison wifi via un Wemos ESP8266. Le coeur du routeur est inchangé. MaxPV! apporte une interface Web de configuration et de visualisation du fonctionnement, ainsi qu'une nouvelle API.

![MaxPV! main page](images/mainpage.png)  
MaxPV! héritant du concept d'EcoPV, il sera utile de se référer au dépôt EcoPV : https://github.com/Jetblack31/EcoPV

La lecture de ces fils de discussion est plus que recommandée pour la mise en oeuvre :  
Forum photovoltaïque, discussion sur EcoPV : https://forum-photovoltaique.fr/viewtopic.php?f=110&t=42721  
Forum photovoltaïque, réalisation d'un PCB : https://forum-photovoltaique.fr/viewtopic.php?f=110&t=42874  
Forum photovoltaïque, montage du PCB : https://forum-photovoltaique.fr/viewtopic.php?f=110&t=43197  

## Mise en garde  
Les programmes et les schémas proposés ont une vocation informative et pédagogique. Ils ont été testés avec succès par les auteurs. Cependant les auteurs de ces programmes et de ces schémas déclinent toute responsabilité. Les auteurs ne pourraient être tenus pour responsables du fonctionnement et des conséquences de l'utilisation des programmes et des schémas mis à disposition.  
Intervenir sur des circuits électriques est dangereux et nécessite le recours à une personne qualifiée et le respect strict des normes de sécurité et de protection en vigueur.

## Changements par rapport à EcoPV et caractéristiques
* Même qualité de routage du surplus photovoltaïque que la version précédente.
* Abandon de l'option de communication MySensors.
* Abandon de l'utilisation du shield ethernet. Toutefois, celui-ci peut rester en place sur le circuit mais il ne sera plus utilisé.
* Interface web responsive avec visualisation graphique.
* Installation et paramétrage réseau facilitée.
* Mise à jour du Wemos en OTA.
* Assistant de configuration des paramètres du routeur.
* Amélioration des index de comptage de la puissance.
* Le SSR et le relais secondaire de délestage peuvent être forcés sur arrêt permanent, marche permanente, en plus du mode automatique (par défaut).
* Nouvelle API pour communiquer avec un serveur domotique : [Documentation API](Documentation%20API/API_MaxPV.pdf).

## Synoptique
![MaxPV! synoptique](images/synoptique.png)

## Installation
### Pré-requis
L'installation de MaxPV! sur votre routeur EcoPV se fait par reprogrammation de l'Arduino Nano et du Wemos/ESP8266 par USB. Vous aurez besoin d'utiliser l'IDE Arduino avec le support pour les cartes ESP8266.
Le fonctionnement de MaxPV! nécessite une connexion à votre réseau local en Wifi et avec une adresse IP statique. En cours d'installation, vous aurez besoin de vous connecter temporairement en Wifi au Wemos à l'aide d'un ordinateur portable ou d'un téléphone.

### Programmation de l'Arduino Nano
* **ATTENTION** : prenez note des paramètres du routeur ! Ceux-ci seront effacés et devront être ré-introduit à la fn de l'installation !
* Ouvrez le programme **EcoPV3.ino** dans l'IDE de l'Arduino configuré pour la programmation de l'Arduino Nano.
* Si vous utilisez l'écran oLed, dé-commentez la ligne 47 du code et vérifiez que la bibliothèque SSD1306Ascii est bien installée.
* Télécharger le programme dans l'Arduino Nano


