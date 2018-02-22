#include "th14.h"
#include "remote_app.h"
#include "hook_util.h"

namespace th {

static void* kPlayerInst = ULongToPtr(0x4db67c);

const GameStateConfig Th14Reader::kConfig = {-192.0f, 0.0f, 192.0f, 450.0f, 1, 1};
typedef struct {
  char unk_0[0x5e0];
  Point2f pos;  // 5e0
  char unk_5e8[0x630 - 0x5e8];
  Point3f tl;  // 630
  Point3f br;
} Th14PlayerFull;

class Th14RemoteReader : public RebaseUtil {
 public:
  Th14RemoteReader(HANDLE hProc, const RebaseUtil& This, GameState* state);
  int Read() const;
  int ReadPlayer() const;
  int ReadBullet() const;
  int ReadEnemy() const;
  int ReadLaser() const;

 private:
  HANDLE hProc;
  GameState* s;
};

Th14RemoteReader::Th14RemoteReader(HANDLE hProc, const RebaseUtil& This, GameState* state)
    : RebaseUtil(This), hProc(hProc), s(state) {}

int Th14RemoteReader::Read() const {
  ReadPlayer();
  ReadBullet();
  ReadEnemy();
  ReadLaser();
  return 0;
}

int Th14RemoteReader::ReadPlayer() const { return 0; }

int Th14RemoteReader::ReadBullet() const { return 0; }

int Th14RemoteReader::ReadEnemy() const { return 0; }

int Th14RemoteReader::ReadLaser() const { return 0; }

class Th14LocalReader : public RebaseUtil {
 public:
  Th14LocalReader(const RebaseUtil& This, GameState* state);
  int Read(bool read_hit) const;
  int ReadPlayer() const;

 private:
  GameState* s;
};

Th14LocalReader::Th14LocalReader(const RebaseUtil& This, GameState* state)
    : RebaseUtil(This), s(state) {}

int Th14LocalReader::Read(bool read_hit) const {
  ReadPlayer();
  if (read_hit) {
  }
  return 0;
}

int Th14LocalReader::ReadPlayer() const {
  Th14PlayerFull* p = (Th14PlayerFull*)*(ULONG*)kPlayerInst;
  if (p != NULL) {
    s->player.pos = p->pos;
    s->player.size.x = p->br.x - p->tl.x;
    s->player.size.y = p->br.y - p->tl.y;
  }
  return 0;
}

namespace hook14 {
typedef int(__stdcall* HitTestCircle_T)(Point2f* pos, int flag);  // radius@xmm2
typedef int(__stdcall* HitTestRotBox_T)(Point2f* pos, float len,
                                        int flag);  // angle@xmm2, width@xmm3

static HitTestCircle_T HitTestCircle_F;
static HitTestRotBox_T HitTestRotBox_F;
static void* Screenshot_Ret;
static void* PreTick_Jmp;
static void* PostTick_Ret;

extern const hook::PATCH_REGION_DEFINITION kPatchDef[];

static int __stdcall HitTestCircleDetour(Point2f* pos, int flag) {
  float rad;
  __asm movss rad, xmm2;
  if (RemoteApp::GetConfig()->hook) {
    GameState* s = RemoteApp::GetApp()->GetState();
    HitBox b;
    b.pos = *pos;
    b.size.x = b.size.y = rad * 2;
    s->bullet.push_back(b);
  }
  __asm movss xmm2, rad;
  return HitTestCircle_F(pos, flag);
}

static int __stdcall HitTestRotBoxDetour(Point2f* pos, float len, int flag) {
  float angle, width;
  __asm movss angle, xmm2;
  __asm movss width, xmm3;

  if (RemoteApp::GetConfig()->hook) {
    GameState* s = RemoteApp::GetApp()->GetState();
    LaserCurveSeg seg;
    seg.pos = *pos;
    seg.len = len;
    seg.angle = angle;
    s->add_merge_laser_seg(width, &seg);
  }

  __asm movss xmm2, angle;
  __asm movss xmm3, width;
  return HitTestRotBox_F(pos, len, flag);
}

static void PreTickDetour() {
  RemoteApp* app = RemoteApp::GetApp();
  app->GetState()->reset(&Th14Reader::kConfig);
  app->OnPreTick();
}

__declspec(naked) static void PreTickJmpDetour() {
  __asm pushad;
  PreTickDetour();
  __asm popad;
  __asm jmp PreTick_Jmp;
}

static void PostTickDetour() {
  RemoteApp* app = RemoteApp::GetApp();
  StateReader* reader = app->GetReader();
  Th14LocalReader lr(*reader, app->GetState());
  lr.Read(false);
  app->OnPostTick();
}

__declspec(naked) static void PostTickRetDetour() {
  __asm pushad;
  PostTickDetour();
  __asm popad;
  __asm retn;
}

static void PrePresentDetour() {
  RemoteApp* app = RemoteApp::GetApp();
  app->OnPrePaint();
  if (StateReader::unhook_req_) {
    StateReader* reader = app->GetReader();
    if (hook::PatchBatch(reader->RebaseGet(), kPatchDef, 1) == 0) {
      SetEvent(StateReader::unhooked_event_);
    }
  }
}

__declspec(naked) static void ScreenshotSurfaceRetDetour() {
  __asm pushad;
  PrePresentDetour();
  __asm popad;
  __asm retn;
}

const hook::PATCH_REGION_DEFINITION kPatchDef[] = {
    {0xE8,
     5,
     {0xe8, 0xa1, 0x81, 0x03, 0x00},
     ULongToPtr(0x416d70 + 0x8a),
     HitTestCircleDetour,
     (void**)&HitTestCircle_F},
    {0xE8,
     5,
     {0xe8, 0xf8, 0xb1, 0x02, 0x00},
     ULongToPtr(0x4238b0 + 0x4f3),
     HitTestCircleDetour,
     (void**)&HitTestCircle_F},
    {0xE8,
     5,
     {0xe8, 0x20, 0xb2, 0x02, 0x00},
     ULongToPtr(0x4238b0 + 0x5db),
     HitTestRotBoxDetour,
     (void**)&HitTestRotBox_F},
    {0xE8,
     5,
     {0xe8, 0x7a, 0x2d, 0x01, 0x00},
     ULongToPtr(0x43c250 + 0xe1),
     HitTestRotBoxDetour,
     (void**)&HitTestRotBox_F},
    {0xE8,
     5,
     {0xe8, 0x18, 0x0d, 0x01, 0x00},
     ULongToPtr(0x43e300 + 0x93),
     HitTestRotBoxDetour,
     (void**)&HitTestRotBox_F},
    {0xE8,
     5,
     {0xe8, 0x67, 0xe2, 0x00, 0x00},
     ULongToPtr(0x440d80 + 0xc4),
     HitTestRotBoxDetour,
     (void**)&HitTestRotBox_F},
    {0xC3, 2, {0xc3, 0xcc}, ULongToPtr(0x46ac17), ScreenshotSurfaceRetDetour, &Screenshot_Ret},
    {0xE9, 5, {0xe9, 0x0b, 0xee, 0xff, 0xff}, ULongToPtr(0x40eb70), PreTickJmpDetour, &PreTick_Jmp},
    {0xC3, 2, {0xc3, 0xcc}, ULongToPtr(0x455ea6), PostTickRetDetour, &PostTick_Ret},
    {0, NULL, NULL, NULL}};
}

int Th14Reader::GetId() const { return kGameId; }

int Th14Reader::Check() const { return hook::PatchCheck(hProc, RebaseGet(), hook14::kPatchDef); }

int Th14Reader::Read(GameState* state) const {
  if (hProc == NULL) {
    // unexpected
    return 0;
  } else {
    state->reset(&kConfig);
    Th14RemoteReader rr(hProc, *this, state);
    return rr.Read();
  }
}

StateReader* Th14Reader::Clone() const { return new Th14Reader(); }

int Th14Reader::Hook() const {
  RemoteApp::GetApp()->GetState()->reset(&kConfig);
  unhook_req_ = 0;
  return hook::PatchBatch(RebaseGet(), hook14::kPatchDef, 0);
}

int Th14Reader::Unhook() const {
  // return hook::PatchBatch(kImageBase, kImageBase, kPatchDef, 1);
  if (unhooked_event_ == NULL) return 0;
  unhook_req_ = 1;
  // attempt 1: unhook on next call, timeout=1s
  if (WaitForSingleObject(unhooked_event_, 1000) != WAIT_OBJECT_0) {
    // attempt 2: unhook now
    hook::PatchBatch(RebaseGet(), hook14::kPatchDef, 1);
  }
  Sleep(500);
  return 0;
}

DWORD_PTR Th14Reader::GetImageBase() const { return 0x400000; }
}
