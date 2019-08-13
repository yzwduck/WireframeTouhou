#include "Th17.h"
#include "remote_app.h"
#include "hook_util.h"

namespace th {

static const void* kPlayerInst = ULongToPtr(0x4B77D0);
static const void* kBulletInst = ULongToPtr(0x4A6DAC);
static const void* kEnemyInst = ULongToPtr(0x4A6DC0);
static const void* kLaserInst = ULongToPtr(0x4A6EE0);

const GameStateConfig Th17Reader::kConfig = {-192.0f, 0.0f, 192.0f, 450.0f, 1, 1};

typedef struct {
  char unk0[0x610];
  Point2f pos;
  char unk618[0x18FE4 - 0x618];
  Point3f tl;
  Point3f br;
} Th17PlayerFull;

typedef struct Th17LinkList {
  ULONG32 This;
  ULONG32 Next;
} Th17LinkList;

// enemy

typedef struct Th17Enemy {
  Point2f pos1;
  char unk8[0x44 - 0x8];
  Point2f pos;
  char unk4c[0x118 - 0x4C];
  float radius;
} Th17Enemy;  // Enemy instance +0x120C

typedef struct Th17EnemyFlag {
  char unk0[0x14];
  UINT32 flag4014;
  char unk4018[0x4060 - 0x4018];
  UINT32 flag4060;
  UINT32 flag4064;
  char unk4068[0x4524 - 0x4068];
  UINT32 flag4524;
} Th17EnemyFlag;  // Enemy instance +0x4000

typedef struct Th17EnemyMgr {
  char unk0[0x180];
  Th17LinkList* head;
} Th17EnemyMgr;

typedef struct Th17EnemyFull {
  char unk0[0x120C];
  // reset: current=0x120C
  Th17Enemy ed;
  char unk11c[0x4000 - 0x11C];
  // reset: current=0x4000
  Th17EnemyFlag ef;
} Th17EnemyFull;

// bullet

typedef struct Th17Bullet {
  char unk_0[0x20 - 0];
  int flag20;
  char unk_24[0xC20 - 0x24];
  Point2f pos;  // +0xC20
  char unk_c28[0xC40 - 0xC28];
  float size;  // +0xC40
  char unk_c44[0xC70 - 0xC44];
  short flag_c70;
  short flag_c72;
  char unk_c74[0x141C - 0xC74];
  float val_141c;
  char unk_1420[0x1450 - 0x1420];
  int flag_1450;
} Th17Bullet;

// laser

typedef struct Th17LaserSeg {
  Point2f pos;
  float unk1;
  float unk2;
  float unk3;
  float unk4;
  float angle;
  float len;
} Th17LaserSeg;

typedef struct Th17LaserLink {
  ULONG32 vtable;  // +0x0
  ULONG32 unk4;    // +0x4
  ULONG32 Next;    // +0x8
  int flag_c;      // +0xC
  int flag_10;     // +0x10

  char unk_14[0x54 - 0x14];

  Point2f pos;  // +0x54
  float pos_z;
  char unk_60[0x6C - 0x60];
  float angle;  // +0x6C
  float len;    // +0x70
  float width;  // +0x74
  char unk_78[0x5F4 - 0x78];
  unsigned int num_seg;  // +0x5F4
  char unk_0x5F8[0x608 - 0x5F8];
  int flag_608;  // +0x608
  char unk_60c[0x1524 - 0x60c];
  ULONG32 segments;
} Th17LaserLink;

typedef struct {
  char unk[0x14];
  Th17LaserLink* head;
} Th17LaserMgr;

static void BulletToState(GameState* s, Th17Bullet* bd) {
  if (((bd->flag_c72 & 0xff) == 2 && bd->flag_1450 >= 8) ||
      ((bd->flag_c72 & 0xff) == 1 && (bd->flag20 & 0x200) == 0)) {
    if (bd->flag20 & 0x50) {
      HitBox b;
      float size = bd->size;
      if ((bd->flag20 & 0x50) == 0x50) size = bd->size * bd->val_141c;
      b.pos = bd->pos;
      b.size.x = size * 2;
      b.size.y = size * 2;
      s->bullet.push_back(b);
    } else {
      // test_unknown
    }
  }
}

static void EnemyToState(GameState* s, Th17Enemy* ed, Th17EnemyFlag* ef) {
  if ((ef->flag4060 & 0x22) == 0 && ef->flag4014 == 0 && (ef->flag4060 & 0x4000000) == 0 &&
      ef->flag4524 == 0) {
    if ((ef->flag4060 & 0x1000) == 0) {
      HitBox b;
      b.pos = ed->pos;
      b.size.x = ed->radius;
      b.size.y = ed->radius;
      s->enemy.push_back(b);
    } else {
      // laser?
    }
  }
}

static void LaserLineToState(GameState* s, Th17LaserLink* ll) {
  RotateBox rb;
  rb.origin.x = ll->pos.x;
  rb.origin.y = ll->pos.y;
  rb.angle = ll->angle;

  if ((ll->flag_608 & 0x2) == 0) {
    rb.len_start = ll->len * 0.1f;
    rb.len_end = ll->len - rb.len_start;
  } else {
    rb.len_start = 0.0f;
    rb.len_end = ll->len;
  }
  if (ll->width >= 32.0f) {
    rb.width = ll->width / 2 - 8.0f;
  } else {
    rb.width = ll->width / 2;
  }
  s->laser_line.push_back(rb);
}

static void LaserCurveToState(GameState* s, Th17LaserLink* ll, Th17LaserSeg* seg, int n_seg) {
  LaserCurve cur;
  cur.idx_start = (short)s->laser_curve_seg.size();
  cur.width = ll->width / 2;
  float total_len = 0.0f;

  for (int i = 0; i < n_seg; i++) {
    total_len += seg[i].len;
    if (total_len >= 16.0f && seg[i].len > 0.01f) {
      LaserCurveSeg ss;
      ss.pos.x = seg[i].pos.x;
      ss.pos.y = seg[i].pos.y;
      ss.angle = seg[i].angle;
      ss.len = seg[i].len;
      s->laser_curve_seg.push_back(ss);
    }
  }
  cur.segments = (short)s->laser_curve_seg.size() - cur.idx_start;
  s->laser_curve.push_back(cur);
}

static void LaserInfiniteToState(GameState* s, Th17LaserLink* ll) {
  RotateBox rb;
  rb.origin.x = ll->pos.x;
  rb.origin.y = ll->pos.y;
  rb.angle = ll->angle;

  if (ll->width >= 32.0f) {
    rb.width = ll->width - (ll->width + 16.0f) / 3;
  } else {
    rb.width = ll->width * 0.5f;
  }
  rb.len_start = 0.0f;
  rb.len_end = ll->len * 0.9f;
  s->laser_line.push_back(rb);
}

class Th17RemoteReader : public RebaseUtil {
 public:
  Th17RemoteReader(HANDLE hProc, const RebaseUtil& This, GameState* state);
  int Read() const;
  int ReadPlayer() const;
  int ReadBullet() const;
  int ReadEnemy() const;
  int ReadLaser() const;

 private:
  HANDLE hProc;
  GameState* s;
};

Th17RemoteReader::Th17RemoteReader(HANDLE hProc, const RebaseUtil& This, GameState* state)
    : RebaseUtil(This), hProc(hProc), s(state) {}

int Th17RemoteReader::Read() const {
  ReadPlayer();
  ReadBullet();
  ReadEnemy();
  ReadLaser();
  return 0;
}

int Th17RemoteReader::ReadPlayer() const {
  do {
    ULONG32 inst;
    if (!ReadProcMemRe(hProc, kPlayerInst, &inst, sizeof inst)) break;
    Point2f pos;
    Point2f tl;
    Point2f br;
    if (!ReadProcMem(hProc, ULongToPtr(inst + offsetof(Th17PlayerFull, pos)), &pos, sizeof pos))
      break;
    if (!ReadProcMem(hProc, ULongToPtr(inst + offsetof(Th17PlayerFull, br)), &tl, sizeof tl)) break;
    if (!ReadProcMem(hProc, ULongToPtr(inst + offsetof(Th17PlayerFull, tl)), &br, sizeof br)) break;
    s->player.pos = pos;
    s->player.size.x = br.x - tl.x;
    s->player.size.y = br.y - tl.y;
  } while (0);
  return 0;
}

int Th17RemoteReader::ReadBullet() const {
  ULONG32 inst = 0;
  if (ReadProcMemRe(hProc, kBulletInst, &inst, sizeof inst)) {
    ULONG Next = 0;
    if (ReadProcMem(hProc, ULongToPtr(inst + 0x70), &Next, sizeof Next)) {
      while (Next != 0 && !s->bullet.full()) {
        Th17LinkList ll;
        if (ReadProcMem(hProc, ULongToPtr(Next), &ll, sizeof ll)) {
          Th17Bullet bd;
          if (ReadProcMem(hProc, ULongToPtr(ll.This), &bd, sizeof bd)) {
            BulletToState(s, &bd);
          }
          Next = ll.Next;
        } else {
          Next = 0;
        }
      }
    }
  }
  return 0;
}

int Th17RemoteReader::ReadEnemy() const {
  ULONG32 inst = 0;
  if (ReadProcMemRe(hProc, kEnemyInst, &inst, sizeof inst) && inst != 0) {
    ULONG32 Next = 0;
    if (ReadProcMem(hProc, ULongToPtr(inst + 0x180), &Next, sizeof Next)) {
      while (Next && !s->enemy.full()) {
        Th17LinkList ll;
        if (ReadProcMem(hProc, ULongToPtr(Next), &ll, sizeof ll)) {
          Next = ll.Next;
          Th17Enemy ed;
          Th17EnemyFlag ef;
          if (ReadProcMem(hProc, ULongToPtr(ll.This + offsetof(Th17EnemyFull, ed)), &ed,
                          sizeof ed) &&
              ReadProcMem(hProc, ULongToPtr(ll.This + offsetof(Th17EnemyFull, ef)), &ef,
                          sizeof ef)) {
            EnemyToState(s, &ed, &ef);
          }
        } else {
          Next = NULL;
        }
      }
    }
  }
  return 0;
}

int Th17RemoteReader::ReadLaser() const {
  ULONG32 inst = 0;
  if (ReadProcMemRe(hProc, kLaserInst, &inst, sizeof inst)) {
    ULONG32 Next = 0;
    if (ReadProcMem(hProc, ULongToPtr(inst + 0x14), &Next, sizeof Next)) {
      while (Next != 0 && !s->laser_line.full() && !s->laser_curve.full() &&
             !s->laser_curve_seg.full()) {
        Th17LaserLink ll;
        if (ReadProcMem(hProc, ULongToPtr(Next), &ll, sizeof ll)) {
          if ((ll.vtable & 0xFFF) == 0x424) {  // LaserLineInf
            LaserLineToState(s, &ll);
          } else if ((ll.vtable & 0xFFF) == 0x2E0) {  // LaserCurve
            Th17LaserSeg ls[256];
            unsigned int n_seg = ll.num_seg - 1;
            if (n_seg > _countof(ls)) n_seg = _countof(ls);
            if (ReadProcMem(hProc, ULongToPtr(ll.segments), ls, n_seg * sizeof ls[0])) {
              LaserCurveToState(s, &ll, ls, n_seg);
            }
          } else if ((ll.vtable & 0xFFF) == 0x3B8) {  // Infinite
            LaserInfiniteToState(s, &ll);
          }
          Next = ll.Next;
        } else {
          Next = 0;
        }
      }
    }
  }
  return 0;
}

class Th17LocalReader : public RebaseUtil {
 public:
  Th17LocalReader(const RebaseUtil& This, GameState* state);
  int Read(bool read_hit) const;
  int ReadPlayer() const;

 private:
  GameState* s;
};

Th17LocalReader::Th17LocalReader(const RebaseUtil& This, GameState* state)
    : RebaseUtil(This), s(state) {}

int Th17LocalReader::Read(bool read_hit) const {
  ReadPlayer();
  if (read_hit) {
  }
  return 0;
}

int Th17LocalReader::ReadPlayer() const {
  int succ = 0;
  do {
    ULONG32 base = *(ULONG32*)Rebase(kPlayerInst);
    if (base == NULL) break;
    Th17PlayerFull* p = (Th17PlayerFull*)base;
    s->player.pos = p->pos;
    s->player.size.x = p->br.x - p->tl.x;
    s->player.size.y = p->br.y - p->tl.y;
    succ = 1;
  } while (0);

  if (!succ) ZeroMemory(&s->player, sizeof s->player);
  return 0;
}

namespace hook17 {
typedef int(__fastcall* RunLogic_T)(void* arg);
typedef int(__fastcall* FinishScene_T)(void* arg);
typedef int(__fastcall* PostGameTick_T)(void* arg);
typedef int(__stdcall* ScreenShotSurface_T)();
typedef int(__stdcall* GetTimeSync_T)();
typedef int(__stdcall* HitTestBox_T)(Point2f* pos, float a3);  // radius@xmm2, player@static_inst
typedef int(__stdcall* HitTestLaser_T)(Point2f* pos, float len,
                                       int flag);  // angle@xmm2, width@xmm3
// typedef int(__stdcall* PreTick_T)();
// static FinishScene_T FinishScene_F1;
// static FinishScene_T FinishScene_F2;
// static FinishScene_T FinishScene_F3;
static PostGameTick_T PostGameTick_F;
static ScreenShotSurface_T ScreenShotSurface_F;
static GetTimeSync_T GetTimeSync_F1;
static GetTimeSync_T GetTimeSync_F2;
static GetTimeSync_T GetTimeSync_F3;
static HitTestBox_T HitTestBox_F1;
static HitTestBox_T HitTestBox_F2;
static HitTestLaser_T HitTestLaser_F1;
static HitTestLaser_T HitTestLaser_F2;
static HitTestLaser_T HitTestLaser_F3;
static HitTestLaser_T HitTestLaser_F4;
static void* PreTick_J;
static void* PostTick_J;

extern const hook::PATCH_REGION_DEFINITION kPatchDef[];

static int __fastcall PostTickCallDetour(void* arg) {
  RemoteApp* app = RemoteApp::GetApp();
  StateReader* reader = app->GetReader();
  Th17LocalReader lr(*reader, app->GetState());
  lr.Read(false);
  app->OnPostTick();
  return PostGameTick_F(arg);
}

static int __stdcall ScreenShotSurfaceDetour() {
  RemoteApp* app = RemoteApp::GetApp();
  app->OnPrePaint();
  StateReader* reader = app->GetReader();
  int ret = ScreenShotSurface_F();
  if (StateReader::unhook_req_) {
    if (hook::PatchBatch(reader->RebaseGet(), kPatchDef, 1) == 0) {
      SetEvent(StateReader::unhooked_event_);
    }
  }
  return ret;
}

static int __stdcall GetTimeSyncDetour() {
  RemoteApp* app = RemoteApp::GetApp();
  app->OnPostPaint();
  int ret = GetTimeSync_F1();
  return ret;
}

static int __fastcall FinishSceneDetour(void* arg) {
  RemoteApp* app = RemoteApp::GetApp();
  app->OnPrePaint();
  StateReader* reader = app->GetReader();
  int ret = 0;  // FinishScene_F1(arg);
  if (StateReader::unhook_req_) {
    if (hook::PatchBatch(reader->RebaseGet(), kPatchDef, 1) == 0) {
      SetEvent(StateReader::unhooked_event_);
    }
  } else {
    app->OnPostPaint();
  }
  return ret;
}

static int __stdcall HitTestBoxDetour(Point2f* pos, float a3) {
  float radius;
  __asm movss radius, xmm2;
  int ret = HitTestBox_F1(pos, a3);
  RemoteApp* app = RemoteApp::GetApp();
  if (RemoteApp::GetConfig()->hook) {
    GameState* s = app->GetState();
    HitBox b;
    b.pos = *pos;
    b.size.x = b.size.y = radius * 2;
    s->bullet.push_back(b);
  }
  return ret;
}

static int __stdcall HitTestLaserDetour(Point2f* pos, float len, int flag) {
  float angle, width;
  __asm movss angle, xmm2;
  __asm movss width, xmm3;
  int ret = HitTestLaser_F1(pos, len, flag);
  RemoteApp* app = RemoteApp::GetApp();
  if (RemoteApp::GetConfig()->hook) {
    GameState* s = app->GetState();
    LaserCurveSeg seg;
    seg.pos = *pos;
    seg.len = len;
    seg.angle = angle;
    s->add_merge_laser_seg(width, &seg);
  }
  return ret;
}

static void PreTickDetour() {
  RemoteApp* app = RemoteApp::GetApp();
  app->GetState()->reset(&Th17Reader::kConfig);
  app->OnPreTick();
}

__declspec(naked) static void PreTickJmpDetour() {
  __asm pushad;
  PreTickDetour();
  __asm popad;
  __asm jmp PreTick_J;
}

static void PostTickDetour() {
  RemoteApp* app = RemoteApp::GetApp();
  StateReader* reader = app->GetReader();
  Th17LocalReader lr(*reader, app->GetState());
  lr.Read(false);
  app->OnPostTick();
}

__declspec(naked) static void PostTickRetDetour() {
  __asm pop edi;
  __asm pop esi;
  __asm pop ebx;
  __asm pushad;
  PostTickDetour();
  __asm popad;
  __asm retn;
}

__declspec(naked) static void PostTickJmpDetour() {
  __asm pushad;
  PostTickDetour();
  __asm popad;
  __asm retn;
}

static void ScreenshotDetour() {
  RemoteApp* app = RemoteApp::GetApp();
  app->OnPrePaint();
  StateReader* reader = app->GetReader();
  if (StateReader::unhook_req_) {
    if (hook::PatchBatch(reader->RebaseGet(), kPatchDef, 1) == 0) {
      SetEvent(StateReader::unhooked_event_);
    }
  }
}

__declspec(naked) static void PostScreenshotRetDetour() {
  __asm pushad;
  ScreenshotDetour();
  __asm popad;
  __asm retn;
}

const hook::PATCH_REGION_DEFINITION kPatchDef[] = {
  {0xE9, 5, {0xe9, 0x3b, 0xf6, 0xff, 0xff}, ULongToPtr(0x40AD50), PreTickJmpDetour, &PreTick_J},
  {0xC3, 5, {0x5f, 0x5e, 0x5b, 0xc3, 0xcc}, ULongToPtr(0x41B7FB), PostTickRetDetour, (void**)&PostGameTick_F},
  {0xC3, 5, {0xc3, 0xcc, 0xcc, 0xcc, 0xcc}, ULongToPtr(0x462709), PostScreenshotRetDetour, (void**)& ScreenShotSurface_F},
  {0xE8, 5, {0xe8, 0x32, 0x3a, 0x03, 0x00}, ULongToPtr(0x4154B9), HitTestBoxDetour, (void**)& HitTestBox_F1},
  {0xE8, 5, {0xe8, 0xe7, 0x90, 0x02, 0x00}, ULongToPtr(0x41FE04), HitTestBoxDetour, (void**)& HitTestBox_F1},
  {0xE8, 5, {0xe8, 0x04, 0x91, 0x02, 0x00}, ULongToPtr(0x41FEE7), HitTestLaserDetour, (void**)&HitTestLaser_F1},
  {0xE8, 5, {0xe8, 0xb2, 0x16, 0x01, 0x00}, ULongToPtr(0x437939), HitTestLaserDetour, (void**)&HitTestLaser_F1},
  {0xE8, 5, {0xe8, 0xd1, 0xf0, 0x00, 0x00}, ULongToPtr(0x439F1A), HitTestLaserDetour, (void**)&HitTestLaser_F1},
  {0xE8, 5, {0xe8, 0x78, 0xee, 0x00, 0x00}, ULongToPtr(0x43A173), HitTestLaserDetour, (void**)& HitTestLaser_F1},
  {0xE8, 5, {0xe8, 0x57, 0xbd, 0x00, 0x00}, ULongToPtr(0x43D294), HitTestLaserDetour, (void**)& HitTestLaser_F1},
  {0, NULL, NULL, NULL}};
}

int Th17Reader::GetId() const { return kGameId; }

int Th17Reader::Check() const { return hook::PatchCheck(hProc, RebaseGet(), hook17::kPatchDef); }

int Th17Reader::Read(GameState* state) const {
  if (hProc == NULL) {
    // unexpected
    return 0;
  } else {
    state->reset(&kConfig);
    Th17RemoteReader rr(hProc, *this, state);
    return rr.Read();
  }
}

StateReader* Th17Reader::Clone() const { return new Th17Reader(); }

int Th17Reader::Hook() const {
  RemoteApp::GetApp()->GetState()->reset(&kConfig);
  unhook_req_ = 0;
  return hook::PatchBatch(RebaseGet(), hook17::kPatchDef, 0);
}

int Th17Reader::Unhook() const {
  // return hook::PatchBatch(kImageBase, kImageBase, kPatchDef, 1);
  if (unhooked_event_ == NULL) return 0;
  unhook_req_ = 1;
  // attempt 1: unhook on next call, timeout=1s
  if (WaitForSingleObject(unhooked_event_, 1000) != WAIT_OBJECT_0) {
    // attempt 2: unhook now
    hook::PatchBatch(RebaseGet(), hook17::kPatchDef, 1);
  }
  Sleep(500);
  return 0;
}

DWORD_PTR Th17Reader::GetImageBase() const { return 0x400000; }
}
