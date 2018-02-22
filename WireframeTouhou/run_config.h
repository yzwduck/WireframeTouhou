#pragma once

typedef struct RunConfig {
  int render;
  int reader;
  int app;
  unsigned int hook : 1;
  unsigned int detach_parent : 1;
  unsigned int render_scale_10x : 1;
  unsigned int render_scale_15x : 1;
  unsigned int render_scale_20x : 1;
} RunConfig;
