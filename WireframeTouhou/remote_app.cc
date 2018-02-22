#include "remote_app.h"

#include "inject.h"
#include "remote_wireframe.h"

RemoteApp *RemoteApp::inst;

RemoteApp *RemoteApp::GetApp() { return inst; }

RemoteApp *RemoteApp::GetApp(bool create) {
  if (inst == nullptr && create) {
    RunConfig *cfg = GetConfig();
    if (cfg != nullptr) {
      inst = BuildApp(cfg->app);
    }
  }
  return inst;
}

RemoteApp *RemoteApp::BuildApp(int id) {
  if (id == RemoteWireframe::getId()) return new RemoteWireframe();
  return nullptr;
}

RunConfig *RemoteApp::GetConfig() {
  InjecteeCtx *ctx = InjecteeGetCtx();
  if (ctx != NULL)
    return reinterpret_cast<RunConfig *>(ctx->data);
  else
    return nullptr;
}
