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
 - 4 : Arrêt d'urgence (trouver un bouton du style AU avec un état enfoncé permanent)

## Mise à jour liste machines

Suivre ces étapes : https://randomnerdtutorials.com/esp32-vs-code-platformio-spiffs/

## Prochaines étapes

- Ajouter des boutons raccourcies pour les fonctions (Feux et F1 à F6) => voir fichier ES_ESP32
  - Utiliser la librairie Keypad
  - Besoin de 8 entrées permettant le pull-up
    - On recycle les entrées des boutons existants (16,17,13,14,15)
    - L'arrêt d'urgence passe sur une entrée sans pull-up (35, libère la 4). Il faut prévoir un résistance pull-up en plus
    - On utilise en plus les pin 5 et 19 (libéré par l'écran)
  - Faire la répartition suivante

| Feux             | F1                        | F2                           | F3                   |
| ---------------- | ------------------------- | ---------------------------- | -------------------- |
| F4               | F5                        | F6                           | F7                   |
| F8               | F9                        | Rotonde (Alt. F10)           | Fonctions (Alt. F11) |
| Echap (Alt. F12) | Select Machine (Alt. F13) | Select Aiguillage (Alt. F14) | Alternative          |

- Permettre la sélection de la vitesse dans le menu des fonctions
- Retenir la dernière machine sélectionnée pour pouvoir la re-sélectionner automatiquement après un reboot
- Séparer la télécommande entre la partie centrale et le plan des aiguillages
  - Ajouter une détection si l'extension est connectée ou non. Utiliser la pin 36, nécessite une résistance pull-up
  - Désactiver la lecture des registres si l'extension n'estpas connectée
- Correction de bugs
  - Lors de la réalisation des trajectoires il arrive que l'application reste coincé sur l'affichage de la trajectoire alors que tous les aiguillages ont été traités
  - Sur l'écrande la rotonde, le sélecteur peut entrainer un reset du microcontrôleur
