#define PIN_CLAP 16  // ПИН D0
#define CLAP_TIME 500 // таймаут ожидания хлопков (датчик шума)

#include "ClapSensor.h"
ClapSensor clapSensor(PIN_CLAP);

#define PIN_TRIG 5  // ПИН D1
#define PIN_ECHO 4  // ПИН D2
#define MIN_DISTANCE 100
#define MAX_DISTANCE 500
#define SOUND_SPEED 0.343
#define EB_DEB_TIME 0
#define EB_CLICK_TIME 900 // таймаут ожидания кликов (датчик расстояния)

#include <EncButton.h>
VirtButton distanceSensor;

#define PIN_LED 12 // ПИН D6
#define NUM_LEDS 60
#define LED_MAX_MA 1500 // ограничение тока ленты, ма

#include <FastLED.h>
CRGB leds[NUM_LEDS];

#include <GRGB.h>
GRGB led;

struct Data {
  int distance;
  bool lampOn = 0;
  byte mode = 0;
  byte bright[2] = {60, 60};
  byte value[2] = {127, 100};
};

Data data;
int prevBright;
byte clap;

void setup() {
  Serial.begin (115200);

  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);

  FastLED.addLeds<WS2812, PIN_LED, GRB>(leds, NUM_LEDS);

  led.setBrightness(0);
  led.attach(setLED);
  led.setCRT(1);

  applyChanges();
}

void loop() {
  if(clapSensor.doubleClap(CLAP_TIME)){
    data.lampOn = !data.lampOn;
    applyChanges();
  }

  static uint32_t tmr;
  if (millis() - tmr >= 30) {
    tmr = millis();

    static int offsetDist;
    static byte offsetVal;

    int filter = getFilterMedian(getDistance());
    data.distance = getFilterSkip(filter);

    distanceSensor.tick(data.distance);

    if(distanceSensor.hasClicks()) {
      switch(distanceSensor.getClicks()){
        case 1:
          data.lampOn = !data.lampOn;
          break;
        case 2:
          if(data.lampOn && ++data.mode >= 2) data.mode = 0;
          break; 
      }
      applyChanges();
    }

    if(data.lampOn && distanceSensor.click()) {
      pulse();
    }
    
    if (data.lampOn && distanceSensor.hold()) {
      pulse();
      offsetDist = data.distance;
      switch (distanceSensor.getClicks()) {
        case 0:
          offsetVal = data.bright[data.mode];
          break;
        case 1:
          offsetVal = data.value[data.mode];
          break;
      }
    }
    
    if (data.lampOn && distanceSensor.holding()) {
      switch (distanceSensor.getClicks()) {
        case 0:
          data.bright[data.mode] = constrain((offsetVal + (data.distance - offsetDist)), 0, 255);
          break;
        case 1:
          data.value[data.mode] = constrain((offsetVal + (data.distance - offsetDist)), 0, 255);
          break;
      }
      applyChanges();
    }
    //Serial.println(data.distance);
  }
  /*
  Serial.println(
    "Dist: " + String(data.distance) + 
    "\t state: " + String(data.lampOn) + 
    "\t mode: " + String(data.mode) + 
    "\t bright:" + String(data.bright[data.mode]) + 
    "\t value:" + String(data.value[data.mode])
  ); 
  */
}

int getDistance(){
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);

  uint32_t duration = pulseIn(PIN_ECHO, HIGH);
  int distance = ((duration * SOUND_SPEED / 2) - MIN_DISTANCE) * 255 / (MAX_DISTANCE - MIN_DISTANCE) ;

  if (distance > 255) return 0;
  return distance;
}

int getFilterMedian(int newVal) {
  static int buf[3];
  static byte count = 0;
  buf[count] = newVal;
  if (++count >= 3) count = 0;
  return (max(buf[0], buf[1]) == max(buf[1], buf[2])) ? max(buf[0], buf[2]) : max(buf[1], min(buf[0], buf[2]));
}

#define FS_WINDOW 5   // количество измерений, в течение которого значение не будет меняться
#define FS_DIFF 80    // разница измерений, с которой начинается пропуск
int getFilterSkip(int val) {
  static int prev;
  static byte count;
  if (!prev && val) prev = val;   // предыдущее значение 0, а текущее нет. Обновляем предыдущее
  // позволит фильтру резко срабатывать на появление руки
  // разница больше указанной ИЛИ значение равно 0 (цель пропала)
  if (abs(prev - val) > FS_DIFF || !val) {
    count++;
    // счётчик потенциально неправильных измерений
    if (count > FS_WINDOW) {
      prev = val;
      count = 0;
    } else val = prev;
  } else count = 0;   // сброс счётчика
  prev = val;
  
  return val;
}

#define BR_STEP 3
void applyChanges(){
  if(data.lampOn) {
    switch(data.mode){
      case 0: led.setWheel8(data.value[0]); break;

      case 1: led.setKelvinFast(data.value[1] * 28); break;
  }
  if (prevBright != data.bright[data.mode]) {
      int shift = prevBright > data.bright[data.mode] ? -BR_STEP : BR_STEP;
      while (abs(prevBright - data.bright[data.mode]) > BR_STEP) {
        prevBright += shift;
        led.setBrightness(prevBright);
        delay(5);
      }
      prevBright = data.bright[data.mode];
    }
  } else {// плавная смена яркости при ВЫКЛЮЧЕНИИ
    while (prevBright > 0) {
      prevBright -= BR_STEP;
      if (prevBright < 0) prevBright = 0;
      led.setBrightness(prevBright);
      delay(10);
    }
  }
}

void pulse() {
  int shift = prevBright > 210 ? -3 : 3;
  for (int i = prevBright; abs(i - prevBright) > 45; i += shift) {
    led.setBrightness(min(255, i));
    delay(10);
  }
  for (int i = prevBright + 15*shift; abs(i - prevBright) > 0; i -= shift) {
    led.setBrightness(min(255, i));
    delay(10);
  }
}

void setLED() {
  FastLED.showColor(CRGB(led.R, led.G, led.B));
}
