#include "th12.h"
#include "remote_app.h"
#include "hook_util.h"

namespace th {

static void* kPlayerInst = ULongToPtr(0x4b4514);

const GameStateConfig Th12Reader::kConfig = {-192.0f, 0.0f, 192.0f, 450.0f, 1, 0};
typedef struct {
  char unk_0[0x97c];
  Point2f pos;
  char unk_984[0x9cc - 0x984];
  Point3f tl;
  Point3f br;
} Th12PlayerFull;

class Th12RemoteReader : public RebaseUtil {
 public:
  Th12RemoteReader(HANDLE hProc, const RebaseUtil& This, GameState* state);
  int Read() const;
  int ReadPlayer() const;
  int ReadBullet() const;
  int ReadEnemy() const;
  int ReadLaser() const;

 private:
  HANDLE hProc;
  GameState* s;
};

Th12RemoteReader::Th12RemoteReader(HANDLE hProc, const RebaseUtil& This, GameState* state)
    : RebaseUtil(This), hProc(hProc), s(state) {}

int Th12RemoteReader::Read() const {
  ReadPlayer();
  ReadBullet();
  ReadEnemy();
  ReadLaser();
  return 0;
}

int Th12RemoteReader::ReadPlayer() const { return 0; }

int Th12RemoteReader::ReadBullet() const { return 0; }

int Th12RemoteReader::ReadEnemy() const { return 0; }

int Th12RemoteReader::ReadLaser() const { return 0; }

class Th12LocalReader : public RebaseUtil {
 public:
  Th12LocalReader(const RebaseUtil& This, GameState* state);
  int Read(bool read_hit) const;
  int ReadPlayer() const;

 private:
  GameState* s;
};

Th12LocalReader::Th12LocalReader(const RebaseUtil& This, GameState* state)
    : RebaseUtil(This), s(state) {}

int Th12LocalReader::Read(bool read_hit) const {
  ReadPlayer();
  if (read_hit) {
  }
  return 0;
}

int Th12LocalReader::ReadPlayer() const {
  Th12PlayerFull* p = (Th12PlayerFull*)*(ULONG*)kPlayerInst;
  if (p != NULL) {
    s->player.pos = p->pos;
    s->player.size.x = p->br.x - p->tl.x;
    s->player.size.y = p->br.y - p->tl.y;
  }
  return 0;
}

namespace hook12 {
static void* HitTestBox_F;
static void* HitTestCircle_F;
static void* HitTestRotBox_F;
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

static void PreTickDetour() {
  RemoteApp* app = RemoteApp::GetApp();
  app->GetState()->reset(&Th12Reader::kConfig);
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
  Th12LocalReader lr(*reader, app->GetState());
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
     {0xe8, 0xc9, 0xdc, 0x02, 0x00},
     ULongToPtr(0x4099E0 + 0x162),
     HitTestBoxDetourJmp,
     (void**)&HitTestBox_F},
    {0xE8,
     5,
     {0xe8, 0xc4, 0xd9, 0x02, 0x00},
     ULongToPtr(0x4099E0 + 0x467),
     HitTestBoxDetourJmp,
     (void**)&HitTestBox_F},
    {0xE8,
     5,
     {0xe8, 0x26, 0xde, 0x02, 0x00},
     ULongToPtr(0x4099E0 + 0x175),
     HitTestCircleDetourJmp,
     (void**)&HitTestCircle_F},
    {0xE8,
     5,
     {0xe8, 0x21, 0xdb, 0x02, 0x00},
     ULongToPtr(0x4099E0 + 0x47A),
     HitTestCircleDetourJmp,
     (void**)&HitTestCircle_F},
    {0xE8,
     5,
     {0xe8, 0x6d, 0x39, 0x02, 0x00},
     ULongToPtr(0x413840 + 0x7CE),
     HitTestCircleDetourJmp,
     (void**)&HitTestCircle_F},
    {0xE8,
     5,
     {0xe8, 0x37, 0xc4, 0x01, 0x00},
     ULongToPtr(0x41B4D0 + 0x74),
     HitTestCircleDetourJmp,
     (void**)&HitTestCircle_F},
    {0xE8,
     5,
     {0xe8, 0xd6, 0xc4, 0x01, 0x00},
     ULongToPtr(0x41B4D0 + 0xD5),
     HitTestRotBoxDetourJmp,
     (void**)&HitTestRotBox_F},
    {0xE8,
     5,
     {0xe8, 0x69, 0xe0, 0x00, 0x00},
     ULongToPtr(0x429610 + 0x402),
     HitTestRotBoxDetourJmp,
     (void**)&HitTestRotBox_F},
    {0xE8,
     5,
     {0xe8, 0x57, 0xca, 0x00, 0x00},
     ULongToPtr(0x42ADA0 + 0x284),
     HitTestRotBoxDetourJmp,
     (void**)&HitTestRotBox_F},
    {0xE8,
     5,
     {0xe8, 0xcb, 0xae, 0x00, 0x00},
     ULongToPtr(0x42C770 + 0x440),
     HitTestRotBoxDetourJmp,
     (void**)&HitTestRotBox_F},
    {0xC3, 2, {0xc3, 0xcc}, ULongToPtr(0x4508ab), ScreenshotSurfaceRetDetour, &Screenshot_Ret},
    {0xE9, 5, {0xe9, 0xd9, 0xfb, 0xff, 0xff}, ULongToPtr(0x43e232), PreTickJmpDetour, &PreTick_Jmp},
    {0xC3, 2, {0xc3, 0xcc}, ULongToPtr(0x43c56a), PostTickRetDetour, &PostTick_Ret},
    {0, NULL, NULL, NULL}};
}

int Th12Reader::GetId() const { return kGameId; }

int Th12Reader::Check() const { return hook::PatchCheck(hProc, RebaseGet(), hook12::kPatchDef); }

int Th12Reader::Read(GameState* state) const {
  if (hProc == NULL) {
    // unexpected
    return 0;
  } else {
    state->reset(&kConfig);
    Th12RemoteReader rr(hProc, *this, state);
    return rr.Read();
  }
}

StateReader* Th12Reader::Clone() const { return new Th12Reader(); }

int Th12Reader::Hook() const {
  RemoteApp::GetApp()->GetState()->reset(&kConfig);
  unhook_req_ = 0;
  return hook::PatchBatch(RebaseGet(), hook12::kPatchDef, 0);
}

int Th12Reader::Unhook() const {
  // return hook::PatchBatch(kImageBase, kImageBase, kPatchDef, 1);
  if (unhooked_event_ == NULL) return 0;
  unhook_req_ = 1;
  // attempt 1: unhook on next call, timeout=1s
  if (WaitForSingleObject(unhooked_event_, 1000) != WAIT_OBJECT_0) {
    // attempt 2: unhook now
    hook::PatchBatch(RebaseGet(), hook12::kPatchDef, 1);
  }
  Sleep(500);
  return 0;
}

DWORD_PTR Th12Reader::GetImageBase() const { return 0x400000; }
}
