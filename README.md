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
* En développement : compteurs journaliers, historique des données

## Synoptique
![MaxPV! synoptique](images/synoptique.png)

## Installation
### Pré-requis
L'installation de MaxPV! sur votre routeur EcoPV se fait par reprogrammation de l'Arduino Nano et du Wemos/ESP8266 par USB. Vous aurez besoin d'utiliser l'IDE Arduino avec le support pour les cartes ESP8266.
Le fonctionnement de MaxPV! nécessite une connexion à votre réseau local en Wifi et avec une adresse IP statique. En cours d'installation, vous aurez besoin de vous connecter temporairement en Wifi au Wemos à l'aide d'un ordinateur portable ou d'un téléphone.

### Programmation de l'Arduino Nano
* **ATTENTION** : prenez note des paramètres du routeur ! Ceux-ci seront effacés et devront être ré-introduits à la fin de l'installation !
* Ouvrez le programme *EcoPV3.ino* dans l'IDE de l'Arduino configuré pour la programmation de l'Arduino Nano.
* Si vous utilisez l'écran oLed, dé-commentez la ligne 47 du code et vérifiez que la bibliothèque SSD1306Ascii est bien installée.
* Téléchargez le programme dans l'Arduino Nano.

### Programmation du Wemos
* Configurez l'IDE Arduino sur la carte Wemos avec les paramètres suivants : 
  * Flash size: 4 MB (FS: 1MB, OTA: 1019KB),
  * Erase Flash: All Flash Contents.
* Installez la librairie **AsyncElegantOTA** à partir du gestionnaire de librairies.
* Installez les 2 librairies disponibles dans le répertoire **"Librairies IDE"**.
* Ouvrez le programme *amorceESP.ino* et téléchargez le dans le Wemos.
* A l'aide d'un ordinateur connectez-vous au réseau Wifi MaxPV créé sur le Wemos et allez à la page http://192.168.4.1
* Une page intitulée elegantOTA s'ouvre à l'écran.
* Téléchargez d'abord le **Filesystem** *MaxPV3_filesystem.bin* disponible dans le répertoire **"Binaires MaxPV"**.
* Le Wemos reboote, connectez-vous de nouveau au réseau Wifi MaxPv.
* Téléchargez ensuite le **Firmware** *MaxPV3_firmware.bin* disponible dans le répertoire **"Binaires MaxPV"**.
* Le Wemos reboote, connectez-vous de nouveau au réseau Wifi MaxPv.
* Un portail captif devrait s'ouvrir, s'il ne s'ouvre pas automatiquement, connectez-vous à l'adresse http://192.168.4.1

![MaxPV! captif portal](images/captif.png)
* Réalisez votre configuration Wifi et votre configuration IP. Pour les adresses DNS, indiquez l'adresse de votre 'Box internet' comme DNS1, et l'adresse 8.8.8.8 comme DNS2.
* Vous pouvez maintenant ré-installer l'Arduino Nano et le Wemos sur la carte électronique de votre routeur.

## Premier démarrage
* Vérifiez que votre ordinateur / téléphone est bien connecté à votre réseau local.
* Connectez-vous à l'adresse IP statique que vous avez attribuée au Wemos au cours de l'installation.
* La page d'accueil de MaxPV! s'ouvre. Vous pouvez vérifiez que l'Arduino Nano fonctionne correctement par l'indication **Routeur running** en haut de la page.
* Si vous êtes nouvel utilisateur de MaxPV!, rendez-vous sur la page **Assistant de configuration** et laissez-vous guider.
* Si vous voulez entrer manuellement les paramètres du routeur que vous utilisiez précédemment :
  * Rendez-vous sur la page **Paramètrage avancé**,
  * Entrez vos paramètres un par un, en validant chaque paramètre,
  * **Nouveauté** : il y a 2 nouveaux paramètres :
    * **P_INSTALLPV** : puissance de votre installation photovoltaïque en Wc,
    * **CNT_CALIB** : poids des impulsions en Wh du compteur d'impulsion pour la mesure de la production PV.
  * **Enregistrez la configuration et redémarrez le routeur**.
* Votre routeur MaxPV! est maintenant opérationnel !

## Mises à jour
Les mises à jour se font par la page **Update** de l'interface. **Attention** : la mise à jour du filesystem nécessite de reconfigurer la connexion Wifi du Wemos comme décrit précédemment.

## Installation avancée
Si vous souhaitez compiler le firmware et le filesystem du Wemos pour réaliser une installation avancée, les codes sources sont disponibles dans le répertoire **"MaxPV3"**.

## API
L'API permet d'interfacer MaxPV! avec des systèmes externes comme un système de domotique. L'API a été revue en profondeur comparativemlent à la version précédente de EcoPV. L'API est décrite dans la [Documentation API](Documentation%20API/API_MaxPV.pdf).

## Accès au système de fichiers
Vous pouvez accéder au système de fichier du Wemos par connexion FTP sur le port 21. L'identifiant est *maxpv*, mot de passe *maxpv*. ATTENTION : le serveur ne supporte qu'une seule connexion simultanée, veillez à configurer votre client FTP en conséquence.

## Accès TELNET
Un accès TELNET est possible sur le port 23. Vous aurez alors accès à des informations de debug, en particulier l'échange de messsages entre l'Arduino Nano et le Wemos.


