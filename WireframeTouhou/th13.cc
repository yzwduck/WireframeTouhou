#include "th13.h"
#include "remote_app.h"
#include "hook_util.h"

namespace th {

static void* kPlayerInst = ULongToPtr(0x4c22c4);

const GameStateConfig Th13Reader::kConfig = {-192.0f, 0.0f, 192.0f, 450.0f, 1, 1};
typedef struct {
  char unk_0[0x5b8];
  Point2f pos;
  char unk_5c0[0x608 - 0x5c0];
  Point3f tl;  // 630
  Point3f br;
} Th13PlayerFull;

class Th13RemoteReader : public RebaseUtil {
 public:
  Th13RemoteReader(HANDLE hProc, const RebaseUtil& This, GameState* state);
  int Read() const;
  int ReadPlayer() const;
  int ReadBullet() const;
  int ReadEnemy() const;
  int ReadLaser() const;

 private:
  HANDLE hProc;
  GameState* s;
};

Th13RemoteReader::Th13RemoteReader(HANDLE hProc, const RebaseUtil& This, GameState* state)
    : RebaseUtil(This), hProc(hProc), s(state) {}

int Th13RemoteReader::Read() const {
  ReadPlayer();
  ReadBullet();
  ReadEnemy();
  ReadLaser();
  return 0;
}

int Th13RemoteReader::ReadPlayer() const { return 0; }

int Th13RemoteReader::ReadBullet() const { return 0; }

int Th13RemoteReader::ReadEnemy() const { return 0; }

int Th13RemoteReader::ReadLaser() const { return 0; }

class Th13LocalReader : public RebaseUtil {
 public:
  Th13LocalReader(const RebaseUtil& This, GameState* state);
  int Read(bool read_hit) const;
  int ReadPlayer() const;

 private:
  GameState* s;
};

Th13LocalReader::Th13LocalReader(const RebaseUtil& This, GameState* state)
    : RebaseUtil(This), s(state) {}

int Th13LocalReader::Read(bool read_hit) const {
  ReadPlayer();
  if (read_hit) {
  }
  return 0;
}

int Th13LocalReader::ReadPlayer() const {
  Th13PlayerFull* p = (Th13PlayerFull*)*(ULONG*)kPlayerInst;
  if (p != NULL) {
    s->player.pos = p->pos;
    s->player.size.x = p->br.x - p->tl.x;
    s->player.size.y = p->br.y - p->tl.y;
  }
  return 0;
}

namespace hook13 {
static void* HitTestCircle_F;
static void* HitTestRotBox_F;
static void* Screenshot_Ret;
static void* PreTick_Jmp;
static void* PostTick_Ret;

extern const hook::PATCH_REGION_DEFINITION kPatchDef[];

// wished to use __fastcall, but MSVC refused to pass float through register
static void __stdcall HitTestCircleDetour(Point2f* pos, float rad) {
  if (RemoteApp::GetConfig()->hook) {
    GameState* s = RemoteApp::GetApp()->GetState();
    HitBox b;
    b.pos = *pos;
    b.size.x = b.size.y = rad * 2;
    s->bullet.push_back(b);
  }
}

__declspec(naked) static void HitTestCircleDetourJmp() {
  __asm {
    push ebp;
    mov ebp, esp;
    pushad;
    mov edx, [ebp+8];
    push edx;
    mov ecx, eax;
    push ecx;
    call HitTestCircleDetour;
    popad;
    mov esp, ebp;
    pop ebp;
    jmp HitTestCircle_F;
  }
}

static void __stdcall HitTestRotBoxDetour(Point2f* pos, float angle, float width, float len) {
  if (RemoteApp::GetConfig()->hook) {
    GameState* s = RemoteApp::GetApp()->GetState();
    LaserCurveSeg seg;
    seg.pos = *pos;
    seg.len = len;
    seg.angle = angle;
    s->add_merge_laser_seg(width, &seg);
  }
}

__declspec(naked) static void HitTestRotBoxDetourJmp() {
  __asm {
    push ebp;
    mov ebp, esp;
    pushad;
    mov edx, [ebp+0x10];
    push edx;
    mov edx, [ebp+0xc];
    push edx;
    mov edx, [ebp+8];
    push edx;
    mov ecx, eax;
    push ecx;
    call HitTestRotBoxDetour;
    popad;
    pop ebp;
    jmp HitTestRotBox_F;
  }
}

static void PreTickDetour() {
  RemoteApp* app = RemoteApp::GetApp();
  app->GetState()->reset(&Th13Reader::kConfig);
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
  Th13LocalReader lr(*reader, app->GetState());
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
     {0xe8, 0x43, 0x64, 0x03, 0x00},
     ULongToPtr(0x40db10 + 0x1e8),
     HitTestCircleDetourJmp,
     (void**)&HitTestCircle_F},
    {0xE8,
     5,
     {0xe8, 0xa6, 0x61, 0x03, 0x00},
     ULongToPtr(0x40db10 + 0x485),
     HitTestCircleDetourJmp,
     (void**)&HitTestCircle_F},
    {0xE8,
     5,
     {0xe8, 0x67, 0xa0, 0x02, 0x00},
     ULongToPtr(0x419a90 + 0x644),
     HitTestCircleDetourJmp,
     (void**)&HitTestCircle_F},
    {0xE8,
     5,
     {0xe8, 0xe4, 0xa0, 0x02, 0x00},
     ULongToPtr(0x419a90 + 0x6e7),
     HitTestRotBoxDetourJmp,
     (void**)&HitTestRotBox_F},
    {0xE8,
     5,
     {0xe8, 0xa1, 0x28, 0x01, 0x00},
     ULongToPtr(0x4315d0 + 0x3ea),
     HitTestRotBoxDetourJmp,
     (void**)&HitTestRotBox_F},
    {0xE8,
     5,
     {0xe8, 0x50, 0x0b, 0x01, 0x00},
     ULongToPtr(0x433470 + 0x29b),
     HitTestRotBoxDetourJmp,
     (void**)&HitTestRotBox_F},
    {0xE8,
     5,
     {0xe8, 0x28, 0xe8, 0x00, 0x00},
     ULongToPtr(0x4355f0 + 0x443),
     HitTestRotBoxDetourJmp,
     (void**)&HitTestRotBox_F},
    {0xC3, 2, {0xc3, 0xcc}, ULongToPtr(0x45d877), ScreenshotSurfaceRetDetour, &Screenshot_Ret},
    {0xE9, 5, {0xe9, 0xfb, 0xef, 0xff, 0xff}, ULongToPtr(0x407680), PreTickJmpDetour, &PreTick_Jmp},
    {0xC3, 2, {0xc3, 0xcc}, ULongToPtr(0x448e93), PostTickRetDetour, &PostTick_Ret},
    {0, NULL, NULL, NULL}};
}

int Th13Reader::GetId() const { return kGameId; }

int Th13Reader::Check() const { return hook::PatchCheck(hProc, RebaseGet(), hook13::kPatchDef); }

int Th13Reader::Read(GameState* state) const {
  if (hProc == NULL) {
    // unexpected
    return 0;
  } else {
    state->reset(&kConfig);
    Th13RemoteReader rr(hProc, *this, state);
    return rr.Read();
  }
}

StateReader* Th13Reader::Clone() const { return new Th13Reader(); }

int Th13Reader::Hook() const {
  RemoteApp::GetApp()->GetState()->reset(&kConfig);
  unhook_req_ = 0;
  return hook::PatchBatch(RebaseGet(), hook13::kPatchDef, 0);
}

int Th13Reader::Unhook() const {
  // return hook::PatchBatch(kImageBase, kImageBase, kPatchDef, 1);
  if (unhooked_event_ == NULL) return 0;
  unhook_req_ = 1;
  // attempt 1: unhook on next call, timeout=1s
  if (WaitForSingleObject(unhooked_event_, 1000) != WAIT_OBJECT_0) {
    // attempt 2: unhook now
    hook::PatchBatch(RebaseGet(), hook13::kPatchDef, 1);
  }
  Sleep(500);
  return 0;
}

DWORD_PTR Th13Reader::GetImageBase() const { return 0x400000; }
}
