#pragma once
#include "remote_app.h"

#include "state_reader.h"
#include "render_base.h"

class RemoteWireframe : public RemoteApp {
 public:
  RemoteWireframe();
  ~RemoteWireframe();
  static int getId() { return 1; }

  int Init() override;
  int Run() override;
  th::GameState *GetState() override;
  th::StateReader *GetReader() override;
  void OnPreTick() override;
  void OnPostTick() override;
  void OnPrePaint() override;
  void OnPostPaint() override;

 private:
  th::GameState state;
  th::StateReader *reader;
  RenderBase *render;
};
