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
 - 32 : Latch registres à décalage
 - 33 : Clock registres à décalage
 - 34 : Data registres à décalage
 - 35 : Arrêt d'urgence (trouver un bouton du style AU avec un état enfoncé permanent)

# Mise à jour liste machines

Suivre ces étapes : https://randomnerdtutorials.com/esp32-vs-code-platformio-spiffs/

# Prochaines étapes

- Intégrer les écrans que j'avais dessiné :
  - Itinéraire d'aiguillages dans un fichier JSON en utilisant la librairie arduinojson (https://arduinojson.org/)
    - compléter le fichier
    - rechercher un itinéraire dans le fichier
  - Idée pour économiser des entrées pour les boutons : faire un bouton pour chaque entrées puis un bouton pour 2 entrée (déjà utilisées auparavent). Ça revient un peu à un codage binaire au niveau des boutons
- Ajouter la gestions des boutons avec les registres à décalage (ajouter des boutons à la maquette)
- Ajouter un bouton d'arrêt d'urgence (trouver un bouton qui va bien)
- Séparer la télécommande entre la partie centrale et le plan des aiguillages
