#ifndef Z21_H
#define Z21_H

#include "WiFiUdp.h"

class Z21{
  public:
    void Setup(const char* ipAdress, unsigned int port);
    void SelectMachine(int machineAdress);
    void SetMachineSpeed(int machineSpeed);
    void SendMachineCommand();
    void AskMachineInfo();
    void SubscribeInfo();
    int Run();
    int getMachineRealSpeed();
    void SelectAiguillage(int aiguillageAdress);
    void SendAiguillageCommand(bool directionAiguillage, bool activation);
    byte getAiguillageState();
    void askAiguillageInfo();
    byte getCircuitState();
    void stopTrackPower();
    void resetTrackPower();
    void SendMachineFunctionCommand(byte numFonction, bool etat);
    bool GetMachineFunctionState(int numFunction);
  private:
    const char* ipAdress;
    int port;
    int machineAdress;
    int machineSpeed;
    bool machineDirection;
    WiFiUDP Udp;
    int machineRealSpeed;
    int aiguillageAdress;
    byte etatAiguillage;
    bool trackPowerState;
    bool shortCircuit;
    bool functionState[21];
    void sendPacket(byte * command, int commandLength);
};

#endif
