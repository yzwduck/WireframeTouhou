#include "th11.h"
#include "remote_app.h"
#include "hook_util.h"

namespace th {

static void* kPlayerInst = ULongToPtr(0x4a8eb4);

const GameStateConfig Th11Reader::kConfig = {-192.0f, 0.0f, 192.0f, 450.0f, 1, 0};
typedef struct {
  char unk_0[0x87c];
  Point2f pos;
  char unk_884[0x8cc - 0x884];
  Point3f tl;
  Point3f br;
} Th11PlayerFull;

class Th11RemoteReader : public RebaseUtil {
 public:
  Th11RemoteReader(HANDLE hProc, const RebaseUtil& This, GameState* state);
  int Read() const;
  int ReadPlayer() const;
  int ReadBullet() const;
  int ReadEnemy() const;
  int ReadLaser() const;

 private:
  HANDLE hProc;
  GameState* s;
};

Th11RemoteReader::Th11RemoteReader(HANDLE hProc, const RebaseUtil& This, GameState* state)
    : RebaseUtil(This), hProc(hProc), s(state) {}

int Th11RemoteReader::Read() const {
  ReadPlayer();
  ReadBullet();
  ReadEnemy();
  ReadLaser();
  return 0;
}

int Th11RemoteReader::ReadPlayer() const { return 0; }

int Th11RemoteReader::ReadBullet() const { return 0; }

int Th11RemoteReader::ReadEnemy() const { return 0; }

int Th11RemoteReader::ReadLaser() const { return 0; }

class Th11LocalReader : public RebaseUtil {
 public:
  Th11LocalReader(const RebaseUtil& This, GameState* state);
  int Read(bool read_hit) const;
  int ReadPlayer() const;

 private:
  GameState* s;
};

Th11LocalReader::Th11LocalReader(const RebaseUtil& This, GameState* state)
    : RebaseUtil(This), s(state) {}

int Th11LocalReader::Read(bool read_hit) const {
  ReadPlayer();
  if (read_hit) {
  }
  return 0;
}

int Th11LocalReader::ReadPlayer() const {
  Th11PlayerFull* p = (Th11PlayerFull*)*(ULONG*)kPlayerInst;
  if (p != NULL) {
    s->player.pos = p->pos;
    s->player.size.x = p->br.x - p->tl.x;
    s->player.size.y = p->br.y - p->tl.y;
  }
  return 0;
}

namespace hook11 {
static void* HitTestBox_F;
static void* HitTestCircle_F;
static void* HitTestRotBox_F;
static void* HitTestRotCenterBox_F;
static void* Screenshot_Ret;
static void* PreTick_Jmp;
static void* PostTick_Ret;

extern const hook::PATCH_REGION_DEFINITION kPatchDef[];

static void __fastcall HitTestBoxDetour(Point2f* pos, Point2f* size) {
  GameState* s = RemoteApp::GetApp()->GetState();
  HitBox b;
  b.pos = *pos;
  b.size = *size;
  s->bullet.push_back(b);
}

__declspec(naked) static void HitTestBoxDetourJmp() {
  __asm {
    pushad;
    mov ecx, edx;
    mov edx, eax;
    call HitTestBoxDetour;
    popad;
    jmp HitTestBox_F;
  }
}

// wished to use __fastcall, but MSVC refused to pass float through register
static void __stdcall HitTestCircleDetour(Point2f* pos, float rad) {
  if (RemoteApp::GetConfig()->hook) {
    GameState* s = RemoteApp::GetApp()->GetState();
    HitBox b;
    b.pos = *pos;
    b.size.x = b.size.y = rad * 2;
    s->enemy.push_back(b);
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

__declspec(naked) static void HitTestRotCenterBoxDetourJmp() {
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
  app->GetState()->reset(&Th11Reader::kConfig);
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
  Th11LocalReader lr(*reader, app->GetState());
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
     {0xe8, 0x4b, 0x92, 0x02, 0x00},
     ULongToPtr(0x408900 + 0x2b0),
     HitTestBoxDetourJmp,
     (void**)&HitTestBox_F},
    {0xE8,
     5,
     {0xe8, 0xa8, 0x93, 0x02, 0x00},
     ULongToPtr(0x408900 + 0x2c3),
     HitTestCircleDetourJmp,
     (void**)&HitTestCircle_F},
    {0xE8,
     5,
     {0xe8, 0x23, 0x01, 0x02, 0x00},
     ULongToPtr(0x411750 + 0x6f8),
     HitTestCircleDetourJmp,
     (void**)&HitTestCircle_F},
    {0xE8,
     5,
     {0xe8, 0x19, 0xc0, 0x00, 0x00},
     ULongToPtr(0x425cc0 + 0x392),
     HitTestRotBoxDetourJmp,
     (void**)&HitTestRotBox_F},
    {0xE8,
     5,
     {0xe8, 0x2f, 0xab, 0x00, 0x00},
     ULongToPtr(0x4272c0 + 0x27c),
     HitTestRotBoxDetourJmp,
     (void**)&HitTestRotBox_F},
    {0xE8,
     5,
     {0xe8, 0xf0, 0xa3, 0x01, 0x00},
     ULongToPtr(0x418050 + 0xdb),
     HitTestRotCenterBoxDetourJmp,
     (void**)&HitTestRotCenterBox_F},
    {0xC3, 2, {0xc3, 0xcc}, ULongToPtr(0x44691b), ScreenshotSurfaceRetDetour, &Screenshot_Ret},
    {0xE9, 5, {0xe9, 0x99, 0xf1, 0xff, 0xff}, ULongToPtr(0x403902), PreTickJmpDetour, &PreTick_Jmp},
    {0xC3, 2, {0xc3, 0xcc}, ULongToPtr(0x436d7a), PostTickRetDetour, &PostTick_Ret},
    {0, NULL, NULL, NULL}};
}

int Th11Reader::GetId() const { return kGameId; }

int Th11Reader::Check() const { return hook::PatchCheck(hProc, RebaseGet(), hook11::kPatchDef); }

int Th11Reader::Read(GameState* state) const {
  if (hProc == NULL) {
    // unexpected
    return 0;
  } else {
    state->reset(&kConfig);
    Th11RemoteReader rr(hProc, *this, state);
    return rr.Read();
  }
}

StateReader* Th11Reader::Clone() const { return new Th11Reader(); }

int Th11Reader::Hook() const {
  RemoteApp::GetApp()->GetState()->reset(&kConfig);
  unhook_req_ = 0;
  return hook::PatchBatch(RebaseGet(), hook11::kPatchDef, 0);
}

int Th11Reader::Unhook() const {
  // return hook::PatchBatch(kImageBase, kImageBase, kPatchDef, 1);
  if (unhooked_event_ == NULL) return 0;
  unhook_req_ = 1;
  // attempt 1: unhook on next call, timeout=1s
  if (WaitForSingleObject(unhooked_event_, 1000) != WAIT_OBJECT_0) {
    // attempt 2: unhook now
    hook::PatchBatch(RebaseGet(), hook11::kPatchDef, 1);
  }
  Sleep(500);
  return 0;
}

DWORD_PTR Th11Reader::GetImageBase() const { return 0x400000; }
}
