# Telecommande_train

# Connexions

| Ecran   |   ESP32 |
| - | - |
| GND     |  GND    |
| VCC     |  3.3V   |
| SCL     |  D18    |
| SDA     |  D23    |
| RES     |  D22    |
| DC      |  D21    |

19 est utilisé par la librairie de l'écran mais pas connectée physiquement

 - 25 : DT codeur
 - 26 : CLK codeur
 - 27 : SW codeur
 - 16 : Sélection machine
 - 17 : OK
 - 13 : Sélection aiguillage
 - 14 : Sélection rotonde
 - 15 : Sélection fonction

# Mise à jour liste machines

Suivre ces étapes : https://randomnerdtutorials.com/esp32-vs-code-platformio-spiffs/

# Prochaines étapes

- Sauter un cran sur le codeur pour la sélection des machines et aiguillages
- Intégrer les écrans que j'avais dessiné :
  - Itinéraire d'aiguillages
  - Rotonde
  - Fonctions (utiliser un button matrix avec les touches de navigation, ou une list ?)
- Ajouter la gestions des boutons avec les registres à décalage
- Ajouter un bouton d'arrêt d'urgence
