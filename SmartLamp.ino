#include <EncButton.h>

#define PIN_NOISE 16  // ПИН D0
#define PIN_TRIG 5    // ПИН D1
#define PIN_ECHO 4    // ПИН D2
#define SOUND_SPEED 0.343
#define MAX_DISTANCE 1000
#define EB_CLICK_TIME 500  // таймаут ожидания кликов (кнопка)

Button NoiseSensor(PIN_NOISE);
VirtButton DistanceSensor;

bool lampOn = false;
byte mode = 0;

void setup() {
  Serial.begin (115200);
  pinMode(PIN_TRIG, OUTPUT);
}

void loop() {
  NoiseSensor.tick();
  if(NoiseSensor.hasClicks(2)){
    lampOn = !lampOn;
  }
  
  int distance = getDistance(MAX_DISTANCE);
  DistanceSensor.tick(distance);

  if(DistanceSensor.hasClicks(1)) {
    lampOn = !lampOn;
    if(lampOn) Serial.println("On");
    else Serial.println("Off");
  }
  
  if (lampOn && DistanceSensor.holding()) {
    int clicks = DistanceSensor.getClicks();
    switch(clicks){
      case 0: {
        Serial.println("mode " + String(mode) + ", parametr 1: " + String(distance));
        break;
      }
      case 1: {
        Serial.println("mode " + String(mode) + ", parametr 2: " + String(distance));
        break;
      }
      default: break;
    }
  }

  if (lampOn && DistanceSensor.hasClicks(2)) {
    if (mode >= 1) mode = 0;
    else mode++;
    Serial.println("mode " + String(mode));
  }

  delay(50);
}

int getDistance(int maxDistance){
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(5);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  int duration = pulseIn(PIN_ECHO, HIGH);
  int distance = duration * SOUND_SPEED / 2;
  if (distance >= maxDistance) distance = 0; 
  return distance;
}
