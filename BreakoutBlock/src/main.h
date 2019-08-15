#ifndef _MAIN_H_
#define _MAIN_H_

#include <vector>
#include "DMADrawer.h"

enum
{ DMA_WIDTH = 320
, DMA_HEIGHT = 40
, DMA_LOOP = 6
};

enum obj_type
{ none   = 0
, player = 1
, ball   = 2
, shot   = 4
, block  = 8
};

struct sprite_img_t {
  const uint16_t* data;
  uint8_t w;
  uint8_t h;
  sprite_img_t(const uint16_t* data = NULL, uint8_t w=0, uint8_t h=0)
              : data(data), w(w), h(h) {}
};

struct object_info_t {
  int32_t x256;
  int32_t y256;
  int32_t nx256;
  int32_t ny256;
  int16_t px;
  int16_t py;
  uint16_t angle;
  uint16_t zoom;
  uint8_t sp;
  obj_type ot;
  uint8_t enabled;
  uint16_t step;
  object_info_t(int32_t x256=0, int32_t y256=0, uint16_t px=0, uint16_t py=0, uint16_t angle=0, uint16_t zoom=256, uint8_t sp=0)
               : x256(x256), y256(y256), px(px), py(py), angle(angle), zoom(zoom), sp(sp), ot(none), enabled(true), step(0) {}
  uint16_t getAngle(const object_info_t& dst) const;
};

struct sprite_info_t {
  int32_t x256;
  int32_t y256;
  uint16_t angle;
  uint16_t zoom;
  uint8_t sp;
  sprite_info_t() {};
  sprite_info_t(const object_info_t& src)
   : x256 (src.x256 )
   , y256 (src.y256 )
   , angle(src.angle)
   , zoom (src.zoom )
   , sp   (src.sp   )
  {};
};

class MainClass
{
public:
  bool setup();
  void close();
  bool loop();

private:
  volatile bool _isRunning;
  uint32_t _gameCount;
  volatile uint8_t _drawCount;
  volatile uint8_t _calcCount;
  uint8_t _fps;
  DMADrawer _dma;
  uint32_t _msecFps;

  static void draw(uint16_t y, uint16_t h, DMADrawer &dma, const std::vector<sprite_info_t> &ps);
  static void taskDraw(void* arg);
};

#endif
