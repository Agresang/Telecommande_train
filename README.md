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

# Prochaines étapes

- remplacer la sélection machine (et aiguillage) par une liste défilante (widget roller?)
    - Utiliser les KEY pour naviguer avec la fonction lv_group_send_data
- intégrer les écrans que j'avais dessiné :
  - aiguillage un par un
  - rotonde
  - fonctions
- ajouter une base de donnée de numéro/nom machine
- ajouter la gestions des boutons avec les registres à décalage
- ajouter des fonctions à la Z21
- récupérer l'état de la Z21 (arrêt d'urgence, reset arrêt d'urgence, court-circuit)
