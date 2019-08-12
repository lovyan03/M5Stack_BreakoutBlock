#pragma GCC optimize ("O3")

#include <M5Stack.h>
#include <M5StackUpdater.h>     // https://github.com/tobozo/M5Stack-SD-Updater/
#include "src/main.h"

MainClass _main;

void setup(void)
{
  M5.begin();
#ifdef ARDUINO_ODROID_ESP32
  M5.battery.begin();
#else
  M5.Speaker.begin();
  M5.Speaker.mute();
  Wire.begin();
#endif
  if(digitalRead(BUTTON_A_PIN) == 0) {
     Serial.println("Will Load menu binary");
     updateFromFS(SD);
     ESP.restart();
  }
  _main.setup();
}

void loop(void)
{
  M5.update();
  _main.loop();
}

