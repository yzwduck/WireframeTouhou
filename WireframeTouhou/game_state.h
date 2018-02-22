#ifndef GAME_STATE_H
#define GAME_STATE_H

#include <cmath>
#include "varray.h"

namespace th {

typedef struct Point2f {
  float x;
  float y;

  Point2f() : x(0.0f), y(0.0f) {}

  Point2f(float len) : x(len), y(0.0f) {}

  Point2f operator+(const Point2f& other) const {
    Point2f r;
    r.x = x + other.x;
    r.y = y + other.y;
    return r;
  }

  Point2f operator-(const Point2f& other) const {
    Point2f r;
    r.x = x - other.x;
    r.y = y - other.y;
    return r;
  }

  Point2f operator-() const {
    Point2f r;
    r.x = -x;
    r.y = -y;
    return r;
  }

  Point2f operator*(int scale) const {
    Point2f r;
    r.x = x * scale;
    r.y = y * scale;
    return r;
  }

  Point2f operator*(float scale) const {
    Point2f r;
    r.x = x * scale;
    r.y = y * scale;
    return r;
  }

  Point2f operator/(int scale) const {
    Point2f r;
    r.x = x / scale;
    r.y = y / scale;
    return r;
  }

  Point2f rotate(float angle) const {
    Point2f r;
    r.x = x * std::cosf(angle) - y * std::sinf(angle);
    r.y = x * std::sinf(angle) + y * std::cosf(angle);
    return r;
  }

  static Point2f rotate(float len, float angle) { return Point2f(len).rotate(angle); }

} Point2f;

typedef struct Point3f {
  float x;
  float y;
  float z;
} Point3f;

typedef struct GameStateConfig {
  float left;
  float top;
  float right;
  float bottom;
  unsigned int circle_enemy : 1;
  unsigned int circle_bullet : 1;
} GameStateConfig;

typedef struct HitBox {
  Point2f pos;  // center pos
  Point2f size;
} HitBox;

typedef struct RotateBox {
  Point2f origin;
  float angle;
  float width;
  float len_start;
  float len_end;
} RotateBox;

typedef struct LaserCurve {
  short idx_start;
  short segments;
  float width;
} LaserCurve;

typedef struct LaserCurveSeg {
  Point2f pos;
  float angle;
  float len;
} LaserCurveSeg;

class GameState {
 public:
  GameStateConfig config;
  HitBox player;
  varray<HitBox, 1024> enemy;
  varray<HitBox, 256 * 8> bullet;
  varray<RotateBox, 256> laser_line;
  varray<LaserCurve, 512> laser_curve;
  varray<LaserCurveSeg, 8192> laser_curve_seg;

  void reset(const GameStateConfig* config);

  void add_merge_laser_seg(float width, const LaserCurveSeg* seg);
};
}

#endif
