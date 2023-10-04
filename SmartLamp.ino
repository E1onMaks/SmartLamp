#include <EncButton>

Button NoiseSensor(0);
bool lampOn = false

void setup() {
  // put your setup code here, to run once:

}

void loop() {
  NoiseSensor.tick();
  if(NoiseSensor.hasClicks(2)){
    lampOn == !lampOn;
  }
  // put your main code here, to run repeatedly:


}
