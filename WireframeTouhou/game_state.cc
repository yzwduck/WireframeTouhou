#include "game_state.h"

void th::GameState::reset(const GameStateConfig* config) {
  this->config = *config;
  player.pos.x = player.pos.y = player.size.x = player.size.y = 0.0f;
  enemy.clear();
  bullet.clear();
  laser_line.clear();
  laser_curve.clear();
  laser_curve_seg.clear();
}

void th::GameState::add_merge_laser_seg(float width, const LaserCurveSeg* seg) {
  const short idx = (short)laser_curve_seg.size();
  if (!laser_curve_seg.empty()) {
    const LaserCurveSeg last = laser_curve_seg[laser_curve_seg.size() - 1];
    if (last.pos.x == seg->pos.x && last.pos.y == seg->pos.y && last.angle == seg->angle &&
        last.len == seg->len)
      return;
  }
  laser_curve_seg.push_back(*seg);

  bool merged = false;
  if (!laser_curve.empty()) {
    const LaserCurve cur = laser_curve[laser_curve.size() - 1];
    const LaserCurveSeg last = laser_curve_seg[cur.idx_start + cur.segments - 1];
    if (cur.width == width) {
      th::Point2f vd = (last.pos - seg->pos).rotate(-last.angle);
      if (vd.x >= last.len / 3 && vd.x < last.len + 1.0f && vd.y > -cur.width / 2 &&
          vd.y < cur.width / 2) {
        merged = true;
        laser_curve[laser_curve.size() - 1].segments =
            (short)laser_curve_seg.size() - cur.idx_start;
      }
    }
  }
  if (!merged) {
    LaserCurve cur;
    cur.idx_start = idx;
    cur.segments = (short)laser_curve_seg.size() - idx;
    cur.width = width;
    laser_curve.push_back(cur);
  }
}
