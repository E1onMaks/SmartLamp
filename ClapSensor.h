class ClapSensor {
  int pin, clapTime = 0, currentTime = 0;
  bool prevClap = false;

public:
  ClapSensor(int pin){
    this->pin = pin;
  }

  bool doubleClap(int period){
    // не было хлопка
    if(digitalRead(pin)) return false;

    currentTime = millis();
    
    // был только 1й хлопок
    if(!prevClap) { 
      clapTime = currentTime;
      prevClap = true;
      return false;
    }

    // был 2й хлопок, но поздно либо слишком быстро (шум)
    if(currentTime - clapTime > period || currentTime - clapTime < 150) {
      clapTime = currentTime;
      return false;
    }

    // был двойной хлопок
    clapTime = currentTime;
    prevClap = false;
    return true;
  }
};