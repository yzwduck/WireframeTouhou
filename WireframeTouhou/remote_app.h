#pragma once

#include "game_state.h"
#include "run_config.h"
#include "state_reader.h"

class RemoteApp {
 public:
  virtual ~RemoteApp() = default;
  static RemoteApp *GetApp();
  static RemoteApp *GetApp(bool create);
  static RemoteApp *BuildApp(int id);
  static RunConfig *GetConfig();
  virtual int Init() = 0;
  virtual int Run() = 0;
  virtual th::GameState *GetState() = 0;
  virtual th::StateReader *GetReader() = 0;
  virtual void OnPreTick() = 0;
  virtual void OnPostTick() = 0;
  virtual void OnPrePaint() = 0;
  virtual void OnPostPaint() = 0;

 private:
  static RemoteApp *inst;
};
