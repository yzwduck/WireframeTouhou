#include "th10.h"
#include "remote_app.h"
#include "hook_util.h"

namespace th {
static const void *kPlayerInst = ULongToPtr(0x477834);

const GameStateConfig Th10Reader::kConfig = {-192.0f, 0.0f, 192.0f, 450.0f, 0, 0};

typedef struct Th10Player {
  char unk0[0x404];
  Point3f tl;
  Point3f br;
} Th10Player;

class Th10LocalReader : public RebaseUtil {
 public:
  Th10LocalReader(const RebaseUtil &This, GameState *state);
  int Read(bool read_hit) const;
  int ReadPlayer() const;

 private:
  GameState *s;
};

Th10LocalReader::Th10LocalReader(const RebaseUtil &This, GameState *state)
    : RebaseUtil(This), s(state) {}

int Th10LocalReader::Read(bool read_hit) const {
  ReadPlayer();
  if (read_hit) {
  }
  return 0;
}

int Th10LocalReader::ReadPlayer() const {
  int succ = 0;
  do {
    ULONG32 base = *(ULONG32 *)Rebase(kPlayerInst);
    if (base == NULL) break;
    Th10Player *p = (Th10Player *)base;
    s->player.pos.x = (p->tl.x + p->br.x) / 2;
    s->player.pos.y = (p->tl.y + p->br.y) / 2;
    s->player.size.x = p->br.x - p->tl.x;
    s->player.size.y = p->br.y - p->tl.y;
    succ = 1;
  } while (0);

  if (!succ) ZeroMemory(&s->player, sizeof s->player);
  return 0;
}

namespace hook10 {
// typedef int (__fastcall *HitTestBox_T)(Point2f *pos, Point2f *player);  // arg hit_size@eax
void *HitTestBox_F1;
void *HitTestBox_F2;
void *HitTestLaser_F1;
void *HitTestLaser_F2;
void *PreTick_Ret;
void *PostTick_Jmp;
void *PostPresent_Ret;

static void __fastcall HitTestBoxDetour(Point2f *pos, Th10Player *player, Point2f *size) {
  if (RemoteApp::GetConfig()->hook) {
    RemoteApp *app = RemoteApp::GetApp();
    GameState *s = app->GetState();
    HitBox b;
    b.pos = *pos;
    b.size = *size;
    s->bullet.push_back(b);
  }
}

__declspec(naked) static void HitTestBoxDetourJmp() {
  __asm {
    pushad;
    push eax;
    call HitTestBoxDetour;
    popad;
    jmp HitTestBox_F1;
  }
}

static void __stdcall HitTestLaserDetour(Point2f *src, Th10Player *player, float angle, float width,
                                         float length) {
  if (RemoteApp::GetConfig()->hook) {
    RemoteApp *app = RemoteApp::GetApp();
    GameState *s = app->GetState();
    RotateBox rb;
    rb.origin = *src;
    rb.angle = angle;
    rb.width = width;
    rb.len_start = 0.0f;
    rb.len_end = length;
    s->laser_line.push_back(rb);
  }
}

__declspec(naked) static void HitTestLaserDetourJmp() {
  __asm {
    push ebp;
    mov ebp, esp;
    pushad;
    mov edx, [ebp+16];
    push edx;
    mov edx, [ebp+12];
    push edx;
    mov edx, [ebp+8];
    push edx;
    push ecx;
    push eax;
    call HitTestLaserDetour;
    popad;
    pop ebp;
    jmp HitTestLaser_F1;
  }
}

static void PreTickDetour() {
  RemoteApp *app = RemoteApp::GetApp();
  app->GetState()->reset(&Th10Reader::kConfig);
  app->OnPreTick();
}

__declspec(naked) static void PreTickRetDetour() {
  __asm pushad;
  PreTickDetour();
  __asm popad;
  __asm retn;
}

static void PostTickDetour() {
  RemoteApp *app = RemoteApp::GetApp();
  StateReader *reader = app->GetReader();
  Th10LocalReader lr(*reader, app->GetState());
  lr.Read(false);
  app->OnPostTick();
}

__declspec(naked) static void PostTickJmpDetour() {
  __asm pushad;
  PostTickDetour();
  __asm popad;
  __asm jmp PostTick_Jmp;
}

static void PostPresentDetour() {
  RemoteApp *app = RemoteApp::GetApp();
  app->OnPrePaint();
}

__declspec(naked) static void PostPresentRetDetour() {
  __asm pushad;
  PostPresentDetour();
  __asm popad;
  __asm retn;
}

const hook::PATCH_REGION_DEFINITION kPatchDef[] = {
    {0xE8,
     5,
     {0xe8, 0x0f, 0x02, 0x02, 0x00},
     ULongToPtr(0x40649C),
     HitTestBoxDetourJmp,
     &HitTestBox_F1},
    {0xE8,
     5,
     {0xe8, 0xcc, 0x83, 0x01, 0x00},
     ULongToPtr(0x40E2DF),
     HitTestBoxDetourJmp,
     &HitTestBox_F2},
    {0xE8,
     5,
     {0xe8, 0x2d, 0x91, 0x00, 0x00},
     ULongToPtr(0x41D6BE),
     HitTestLaserDetourJmp,
     &HitTestLaser_F1},
    {0xE8,
     5,
     {0xe8, 0xb6, 0x7e, 0x00, 0x00},
     ULongToPtr(0x41E935),
     HitTestLaserDetourJmp,
     &HitTestLaser_F2},
    {0xC3, 2, {0xc3, 0xcc}, ULongToPtr(0x401509), PreTickRetDetour, &PreTick_Ret},
    {0xE9,
     5,
     {0xe9, 0x89, 0x02, 0x00, 0x00},
     ULongToPtr(0x4485D2),
     PostTickJmpDetour,
     &PostTick_Jmp},
    {0xC3, 2, {0xc3, 0xcc}, ULongToPtr(0x4392DB), PostPresentRetDetour, &PostPresent_Ret},
    {0, NULL, NULL, NULL}};
}

int Th10Reader::GetId() const { return kGameId; }

int Th10Reader::Check() const { return hook::PatchCheck(hProc, RebaseGet(), hook10::kPatchDef); }

int Th10Reader::Read(GameState *state) const { return 0; }

StateReader *Th10Reader::Clone() const { return new Th10Reader(); }

int Th10Reader::Hook() const {
  RemoteApp::GetApp()->GetState()->reset(&kConfig);
  unhook_req_ = 0;
  return hook::PatchBatch(RebaseGet(), hook10::kPatchDef, 0);
}

int Th10Reader::Unhook() const {
  // return hook::PatchBatch(kImageBase, kImageBase, kPatchDef, 1);
  if (unhooked_event_ == NULL) return 0;
  unhook_req_ = 1;
  // attempt 1: unhook on next call, timeout=1s
  if (WaitForSingleObject(unhooked_event_, 1000) != WAIT_OBJECT_0) {
    // attempt 2: unhook now
    hook::PatchBatch(RebaseGet(), hook10::kPatchDef, 1);
  }
  Sleep(500);
  return 0;
}

DWORD_PTR Th10Reader::GetImageBase() const { return 0x400000; }
}
