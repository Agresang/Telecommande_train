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

- Debugger le passage halfquad/sigle edge => fonctionne mais au bout d'un moment il y a trop d'encodeur. La fonction ne permet plus d'en ajouter. A voir s'il est possible d'en supprimer
- Intégrer les écrans que j'avais dessiné :
  - Itinéraire d'aiguillages
  - Rotonde (essayer d'optimiser le temps de réaction)
  - Fonctions
- Ajouter la gestions des boutons avec les registres à décalage
- Ajouter un bouton d'arrêt d'urgence
