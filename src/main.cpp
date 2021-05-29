#include <Arduino.h>
#include <lvgl.h>
#include <TFT_eSPI.h>
#include <ESP32Encoder.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiGeneric.h>
#include <Z21.h>
#include <ArduinoOTA.h>
#include <SPIFFS.h>
#include <SD.h>

#define BT_STOP               27
#define BT_SELECT_MACHINE     16
#define BT_OK                 17
#define BT_SELECT_AIGUILLAGE  13
#define BT_SELECT_ROTONDE     14
#define BT_SELECT_FONCTION    15

#define TIME_AIGUILLAGE_SELECT  1000
#define TIME_RESET  500
#define TIME_SUBSCRIBE_RENOUVELLEMENT 60000

// Initialisation codeur
ESP32Encoder encoder;
long oldPosition  = -999;
long newPosition = 0;

// Configuration wifi
const char* ssid = "Z21_3677";
const char* password = "06338855";
const unsigned int localPort = 21105;
const char* ipAdress = "192.168.0.111";

Z21 myZ21;

byte numMachine = 5;
unsigned int numAiguillage = 5;
byte boutonFonction = 0;
byte etatCircuit = 1;
byte premiereInfoMachine = 0;     // 0: Pas besoin d'information  1: Attente retour information   2: Information disponible
bool lastEtatBoutonStop = true;
bool lastEtatBoutonSelectMachine = true;
bool lastEtatBoutonOK = true;
bool lastEtatBoutonSelectAiguillage = true;
bool lastEtatBoutonSelectRotonde = true;
bool lastEtatBoutonSelectFonction = true;
bool BtStopPressed = false;
bool BtSelectMachinePressed = false;
bool BtOKPressed = false;
bool BtSelectAiguillagePressed = false;
bool BtSelectRotondePressed = false;
bool BtSelectFonctionPressed = false;
bool BtSelectModified = false;
bool BtItinerairePressed = false;

int etatEcran = 0;
int lastEtatEcran = 0;

unsigned long timer = 0;
unsigned long timerAiguillage = 0;
unsigned long timerReset = 0;

TFT_eSPI tft = TFT_eSPI(); /* TFT instance */
static lv_disp_buf_t disp_buf;
static lv_color_t buf[LV_HOR_RES_MAX * 10];

#if USE_LV_LOG != 0
/* Serial debugging */
void my_print(lv_log_level_t level, const char * file, uint32_t line, const char * dsc)
{

    Serial.printf("%s@%d->%s\r\n", file, line, dsc);
    Serial.flush();
}
#endif

/* Display flushing */
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors(&color_p->full, w * h, true);
    tft.endWrite();

    lv_disp_flush_ready(disp);
}

// Variables globales affichage
lv_obj_t * gauge1;
lv_obj_t * speedLabel;
lv_obj_t * machineNameLabel;
lv_obj_t * rollerMachine;
lv_obj_t * ecranAcceuil;
lv_obj_t * ecranVitesse;
lv_obj_t * ecranMachine;
lv_obj_t * ecranAiguillage;
lv_obj_t * rollerAiguillage;
lv_obj_t * aiguillagePosDroit;
lv_obj_t * aiguillagePosDevie;
lv_obj_t * mbox;
lv_obj_t * ecranFonction;
lv_obj_t * btnmFonction;
lv_group_t * g;

void updateEncoder(){
  //Lecture position codeur
  newPosition = -1 * encoder.getCount();
  //Serial.println(newPosition);

  // S'il y a un changement de position codeur
  BtSelectModified = false;
  if (newPosition != oldPosition){
    BtSelectModified = true;
  }
}

void updateBouton(){
  // Lecture entrées
  bool etatBoutonStop = digitalRead(BT_STOP);
  bool etatBoutonSelectMachine = digitalRead(BT_SELECT_MACHINE);
  bool etatBoutonOK = digitalRead(BT_OK);
  bool etatBoutonSelectAiguillage = digitalRead(BT_SELECT_AIGUILLAGE);
  bool etatBoutonSelectRotonde = digitalRead(BT_SELECT_ROTONDE);
  bool etatBoutonSelectFonction = digitalRead(BT_SELECT_FONCTION);
  // Initialisation One Shot
  BtStopPressed = false;
  BtSelectMachinePressed = false;
  BtOKPressed = false;
  BtSelectAiguillagePressed = false;
  BtSelectRotondePressed = false;
  BtSelectFonctionPressed = false;
  // Détection front montant
  if((etatBoutonSelectMachine != lastEtatBoutonSelectMachine) && !etatBoutonSelectMachine){
    BtSelectMachinePressed = true;
  }
  if((etatBoutonOK != lastEtatBoutonOK) && !etatBoutonOK){
    BtOKPressed = true;
  }
  if((etatBoutonSelectAiguillage != lastEtatBoutonSelectAiguillage) && !etatBoutonSelectAiguillage){
    BtSelectAiguillagePressed = true;
  }
  if((etatBoutonSelectRotonde != lastEtatBoutonSelectRotonde) && !etatBoutonSelectRotonde){
    BtSelectRotondePressed = true;
  }
  if((etatBoutonSelectFonction != lastEtatBoutonSelectFonction) && !etatBoutonSelectFonction){
    BtSelectFonctionPressed = true;
  }
  if((etatBoutonStop != lastEtatBoutonStop) && !etatBoutonStop){
    BtStopPressed = true;
  }
  // MAJ état précédent
  lastEtatBoutonStop = etatBoutonStop;
  lastEtatBoutonSelectMachine = etatBoutonSelectMachine;
  lastEtatBoutonOK = etatBoutonOK;
  lastEtatBoutonSelectAiguillage = etatBoutonSelectAiguillage;
  lastEtatBoutonSelectRotonde = etatBoutonSelectRotonde;
  lastEtatBoutonSelectFonction = etatBoutonSelectFonction;
  // Gestion des boutons itiléraires (à modifier plus tard)
  BtItinerairePressed = false;
}

void ecritValeur(lv_obj_t * lvglLabel, int valeur){
  char buffer[5];
  itoa(valeur,buffer,10);
  lv_label_set_text(lvglLabel, buffer);
}

void majEcran(){
  // Appeler cette fonction avant la connexion à la wifi puis périodiquement dans la boucle
  if(etatEcran == 0){
    // Initialisation écran d'acceuil connection wifi
    // Initialisation de tous les écrans (si ça marche)

    // Init les styles
    static lv_style_t style_small;
    lv_style_init(&style_small);
    lv_style_set_text_font(&style_small, LV_STATE_DEFAULT, &lv_font_montserrat_14);
    //---------------------------
    static lv_style_t style_medium;
    lv_style_init(&style_medium);
    lv_style_set_text_font(&style_medium, LV_STATE_DEFAULT, &lv_font_montserrat_26);
    //---------------------------
    static lv_style_t style_big;
    lv_style_init(&style_big);
    lv_style_set_text_font(&style_big, LV_STATE_DEFAULT, &lv_font_montserrat_30);

    // Ecran d'acceuil
    ecranAcceuil = lv_obj_create(NULL, NULL);
    lv_scr_load(ecranAcceuil);
    lv_obj_t * labelBienvenue = lv_label_create(ecranAcceuil, NULL);
    lv_label_set_text(labelBienvenue, "Connexion wifi...");
    lv_obj_add_style(labelBienvenue, LV_OBJ_PART_MAIN, &style_big);
    lv_obj_t * loader = lv_spinner_create(ecranAcceuil, NULL);
    lv_obj_set_size(loader, 150, 150);
    lv_obj_align(loader, NULL, LV_ALIGN_CENTER, 0, 0);

    // Ecran vitesse
    ecranVitesse = lv_obj_create(NULL, NULL); 
    /*Describe the color for the needles*/
    static lv_color_t needle_colors[2];
    needle_colors[0] = LV_COLOR_BLUE;
    needle_colors[1] = LV_COLOR_ORANGE;
    /*Create a gauge*/
    gauge1 = lv_gauge_create(ecranVitesse, NULL);
    lv_gauge_set_needle_count(gauge1, 2, needle_colors);
    lv_obj_set_size(gauge1, 230, 230);
    lv_obj_align(gauge1, NULL, LV_ALIGN_CENTER, 0, 0);
    lv_gauge_set_range(gauge1, -128, 128);
    lv_obj_add_style(gauge1, LV_OBJ_PART_MAIN, &style_small);
    /*Set the values*/
    lv_gauge_set_value(gauge1, 0, 0);
    lv_gauge_set_value(gauge1, 1, 0);
    // Label visualisation vitesse (fils de gauge1)
    speedLabel = lv_label_create(gauge1, NULL);
    lv_obj_align(speedLabel, gauge1, LV_ALIGN_IN_BOTTOM_MID, 0, -50);
    lv_label_set_align(speedLabel, LV_LABEL_ALIGN_CENTER);
    lv_label_set_text(speedLabel, "0");
    lv_obj_add_style(speedLabel, LV_OBJ_PART_MAIN, &style_big);
    // Label nom de machine
    machineNameLabel = lv_label_create(ecranVitesse, NULL);
    lv_obj_align(machineNameLabel, ecranVitesse, LV_ALIGN_IN_BOTTOM_MID, 0, 0);
    lv_label_set_text(machineNameLabel, "-");
    lv_obj_add_style(machineNameLabel, LV_OBJ_PART_MAIN, &style_medium);
    lv_label_set_align(machineNameLabel, LV_LABEL_ALIGN_CENTER);
    
    // Ecran machine
    ecranMachine = lv_obj_create(NULL, NULL);
    rollerMachine = lv_roller_create(ecranMachine, NULL);
    lv_obj_add_style(rollerMachine, LV_OBJ_PART_MAIN, &style_medium);
    if(!SPIFFS.begin(true)){
      Serial.println("An Error has occurred while mounting SPIFFS");
      return;
    }
    File file = SPIFFS.open("/Machines.txt");
    if(!file){
      Serial.println("Failed to open file for reading");
      return;
    }
    char listMachines[1024] = "";
    int fileLength = file.available();
    for(int i=0; i<fileLength; i++){
      char lettre = file.read();
      strncat(listMachines, &lettre, 1);
    }
    file.close();
    //Serial.println(listMachines);
    lv_roller_set_options(rollerMachine,
                        listMachines,
                        LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(rollerMachine, 5);
    lv_obj_align(rollerMachine, NULL, LV_ALIGN_CENTER, 0, 0);
    lv_roller_set_auto_fit(rollerMachine, true);

    // Ecran aiguillage
    ecranAiguillage = lv_obj_create(NULL, NULL);
    // Roller sélection aiguillage
    rollerAiguillage = lv_roller_create(ecranAiguillage, NULL);
    lv_obj_add_style(rollerAiguillage, LV_OBJ_PART_MAIN, &style_medium);
    char listAiguillage[1024] = "0\n";
    for(int i=1; i<50; i++){
      char buff[16];
      itoa(i,buff,10);
      strncat(buff, "\n", 1);
      strcat(listAiguillage, buff);
    }
    strcat(listAiguillage, "50");
    //Serial.println(listAiguillage);
    lv_roller_set_options(rollerAiguillage,
                        listAiguillage,
                        LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(rollerAiguillage, 4);
    lv_obj_align(rollerAiguillage, NULL, LV_ALIGN_CENTER, 0, 0);
    // Label position aiguillage droit
    aiguillagePosDroit = lv_led_create(ecranAiguillage, NULL);
    lv_obj_align(aiguillagePosDroit, NULL, LV_ALIGN_IN_LEFT_MID, 20, 0);
    lv_led_off(aiguillagePosDroit);
    // Label position aiguillage dévié
    aiguillagePosDevie = lv_led_create(ecranAiguillage, NULL);
    lv_obj_align(aiguillagePosDevie, NULL, LV_ALIGN_IN_RIGHT_MID, -20, 0);
    lv_led_off(aiguillagePosDevie);

    // Ecran fonctions
    ecranFonction = lv_obj_create(NULL, NULL);
    // Matrice de boutons
    btnmFonction = lv_btnmatrix_create(ecranFonction, NULL);
    static const char * btnm_map[] = {"Feux", "\n",
                                  "1", "2", "3", "4", "5", "\n",
                                  "6", "7", "8", "9", "10", "\n",
                                  "11", "12", "13", "14", "15", "\n",
                                  "16", "17", "18", "19", "20", ""};
    lv_btnmatrix_set_map(btnmFonction, btnm_map);
    lv_btnmatrix_set_btn_ctrl_all(btnmFonction, LV_BTNMATRIX_CTRL_CHECKABLE);
    lv_obj_align(btnmFonction, NULL, LV_ALIGN_CENTER, 0, 0);
    g = lv_group_create();
    lv_group_add_obj(g, btnmFonction);

    // Passage à l'étape 10 si la wifi est connectée
    etatEcran = 10;
    
  } else if(etatEcran == 10){
    // Initialisation de l'écran vitesse
    lv_scr_load(ecranVitesse);
    lv_roller_set_selected(rollerMachine, numMachine, LV_ANIM_OFF);
    char buff[32];
    lv_roller_get_selected_str(rollerMachine, buff, 32);
    lv_label_set_text(machineNameLabel, buff);
    lv_label_set_align(machineNameLabel, LV_LABEL_ALIGN_CENTER);
    lv_obj_align(machineNameLabel, ecranVitesse, LV_ALIGN_IN_BOTTOM_MID, 0, 0);
    encoder.attachHalfQuad(25, 26);
    encoder.setFilter(250);
    encoder.setCount(0);
    oldPosition = 0;
    myZ21.AskMachineInfo();
    premiereInfoMachine = 1;
    
    // Passage à l'étape 11
    etatEcran = 11;
    
  } else if(etatEcran == 11){
    // Maintient de l'écran vitesse
    // Mise à jour des variables de l'écran et traitement des informations

    // Positionnement du codeur sur la vitesse actuelle de la machine
    if(premiereInfoMachine == 2){
      int realSpeed = myZ21.getMachineRealSpeed();
      encoder.setCount(realSpeed);
      oldPosition = realSpeed;
      premiereInfoMachine = 0;  // Info traitée
    }
    
    // Arrêt sur un appuie du bouton STOP
    if(BtStopPressed){
      encoder.setCount(0);
    }

    //Modification de la vitesse si on tourne la roue
    if(BtSelectModified){
      int vitesse = -1*newPosition;
      lv_gauge_set_value(gauge1, 0, vitesse);
      ecritValeur(speedLabel, vitesse);
      lv_label_set_align(speedLabel, LV_LABEL_ALIGN_CENTER);
      if(vitesse<128 && vitesse>=-128){
        myZ21.SetMachineSpeed(vitesse);
      }
      myZ21.SendMachineCommand();
      oldPosition = newPosition;
    }

    lastEtatEcran = etatEcran;

    // Changement d'étape suivant le bouton pressé
    if(BtSelectMachinePressed){
      etatEcran = 20;
    } else if(BtSelectAiguillagePressed){
      etatEcran = 30;
    } else if(BtItinerairePressed){
      etatEcran = 40;
    } else if(BtSelectRotondePressed){
      etatEcran = 50;
    } else if(BtSelectFonctionPressed){
      etatEcran = 60;
    }
    
  } else if(etatEcran == 20){
    // Initialisation de l'écran changement de machine
    lv_scr_load(ecranMachine);
    lv_roller_set_selected(rollerMachine, numMachine, LV_ANIM_OFF);
    encoder.attachSingleEdge(25, 26);
    encoder.setFilter(1023);
    encoder.setCount(0);
    oldPosition = 0;
    
    // Passage à l'étape 21
    etatEcran = 21;
    
  } else if(etatEcran == 21){
    // Maintient de l'écran de changement de machine
    // Mise à jour des variables de l'écran et traitement des informations
    if(BtSelectModified){
      // MAJ trame
      numMachine = numMachine - (newPosition - oldPosition);
      if(numMachine<1){
        numMachine = 1;
      } else if(numMachine > 128){
        numMachine = 128;
      }
      oldPosition = newPosition;
      myZ21.SelectMachine(numMachine);
      lv_roller_set_selected(rollerMachine, numMachine, LV_ANIM_OFF);
      /*Serial.print("Machine ");
      Serial.println(numMachine);*/
    }

    lastEtatEcran = etatEcran;
    
    // Passage à l'étape 10 si changement de machine
    if(BtOKPressed || BtStopPressed){
      etatEcran = 10;
    }
    
  } else if(etatEcran == 30){
    // Initialisation de l'écran aiguillage manuel
    lv_scr_load(ecranAiguillage);
    lv_roller_set_selected(rollerAiguillage, numAiguillage, LV_ANIM_OFF);
    encoder.attachSingleEdge(25, 26);
    encoder.setFilter(1023);
    encoder.setCount(0);
    oldPosition = 0;
    // Demande état aiguillage actif
    myZ21.askAiguillageInfo();

    // Passage à l'étape 31
    etatEcran = 31;
  } else if(etatEcran == 31){
    // Maintient de l'écran aiguillage manuel
    // Mise à jour des variables de l'écran et traitement des informations
    if(BtSelectModified){
      // MAJ trame
      numAiguillage = numAiguillage - (newPosition - oldPosition);
      if(numAiguillage<1){
        numAiguillage = 1;
      } else if(numAiguillage > 128){
        numAiguillage = 128;
      }
      oldPosition = newPosition;
      lv_roller_set_selected(rollerAiguillage, numAiguillage, LV_ANIM_OFF);
      myZ21.SelectAiguillage(numAiguillage);
      myZ21.askAiguillageInfo();
    }

    lastEtatEcran = etatEcran;

    // Passage à l'étape 32
    if(BtOKPressed || BtStopPressed){
      lv_roller_set_visible_row_count(rollerAiguillage, 1);
      lv_obj_align(rollerAiguillage, NULL, LV_ALIGN_CENTER, 0, 0);
      etatEcran = 32;
    }
  } else if(etatEcran == 32){
    // Sélection de l'état de l'aiguillage et envoie de la commande
    if(BtSelectModified){
      // MAJ trame
      bool directionAiguillage = (newPosition - oldPosition) < 0;
      oldPosition = newPosition;
      myZ21.SendAiguillageCommand(directionAiguillage, true);
      myZ21.SendAiguillageCommand(directionAiguillage, false);
      timerAiguillage = millis() + TIME_AIGUILLAGE_SELECT;
      etatEcran = 33;
    }

    // Passage à l'étape 10 si on souhaite quitter, sinon on retourne à 31
    if(BtOKPressed){
      lv_roller_set_visible_row_count(rollerAiguillage, 4);
      lv_obj_align(rollerAiguillage, NULL, LV_ALIGN_CENTER, 0, 0);
      etatEcran = 10;
    } else if(BtStopPressed){
      lv_roller_set_visible_row_count(rollerAiguillage, 4);
      lv_obj_align(rollerAiguillage, NULL, LV_ALIGN_CENTER, 0, 0);
      etatEcran = 31;
    }

  } else if(etatEcran == 33){
    // Attente
    encoder.setCount(0);
    oldPosition = 0;
    if(millis() > timerAiguillage){
      etatEcran = 32;
    }
    
  } else if(etatEcran == 40){
    // Initialisation de l'écran sélection d'ininéraire
    encoder.setCount(0);
    oldPosition = 0;

    // Passage à l'étape 41
    etatEcran = 41;
  } else if(etatEcran == 41){
    // Maintient de l'écran sélection d'ininéraire

    lastEtatEcran = etatEcran;
    
    // Passage à l'étape 10
    if(BtOKPressed){
      etatEcran = 10;
    }
  } else if(etatEcran == 50){
    // Initialisation de l'écran rotonde
    encoder.setCount(0);
    oldPosition = 0;

    //Passage à l'étape 51
    etatEcran = 51;
  } else if(etatEcran == 51){
    // Maintient de l'écran rotonde

    lastEtatEcran = etatEcran;

    // Passage à l'étape 10
    if(BtOKPressed){
      etatEcran = 10;
    }
  } else if(etatEcran == 60){
    // Initialisation de l'écran fonctions
    lv_scr_load(ecranFonction);
    lv_group_focus_obj(btnmFonction);
    lv_btnmatrix_set_focused_btn(btnmFonction, boutonFonction);
    encoder.attachSingleEdge(25, 26);
    encoder.setFilter(1023);
    encoder.setCount(0);
    oldPosition = 0;

    //Passage à l'étape 61
    etatEcran = 61;
  } else if(etatEcran == 61){
    // Maintient de l'écran fonctions
    if(BtSelectModified){
      boutonFonction = boutonFonction - (newPosition - oldPosition);
      if(boutonFonction<0){
        boutonFonction = 0;
      } else if(boutonFonction > 20){
        boutonFonction = 20;
      }
      oldPosition = newPosition;
      lv_btnmatrix_set_focused_btn(btnmFonction, boutonFonction);
    }
    
    lastEtatEcran = etatEcran;
    
    // Passage à l'étape 10
    if(BtOKPressed){
      etatEcran = 10;
    }
  } else if(etatEcran == 70){
    // Initialisation de la pop-up court-jus
    mbox = lv_msgbox_create(lv_scr_act(), NULL);
    if(etatCircuit == 0){
      lv_msgbox_set_text(mbox, "STOP actif");
    } else {
      lv_msgbox_set_text(mbox, "Court-circuit");
    }
    lv_obj_align(mbox, NULL, LV_ALIGN_CENTER, 0, 0);
    
    encoder.setCount(0);
    oldPosition = 0;

    //Passage à l'étape 61
    etatEcran = 71;
  } else if(etatEcran == 71){
    // Maintient de la pop-up court-jus
    
    // Passage à l'étape 10 si le circuit reviens à la normale
    if(etatCircuit == 1){
      etatEcran = lastEtatEcran;
      lv_msgbox_start_auto_close(mbox, 0);
    } else if(BtOKPressed || BtStopPressed){
      // Envoie d'un reset
      etatEcran = 72;
    }
  } else if(etatEcran == 72){
    // Envoie de la commande Reset

    myZ21.resetTrackPower();
    timerReset = millis() + TIME_RESET;
    
    // Passage à l'étape 73 pour attendre un peu
    etatEcran = 73;
    
  } else if(etatEcran == 73){
    // Attente
    if(millis() > timerReset){
      etatEcran = 71;
    }
  }

  // Forçage étape 70
   if(etatCircuit != 1 && (etatEcran < 70 || etatEcran >= 80)){
      // Test
      etatEcran = 70;
    }

  lv_task_handler(); /* let the GUI do its work */
}

void setup()
{
    Serial.begin(115200); /* prepare for possible serial debug */

    // Initialisation entrées
    pinMode(BT_STOP, INPUT);
    pinMode(BT_SELECT_MACHINE, INPUT_PULLUP);
    pinMode(BT_OK, INPUT_PULLUP);
    pinMode(BT_SELECT_AIGUILLAGE, INPUT_PULLUP);
    pinMode(BT_SELECT_ROTONDE, INPUT_PULLUP);
    pinMode(BT_SELECT_FONCTION, INPUT_PULLUP);

    lv_init();

#if USE_LV_LOG != 0
    lv_log_register_print_cb(my_print); /* register print function for debugging */
#endif

    tft.begin(); /* TFT init */
    tft.setRotation(1); /* Landscape orientation */
  
    lv_disp_buf_init(&disp_buf, buf, NULL, LV_HOR_RES_MAX * 10);

    /*Initialize the display*/
    lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = 240;
    disp_drv.ver_res = 240;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.buffer = &disp_buf;
    lv_disp_drv_register(&disp_drv);

    // Init codeur
    // Enable the weak pull up resistors
    ESP32Encoder::useInternalWeakPullResistors=UP;
    // Attache pins for use as encoder pins
    encoder.attachHalfQuad(25, 26);
    // encoder.attachSingleEdge(25, 26); // Passage en single edge au lieu de half quad attachHalfQuad()
    // encoder.setFilter(1023);  // Filtre inutile en half quad
    // set starting count value after attaching
    encoder.setCount(0);

    // Affichage écran acceuil
    majEcran();

    // Connexion Wifi
    WiFi.begin(ssid, password);
    Serial.print("Connexion Wifi");
    while (WiFi.status() != WL_CONNECTED) {
      delay(10);
      lv_task_handler();
    }
    ArduinoOTA.begin();
    myZ21.Setup(ipAdress, localPort);
    myZ21.SelectMachine(numMachine);
    myZ21.SelectAiguillage(numAiguillage);
    myZ21.SubscribeInfo();
}

void loop()
{
  //Etat bouton
  updateBouton();

  //Etat encodeur
  updateEncoder();

  // Renouvellement souscription Z21
  unsigned long now = millis();
  if(now >= timer){
    myZ21.SubscribeInfo();
    timer = now + TIME_SUBSCRIBE_RENOUVELLEMENT;
  }

  // Analyse des informations reçues
  if(myZ21.Run() > 0){
    // Récupération vitesse machine
    int realSpeed = myZ21.getMachineRealSpeed();
    if(premiereInfoMachine == 1){   // Si demande d'une vitesse machine
      premiereInfoMachine = 2;      // Indication qu'une nouvelle vitesse est disponible
    }
    lv_gauge_set_value(gauge1, 1, realSpeed);
    /*Serial.print("Vitesse : ");
    Serial.println(realSpeed);*/
    // Récupération état aiguillage
    byte etatAiguillage = myZ21.getAiguillageState();
    if(etatAiguillage == 1){
      lv_led_off(aiguillagePosDroit);
      lv_led_on(aiguillagePosDevie);
    } else if(etatAiguillage == 2){
      lv_led_on(aiguillagePosDroit);
      lv_led_off(aiguillagePosDevie);
    } else {
      lv_led_off(aiguillagePosDroit);
      lv_led_off(aiguillagePosDevie);
    }
    // Récupération état circuit
    etatCircuit = myZ21.getCircuitState();
  }
  
  majEcran();
  ArduinoOTA.handle();
}
