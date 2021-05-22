#include "Z21.h"
#include "WiFiUdp.h"

void Z21::Setup(const char* ipAdress, unsigned int port){
  this->ipAdress = ipAdress;
  this->port = port;
  this->machineAdress = 0;
  this->machineSpeed = 0;
  // Commande vitesse machine
  this->machineCommand[0] = 0xA;
  this->machineCommand[1] = 0x0;
  this->machineCommand[2] = 0x40;
  this->machineCommand[3] = 0x0;
  this->machineCommand[4] = 0xE4;
  this->machineCommand[5] = 0x13;
  this->machineCommand[6] = 0x0;
  this->machineCommand[7] = 0x0;
  this->machineCommand[8] = 0x0;
  this->machineCommand[9] = 0x0;
  this->machineCommandLength = 10;
  // Commande GET_INFO_LOCO
  this->machineInfo[0] = 0x09;
  this->machineInfo[1] = 0x00;
  this->machineInfo[2] = 0x40;
  this->machineInfo[3] = 0x00;
  this->machineInfo[4] = 0xE3;
  this->machineInfo[5] = 0xF0;
  this->machineInfo[6] = 0x00;
  this->machineInfo[7] = 0x00;
  this->machineInfo[8] = 0x00;
  this->machineInfoLength = 9;

  this->aiguillageAdress = 0;
  // Commande SET_TURNOUT
  this->aiguillageCommand[0] = 0x09;
  this->aiguillageCommand[1] = 0x00;
  this->aiguillageCommand[2] = 0x40;
  this->aiguillageCommand[3] = 0x00;
  this->aiguillageCommand[4] = 0x53;
  this->aiguillageCommand[5] = 0x00;
  this->aiguillageCommand[6] = 0x00;
  this->aiguillageCommand[7] = 0x00;
  this->aiguillageCommand[8] = 0x00;
  this->aiguillageCommandLength = 9;
  
  this->Udp.begin(this->port);
  this->machineRealSpeed = 0;
  this->etatAiguillage = 0;
  this->trackPowerState = true;
  this->shortCircuit = false;
}

void Z21::SelectMachine(int machineAdress){
  this->machineAdress = machineAdress;
  this->machineCommand[7] = this->machineAdress;
}

void Z21::SetMachineSpeed(int machineSpeed){
  this->machineSpeed = machineSpeed;
  byte vitesse = abs(this->machineSpeed);
  bitWrite(vitesse,7, this->machineSpeed >= 0); // Met le bit de poids fort à 1 si vitesse positive
  this->machineCommand[8] = vitesse;
}

void Z21::calculMachineChecksum(){
  byte chk = 0x0;
  for(int i=5; i < this->machineCommandLength - 1; i++){
    chk = chk ^ this->machineCommand[i];
  }
  this->machineCommand[this->machineCommandLength - 1] = chk;
}

void Z21::SendMachineCommand(){
  calculMachineChecksum();
  this->Udp.beginPacket(this->ipAdress, this->port);
  for(int i=0; i<this->machineCommandLength; i++){
    Udp.write(this->machineCommand[i]);
  }
  Udp.endPacket();
}

void Z21::calculAskMachineChecksum(){
  byte chk = 0x0;
  for(int i=5; i < this->machineInfoLength - 1; i++){
    chk = chk ^ this->machineInfo[i];
  }
  this->machineInfo[this->machineInfoLength - 1] = chk;
}

void Z21::AskMachineInfo(){
  machineInfo[7] = (byte) this->machineAdress;
  calculAskMachineChecksum();
  this->Udp.beginPacket(this->ipAdress, this->port);
  for(int i=0; i<this->machineInfoLength; i++){
    Udp.write(this->machineInfo[i]);
  }
  Udp.endPacket();
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
  command[6] = 0x0;  // Recevoir les changements de TOUTES les machines
  command[7] = 0x0;
  this->Udp.beginPacket(this->ipAdress, this->port);
  for(int i=0; i<commandLength; i++){
    Udp.write(command[i]);
  }
  Udp.endPacket();
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
      // Réception d'une vitesse d'une machine et vérification qu'il s'agit de la machine actuelle
      bool direction = bitRead(buffer[8], 7);
      byte realSpeed = buffer[8];
      bitClear(realSpeed, 7);
      if(direction){
        this->machineRealSpeed = realSpeed;
      } else {
        this->machineRealSpeed = -1 * realSpeed;
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
  this->aiguillageCommand[6] = this->aiguillageAdress;
}

void Z21::SendAiguillageCommand(bool directionAiguillage, bool activation){
  this->aiguillageCommand[7] = 0b10100000;
  bitWrite(this->aiguillageCommand[7], 0, not directionAiguillage);
  bitWrite(this->aiguillageCommand[7], 3, activation);
  calculAiguillageChecksum();
  this->Udp.beginPacket(this->ipAdress, this->port);
  for(int i=0; i<this->aiguillageCommandLength; i++){
    Udp.write(this->aiguillageCommand[i]);
  }
  Udp.endPacket();
}

void Z21::calculAiguillageChecksum(){
  byte chk = 0x0;
  for(int i=5; i < this->aiguillageCommandLength - 1; i++){
    chk = chk ^ this->aiguillageCommand[i];
  }
  this->aiguillageCommand[this->aiguillageCommandLength - 1] = chk;
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
  byte chk = 0x0;
  for(int i=5; i < commandeAiguillageInfoLength - 1; i++){
    chk = chk ^ commandeAiguillageInfo[i];
  }
  commandeAiguillageInfo[commandeAiguillageInfoLength - 1] = chk;
  // Envoie packet
  this->Udp.beginPacket(this->ipAdress, this->port);
  for(int i=0; i<commandeAiguillageInfoLength; i++){
    Udp.write(commandeAiguillageInfo[i]);
  }
  Udp.endPacket();
}

byte Z21::getCircuitState(){
  byte etatCircuit = 0;
  bitWrite(etatCircuit, 0, this->trackPowerState);
  bitWrite(etatCircuit, 1, this->shortCircuit);
  return etatCircuit;
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
  this->Udp.beginPacket(this->ipAdress, this->port);
  for(int i=0; i<commandeResetTrackPowerLength; i++){
    Udp.write(commandeResetTrackPower[i]);
  }
  Udp.endPacket();
}
