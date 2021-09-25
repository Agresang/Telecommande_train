#ifndef IN_REGISTER
#define IN_REGISTER

class inregister{
  public:
    void Setup(int latchPin, int clockPin, int dataPin);
    bool readInput(int bitNumber);
    void fetch();
  private:
    int latchPin;
    int clockPin;
    int dataPin;
    unsigned int result;
};

#endif
