#pragma GCC optimize ("O3")

#include "main.h"
#include "input.h"

#include <M5Stack.h>
#undef min
#include <utility>
#include <esp_heap_alloc_caps.h>
#include <pgmspace.h>
#include "icons.h"

#define dmaColor(r, g, b) \
    (uint16_t)(((uint8_t)(r) & 0xF8) | ((uint8_t)(b) & 0xF8) << 5 | ((uint8_t)(g) & 0xE0) >> 5 | ((uint8_t)(g) & 0x1C) << 11)

const uint16_t ChromaKey = 0x0000;

const uint16_t CosMax = 1024;
static int16_t CosTbl[CosMax / 4 + 1];
static std::vector<sprite_img_t> sprite_img;
static std::vector<sprite_info_t> draw_sprite;
static std::vector<object_info_t> _objs;
static int32_t _player_x,_player_nx,_player_y,_score;
static int16_t _ball_stock;
static int16_t _shootpos;
static bool _shootmode;

enum state
{ alive
, dead
};
static state _state;

enum spname
{ num0 = 0, num1, num2, num3, num4, num5, num6, num7, num8, num9
, Bar0 = 10, Bar1, Bar2, Bar3, Bar4, Bar5, Bar6, Bar7
, Ball0= 18, Ball1, Ball2
, NormalBlock= 21
, BallBlock = 22
, HardBlock = 23
, ZoomBlock = 24
};

#define Sin(angle) (Cos(angle - CosMax / 4))

int16_t Cos(int angle)
{
    if (angle < 0) {
      angle = -angle;
    }
    if (angle >= CosMax) {
      angle = angle % CosMax;
    }
    if (angle > CosMax / 2) {
      angle = CosMax - angle;
    }
    if (angle > CosMax / 4) {
      return -CosTbl[CosMax / 2 - angle];
    }
    return CosTbl[angle];
}

bool MainClass::setup()
{
  _dma.setup(DMA_WIDTH * DMA_HEIGHT);

  disableCore0WDT();
  disableCore1WDT();

  _objs.reserve(1200);

  for (int i = 0; i != CosMax / 4; i++) {
    CosTbl[i] = cos(i * TWO_PI / CosMax) * 256;
  }
  CosTbl[CosMax] = 0;
  sprite_img.reserve(30);
  for (int i = 0; i < 10; i++)  sprite_img.push_back(sprite_img_t{sp8x8[i], 8, 8}); // number
  for (int i = 0; i <  8; i++)  sprite_img.push_back(sprite_img_t{sp32x8[i], 32, 8}); // bar
  for (int i = 0; i <  3; i++)  sprite_img.push_back(sprite_img_t{sp16x16[i], 16, 16}); // ball
  for (int i = 0; i <  3; i++)  sprite_img.push_back(sprite_img_t{sp32x16[i], 32, 16}); // block


  _player_x = (DMA_WIDTH << 7);
  _player_y = 212 << 8;
  object_info_t oi;
  oi.x256 = _player_x;
  oi.y256 = _player_y;
  oi.zoom = 512;
  oi.sp = spname::Bar0;
  oi.ot = obj_type::player;
  _objs.push_back(oi);

  _isRunning = true;
  _drawCount = 0;
  _calcCount = 0;
  _msecFps = 0;
  xTaskCreatePinnedToCore(taskDraw, "taskDraw", 1024, this, 1, NULL, 0);

  Input.update();
  _ball_stock = 3;
  _state = alive;
  _shootmode = false;
  return true;
}

void MainClass::close()
{
  _isRunning = false;
  _dma.close();
  delay(10);
}

bool MainClass::loop()
{
  while (_calcCount != _drawCount) taskYIELD();
  Input.update();
  draw_sprite.clear();

  int32_t xtmp, ytmp, tmp;

  tmp = _score;
  sprite_info_t si;
  si.angle = 0;
  si.zoom = 512;
  si.y256 = 10 << 8;
  si.x256 = (DMA_WIDTH + 2) << 8;
  do {
    si.sp = num0 + tmp % 10;
    si.x256 -= 18 << 8;
    draw_sprite.push_back(si);
    tmp /= 10;
  } while (tmp);

  si.zoom = 160;
  si.sp = spname::Ball0;
  for (const auto &p : _objs) {
    if (!p.enabled) continue;
    draw_sprite.push_back(sprite_info_t(p));
    if (p.ot == obj_type::player) {
      for (int i = 0; i < _ball_stock; i++) { // ストックボール表示
        uint16_t a = _gameCount * 16 + i * CosMax / _ball_stock;
        si.x256 = p.x256 + Cos(a) * 24;
        si.y256 = p.y256 + Sin(a) * 2;
        draw_sprite.push_back(si);
      }
      if (_shootmode) {
        si.zoom = 256;
        si.sp = spname::Ball0;
        si.x256 = p.x256 + (_shootpos << 3);
        si.y256 = p.y256 - (16 << 8);
        draw_sprite.push_back(si);
      }
    }
  }
  _calcCount++;

  if (_msecFps / 1000 != millis() / 1000) {
    _msecFps = millis();
    Serial.printf("fps:%d obj:%d\r\n", _fps, _objs.size());
    _fps = 0;
  }

  if (_state == dead) {
    return true;
  }

  _gameCount++;
  std::vector<object_info_t> nextlist;

  object_info_t oi;
  if (0 == (_gameCount & 31)) { // Create new blocks
    _score++;
    oi.y256 = -8 << 8;
    oi.zoom = 256;
    oi.px = 0;
    oi.py = 128;
    oi.ot = obj_type::block;
    for (int x = 0; x < 10; x++) {
      tmp = rand() & 63;
      if (tmp < 1 + ((_gameCount >> 10) & 0x0f)) {
        oi.sp = spname::HardBlock;
      } else if (tmp < 10 || tmp > 60) {
        oi.sp = spname::BallBlock;
      } else  {
        oi.sp = spname::NormalBlock;
      }
      oi.x256 = 16 + 32 * x << 8;
      _objs.push_back(oi);
    }
  }

  if (_shootmode) {
    if (Input.wasReleased()) { // Shoot ball
      _shootmode = false;
      oi.nx256 = oi.x256 = _player_x + (_shootpos << 3);
      oi.ny256 = oi.y256 = _player_y - (16 << 8);
      // oi.px = ((DMA_WIDTH << 7) - _player_x >> 6) + (_gameCount & 0xF0) * (_gameCount & 1 ? 1 : - 1);
      oi.px = _shootpos; // + (_gameCount & 0xF0) * (_gameCount & 1 ? 1 : - 1);
      oi.py = - 1024;
      oi.angle = 0;
      oi.zoom = 256;
      oi.sp = spname::Ball0;
      oi.ot = obj_type::ball;
      nextlist.push_back(oi);
    } else {
      _shootpos += Input.lrValue() << 6;
      if (_shootpos > 1024) _shootpos = 1024;
      else if (_shootpos < -1024) _shootpos = -1024;
    }
  } else
  if (_ball_stock && Input.wasPressed()) { // Move shoot angle
    _shootmode = true;
    _ball_stock--;
    _shootpos = 128;
  }

  if (Input.isReleased()) {
    _player_x += Input.lrValue() << 10;
    if (_player_x < 0) { _player_x = 0; }
    else if(_player_x > DMA_WIDTH<<8) { _player_x = (DMA_WIDTH<<8); }

    _player_y += Input.udValue() << 10;
    int lh = M5.Lcd.height() << 7;
    if (_player_y < lh) { _player_y = lh; }
    else if(_player_y > lh << 1) { _player_y = lh << 1; }
  }

  for (auto& pt : _objs) { // move objects
    if (!pt.enabled) continue;

    pt.step++;
    const sprite_img_t& si{sprite_img[pt.sp]};

    if (pt.ot == obj_type::player) {
      pt.sp = spname::Bar0 + (pt.step & 7);
      pt.nx256 = (pt.x256 + _player_x) >> 1;
      pt.ny256 = (pt.y256 + _player_y) >> 1;
      if (abs(pt.nx256 - _player_x) < 256) pt.nx256 = _player_x;
      if (pt.nx256 < 0) { pt.nx256 = 0; }
      else if(pt.nx256 > DMA_WIDTH<<8) { pt.nx256 = (DMA_WIDTH<<8); }
    } else {
      if (pt.y256 < 0 && pt.py < 0) {
        pt.enabled = false;
        continue;
      }
      if (pt.y256 >> 8 >= 240       + si.h) {
        pt.enabled = false;
        continue;
      }
      if ((pt.px < 0 && pt.x256 < si.w * pt.zoom >> 1) 
       || (pt.px > 0 && pt.x256 > (DMA_WIDTH << 8) - (si.w * pt.zoom >> 1))) {
        pt.px = - pt.px;
      }
      pt.nx256 = pt.x256 + pt.px;
      pt.ny256 = pt.y256 + pt.py;
    }
    nextlist.push_back(pt);
  }
  nextlist.swap(_objs);

  _player_nx = _player_x;

  // check collision 
  for (auto it1 = _objs.begin(); it1 != _objs.end(); it1++) {
    if (!it1->enabled) continue;
    for (auto it2 = it1+1; it2 != _objs.end(); it2++) {
      if (!it2->enabled) continue;
      if (it1->ot == it2->ot) continue;
      switch (it1->ot | it2->ot) {

      case (obj_type::player | obj_type::block):

        ytmp = abs(it1->ny256 - it2->ny256);
        if (ytmp >= (sprite_img[it1->sp].h * it1->zoom>>1) + (sprite_img[it2->sp].h * it2->zoom>>1)) continue;
        {
          auto player = it1->ot == obj_type::player? it1 : it2;
          auto block  = it1->ot == obj_type::block ? it1 : it2;
          xtmp = abs(_player_x - block->nx256);
          tmp = (sprite_img[player->sp].w * player->zoom >> 1) + (sprite_img[block->sp].w * block->zoom>>1);
          if (xtmp >= tmp) continue;
          _player_nx += (tmp - xtmp >> 1) * (_player_x > block->nx256 ? 1 : - 1);
          if (ytmp <= (sprite_img[block->sp].h * block->zoom>>1)
           && abs(player->nx256 - block->nx256) <= (tmp / 3)) {
            _state = dead;
          }
        }
        break;

      case (obj_type::player | obj_type::ball):

        if (_state == dead) continue;

        xtmp = abs(it1->nx256 - it2->nx256);
        if (xtmp >= (sprite_img[it1->sp].w * it1->zoom>>1) + (sprite_img[it2->sp].w * it2->zoom>>1)) continue;
        ytmp = abs(it1->ny256 - it2->ny256);
        if (ytmp >= (sprite_img[it1->sp].h * it1->zoom>>1) + (sprite_img[it2->sp].h * it2->zoom>>1)) continue;
        {
          auto bar = it1;
          auto ball = it2;
          if (it2->ot != obj_type::ball) {
            bar = it2;
            ball = it1;
          }
          xtmp = (bar->nx256 - ball->nx256) >> 2;
          ytmp = (bar->ny256 - ball->ny256);
          if (sprite_img[bar->sp].h*bar->zoom>>1 < abs (ytmp)) { 
            if (ball->py < 0 == ytmp < 0) ball->py = - ball->py;
            ball->px += ((ball->nx256 - bar->nx256)>>4);
          } else {
            ball->enabled = false;
            _ball_stock++;
          }
        }
        break;

      case (obj_type::block | obj_type::ball):

        xtmp = abs(it1->nx256 - it2->nx256);
        if (xtmp >= (sprite_img[it1->sp].w * it1->zoom>>1) + (sprite_img[it2->sp].w * it2->zoom>>1)) continue;
        ytmp = abs(it1->ny256 - it2->ny256);
        if (ytmp >= (sprite_img[it1->sp].h * it1->zoom>>1) + (sprite_img[it2->sp].h * it2->zoom>>1)) continue;
        {
          auto block = it1, ball = it2;
          if (it2->ot != obj_type::ball) {
            block = it2;
            ball = it1;
          }
          xtmp = (block->x256 - ball->x256);
          ytmp = (block->y256 - ball->y256);
          int xyflg = 0;

          if (abs(ytmp) <= sprite_img[block->sp].h * block->zoom>>1) {
            xyflg |= 1;
          }
          if (abs(xtmp) <= sprite_img[block->sp].w * block->zoom>>1) {
            xyflg |= 2;
          }
          if (xyflg == 0) {
            int xt = block->x256 + (sprite_img[block->sp].w * block->zoom>>1) * (xtmp < 0 ? 1 : -1);
            int yt = block->y256 + (sprite_img[block->sp].h * block->zoom>>1) * (ytmp < 0 ? 1 : -1);
            xt = (xt - ball->x256) >> 8;
            yt = (yt - ball->y256) >> 8;
            if (ball->zoom * sprite_img[ball->sp].w >> 9 < sqrt(pow(xt,2) + pow(yt,2))) {
              continue;
            }
            xyflg = 3;
          }

          if (xyflg & 1) {
            if (ball->px < 0 == xtmp < 0) {
              ball->px = - ball->px;
            }
          }
          if (xyflg & 2) {
            if (ball->py < 0 == ytmp < 0) {
              ball->py = - ball->py;
            }
          }
/*
          if (xyflg == 3) {
            int16_t px = (ytmp < 0 == xtmp < 0) ? - ball->py : ball->py;
            ball->py = (ytmp < 0 == xtmp < 0) ? - ball->px : ball->px;
            ball->px = px;
            if (ytmp < 0 == ball->py < 0) ball->py = -ball->py;
            if (xtmp < 0 == ball->px < 0) ball->px = -ball->px;
          }
*/
          switch (block->sp) {
          default:
            block->enabled = false;
            if (_state == alive) _score += 100;
            break;
          case spname::BallBlock:
            block->sp = spname::Ball0;
            block->ot = obj_type::ball;
            block->px = ball->px >> 1;
            block->py = ball->py;
            block->zoom = 256;
            if (_state == alive) _score += 100;
            break;
          case spname::HardBlock:
            break;
          case spname::ZoomBlock:
            ball->zoom += 32;
            block->enabled = false;
            if (_state == alive) _score += 100;
            break;
          }
        }
        break;
      }
    }
  }
  _player_x = _player_nx;

  for (auto& pt : _objs) {
    if (!pt.enabled) continue;
    pt.x256 = pt.nx256;
    pt.y256 = pt.ny256;
  }

  return true;
}

static const uint16_t back[] = {0x4243,0x6789,0x89ab,0x9abc,0xabcd,0xbcdf,0xcedc,0x5753,0x4011,0,0,0,0,0,0,0};

void MainClass::draw(uint16_t dmaY, uint16_t dmaH, DMADrawer &dma, const std::vector<sprite_info_t> &ss) {
  uint16_t *d, *ds, *de, *dst = dma.getNextBuffer();
  const uint16_t *s;
  int32_t px,py;
  int32_t ox,oy;
  int32_t bx,by;
  int32_t ex,ey;
  int32_t i,t1,t2;
  int16_t sx,sy,sx256,sy256;
  int16_t cosa,sina;
  int16_t cosb,sinb;
  uint16_t pzoom;
  uint8_t pw, ph;

  d = dst;

  static uint32_t bkscroll;
  if (0 == dmaY) bkscroll -= 1;
  int32_t po = ((bkscroll + dmaY) * DMA_WIDTH);
  for (i = DMA_WIDTH * DMA_HEIGHT; i; i--, po++) {
    *d++ = ((po % 2042) == 0 || (po % 2057) == 0 ) 
         ? back[(i>>10)&15] 
         : 0;
  }

  

  for (const auto& si : ss) {
    pw = sprite_img[si.sp].w;
    ph = sprite_img[si.sp].h;

    pzoom = si.zoom;

    cosb = Cos(si.angle);
    sinb = Sin(si.angle);
    if (pzoom != 256){
      cosa = cosb * pzoom >> 8;
      cosb = (cosb << 8) / pzoom;
      sina = sinb * pzoom >> 8;
      sinb = (sinb << 8) / pzoom ;
    } else {
      cosa = cosb;
      sina = sinb;
    }

    by = pw * sina + ph * cosa >> 1;
    ey = ph * cosa - pw * sina >> 1;
    if (by < 0) by = -by;
    if (ey < 0) ey = -ey;
    if (by > ey) ey = by;
    py = si.y256 - (dmaY << 8);
    by = (py - ey >> 8);
    if (by < 0) by = 0;
    ey = (py + ey + 255 >> 8) + 1;
    if (ey > dmaH) ey = dmaH;
    if (by >= ey) continue;

    oy = by - (py >> 8);

    bx =   pw * cosa - ph * sina >> 1;
    ex = - ph * sina - pw * cosa >> 1;
    if (bx < 0) bx = -bx;
    if (ex < 0) ex = -ex;
    if (bx > ex) ex = bx;
    px = si.x256;
    bx = (px - ex >> 8);
    if (bx < 0) bx = 0;
    ex = (px + ex + 255 >> 8) + 1;
    if (ex > DMA_WIDTH) ex = DMA_WIDTH;
    ox = bx - (px >> 8);

    s = sprite_img[si.sp].data;

    ds = dst + bx + (by) * DMA_WIDTH;
    de = ds + (ex - bx);
    t1 = ox * cosb - oy * sinb + (pw << 7);
    t2 = ox * sinb + oy * cosb + (ph << 7);
    for (i = by; i < ey; i++, t1 -= sinb, t2 += cosb, ds += DMA_WIDTH, de += DMA_WIDTH) {
      sx256 = t1;
      sy256 = t2;
      for (d = ds; d < de; d++, sx256 += cosb, sy256 += sinb) {
        if (sx256 >= 0 && sy256 >= 0) {
          sx = sx256 >> 8;
          if (sx < pw) {
            sy = sy256 >> 8;
            if (sy < ph) {
              //if (ChromaKey != s[sx + pw * sy]) *d |= s[sx + pw * sy];
              *d |= s[sx + pw * sy];
            }
          }
        }
      }
    }
  }
  dma.draw( 0, dmaY, DMA_WIDTH, dmaH);
}

void MainClass::taskDraw(void* arg)
{
  MainClass* me = (MainClass*) arg;
  volatile uint8_t &calcCount(me->_calcCount);
  volatile uint8_t &drawCount(me->_drawCount);
  DMADrawer &dma(me->_dma);

  while ( me->_isRunning ) {
    if (calcCount != drawCount) {
      for (uint16_t i = 0; i != DMA_LOOP; i++) {
        draw(i*DMA_HEIGHT, DMA_HEIGHT, dma, draw_sprite);
      }
      me->_fps++;
      drawCount++;
    } else {
      taskYIELD();
    }
  }
  vTaskDelete(NULL);
}

uint16_t object_info_t::getAngle(const object_info_t& dst) const
{
  if (x256 < dst.x256)
    return atan2(dst.x256 - x256, dst.y256 - y256) / TWO_PI * CosMax + (CosMax>>1);
  return atan2(x256 - dst.x256, y256 - dst.y256) / TWO_PI * CosMax;
}

