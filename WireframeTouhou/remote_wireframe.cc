#include "remote_wireframe.h"
#include "inject.h"

RemoteWireframe::RemoteWireframe() : reader(nullptr), render(nullptr) {}

RemoteWireframe::~RemoteWireframe() {
  delete reader;
  delete render;
}

int RemoteWireframe::Init() {
  RunConfig *cfg = GetConfig();
  memset(&state, 0, sizeof state);
  if (cfg != nullptr) {
    reader = th::StateReader::BuildReader(cfg->reader);
    reader->Init(NULL);
    render = RenderBase::BuildRender(cfg->render);
  }
  return 0;
}

int RemoteWireframe::Run() {
  th::StateReader::unhooked_event_ = CreateEvent(NULL, FALSE, FALSE, NULL);

  RunConfig *cfg = GetConfig();
  HANDLE evt_quit = CreateEvent(NULL, TRUE, FALSE, NULL);
  render->RegEvent(evt_quit);
  reader->Init(GetCurrentProcess());
  if (reader->Check() != 0) {
    return 2;
  }
  reader->Hook();

  InjecteeCtx *ctx = InjecteeGetCtx();
  HANDLE events[] = {evt_quit, ctx->event_term, ctx->parent_proc};
  DWORD ret =
      WaitForMultipleObjects(_countof(events) - cfg->detach_parent, events, FALSE, INFINITE);
  if (ret > WAIT_OBJECT_0 && ret < WAIT_OBJECT_0 + 3) {
    render->ExitRequest();
    ret = WaitForSingleObject(evt_quit, INFINITE);
  }
  CloseHandle(evt_quit);
  reader->Unhook();
  CloseHandle(th::StateReader::unhooked_event_);
  return 0;
}

th::GameState *RemoteWireframe::GetState() { return &state; }

th::StateReader *RemoteWireframe::GetReader() { return reader; }

void RemoteWireframe::OnPreTick() {}

void RemoteWireframe::OnPostTick() {}

void RemoteWireframe::OnPrePaint() { render->Render(&state); }

void RemoteWireframe::OnPostPaint() {}
