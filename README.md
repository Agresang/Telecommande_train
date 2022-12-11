# Telecommande_train

## Connexions

Voir visuel dans le fichier calc du dossier `Train\Télécommande train\ES_ESP32.ods`

| Ecran | ESP32 |
| ----- | ----- |
| GND   | GND   |
| VCC   | 3.3V  |
| SCL   | D18   |
| SDA   | D23   |
| RES   | D22   |
| DC    | D21   |

19 est utilisé par la librairie de l'écran mais pas connectée physiquement.
Je l'ai commenté dans `User_Setup.h` (ligne 191) de la librairie TFT.

 - 25 : DT codeur
 - 26 : CLK codeur
 - 27 : SW codeur
 - 16 : Sélection machine
 - 17 : Echap
 - 13 : Sélection aiguillage
 - 14 : Sélection rotonde
 - 15 : Sélection fonction
 - 32 : Latch registres à décalage
 - 33 : Clock registres à décalage
 - 34 : Data registres à décalage
 - 35 : Arrêt d'urgence (trouver un bouton du style AU avec un état enfoncé permanent)
 - 4,5,13,14,15,16,17,19 : Keypad
 - 36 : entrée analogique tension batterie

## Disposition des touches du keypad

| Feux             | F1                        | F2                           | F3                   |
| ---------------- | ------------------------- | ---------------------------- | -------------------- |
| F4               | F5                        | F6                           | F7                   |
| F8               | F9                        | Rotonde (Alt. F10)           | Fonctions (Alt. F11) |
| Echap (Alt. F12) | Select Machine (Alt. F13) | Select Aiguillage (Alt. F14) | Alternative          |

## Mise à jour liste machines

Suivre ces étapes : https://randomnerdtutorials.com/esp32-vs-code-platformio-spiffs/

## Prochaines étapes

- Passer la surveillance des boutons du keypad dans un programme parallèle ? (Tester au préalable combien de temps ça prend)
- Ajouter une batterie ainsi que la surveillance de sa tension (voir plus haut pour la pin utilisée)
- Séparer la télécommande entre la partie centrale et le plan des aiguillages
  - Ajouter une détection si l'extension est connectée ou non. Utiliser la pin 36, nécessite une résistance pull-up
  - Désactiver la lecture des registres si l'extension n'est pas connectée
- Correction de bugs
  - Lors de la réalisation des trajectoires il arrive que l'application reste coincé sur l'affichage de la trajectoire alors que tous les aiguillages ont été traités
  - Sur l'écrande la rotonde, le sélecteur peut entrainer un reset du microcontrôleur
    - J'ai tout testé côté tâche parallèle, il n'y a rien à faire
    - Tenter une autre méthode de dessein : utiliser uniquement des carés/droites de LVGL
