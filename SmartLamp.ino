#define PIN_CLAP 16  // ПИН D0
#define CLAP_TIME 500 // таймаут ожидания хлопков (датчик хлопков)

#include "ClapSensor.h"
ClapSensor clapSensor(PIN_CLAP);

#define PIN_TRIG 5  // ПИН D1
#define PIN_ECHO 4  // ПИН D2
#define MIN_DISTANCE 30
#define MAX_DISTANCE 500
#define SOUND_SPEED 0.343
#define EB_DEB_TIME 0
#define EB_CLICK_TIME 900 // таймаут ожидания кликов (датчик расстояния)

#include <EncButton.h>
VirtButton distanceSensor;

#define PIN_LED 12 // ПИН D6
#define NUM_LEDS 60
#define LED_MAX_MA 2000 // ограничение тока ленты, ма

#include <FastLED.h>
CRGB leds[NUM_LEDS];

#include <GRGB.h>
GRGB led;

#include <GyverHub.h>
GyverHub hub("MyDevices", "Smart lamp", "");

#define AP_SSID "TP-Link_AAD3"
#define AP_PASS "78643826"

struct Data {
  int distance;
  bool lampOn = 0;
  byte mode = 1;
  byte bright[2] = {77, 77};
  byte value[2] = {127, 100};
};

Data data;
int prevBright;
byte clap;
bool distSens = 1, clapSens = 0;

void setup() {
  Serial.begin (115200);

  #ifdef GH_ESP_BUILD
    WiFi.mode(WIFI_STA);
    WiFi.begin(AP_SSID, AP_PASS);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();
    Serial.println(WiFi.localIP());
  #endif

  hub.onBuild(build);     // подключаем билдер
  hub.begin();            // запускаем систему

  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);

  FastLED.addLeds<WS2812, PIN_LED, GRB>(leds, NUM_LEDS);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, LED_MAX_MA);
  FastLED.setBrightness(255);

  led.setBrightness(0);
  led.attach(setLED);
  led.setCRT(1);

  applyChanges();
}

void loop() {
  hub.tick();
  
  if(clapSens && clapSensor.doubleClap(CLAP_TIME)){
    data.lampOn = !data.lampOn;
    hub.sendUpdate("state");
    applyChanges();
  }

  static uint32_t tmr;
  if (distSens && millis() - tmr >= 50) {
    tmr = millis();

    static uint32_t tout;
    static int offsetDist;
    static byte offsetVal;

    data.distance = getFilterExp(getFilterSkip(getFilterMedian(getDistance())));

    distanceSensor.tick(data.distance);

    if(distanceSensor.hasClicks() && millis() - tout > 2000) {
      switch(distanceSensor.getClicks()){
        case 1:
          data.lampOn = !data.lampOn;
          hub.sendUpdate("state");
          break;
        case 2:
          if(data.lampOn && ++data.mode >= 2){
            data.mode = 0;
            hub.sendUpdate("brightSlider", String(data.bright[0]));
            hub.sendUpdate("valueSlider", String(data.value[0]));
            hub.sendUpdate("image", "rgb.png");
          }
          else {
            hub.sendUpdate("brightSlider", String(data.bright[1]));
            hub.sendUpdate("valueSlider", String(data.value[1]));
            hub.sendUpdate("image", "kelvin.jpg");
          }
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
      tout = millis();
      switch (distanceSensor.getClicks()) {
        case 0:
          data.bright[data.mode] = constrain((offsetVal + (data.distance - offsetDist)), 25, 255);
          hub.sendUpdate("brightSlider");
          break;
        case 1:
          data.value[data.mode] = constrain((offsetVal + (data.distance - offsetDist)), 0, 255);
          hub.sendUpdate("valueSlider");
          break;
      }
      applyChanges();
    }
  }
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

// экспоненциальный фильтр со сбросом снизу
#define ES_EXP 3L     // коэффициент плавности (больше - плавнее)
#define ES_MULT 12L   // мультипликатор повышения разрешения фильтра
int getFilterExp(int val) {
  static long filt;
  if (val) filt += (val * ES_MULT - filt) / ES_EXP;
  else filt = 0;  // если значение 0 - фильтр резко сбрасывается в 0
  // в нашем случае - чтобы применить заданную установку и не менять её вниз к нулю
  return filt / ES_MULT;
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
      while (abs(prevBright - data.bright[data.mode]) >= BR_STEP) {
        prevBright += shift;
        led.setBrightness(prevBright);
        delay(10);
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

void build() {
  hub.BeginWidgets();
  if(hub.SwitchIcon_(F("state"), &data.lampOn, F("On / Off"), F(""))){
    applyChanges();
  }
  hub.WidgetSize(50);
  if(hub.Button_(F("tempMode"), 0, F("Температурный режим"))) {
    data.mode = 1;
    hub.sendUpdate("brightSlider", String(data.bright[1]));
    hub.sendUpdate("valueSlider", String(data.value[1]));
    hub.sendUpdate("image", "kelvin.jpg");
    applyChanges();
  }
  if(hub.Button_(F("rgbMode"), 0, F("Цветной режим"))) {
    data.mode = 0;
    hub.sendUpdate("brightSlider", String(data.bright[0]));
    hub.sendUpdate("valueSlider", String(data.value[0]));
    hub.sendUpdate("image", "rgb.png");
    applyChanges();
  }
  hub.WidgetSize(100);
  if(hub.Slider_(F("brightSlider"), &data.bright[data.mode], GH_UINT8, F("Яркость"), 25, 255, 1)) {
    applyChanges();
  }
  if(hub.Slider_(F("valueSlider"), &data.value[data.mode], GH_UINT8, F("Цвет"), 0, 255, 1)) {
    applyChanges();
  }
  if(data.mode == 1) hub.Image_(F("image"), "kelvin.jpg");
  else hub.Image_(F("image"), "rgb.png");
  hub.WidgetSize(50);
  hub.SwitchIcon(&distSens, F("Управление жестами"), F(""));
  hub.SwitchIcon(&clapSens, F("Управление хлопками"), F(""));
}