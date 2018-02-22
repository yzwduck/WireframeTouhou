#include "state_reader.h"

#include <Psapi.h>

#include "inject.h"
#include "th08.h"
#include "th10.h"
#include "th11.h"
#include "th12.h"
#include "th13.h"
#include "th14.h"
#include "th15.h"
#include "th16.h"

#pragma comment(lib, "psapi.lib")

namespace th {

int StateReader::unhook_req_;

HANDLE StateReader::unhooked_event_;

StateReader::StateReader() {}

int StateReader::Init(HANDLE hProc) {
  if (hProc == NULL) {
    // local mode
    DWORD_PTR base = (DWORD_PTR)GetModuleHandle(NULL);
    // IMAGE_NT_HEADERS *pe = (IMAGE_NT_HEADERS *)(base + ((PIMAGE_DOS_HEADER)base)->e_lfanew);
    RebaseSet(GetImageBase() - base);
  } else {
    RebaseSet(0);
    DWORD pid = GetProcessId(hProc);
    TCHAR path[MAX_PATH];
    if (pid != 0 && GetModuleFileNameEx(hProc, NULL, path, _countof(path))) {
      HANDLE snapshot = RemoteModuleList(pid);
      DWORD_PTR base = (DWORD_PTR)RemoteModuleLocate(snapshot, path);
      RebaseSet(GetImageBase() - base);
      CloseHandle(snapshot);
    } else {
      // TODO: error handling
    }
  }
  this->hProc = hProc;
  return 0;
}

int StateReader::Init(const StateReader &other) {
  this->hProc = other.hProc;
  RebaseSet(other.RebaseGet() - other.GetImageBase() + GetImageBase());
  return 0;
}

StateReader *th::StateReader::BuildReader(int id) {
  for (int i = 0; instances[i] != nullptr; i++) {
    if (instances[i]->GetId() == id) {
      return instances[i]->Clone();
    }
  }
  return nullptr;
}

static Th08Reader r08;
static Th10Reader r10;
static Th11Reader r11;
static Th12Reader r12;
static Th13Reader r13;
static Th14Reader r14;
static Th15Reader r15;
static Th16Reader r16;

StateReader *StateReader::instances[] = {&r10, &r11, &r12, &r13, &r14, &r15, &r16, nullptr};
}
