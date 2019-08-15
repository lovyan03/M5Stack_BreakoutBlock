#ifndef _INPUT_H_
#define _INPUT_H_

#include <M5Stack.h>

class InputClass
{
public:
  bool update();

  int8_t lrValue() const { return _lr; }
  int8_t udValue() const { return _ud; }

  bool isPressed()  const { return _press; }
  bool isReleased() const { return !_press; }
  bool wasPressed()  const { return _press && !_oldPress; }
  bool wasReleased() const { return !_press && _oldPress; }

private:
  int8_t _lr = 0;
  int8_t _ud = 0;
  uint8_t _press = 0;
  uint8_t _oldPress = 0;
};

#endif

extern InputClass Input;

