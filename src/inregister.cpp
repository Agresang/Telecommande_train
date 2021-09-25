#include "inregister.h"
#include "Arduino.h"

void inregister::Setup(int latchPin, int clockPin, int dataPin){
    this->latchPin = latchPin;
    this->clockPin = clockPin;
    this->dataPin = dataPin;
    this->result = 0;
    pinMode(this->latchPin, OUTPUT);
    pinMode(this->clockPin, OUTPUT);
    pinMode(this->dataPin, INPUT);
    digitalWrite(this->latchPin, LOW);
    digitalWrite(this->clockPin, LOW);
}

bool inregister::readInput(int bitNumber){
    return bitRead(this->result, bitNumber);
}

void inregister::fetch(){
    // Latch pour receuillir les états des boutons, à modifier pour se débarasser du delay (latcher à la fin, unlatch au début)
    digitalWrite(this->latchPin, HIGH);
    delayMicroseconds(20);
    digitalWrite(this->latchPin, LOW);
    // Lecture état boutons
    this->result = 0;
    for(int i=15; i>=0; i--){
        digitalWrite(this->clockPin, LOW);
        delayMicroseconds(2);
        bool res = digitalRead(this->dataPin);
        if(res){
            this->result = this->result | (1 << i);
        }
        digitalWrite(this->clockPin, HIGH);
    }
}