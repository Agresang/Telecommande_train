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
    void resetTrackPower();
  private:
    const char* ipAdress;
    int port;
    int machineAdress;
    int machineSpeed;
    WiFiUDP Udp;
    int machineRealSpeed;
    int aiguillageAdress;
    byte etatAiguillage;
    bool trackPowerState;
    bool shortCircuit;
    void sendPacket(byte * command, int commandLength);
};

#endif
