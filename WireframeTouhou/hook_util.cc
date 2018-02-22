#include "hook_util.h"

#include <Windows.h>
#include <TlHelp32.h>

namespace th {
namespace hook {

typedef struct _FROZEN_THREADS {
  LPDWORD pItems;  // Data heap
  UINT capacity;   // Size of allocated data heap, items
  UINT size;       // Actual number of data items
} FROZEN_THREADS, *PFROZEN_THREADS;

VOID EnumerateThreads(PFROZEN_THREADS pThreads);

VOID Freeze(PFROZEN_THREADS pThreads);

VOID Unfreeze(PFROZEN_THREADS pThreads);

#pragma pack(push, 1)
typedef struct AsmOpRel32 {
  UINT8 op;
  INT32 offset;
} AsmOpRel32;
#pragma pack(pop)

#define INITIAL_THREAD_CAPACITY 32
#define THREAD_ACCESS (THREAD_SUSPEND_RESUME | THREAD_QUERY_INFORMATION)

static void PatchCallE8(void *target, const void *detour, void **origin) {
  AsmOpRel32 *code = (AsmOpRel32 *)target;
  char *callee = (char *)(code + 1) + code->offset;
  INT32 detour_offset = (char *)detour - (char *)(code + 1);
  code->offset = detour_offset;
  *origin = callee;
}

void PatchJmpFF(void *target, const void *detour, void **origin) {
  DWORD_PTR *writer = (DWORD_PTR *)target;
  *origin = (void *)*writer;
  *writer = (DWORD_PTR)detour;
}

void PatchTailRetC3(void *target, const void *detour, void **origin, int revert) {
  if (!revert) {
    AsmOpRel32 *code = (AsmOpRel32 *)target;
    DWORD_PTR *writer = (DWORD_PTR *)origin;
    DWORD_PTR *reader = (DWORD_PTR *)target;
    *writer = *reader;

    INT32 detour_offset = (char *)detour - (char *)(code + 1);
    code->op = 0xE9;
    code->offset = detour_offset;
  } else {
    DWORD_PTR *writer = (DWORD_PTR *)target;
    *writer = (DWORD_PTR)*origin;
  }
}

int PatchCheck(HANDLE hProc, DWORD_PTR delta, const PATCH_REGION_DEFINITION *def) {
  UINT8 buf[8];
  SIZE_T len_out;
  int failed = 0;
  for (const PATCH_REGION_DEFINITION *iter = def; iter->target != NULL && !failed; iter++) {
    int ok = 0;
    DWORD_PTR re_addr = (DWORD_PTR)iter->target + delta;
    if (ReadProcessMemory(hProc, (void *)re_addr, buf, iter->len, &len_out)) {
      if (len_out == iter->len && memcmp(buf, iter->code, iter->len) == 0) {
        ok = 1;
      }
    }
    if (!ok) failed = 1;
  }
  return failed;
}

// some unused value from
// https://msdn.microsoft.com/en-us/library/windows/desktop/aa366786(v=vs.85).aspx
#define PAGE_OVERLAPPED 0x10000

typedef struct {
  DWORD old_protect;
  PVOID base;
  SIZE_T size;
} MemoryPatchInfo;

int PatchBatch(DWORD_PTR delta, const PATCH_REGION_DEFINITION *def, int revert) {
  MemoryPatchInfo *mpi;
  int n_def = 0;
  for (const PATCH_REGION_DEFINITION *iter = def; iter->target != NULL; iter++) n_def++;
  mpi = (MemoryPatchInfo *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, n_def * sizeof mpi[0]);
  if (mpi == NULL) return 2;

  do {
    bool failed = false;
    // step 1: prepare memory protection
    for (int i = 0; i < n_def && !failed; i++) {
      if (mpi[i].old_protect == PAGE_OVERLAPPED) continue;
      MEMORY_BASIC_INFORMATION info;
      DWORD_PTR re_addr = (DWORD_PTR)def[i].target + delta;
      if (VirtualQuery((void *)re_addr, &info, sizeof info) == 0) {
        failed = true;
      } else if (info.State != MEM_COMMIT || info.Protect == 0) {
        failed = true;
      } else if (info.Protect & PAGE_READWRITE) {
        mpi[i].old_protect = PAGE_OVERLAPPED;
      } else if (!VirtualProtect(info.BaseAddress, info.RegionSize, PAGE_EXECUTE_READWRITE,
                                 &mpi[i].old_protect)) {
        failed = true;
      } else {
        mpi[i].base = info.BaseAddress;
        mpi[i].size = info.RegionSize;
        DWORD_PTR start = (DWORD_PTR)info.BaseAddress;
        DWORD_PTR end = start + info.RegionSize;
        for (int j = i + 1; def[j].target != NULL; j++) {
          DWORD_PTR next_addr = (DWORD_PTR)def[j].target + delta;
          if (start <= next_addr && next_addr < end) {
            mpi[j].old_protect = PAGE_OVERLAPPED;
          }
        }
      }
    }
    if (failed) break;

    // step 2: patch memory
    for (int i = 0; i < n_def; i++) {
      const PATCH_REGION_DEFINITION *iter = &def[i];
      void *origin;
      void *target = (void *)(DWORD_PTR(iter->target) + delta);
      switch (iter->kind) {
        case 0xE8:
        case 0xE9:
          if (!revert)
            PatchCallE8(target, iter->detour, iter->origin);
          else
            PatchCallE8(target, *iter->origin, &origin);
          break;
        case 0xC3:
          PatchTailRetC3(target, iter->detour, iter->origin, revert);
          break;
        default:
          return 3;
      }
    }

    // step 3: restore memory protection
    for (int j = n_def; j > 0; j--) {
      const int i = n_def - j - 1;
      if (mpi[i].old_protect != PAGE_OVERLAPPED) {
        DWORD tmp;
        VirtualProtect(mpi[i].base, mpi[i].size, mpi[i].old_protect, &tmp);
        FlushInstructionCache(GetCurrentProcess(), mpi[i].base, mpi[i].size);
      }
    }
  } while (0);
  if (mpi != NULL) HeapFree(GetProcessHeap(), 0, mpi);
  return 0;
}

VOID EnumerateThreads(PFROZEN_THREADS pThreads) {
  HANDLE g_hHeap = GetProcessHeap();
  HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
  if (hSnapshot != INVALID_HANDLE_VALUE) {
    THREADENTRY32 te;
    te.dwSize = sizeof(THREADENTRY32);
    if (Thread32First(hSnapshot, &te)) {
      do {
        if (te.dwSize >= (FIELD_OFFSET(THREADENTRY32, th32OwnerProcessID) + sizeof(DWORD)) &&
            te.th32OwnerProcessID == GetCurrentProcessId() &&
            te.th32ThreadID != GetCurrentThreadId()) {
          if (pThreads->pItems == NULL) {
            pThreads->capacity = INITIAL_THREAD_CAPACITY;
            pThreads->pItems = (LPDWORD)HeapAlloc(g_hHeap, 0, pThreads->capacity * sizeof(DWORD));
            if (pThreads->pItems == NULL) break;
          } else if (pThreads->size >= pThreads->capacity) {
            LPDWORD p = (LPDWORD)HeapReAlloc(g_hHeap, 0, pThreads->pItems,
                                             (pThreads->capacity * 2) * sizeof(DWORD));
            if (p == NULL) break;

            pThreads->capacity *= 2;
            pThreads->pItems = p;
          }
          pThreads->pItems[pThreads->size++] = te.th32ThreadID;
        }

        te.dwSize = sizeof(THREADENTRY32);
      } while (Thread32Next(hSnapshot, &te));
    }
    CloseHandle(hSnapshot);
  }
}

VOID Freeze(PFROZEN_THREADS pThreads) {
  pThreads->pItems = NULL;
  pThreads->capacity = 0;
  pThreads->size = 0;
  EnumerateThreads(pThreads);

  if (pThreads->pItems != NULL) {
    UINT i;
    for (i = 0; i < pThreads->size; ++i) {
      HANDLE hThread = OpenThread(THREAD_ACCESS, FALSE, pThreads->pItems[i]);
      if (hThread != NULL) {
        SuspendThread(hThread);
        CloseHandle(hThread);
      }
    }
  }
}

VOID Unfreeze(PFROZEN_THREADS pThreads) {
  if (pThreads->pItems != NULL) {
    UINT i;
    for (i = 0; i < pThreads->size; ++i) {
      HANDLE hThread = OpenThread(THREAD_ACCESS, FALSE, pThreads->pItems[i]);
      if (hThread != NULL) {
        ResumeThread(hThread);
        CloseHandle(hThread);
      }
    }

    HeapFree(GetProcessHeap(), 0, pThreads->pItems);
  }
}
}
}
