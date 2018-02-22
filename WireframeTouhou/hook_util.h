#ifndef HOOK_UTIL_H
#define HOOK_UTIL_H

#include <Windows.h>

namespace th {
namespace hook {

typedef struct {
  UINT8 kind;
  UINT8 len;
  UINT8 code[6];
  const void *target;  // address of code to be patched
  const void *detour;  // address of the relay function
  void **origin;       // original callee address
} PATCH_REGION_DEFINITION;

int PatchCheck(HANDLE hProc, DWORD_PTR delta, const PATCH_REGION_DEFINITION *def);

int PatchBatch(DWORD_PTR delta, const PATCH_REGION_DEFINITION *def, int revert);
}
}

#endif
