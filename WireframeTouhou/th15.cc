#include "th15.h"
#include "remote_app.h"
#include "hook_util.h"

namespace th {

static void* kPlayerInst = ULongToPtr(0x4e9bb8);

const GameStateConfig Th15Reader::kConfig = {-192.0f, 0.0f, 192.0f, 450.0f, 1, 1};

typedef struct {
  char unk_0[0x618];
  Point2f pos;
  char unk_620[0x16284 - 0x620];
  int imm_switch;
  char unk_16288[0x2bfb0 - 0x16288];
  Point3f tl;
  Point3f br;
} Th15PlayerFull;

class Th15RemoteReader : public RebaseUtil {
 public:
  Th15RemoteReader(HANDLE hProc, const RebaseUtil& This, GameState* state);
  int Read() const;
  int ReadPlayer() const;
  int ReadBullet() const;
  int ReadEnemy() const;
  int ReadLaser() const;

 private:
  HANDLE hProc;
  GameState* s;
};

Th15RemoteReader::Th15RemoteReader(HANDLE hProc, const RebaseUtil& This, GameState* state)
    : RebaseUtil(This), hProc(hProc), s(state) {}

int Th15RemoteReader::Read() const {
  ReadPlayer();
  ReadBullet();
  ReadEnemy();
  ReadLaser();
  return 0;
}

int Th15RemoteReader::ReadPlayer() const { return 0; }

int Th15RemoteReader::ReadBullet() const { return 0; }

int Th15RemoteReader::ReadEnemy() const { return 0; }

int Th15RemoteReader::ReadLaser() const { return 0; }

class Th15LocalReader : public RebaseUtil {
 public:
  Th15LocalReader(const RebaseUtil& This, GameState* state);
  int Read(bool read_hit) const;
  int ReadPlayer() const;

 private:
  GameState* s;
};

Th15LocalReader::Th15LocalReader(const RebaseUtil& This, GameState* state)
    : RebaseUtil(This), s(state) {}

int Th15LocalReader::Read(bool read_hit) const {
  ReadPlayer();
  if (read_hit) {
  }
  return 0;
}

int Th15LocalReader::ReadPlayer() const {
  Th15PlayerFull* p = (Th15PlayerFull*)*(ULONG*)kPlayerInst;
  if (p != NULL) {
    s->player.pos = p->pos;
    s->player.size.x = p->br.x - p->tl.x;
    s->player.size.y = p->br.y - p->tl.y;
  }
  return 0;
}

namespace hook15 {
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
  app->GetState()->reset(&Th15Reader::kConfig);
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
  Th15LocalReader lr(*reader, app->GetState());
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
     {0xe8, 0x04, 0xc2, 0x03, 0x00},
     ULongToPtr(0x419af7),
     HitTestCircleDetour,
     (void**)&HitTestCircle_F},
    {0xE8,
     5,
     {0xe8, 0xeb, 0xdd, 0x02, 0x00},
     ULongToPtr(0x427f10),
     HitTestCircleDetour,
     (void**)&HitTestCircle_F},
    {0xE8,
     5,
     {0xe8, 0x0d, 0xde, 0x02, 0x00},
     ULongToPtr(0x427a10 + 0x5ee),
     HitTestRotBoxDetour,
     (void**)&HitTestRotBox_F},
    {0xE8,
     5,
     {0xe8, 0xc6, 0x26, 0x01, 0x00},
     ULongToPtr(0x443640 + 0x105),
     HitTestRotBoxDetour,
     (void**)&HitTestRotBox_F},
    {0xE8,
     5,
     {0xe8, 0x28, 0x02, 0x01, 0x00},
     ULongToPtr(0x445b50 + 0x93),
     HitTestRotBoxDetour,
     (void**)&HitTestRotBox_F},
    {0xE8,
     5,
     {0xe8, 0x87, 0xd6, 0x00, 0x00},
     ULongToPtr(0x4486c0 + 0xc4),
     HitTestRotBoxDetour,
     (void**)&HitTestRotBox_F},
    {0xC3, 2, {0xc3, 0xcc}, ULongToPtr(0x472C87), ScreenshotSurfaceRetDetour, &Screenshot_Ret},
    {0xE9, 5, {0xe9, 0xab, 0xf6, 0xff, 0xff}, ULongToPtr(0x40f000), PreTickJmpDetour, &PreTick_Jmp},
    {0xC3, 2, {0xc3, 0xcc}, ULongToPtr(0x45cef6), PostTickRetDetour, &PostTick_Ret},
    {0, NULL, NULL, NULL}};
}

int Th15Reader::GetId() const { return kGameId; }

int Th15Reader::Check() const { return hook::PatchCheck(hProc, RebaseGet(), hook15::kPatchDef); }

int Th15Reader::Read(GameState* state) const {
  if (hProc == NULL) {
    // unexpected
    return 0;
  } else {
    state->reset(&kConfig);
    Th15RemoteReader rr(hProc, *this, state);
    return rr.Read();
  }
}

StateReader* Th15Reader::Clone() const { return new Th15Reader(); }

int Th15Reader::Hook() const {
  RemoteApp::GetApp()->GetState()->reset(&kConfig);
  unhook_req_ = 0;
  return hook::PatchBatch(RebaseGet(), hook15::kPatchDef, 0);
}

int Th15Reader::Unhook() const {
  // return hook::PatchBatch(kImageBase, kImageBase, kPatchDef, 1);
  if (unhooked_event_ == NULL) return 0;
  unhook_req_ = 1;
  // attempt 1: unhook on next call, timeout=1s
  if (WaitForSingleObject(unhooked_event_, 1000) != WAIT_OBJECT_0) {
    // attempt 2: unhook now
    hook::PatchBatch(RebaseGet(), hook15::kPatchDef, 1);
  }
  Sleep(500);
  return 0;
}

DWORD_PTR Th15Reader::GetImageBase() const { return 0x400000; }
}
