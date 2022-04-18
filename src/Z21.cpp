#include "Z21.h"
#include "WiFiUdp.h"

void Z21::Setup(const char* ipAdress, unsigned int port){
  this->ipAdress = ipAdress;
  this->port = port;
  this->machineAdress = 0;
  this->machineSpeed = 0;
  this->machineDirection = true;  // Direction machine (1: avant, 2: arrière)
  this->aiguillageAdress = 0;
  this->Udp.begin(this->port);
  this->machineRealSpeed = 0;
  this->etatAiguillage = 0;
  this->trackPowerState = true;
  this->shortCircuit = false;
}

void Z21::SelectMachine(int machineAdress){
  this->machineAdress = machineAdress;
}

void Z21::SetMachineSpeed(int machineSpeed){
  this->machineSpeed = machineSpeed;
}

void calculChecksum(byte * command, int commandLength){
  byte chk = 0x0;
  for(int i=5; i < commandLength - 1; i++){
    chk = chk ^ command[i];
  }
  command[commandLength - 1] = chk;
}

void Z21::sendPacket(byte * command, int commandLength){
  this->Udp.beginPacket(this->ipAdress, this->port);
  for(int i=0; i<commandLength; i++){
    Udp.write(command[i]);
  }
  Udp.endPacket();
}

void Z21::SendMachineCommand(){
  // Commande vitesse machine
  byte machineCommand[10];
  machineCommand[0] = 0xA;
  machineCommand[1] = 0x0;
  machineCommand[2] = 0x40;
  machineCommand[3] = 0x0;
  machineCommand[4] = 0xE4;
  machineCommand[5] = 0x13;
  machineCommand[6] = 0x0;
  machineCommand[7] = 0x0;
  machineCommand[8] = 0x0;
  machineCommand[9] = 0x0;
  int machineCommandLength = 10;
  machineCommand[7] = this->machineAdress;  // Sélection machine
  byte vitesse = abs(this->machineSpeed);         // Vitesse machine
  // Détermination si la machine va en avant ou en arrière. Ne change pas si la machine est à l'arrêt
  if(this->machineSpeed > 0){
    this->machineDirection = true;
  } else if(this->machineSpeed < 0){
    this->machineDirection = false;
  }
  bitWrite(vitesse,7, this->machineDirection);   // Met le bit de poids fort à 1 si vitesse positive
  machineCommand[8] = vitesse;              // Ecriture de la vitesse
  calculChecksum(machineCommand, machineCommandLength);
  sendPacket(machineCommand, machineCommandLength);
}

void Z21::AskMachineInfo(){
  // Commande GET_INFO_LOCO
  byte machineInfo[9];
  machineInfo[0] = 0x09;
  machineInfo[1] = 0x00;
  machineInfo[2] = 0x40;
  machineInfo[3] = 0x00;
  machineInfo[4] = 0xE3;
  machineInfo[5] = 0xF0;
  machineInfo[6] = 0x00;
  machineInfo[7] = (byte) this->machineAdress;
  machineInfo[8] = 0x00;
  int machineInfoLength = 9;
  calculChecksum(machineInfo, machineInfoLength);
  sendPacket(machineInfo, machineInfoLength);
}

void Z21::SubscribeInfo(){
  byte command[8];
  byte commandLength = 8;
  command[0] = 0x8;
  command[1] = 0x0;
  command[2] = 0x50;
  command[3] = 0x0;
  command[4] = 0x1;  // Mettre à 1 pour recevoir les infos générales de la Z21
  command[5] = 0x0;
  command[6] = 0x0;
  command[7] = 0x0;
  sendPacket(command, commandLength);
}

int Z21::Run(){
  int packetSize = this->Udp.parsePacket();
  int replySize = 32;
  byte buffer[replySize];
  if(packetSize > 0){
    this->Udp.read(buffer,replySize);

    //Affichage de la trame reçue
    /*for(int i=0; i<replySize; i++){
      Serial.print(buffer[i],HEX);
      Serial.print(",");
    }
    Serial.println();*/

    if(buffer[1] == 0x0 && buffer[2] == 0x40 && buffer[3] == 0x0 && buffer[4] == 0xEF && buffer[6] == this->machineAdress){
      // Réception d'une vitesse (et état des foncitons) d'une machine et vérification qu'il s'agit de la machine actuelle
      // Lecture vitesse
      bool direction = bitRead(buffer[8], 7);
      byte realSpeed = buffer[8];
      bitClear(realSpeed, 7);
      if(direction){
        this->machineRealSpeed = realSpeed;
      } else {
        this->machineRealSpeed = -1 * realSpeed;
      }
      // Lecture fonctions F0 à F4
      this->functionState[0] = bitRead(buffer[9], 4);
      this->functionState[1] = bitRead(buffer[9], 0);
      this->functionState[2] = bitRead(buffer[9], 1);
      this->functionState[3] = bitRead(buffer[9], 2);
      this->functionState[4] = bitRead(buffer[9], 3);
      // Lecture fonctions F5 à F12
      for(int i=5; i<=12; i++){
        this->functionState[i] = bitRead(buffer[10], i-5);
      }
      // Lecture fonctions F13 à F20
      for(int i=13; i<=20; i++){
        this->functionState[i] = bitRead(buffer[11], i-13);
      }
      
    } else if(buffer[0] == 0x9 && buffer[1] == 0x0 && buffer[2] == 0x40 && buffer[3] == 0x0 && buffer[4] == 0x43 && buffer[6] == this->aiguillageAdress){
      // Réception d'un état d'un aiguillage et vérification s'il s'agit de l'éguillage en cours
      this->etatAiguillage = buffer[7];
      
    } else if(buffer[0] == 0x7 && buffer[1] == 0x0 && buffer[2] == 0x40 && buffer[3] == 0x0 && buffer[4] == 0x61 && buffer[5] == 0x0 && buffer[6] == 0x61){
      // Réception d'une information plus d'alimentation dans les rails
      this->trackPowerState = false;
      
    } else if(buffer[0] == 0x7 && buffer[1] == 0x0 && buffer[2] == 0x40 && buffer[3] == 0x0 && buffer[4] == 0x61 && buffer[5] == 0x1 && buffer[6] == 0x60){
      // Réception d'une information retour de l'alimentation dans les rails
      this->trackPowerState = true;
      this->shortCircuit = false;
      
    } else if(buffer[0] == 0x7 && buffer[1] == 0x0 && buffer[2] == 0x40 && buffer[3] == 0x0 && buffer[4] == 0x61 && buffer[5] == 0x8 && buffer[6] == 0x69){
      // Réception d'une information d'un court-circuit
      this->shortCircuit = true;
    }
    
    this->Udp.flush();
  }
  return packetSize;
}

int Z21::getMachineRealSpeed(){
  return this->machineRealSpeed;
}

void Z21::SelectAiguillage(int aiguillageAdress){
  this->aiguillageAdress = aiguillageAdress-1;
}

void Z21::SendAiguillageCommand(bool directionAiguillage, bool activation){
  // Commande SET_TURNOUT
  byte aiguillageCommand[9];
  aiguillageCommand[0] = 0x09;
  aiguillageCommand[1] = 0x00;
  aiguillageCommand[2] = 0x40;
  aiguillageCommand[3] = 0x00;
  aiguillageCommand[4] = 0x53;
  aiguillageCommand[5] = 0x00;
  aiguillageCommand[6] = this->aiguillageAdress;
  aiguillageCommand[7] = 0b10100000;
  aiguillageCommand[8] = 0x00;
  int aiguillageCommandLength = 9;
  bitWrite(aiguillageCommand[7], 0, not directionAiguillage);
  bitWrite(aiguillageCommand[7], 3, activation);
  calculChecksum(aiguillageCommand, aiguillageCommandLength);
  sendPacket(aiguillageCommand, aiguillageCommandLength);
}

byte Z21::getAiguillageState(){
  return this->etatAiguillage;
}

void Z21::askAiguillageInfo(){
  byte commandeAiguillageInfoLength = 8;
  byte commandeAiguillageInfo[8];
  commandeAiguillageInfo[0] = 0x08;
  commandeAiguillageInfo[1] = 0x0;
  commandeAiguillageInfo[2] = 0x40;
  commandeAiguillageInfo[3] = 0x0;
  commandeAiguillageInfo[4] = 0x43;
  commandeAiguillageInfo[5] = 0x0;
  commandeAiguillageInfo[6] = aiguillageAdress;
  // Calcul checksum
  calculChecksum(commandeAiguillageInfo, commandeAiguillageInfoLength);
  // Envoie packet
  sendPacket(commandeAiguillageInfo, commandeAiguillageInfoLength);
}

byte Z21::getCircuitState(){
  byte etatCircuit = 0;
  bitWrite(etatCircuit, 0, this->trackPowerState);
  bitWrite(etatCircuit, 1, this->shortCircuit);
  return etatCircuit;
}

void Z21::stopTrackPower(){
  byte commandeResetTrackPowerLength = 7;
  byte commandeResetTrackPower[7];
  commandeResetTrackPower[0] = 0x07;
  commandeResetTrackPower[1] = 0x0;
  commandeResetTrackPower[2] = 0x40;
  commandeResetTrackPower[3] = 0x0;
  commandeResetTrackPower[4] = 0x21;
  commandeResetTrackPower[5] = 0x80;
  commandeResetTrackPower[6] = 0xa0;
  // Envoie packet
  sendPacket(commandeResetTrackPower, commandeResetTrackPowerLength);
}

void Z21::resetTrackPower(){
  byte commandeResetTrackPowerLength = 7;
  byte commandeResetTrackPower[7];
  commandeResetTrackPower[0] = 0x07;
  commandeResetTrackPower[1] = 0x0;
  commandeResetTrackPower[2] = 0x40;
  commandeResetTrackPower[3] = 0x0;
  commandeResetTrackPower[4] = 0x21;
  commandeResetTrackPower[5] = 0x81;
  commandeResetTrackPower[6] = 0xa0;
  // Envoie packet
  sendPacket(commandeResetTrackPower, commandeResetTrackPowerLength);
}

void Z21::SendMachineFunctionCommand(byte numFonction, bool etat){
  // Commande fonction machine
  byte machineFonctionCommand[10];
  machineFonctionCommand[0] = 0xA;
  machineFonctionCommand[1] = 0x0;
  machineFonctionCommand[2] = 0x40;
  machineFonctionCommand[3] = 0x0;
  machineFonctionCommand[4] = 0xE4;
  machineFonctionCommand[5] = 0xF8;
  machineFonctionCommand[6] = 0x0;
  machineFonctionCommand[7] = 0x0;
  machineFonctionCommand[8] = 0x0;
  machineFonctionCommand[9] = 0x0;
  int machineFoncitonCommandLength = 10;
  machineFonctionCommand[7] = this->machineAdress;  // Sélection machine
  byte fontion = numFonction;         // Sélection fonction machine
  bitWrite(fontion,6, etat);   // Ecrit état bouton
  machineFonctionCommand[8] = fontion;              // Ecriture
  calculChecksum(machineFonctionCommand, machineFoncitonCommandLength);
  sendPacket(machineFonctionCommand, machineFoncitonCommandLength);
}

bool Z21::GetMachineFunctionState(int numFunction){
  return this->functionState[numFunction];
}
