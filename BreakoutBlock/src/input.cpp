#include "input.h"

InputClass Input;

bool InputClass::update()
{
  _oldPress = _press;
  _lr = 0;
  _ud = 0;
  _press = false;

  if (Wire.requestFrom(0x5E, 2) && Wire.available()) {
    _lr = Wire.read() * 4;
    _press = (Wire.read() == 0);
  }

  if (Wire.requestFrom(0x08, 1)) {
    while (Wire.available()){
      char key = Wire.read();
      if (!(key & 0x10)) { _press = true; }  // FACES GameBoy A btn

       // left_right
      _lr += (0 == (key & 0x08)) ? 1 : (0 == (key & 0x04)) ? -1 : 0;
      if (!(key & 0x20)) { _lr *= 2; }  // FACES GameBoy B btn

       // up_down
      _ud += (0 == (key & 0x02)) ? 1 : (0 == (key & 0x01)) ? -1 : 0;

/*
      if (!(key & 0x80)) { }  // FACES GameBoy Start
      if (!(key & 0x40)) { }  // FACES GameBoy Select
*/
    }
  }

  if (M5.BtnA.isPressed()) _lr -= 2;
  if (M5.BtnC.isPressed()) _lr += 2;
  if (M5.BtnB.isPressed()) _press = true;

  return true;
}
