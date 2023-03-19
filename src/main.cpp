#include <Arduino.h>
#include <lvgl.h>
#include <TFT_eSPI.h>
#include <ESP32Encoder.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiGeneric.h>
#include <Z21.h>
#include <inregister.h>
#include <ArduinoOTA.h>
#include <SPIFFS.h>
#include <SD.h>
#include <rotonde.h>
#include <ArduinoJson.h>
#include <Keypad.h>
#include <EEPROM.h>

// 1 byte est utilisé pour retenir la dernière machine sélectionnée
#define EEPROM_SIZE 1
#define MACHINE_NUMBER_ADRESS 0

#define BT_STOP               27
#define LATCH_PIN             32
#define CLOCK_PIN             33
#define DATA_PIN              34
#define BT_ARRET_URGENCE      35
#define NIVEAU_BATTERIE       36

#define BT_STATE_ROTONDE BTPressed[2][2] and not altKeyPressed
#define BT_STATE_FUNCTION BTPressed[2][3] and not altKeyPressed
#define BT_STATE_ECHAP BTPressed[3][0] and not altKeyPressed
#define BT_STATE_SELECT_MACHINE BTPressed[3][1] and not altKeyPressed
#define BT_STATE_SELECT_AIGUILLAGE BTPressed[3][2] and not altKeyPressed
#define BT_STATE_ALT BTPressed[3][3]

#define TIME_AIGUILLAGE_SELECT  200
#define TIME_RESET  500
#define TIME_SUBSCRIBE_RENOUVELLEMENT 60000
#define TIME_ROTONDE_SELECT   500

#define JSON_SIZE 16384

// Augmentation du stack pour éviter un stack overflow dû à l'ouverture d'un gros fichier JSON
// Voir le poste de khoih.prog : https://community.platformio.org/t/esp32-stack-configuration-reloaded/20994/4
// Modifier le fichier C:\Users\antoi\.platformio\packages\framework-arduinoespressif32\cores\esp32\main.cpp
#if !(USING_DEFAULT_ARDUINO_LOOP_STACK_SIZE)
  uint16_t USER_CONFIG_ARDUINO_LOOP_STACK_SIZE = 32768;
#endif

// Initialisation codeur
ESP32Encoder encoderH;  // Encodeur Half-quad
ESP32Encoder encoderS;  // Encodeur Single-quad
bool encoderType = true;       // 0: Single-quad   1: Half-quad
long oldPosition  = -999;
long newPosition = 0;

// Configuration wifi
const char* ssid = "Z21_3677";
const char* password = "06338855";
const unsigned int localPort = 21105;
const char* ipAdress = "192.168.0.111";

// Clavier 4x4
const byte KROWS = 4; //four rows
const byte KCOLUMNS = 4; //three columns
const char hexaKeys[KROWS][KCOLUMNS] = {
  {'0','1','2','3'},
  {'4','5','6','7'},
  {'8','9','A','B'},
  {'C','D','E','F'}
};
byte rowPins[KROWS] = {13, 14, 15, 16}; //connect to the row pinouts of the keypad
byte colPins[KCOLUMNS] = {17, 19, 4, 5}; //connect to the column pinouts of the keypad
Keypad kpd = Keypad(makeKeymap(hexaKeys), rowPins, colPins, KROWS, KCOLUMNS); 
bool BTPressed[KROWS][KCOLUMNS] = {
  {0,0,0,0},
  {0,0,0,0},
  {0,0,0,0},
  {0,0,0,0}
};
bool altKeyPressed = false;

Z21 myZ21;
inregister myRegister;

byte numMachine = 3;
unsigned int numAiguillage = 5;
byte boutonFonction = 0;
unsigned long etatFonctions = 0;
byte etatCircuit = 1;
byte premiereInfoMachine = 0;     // 0: Pas besoin d'information  1: Attente retour information   2: Information disponible
int numStationRotonde = 0;
ushort stationRotonde[] = {32,33,34,36,37,38,40,8,9,10,12,13,14,16};
ushort fonctionRotonde[] = {1,16,2,3,19,20,4,5,6,7,8,9,10,14};
String  listeItineraire[] = {"A-gauche", "B-gauche", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "A-droite", "B-droite", "C", "D", "ilot-1", "ilot-2", "ilot-3", "ilot-4", "ilot-5", "ROTONDE", "SERVICE", "GareCach1", "GareCach2", "GareCach3", "GareCach4"};
unsigned short itineraire[10];
bool itineraireDirection[10];
unsigned short compteurItineraire;
bool lastEtatBoutonStop = true;
bool lastEtatBoutonItineraire = true;
bool lastEtatAU = true;
bool BtStopPressed = false;
bool BtItinerairePressed = false;
bool BtAUPressed = false;
bool BtSelectModified = false;
unsigned long BtItineraireNumero = 0;
DynamicJsonDocument doc(JSON_SIZE);  // JSON_SIZE à recalculer en cas de modification du fichier

int etatEcran = 0;
int lastEtatEcran = 0;
bool ecranProcessComplete = true;
bool forceScreenUpdate = false;

unsigned long timer = 0;
unsigned long timerAiguillage = 0;
unsigned long timerReset = 0;
unsigned long timerRotonde = 0;

int niveau_batterie = 0;

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
lv_obj_t * battery_level_label;
lv_obj_t * battery_level_slider;
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
lv_obj_t * ecranRotonde;
lv_obj_t * imgRotonde;
lv_obj_t * line1;
lv_obj_t * line3;
lv_obj_t * line4;
lv_obj_t * line7;
lv_obj_t * line8;
lv_obj_t * line9;
lv_obj_t * line10;
lv_obj_t * line11;
lv_obj_t * line12;
lv_obj_t * line13;
lv_obj_t * ecranItineraire;
lv_obj_t * arrowLine1;
lv_obj_t * arrowLine2;
lv_obj_t * departLabel;
lv_obj_t * arriveLabel;

bool readTrajectoireFile(){
  //Ouverture du fichier
  if(!SPIFFS.begin(true)){
    Serial.println("An Error has occurred while mounting SPIFFS");
    return false;
  }
  File file = SPIFFS.open("/Trajectoires.json");
  if(!file){
    Serial.println("Failed to open file for reading");
    return false;
  }
  char input[JSON_SIZE] = "";
  int fileLength = file.available();
  for(int i=0; i<fileLength; i++){
    char lettre = file.read();
    strncat(input, &lettre, 1);
  }
  file.close();
  SPIFFS.end();
  //Copie du fichier au format JSON
  DeserializationError err = deserializeJson(doc, input);
  if (err) {
    Serial.print(F("deserializeJson() failed with code "));
    Serial.println(err.f_str());
    return false;
  }
  return true;
}

bool trajectoire(char* depart, char* arrivee){
  //Référence à l'objet racine
  JsonObject obj = doc.as<JsonObject>();
  JsonArray circuit = obj["circuit"];
  //Boucle dans le tableau des trajectoires et recherche des départs/arrivées
  for(JsonObject traj : circuit){
    const char* start = traj["start"];
    const char* end = traj["end"];
    if((strcmp(start,depart)==0 && strcmp(end,arrivee)==0) || (strcmp(start,arrivee)==0 && strcmp(end,depart)==0)){
      // Association départ/arrivée trouvée, on parcour maintenant la liste des aiguillages
      JsonArray trajectoire = traj["trajectoire"].as<JsonArray>();
      // Serial.print("Trajectoire trouvée start: ");
      // Serial.print(start);
      // Serial.print(" end: ");
      // Serial.println(end);
      // Serial.println("Trajectoire à réaliser");
      int i=0;
      for(JsonObject repo2 : trajectoire){
        int aiguillage = repo2["num"];
        bool direction = repo2["dir"];
        itineraire[i] = aiguillage;
        itineraireDirection[i] = direction;
        i++;
        // Serial.print("aiguillage: ");
        // Serial.print(aiguillage);
        // Serial.print(" direction: ");
        // Serial.println(direction);
      }
      return true; // Itinéraire trouvé
    }
  }
  //Itinéraire non trouvé
  return false;
}

void updateEncoder(){
  //Lecture position codeur
  if(encoderType){
    newPosition = -1 * encoderH.getCount();
  }else{
    newPosition = -1 * encoderS.getCount();
  }
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
  bool etatBoutonAU = digitalRead(BT_ARRET_URGENCE);
  // Initialisation One Shot
  BtStopPressed = false;
  BtAUPressed = false;
  // Détection front montant
  if((etatBoutonStop != lastEtatBoutonStop) && !etatBoutonStop){
    BtStopPressed = true;
  }
  if((etatBoutonAU != lastEtatAU) && !etatBoutonAU){
    BtAUPressed = true;
  }
  // MAJ état précédent
  lastEtatBoutonStop = etatBoutonStop;
  lastEtatAU = etatBoutonAU;
  // Gestion du keypad
  if(kpd.getKeys()){
    for(int i=0; i<LIST_MAX; i++){
      char keyChar[] = {kpd.key[i].kchar,'\0'};
      int key_int = strtol(keyChar, NULL, 16);
      if(kpd.key[i].stateChanged){
        int j = key_int / KROWS;
        int k = key_int % KCOLUMNS;
        BTPressed[j][k] = kpd.key[i].kstate == PRESSED;
      }
      // gestion du bouton alternatif
      if(key_int == 15){
        altKeyPressed = kpd.key[i].kstate == PRESSED or kpd.key[i].kstate == HOLD;
      }
    }
  }
  // Gestion des boutons itinéraires
  bool etatBoutonItineraire = false;
  BtItineraireNumero = 0;
  int i = 0;
  while(i<32 && not etatBoutonItineraire){
    bool state = myRegister.readInput(i);
    BtItineraireNumero += state * (i+1);
    etatBoutonItineraire = etatBoutonItineraire || state;
    i++;
  }
  BtItinerairePressed = false;
  if((etatBoutonItineraire != lastEtatBoutonItineraire) && etatBoutonItineraire){
    BtItinerairePressed = true;
  }
  lastEtatBoutonItineraire = etatBoutonItineraire;
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
    //---------------------------
    static lv_style_t style_bouton;
    lv_style_init(&style_bouton);
    lv_style_set_text_font(&style_bouton, LV_STATE_DEFAULT, &lv_font_montserrat_26);
    lv_style_set_border_color(&style_bouton, LV_STATE_FOCUSED, LV_COLOR_RED);
    lv_style_set_border_width(&style_bouton, LV_STATE_FOCUSED, 5);
    //---------------------------
    static lv_style_t style_bg_blanc;
    lv_style_init(&style_bg_blanc);
    lv_style_set_bg_color(&style_bg_blanc, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    //---------------------------
    static lv_style_t style_line;
    lv_style_init(&style_line);
    lv_style_set_line_width(&style_line, LV_STATE_DEFAULT, 8);
    lv_style_set_line_color(&style_line, LV_STATE_DEFAULT, LV_COLOR_BLUE);
    lv_style_set_line_rounded(&style_line, LV_STATE_DEFAULT, true);

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
    // Niveau de la batterie
    battery_level_label = lv_label_create(ecranVitesse, NULL);
    lv_obj_align(battery_level_label, ecranVitesse, LV_ALIGN_IN_TOP_RIGHT, 30, 20);
    lv_label_set_align(battery_level_label, LV_LABEL_ALIGN_CENTER);
    lv_label_set_text(battery_level_label, "0");
    lv_obj_add_style(battery_level_label, LV_OBJ_PART_MAIN, &style_small);
    battery_level_slider = lv_slider_create(ecranVitesse, NULL);
    lv_obj_align(battery_level_slider, ecranVitesse, LV_ALIGN_IN_TOP_LEFT, 185, 5);
    lv_slider_set_range(battery_level_slider, 0, 100);
    lv_slider_set_value(battery_level_slider, 0, LV_ANIM_OFF);
    lv_obj_set_size(battery_level_slider, 50, 10);
    
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
    SPIFFS.end();
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

    // Ecran rotonde
    ecranRotonde = lv_obj_create(NULL, NULL);
    lv_obj_add_style(ecranRotonde, LV_STATE_DEFAULT, &style_bg_blanc);
    // Image
    LV_IMG_DECLARE(rotonde);
    imgRotonde = lv_img_create(ecranRotonde, NULL);
    lv_img_set_src(imgRotonde, &rotonde);
    lv_obj_align(imgRotonde, NULL, LV_ALIGN_CENTER, 0, 0);
    lv_img_set_pivot(imgRotonde, 90, 90);
    // Lignes
    static lv_point_t line_points1[] = { {51, 80}, {16, 60} };
    line1 = lv_line_create(ecranRotonde, NULL);
    lv_line_set_y_invert(line1, true);
    lv_line_set_points(line1, line_points1, 2);
    lv_obj_add_style(line1, LV_LINE_PART_MAIN, &style_line);
    lv_obj_align(line1, NULL, LV_ALIGN_IN_BOTTOM_LEFT, 0, 0);
    static lv_point_t line_points3[] = { {43, 99}, {4, 89} };
    line3 = lv_line_create(ecranRotonde, NULL);
    lv_line_set_y_invert(line3, true);
    lv_line_set_points(line3, line_points3, 2);
    lv_obj_add_style(line3, LV_LINE_PART_MAIN, &style_line);
    lv_obj_align(line3, NULL, LV_ALIGN_IN_BOTTOM_LEFT, 0, 0);
    static lv_point_t line_points4[] = { {40, 120}, {0, 120} };
    line4 = lv_line_create(ecranRotonde, NULL);
    lv_line_set_y_invert(line4, true);
    lv_line_set_points(line4, line_points4, 2);
    lv_obj_add_style(line4, LV_LINE_PART_MAIN, &style_line);
    lv_obj_align(line4, NULL, LV_ALIGN_IN_BOTTOM_LEFT, 0, 0);
    static lv_point_t line_points7[] = { {51, 160}, {16, 180} };
    line7 = lv_line_create(ecranRotonde, NULL);
    lv_line_set_y_invert(line7, true);
    lv_line_set_points(line7, line_points7, 2);
    lv_obj_add_style(line7, LV_LINE_PART_MAIN, &style_line);
    lv_obj_align(line7, NULL, LV_ALIGN_IN_BOTTOM_LEFT, 0, 0);
    static lv_point_t line_points8[] = { {189, 160}, {224, 180} };
    line8 = lv_line_create(ecranRotonde, NULL);
    lv_line_set_y_invert(line8, true);
    lv_line_set_points(line8, line_points8, 2);
    lv_obj_add_style(line8, LV_LINE_PART_MAIN, &style_line);
    lv_obj_align(line8, NULL, LV_ALIGN_IN_BOTTOM_LEFT, 0, 0);
    static lv_point_t line_points9[] = { {194, 151}, {231, 166} };
    line9 = lv_line_create(ecranRotonde, NULL);
    lv_line_set_y_invert(line9, true);
    lv_line_set_points(line9, line_points9, 2);
    lv_obj_add_style(line9, LV_LINE_PART_MAIN, &style_line);
    lv_obj_align(line9, NULL, LV_ALIGN_IN_BOTTOM_LEFT, 0, 0);
    static lv_point_t line_points10[] = { {197, 141}, {236, 151} };
    line10 = lv_line_create(ecranRotonde, NULL);
    lv_line_set_y_invert(line10, true);
    lv_line_set_points(line10, line_points10, 2);
    lv_obj_add_style(line10, LV_LINE_PART_MAIN, &style_line);
    lv_obj_align(line10, NULL, LV_ALIGN_IN_BOTTOM_LEFT, 0, 0);
    static lv_point_t line_points11[] = { {200, 120}, {240, 120} };
    line11 = lv_line_create(ecranRotonde, NULL);
    lv_line_set_y_invert(line11, true);
    lv_line_set_points(line11, line_points11, 2);
    lv_obj_add_style(line11, LV_LINE_PART_MAIN, &style_line);
    lv_obj_align(line11, NULL, LV_ALIGN_IN_BOTTOM_LEFT, 0, 0);
    static lv_point_t line_points12[] = { {199, 110}, {239, 104} };
    line12 = lv_line_create(ecranRotonde, NULL);
    lv_line_set_y_invert(line12, true);
    lv_line_set_points(line12, line_points12, 2);
    lv_obj_add_style(line12, LV_LINE_PART_MAIN, &style_line);
    lv_obj_align(line12, NULL, LV_ALIGN_IN_BOTTOM_LEFT, 0, 0);
    static lv_point_t line_points13[] = { {197, 99}, {236, 89} };
    line13 = lv_line_create(ecranRotonde, NULL);
    lv_line_set_y_invert(line13, true);
    lv_line_set_points(line13, line_points13, 2);
    lv_obj_add_style(line13, LV_LINE_PART_MAIN, &style_line);
    lv_obj_align(line13, NULL, LV_ALIGN_IN_BOTTOM_LEFT, 0, 0);

    // Ecran fonctions
    ecranFonction = lv_obj_create(NULL, NULL);
    // Matrice de boutons
    btnmFonction = lv_btnmatrix_create(ecranFonction, NULL);
    static const char * btnm_map[] = {"Feux", "\n",
                                  "F1", "F2", "F3", "F4", "F5", "\n",
                                  "F6", "F7", "F8", "F9", "F10", "\n",
                                  "F11", "F12", "F13", "F14", "F15", "\n",
                                  "F16", "F17", "F18", "F19", "F20", ""};
    lv_btnmatrix_set_map(btnmFonction, btnm_map);
    lv_obj_set_size(btnmFonction, 240, 240);
    lv_btnmatrix_set_btn_ctrl_all(btnmFonction, LV_BTNMATRIX_CTRL_CHECKABLE);
    lv_obj_align(btnmFonction, NULL, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_style(btnmFonction, LV_BTNMATRIX_PART_BTN, &style_bouton);

    // Ecran itinéraire
    ecranItineraire = lv_obj_create(NULL, NULL);
    // Dessin de la fèche
    static lv_point_t arrowPoints1[] = { {120, 140}, {120, 100}, {125, 105} };
    arrowLine1 = lv_line_create(ecranItineraire, NULL);
    lv_line_set_y_invert(arrowLine1, true);
    lv_line_set_points(arrowLine1, arrowPoints1, 3);
    lv_obj_add_style(arrowLine1, LV_LINE_PART_MAIN, &style_line);
    lv_obj_align(arrowLine1, NULL, LV_ALIGN_IN_BOTTOM_LEFT, 0, 0);
    static lv_point_t arrowPoints2[] = { {120, 100}, {115, 105} };
    arrowLine2 = lv_line_create(ecranItineraire, NULL);
    lv_line_set_y_invert(arrowLine2, true);
    lv_line_set_points(arrowLine2, arrowPoints2, 2);
    lv_obj_add_style(arrowLine2, LV_LINE_PART_MAIN, &style_line);
    lv_obj_align(arrowLine2, NULL, LV_ALIGN_IN_BOTTOM_LEFT, 0, 0);
    // Label départ
    departLabel = lv_label_create(ecranItineraire, NULL);
    lv_obj_align(departLabel, ecranItineraire, LV_ALIGN_IN_TOP_MID, -20, 20);
    lv_label_set_text(departLabel, "----");
    lv_obj_add_style(departLabel, LV_OBJ_PART_MAIN, &style_big);
    lv_label_set_align(departLabel, LV_LABEL_ALIGN_CENTER);
    // Label arrivée
    arriveLabel = lv_label_create(ecranItineraire, NULL);
    lv_obj_align(arriveLabel, ecranItineraire, LV_ALIGN_IN_BOTTOM_MID, -20, -20);
    lv_label_set_text(arriveLabel, "----");
    lv_obj_add_style(arriveLabel, LV_OBJ_PART_MAIN, &style_big);
    lv_label_set_align(arriveLabel, LV_LABEL_ALIGN_CENTER);

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
    encoderType = true;
    encoderH.setCount(0);
    oldPosition = 0;
    myZ21.AskMachineInfo();
    premiereInfoMachine = 1;
    // Sauvegarde du numéro de machine sélectionnée
    EEPROM.write(MACHINE_NUMBER_ADRESS, numMachine);
    EEPROM.commit();
    
    // Passage à l'étape 11
    etatEcran = 11;
    
  } else if(etatEcran == 11){
    // Maintient de l'écran vitesse
    // Mise à jour des variables de l'écran et traitement des informations

    // Positionnement du codeur sur la vitesse actuelle de la machine
    if(premiereInfoMachine == 2){
      int realSpeed = myZ21.getMachineRealSpeed();
      encoderH.setCount(realSpeed);
      oldPosition = realSpeed;
      premiereInfoMachine = 0;  // Info traitée
    }
    
    // Arrêt sur un appuie du bouton STOP
    if(BtStopPressed){
      encoderH.setCount(0);
    }

    // Arrêt d'urgence
    if(BtAUPressed){
      myZ21.stopTrackPower();
      Serial.println("Arrêt puissance");
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

    // Niveau de la batterie
    ecritValeur(battery_level_label, niveau_batterie);
    lv_slider_set_value(battery_level_slider, niveau_batterie, LV_ANIM_OFF);

    lastEtatEcran = etatEcran;

    // Changement d'étape suivant le bouton pressé
    if(BT_STATE_SELECT_MACHINE){
      etatEcran = 20;
    } else if(BT_STATE_SELECT_AIGUILLAGE){
      etatEcran = 30;
    } else if(BtItinerairePressed){
      etatEcran = 40;
    } else if(BT_STATE_ROTONDE){
      etatEcran = 50;
    } else if(BT_STATE_FUNCTION){
      etatEcran = 60;
    }
    
  } else if(etatEcran == 20){
    // Initialisation de l'écran changement de machine
    lv_scr_load(ecranMachine);
    lv_roller_set_selected(rollerMachine, numMachine, LV_ANIM_OFF);
    encoderType = false;
    encoderS.setCount(0);
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
    if(BT_STATE_ECHAP || BtStopPressed){
      etatEcran = 10;
    }
    
  } else if(etatEcran == 30){
    // Initialisation de l'écran aiguillage manuel
    lv_scr_load(ecranAiguillage);
    lv_roller_set_selected(rollerAiguillage, numAiguillage, LV_ANIM_OFF);
    encoderType = false;
    encoderS.setCount(0);
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

    // Passage à l'étape 32 ou 10 selon le bouton pressé
    if(BtStopPressed){
      lv_roller_set_visible_row_count(rollerAiguillage, 1);
      lv_obj_align(rollerAiguillage, NULL, LV_ALIGN_CENTER, 0, 0);
      etatEcran = 32;
    } else if(BT_STATE_ECHAP){
      etatEcran = 10;
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
    if(BT_STATE_ECHAP){
      etatEcran = 10;
    } else if(BtStopPressed){
      lv_roller_set_visible_row_count(rollerAiguillage, 4);
      lv_obj_align(rollerAiguillage, NULL, LV_ALIGN_CENTER, 0, 0);
      etatEcran = 31;
    }

  } else if(etatEcran == 33){
    // Attente
    encoderS.setCount(0);
    oldPosition = 0;
    if(millis() > timerAiguillage){
      etatEcran = 32;
    }
    
  } else if(etatEcran == 40){
    // Initialisation de l'écran sélection d'ininéraire
    lv_scr_load(ecranItineraire);
    encoderS.setCount(0);
    oldPosition = 0;

    // Sélection d'un départ
    char buff[10];
    listeItineraire[BtItineraireNumero-1].toCharArray(buff, 10);
    lv_label_set_text(departLabel, buff);
    lv_label_set_text(arriveLabel, "------");

    // Passage à l'étape 41
    etatEcran = 41;
  } else if(etatEcran == 41){
    // Maintient de l'écran sélection d'ininéraire

    lastEtatEcran = etatEcran;

    if(BtItinerairePressed){
      // Sélection d'une arrivée
      char buff[10];
      listeItineraire[BtItineraireNumero-1].toCharArray(buff, 10);
      lv_label_set_text(arriveLabel, buff);
      etatEcran = 42;
    } else if(BT_STATE_ECHAP){
      etatEcran = 10;
    }
  
  } else if(etatEcran == 42){
    // Détermination de la séquence d'aiguillage
    char* depatText = lv_label_get_text(departLabel);
    char* arriveeText = lv_label_get_text(arriveLabel);
    trajectoire(depatText,arriveeText);
    // Passage à l'étape suivante
    if(itineraire[0] > 0){
      etatEcran = 43;
    } else {
      etatEcran = 10;
    }

  } else if(etatEcran == 43){
    // Changement d'état d'un aiguillage
    unsigned short aiguillageNumber = itineraire[0];
    bool aiguillageDirection = itineraireDirection[0];
    // Serial.print("Aiguillage: ");
    // Serial.print(aiguillageNumber);
    // Serial.print(" Direction: ");
    // Serial.println(aiguillageDirection);
    myZ21.SelectAiguillage(aiguillageNumber);
    myZ21.SendAiguillageCommand(aiguillageDirection, true);
    myZ21.SendAiguillageCommand(aiguillageDirection, false);
    timerAiguillage = millis() + TIME_AIGUILLAGE_SELECT;
    // On attend à l'étape suivante
    etatEcran = 44;

  } else if(etatEcran == 44){
    // Attente entre chaque changement d'aiguillage
    // Retour sur à 43 s'il reste un aiguillage dans la liste, sinon on part en 10
    if(millis() > timerAiguillage){
      //Décalage de l'itinéraire
      for(int i=0; i<9; i++){
        itineraire[i] = itineraire[i+1];
        itineraireDirection[i] = itineraireDirection[i+1];
      }
      if(itineraire[0] > 0){
        etatEcran = 43;
      } else {
        etatEcran = 10;
      }
    }

  } else if(etatEcran == 50){
    // Initialisation de l'écran rotonde
    lv_scr_load(ecranRotonde);
    lv_img_set_angle(imgRotonde, stationRotonde[numStationRotonde]*75); // Initialisation position rotonde (on ne la connait pas bien sûr)
    encoderType = false;
    encoderS.setCount(0);
    oldPosition = 0;

    //Passage à l'étape 51
    etatEcran = 51;
  } else if(etatEcran == 51){
    // Maintient de l'écran rotonde
    if(BtSelectModified){
      numStationRotonde = numStationRotonde - (newPosition - oldPosition);
      if(numStationRotonde > 13){
        numStationRotonde = 0;
      } else if(numStationRotonde < 0){
        numStationRotonde = 13;
      }
      oldPosition = newPosition;
      lv_img_set_angle(imgRotonde, stationRotonde[numStationRotonde]*75);
    }

    if(BtStopPressed){
      // Validation position rotonde
      etatEcran = 52;
    }

    lastEtatEcran = etatEcran;

    // Passage à l'étape 10
    if(BT_STATE_ECHAP){
      etatEcran = 10;
    }

  } else if(etatEcran == 52){
    // Sélection numéro de machine (la rotonde est la 15)
    myZ21.SelectMachine(15);    
    // Envoie consigne rotonde
    myZ21.SendMachineFunctionCommand(fonctionRotonde[numStationRotonde], true);
    // Init tempo
    timerRotonde = millis() + TIME_ROTONDE_SELECT;

    etatEcran = 53;

  } else if(etatEcran == 53){
    // Attente de quelques secondes
    if(millis() >= timerRotonde){
      etatEcran = 54;
    }

  } else if(etatEcran == 54){
    // Arrêt envoie consigne
    myZ21.SendMachineFunctionCommand(fonctionRotonde[numStationRotonde], false);
    // Re-sélection du numéro de la machine
    myZ21.SelectMachine(numMachine);

    etatEcran = 51;

  } else if(etatEcran == 60){
    // Initialisation de l'écran fonctions
    lv_scr_load(ecranFonction);
    lv_group_focus_obj(btnmFonction);
    lv_btnmatrix_set_focused_btn(btnmFonction, boutonFonction);
    encoderType = false;
    encoderS.setCount(0);
    oldPosition = 0;

    //Passage à l'étape 61
    etatEcran = 61;
  } else if(etatEcran == 61){
    // Maintient de l'écran fonctions
    if(BtSelectModified){
      boutonFonction = boutonFonction - (newPosition - oldPosition);
      if(boutonFonction >= 200){
        boutonFonction = 20;
      } else if(boutonFonction > 20){
        boutonFonction = 0;
      }
      oldPosition = newPosition;
      lv_btnmatrix_set_focused_btn(btnmFonction, boutonFonction);
    }

    if(BtStopPressed){
      bool etat = not bitRead(etatFonctions, boutonFonction);
      bitWrite(etatFonctions, boutonFonction, etat);
      myZ21.SendMachineFunctionCommand(boutonFonction, etat);
    }
    
    lastEtatEcran = etatEcran;
    
    // Passage à l'étape 10
    if(BT_STATE_ECHAP){
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
    
    encoderH.setCount(0);
    encoderS.setCount(0);
    oldPosition = 0;

    //Passage à l'étape 71
    etatEcran = 71;
  } else if(etatEcran == 71){
    // Maintient de la pop-up court-jus
    
    // Passage à l'étape 10 si le circuit reviens à la normale
    if(etatCircuit == 1){
      etatEcran = lastEtatEcran;
      lv_msgbox_start_auto_close(mbox, 0);
    } else if(BT_STATE_ECHAP || BtStopPressed){
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
}

void majLVGL(void * parameter){
  // Synchro avec la gestion de l'écran qui tourne en parallèle
  while (true){
    if(not ecranProcessComplete || forceScreenUpdate){
      lv_task_handler();
      ecranProcessComplete = true;
    }
    vTaskDelay(1);
  }
}

void toggleFunction(int functionNumber){
  bool functionState = myZ21.GetMachineFunctionState(functionNumber);
  myZ21.SendMachineFunctionCommand(functionNumber, not functionState);
}

void fonctionKeypad(){
  // Gestion des fonctions via les touches du keypad
  int number = -1;
  for(int i=0; i<16; i++){
    int j = i / KROWS;
    int k = i % KCOLUMNS;
    if(BTPressed[j][k]){
      number = i;
    }
  }
  if(number < 10 or (altKeyPressed and number < 15)){
    toggleFunction(number);
  }
}

void read_battery_level(){
  int sensor_level = analogRead(NIVEAU_BATTERIE);
  Serial.println(sensor_level);
  niveau_batterie = map(sensor_level, 0, 4095, 0, 100);
}

void setup()
{
    Serial.begin(115200); /* prepare for possible serial debug */

    // Initialisation entrées
    pinMode(BT_STOP, INPUT);
    pinMode(BT_ARRET_URGENCE, INPUT_PULLUP);
    pinMode(NIVEAU_BATTERIE, INPUT);

    // Initialisation registres à décalage d'entré
    myRegister.Setup(LATCH_PIN, CLOCK_PIN, DATA_PIN);

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
    // Init encodeur Half-quad
    encoderH.attachHalfQuad(25, 26);
    encoderH.setFilter(250);
    encoderH.setCount(0);
    // Init encodeur Single-quad
    encoderS.attachSingleEdge(25, 26);
    encoderS.setFilter(8000);
    encoderS.setCount(0);

    // Init EEPROM
    EEPROM.begin(EEPROM_SIZE);
    numMachine = EEPROM.read(MACHINE_NUMBER_ADRESS);

    // Affichage écran acceuil
    majEcran();

    // Ajout tâche parallèle pour la gestion de l'écran
    xTaskCreatePinnedToCore(majLVGL,"MAJ Ecran",25000,NULL,1,NULL,0);

    // Lecture fichier Trajectoires. A faire avant d'utiliser la wifi
    forceScreenUpdate = true; // Forçage de la mise à jour de l'écran
    readTrajectoireFile();

    // Connexion Wifi
    WiFi.begin(ssid, password);
    Serial.print("Connexion Wifi");
    while (WiFi.status() != WL_CONNECTED) {
      delay(10);
    }
    forceScreenUpdate = false; // Arrêt forçage de la mise à jour de l'écran
    ArduinoOTA.begin();
    myZ21.Setup(ipAdress, localPort);
    myZ21.SelectMachine(numMachine);
    myZ21.SelectAiguillage(numAiguillage);
    myZ21.SubscribeInfo();
}

void loop()
{
  //Etat bouton
  myRegister.fetch();
  updateBouton();

  //Etat encodeur
  updateEncoder();

  //Gestion des fonctions via les touches du keypad
  fonctionKeypad();

  //Lecture du niveau de la batterie
  read_battery_level();

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
    //Récupération état fonctions
    for(int i=0; i<21; i++){
      bool etat = myZ21.GetMachineFunctionState(i);
      if(etat){
        lv_btnmatrix_set_btn_ctrl(btnmFonction, i, LV_BTNMATRIX_CTRL_CHECK_STATE);
      } else {
        lv_btnmatrix_clear_btn_ctrl(btnmFonction, i, LV_BTNMATRIX_CTRL_CHECK_STATE);
      }
      bitWrite(etatFonctions, i, etat);
    }
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
  if (ecranProcessComplete && (etatEcran >= 50 && etatEcran < 60)){
    // Mise à jour de l'écran dans une tâche parallèle si on est dans l'affichage de la rotonde
    ecranProcessComplete = false;
  } else if(etatEcran < 50 || etatEcran >= 60){
    // Mise à jour de l'écran lorsque l'on n'est pas dans l'écran de la rotonde
    lv_task_handler();
  }

  ArduinoOTA.handle();
}
