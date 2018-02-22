#include "th08.h"
#include "remote_app.h"
#include "hook_util.h"

// Note: th08 is not supported (yet)
// The game has some optimization, only fraction of the bullets are hit-tested.
// Thus (most) bullets are not visible.

namespace th {

static const void *kPlayerInst = ULongToPtr(0x17D5EF8);
static const DWORD_PTR kBulletArray = 0xf6f710;

const GameStateConfig Th08Reader::kConfig = {0.0f, 0.0f, 384.0f, 450.0f, 0, 0};

typedef struct Th08Player {
  char unk_0[0x2B4];
  Point2f pos;
  char unk_2bc[0x3d4 - 0x2bc];
  Point2f size;
} Th08Player;

typedef struct Th08Bullet {
  char unk_0[0xd34];
  Point2f size;
  Point2f unk_d3c;
  Point2f pos;
  char unk_d4c[0xdb8 - 0xd4c];
  int kind;
  char unk_dbc[0x10b8 - 0xdbc];
} Th08Bullet;

static void BulletToState(GameState *s, Th08Bullet *bd) {
  if (bd->kind != 0) {
    HitBox b;
    b.pos = bd->pos;
    b.size = bd->size;
    s->bullet.push_back(b);
  }
}

class Th08RemoteReader : public RebaseUtil {
 public:
  Th08RemoteReader(HANDLE hProc, const RebaseUtil &This, GameState *state);
  int Read() const;
  int ReadPlayer() const;
  int ReadBullet() const;

 private:
  HANDLE hProc;
  GameState *s;
};

Th08RemoteReader::Th08RemoteReader(HANDLE hProc, const RebaseUtil &This, GameState *state)
    : RebaseUtil(This), hProc(hProc), s(state) {}

int Th08RemoteReader::Read() const {
  ReadPlayer();
  ReadBullet();
  return 0;
}

int Th08RemoteReader::ReadPlayer() const {
  Th08Player player;
  if (ReadProcMemRe(hProc, kPlayerInst, &player, sizeof player)) {
    s->player.pos = player.pos;
    s->player.size = player.size;
  }
  return 0;
}

int Th08RemoteReader::ReadBullet() const {
  const int pack = 16;
  for (int i = 0; i < 0x600; i += pack) {
    Th08Bullet bullet[pack];
    if (ReadProcMemRe(hProc, (void *)(kBulletArray + i * sizeof bullet[0]), bullet,
                      sizeof bullet)) {
      for (int j = 0; j < pack; j++) BulletToState(s, &bullet[j]);
    }
  }
  return 0;
}

class Th08LocalReader : public RebaseUtil {
 public:
  Th08LocalReader(const RebaseUtil &This, GameState *state);
  int Read(bool read_hit) const;
  int ReadPlayer() const;
  int ReadBullet() const;

 private:
  GameState *s;
};

Th08LocalReader::Th08LocalReader(const RebaseUtil &This, GameState *state)
    : RebaseUtil(This), s(state) {}

int Th08LocalReader::Read(bool read_hit) const {
  ReadPlayer();
  if (read_hit) {
    ReadBullet();
  }
  return 0;
}

int Th08LocalReader::ReadPlayer() const {
  int succ = 0;
  do {
    Th08Player *p = (Th08Player *)kPlayerInst;
    s->player.pos = p->pos;
    s->player.size = p->size;
    succ = 1;
  } while (0);

  if (!succ) ZeroMemory(&s->player, sizeof s->player);
  return 0;
}

int Th08LocalReader::ReadBullet() const {
  Th08Bullet *bd = (Th08Bullet *)Rebase((void *)kBulletArray);
  for (int i = 0; i < 0x600; i++) BulletToState(s, &bd[i]);
  return 0;
}

namespace hook08 {
typedef int(__fastcall *HitTestBox_T)(void *player, void *unused, Point2f *pos, Point2f *size);
typedef int(__fastcall *HitTestLaser_T)(void *player, void *usuned, Point2f *mid, Point2f *width,
                                        Point2f *origin, float angle, int flag);
typedef int(__stdcall *PresentAndSnapshot_T)();
static HitTestBox_T HitTestBullet_F;
static HitTestBox_T HitTestEnemy_F;
static void *HitTestLaser_Ret;
static void *PreTick_ReplayRet;
static void *PreTick_PlayRet;
static void *PostTick_ReplayRet;
static void *PostTick_PlayRet;
static PresentAndSnapshot_T PresentAndSnapshot_F;
static void *PostPresent_Ret;

extern const hook::PATCH_REGION_DEFINITION kPatchDef[];

static int __fastcall HitTestBulletDetour(void *player, void *unused, Point2f *pos, Point2f *size) {
  if (RemoteApp::GetConfig()->hook) {
    RemoteApp *app = RemoteApp::GetApp();
    GameState *s = app->GetState();
    HitBox b;
    b.pos = *pos;
    b.size = *size;
    if (b.size.x < 1.0f) b.size.x = 1.0f;
    if (b.size.y < 1.0f) b.size.y = 1.0f;
    s->bullet.push_back(b);
  }
  return HitTestBullet_F(player, unused, pos, size);
}

static int __fastcall HitTestEnemyDetour(void *player, void *unused, Point2f *pos, Point2f *size) {
  if (RemoteApp::GetConfig()->hook) {
    RemoteApp *app = RemoteApp::GetApp();
    GameState *s = app->GetState();
    HitBox b;
    b.pos = *pos;
    b.size = *size;
    s->bullet.push_back(b);
  }
  return HitTestEnemy_F(player, unused, pos, size);
}

static int __fastcall HitTestLaserDetour(void *player, int ret, Point2f *mid, Point2f *width,
                                         Point2f *origin, float angle, int flag) {
  // detour for sub_44A6A0
  if (RemoteApp::GetConfig()->hook) {
    RemoteApp *app = RemoteApp::GetApp();
    GameState *s = app->GetState();
    RotateBox rb;
    rb.origin = *origin;
    rb.angle = angle;
    rb.width = width->y;
    float delta = mid->x - origin->x;
    rb.len_start = delta - width->x / 2;
    rb.len_end = delta + width->x / 2;
    s->laser_line.push_back(rb);
  }
  return ret;
}

__declspec(naked) static void HitTestLaserRetDetour() {
  __asm {
    mov edx, eax;
    jmp HitTestLaserDetour;
  }
}

static void PreTickDetour() {
  RemoteApp *app = RemoteApp::GetApp();
  app->GetState()->reset(&Th08Reader::kConfig);
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
  Th08LocalReader lr(*reader, app->GetState());
  lr.Read(!app->GetConfig()->hook);
  app->OnPostTick();
}

__declspec(naked) static void PostTickRetDetour() {
  __asm pushad;
  PostTickDetour();
  __asm popad;
  __asm retn;
}

static int __stdcall PresentAndSnapshotDetour() {
  RemoteApp *app = RemoteApp::GetApp();
  app->OnPrePaint();
  StateReader *reader = app->GetReader();
  int ret = PresentAndSnapshot_F();
  if (StateReader::unhook_req_) {
    if (hook::PatchBatch(reader->RebaseGet(), kPatchDef, 1) == 0) {
      SetEvent(StateReader::unhooked_event_);
    }
  }
  return ret;
}

static void PostPresentDetour() {
  RemoteApp *app = RemoteApp::GetApp();
  app->OnPostPaint();
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
     {0xe8, 0x7a, 0x8b, 0x01, 0x00},
     ULongToPtr(0x431240 + 0x471),
     HitTestBulletDetour,
     (void **)&HitTestBullet_F},
    {0xE8,
     5,
     {0xe8, 0xf9, 0xdf, 0x01, 0x00},
     ULongToPtr(0x42C290 + 0xD2),
     HitTestEnemyDetour,
     (void **)&HitTestEnemy_F},
    {0xC3, 2, {0xc2, 0x14}, ULongToPtr(0x44A928), HitTestLaserRetDetour, &HitTestLaser_Ret},
    {0xC3, 2, {0xc3, 0xcc}, ULongToPtr(0x452305), PreTickRetDetour, (void **)&PreTick_ReplayRet},
    {0xC3, 2, {0xc3, 0xcc}, ULongToPtr(0x4526B9), PreTickRetDetour, (void **)&PreTick_PlayRet},
    {0xC3, 2, {0xc3, 0xcc}, ULongToPtr(0x452547), PostTickRetDetour, (void **)&PostTick_ReplayRet},
    {0xC3, 2, {0xc3, 0xcc}, ULongToPtr(0x452483), PostTickRetDetour, (void **)&PostTick_PlayRet},
    {0xE8,
     5,
     {0xe8, 0x1e, 0x00, 0x00, 0x00},
     ULongToPtr(0x44203D),
     PresentAndSnapshotDetour,
     (void **)&PresentAndSnapshot_F},
    {0xC3, 2, {0xc3, 0xcc}, ULongToPtr(0x442186), PostPresentRetDetour, (void **)&PostPresent_Ret},
    {0, NULL, NULL, NULL}};
}

int Th08Reader::GetId() const { return kGameId; }

int Th08Reader::Check() const { return hook::PatchCheck(hProc, RebaseGet(), hook08::kPatchDef); }

int Th08Reader::Read(GameState *state) const {
  if (hProc == NULL) {
    // unexpected
    return 0;
  } else {
    state->reset(&kConfig);
    Th08RemoteReader rr(hProc, *this, state);
    return rr.Read();
  }
}

StateReader *Th08Reader::Clone() const { return new Th08Reader(); }

int Th08Reader::Hook() const {
  RemoteApp::GetApp()->GetState()->reset(&kConfig);
  unhook_req_ = 0;
  return hook::PatchBatch(RebaseGet(), hook08::kPatchDef, 0);
}

int Th08Reader::Unhook() const {
  // return hook::PatchBatch(kImageBase, kImageBase, kPatchDef, 1);
  if (unhooked_event_ == NULL) return 0;
  unhook_req_ = 1;
  // attempt 1: unhook on next call, timeout=1s
  if (WaitForSingleObject(unhooked_event_, 1000) != WAIT_OBJECT_0) {
    // attempt 2: unhook now
    hook::PatchBatch(RebaseGet(), hook08::kPatchDef, 1);
  }
  Sleep(500);
  return 0;
}

DWORD_PTR Th08Reader::GetImageBase() const { return 0x400000; }
}
