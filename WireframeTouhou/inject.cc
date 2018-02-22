#include "inject.h"

#include <Windows.h>
#include <TlHelp32.h>
#include <winnt.h>

typedef FARPROC(WINAPI *GetProcAddress_T)(HMODULE hModule, LPCSTR lpProcName);

typedef VOID(WINAPI *FreeLibraryAndExitThread_T)(HMODULE hModule, DWORD dwExitCode);

typedef BOOL(WINAPI *FreeLibrary_T)(HMODULE hLibModule);

typedef HMODULE(WINAPI *LoadLibraryA_T)(LPCSTR lpLibFileName);

typedef BOOL(WINAPI *VirtualProtect_T)(LPVOID lpAddress, SIZE_T dwSize, DWORD flNewProtect,
                                       PDWORD lpflOldProtect);

typedef struct {
  // phrase 1
  TCHAR path[MAX_PATH];

  // phrase 2
  HMODULE this_mod;
  HMODULE hKernel32;
  GetProcAddress_T GetProcAddr_F;
  FreeLibraryAndExitThread_T FreeLibraryAndExitThread_F;
  FreeLibrary_T FreeLibrary_F;
  LoadLibraryA_T LoadLibraryA_F;
  VirtualProtect_T VirtualProtect_F;
  HANDLE event_init;
  InjecteeCtx injectee;
  DWORD n_modules;

  // HMODULE modules[0];
  // char payloads[0];
} InjecteeLdrCtx;

static InjecteeLdrCtx *g_injectee_ldr;
static int g_injectee_loaded;

static DWORD WINAPI InjecteeLdrEntry(InjecteeLdrCtx *ldr) {
  if (g_injectee_loaded) {
    return 0;
  }
  g_injectee_loaded = 1;
  // Resolve import function when loading EXE as DLL

  // WARN: DO NOT CALL ANY EXTERNAL FUNCTIONS until import functions are resolved

  // record
  g_injectee_ldr = ldr;

  // find import descriptor
  DWORD_PTR base = (DWORD_PTR)ldr->this_mod;
  IMAGE_NT_HEADERS *pe = (IMAGE_NT_HEADERS *)(base + ((PIMAGE_DOS_HEADER)base)->e_lfanew);
  IMAGE_IMPORT_DESCRIPTOR *pImportDesc =
      (IMAGE_IMPORT_DESCRIPTOR
           *)(base + pe->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
  DWORD max_virt_addr = 0;
  for (int i = 0; i < IMAGE_NUMBEROF_DIRECTORY_ENTRIES; i++) {
    IMAGE_DATA_DIRECTORY *dir = &pe->OptionalHeader.DataDirectory[i];
    if (dir->VirtualAddress + dir->Size > max_virt_addr)
      max_virt_addr = dir->VirtualAddress + dir->Size;
  }
  // resolve each module
  int n_dll = 0;
  HMODULE *modules = (HMODULE *)(&ldr[1]);
  DWORD ret = ERROR_SUCCESS;
  for (; pImportDesc->Name; pImportDesc++) {
    PSTR pszModName = (PSTR)(base + pImportDesc->Name);
    if (!pszModName) break;

    HINSTANCE hImportDLL = ldr->LoadLibraryA_F(pszModName);
    modules[n_dll++] = hImportDLL;
    if (hImportDLL == NULL) {
      ret = ERROR_MOD_NOT_FOUND;
      break;
    }

    IMAGE_THUNK_DATA *thunk_head = (IMAGE_THUNK_DATA *)(base + pImportDesc->FirstThunk);
    IMAGE_THUNK_DATA *thunk;
    SIZE_T size = 0;
    for (thunk = thunk_head; thunk->u1.Function; thunk++) {
      size += sizeof(*thunk);
    }
    DWORD old_protect = 0;
    if (!ldr->VirtualProtect_F(thunk_head, size, PAGE_READWRITE, &old_protect)) {
      ret = ERROR_ACCESS_DENIED;
      break;
    }

    // resolve each function
    for (thunk = thunk_head; thunk->u1.Function; thunk++) {
      FARPROC pfnNew = NULL;
      if (thunk->u1.Ordinal & IMAGE_ORDINAL_FLAG) {
        DWORD_PTR ord = IMAGE_ORDINAL(thunk->u1.Ordinal);
        pfnNew = ldr->GetProcAddr_F(hImportDLL, (LPCSTR)ord);
      } else {
        if (thunk->u1.Function < max_virt_addr) {
          PSTR fName = (PSTR)(base + thunk->u1.Function + 2);
          if (fName == NULL) {
            ret = ERROR_PROC_NOT_FOUND;
            break;
          }
          pfnNew = ldr->GetProcAddr_F(hImportDLL, fName);
        } else {
          pfnNew = (FARPROC)thunk->u1.Function;
        }
      }
      if (pfnNew == NULL) {
        ret = ERROR_PROC_NOT_FOUND;
        break;
      }
      // replace with real function address
      thunk->u1.Function = (DWORD_PTR)pfnNew;
    }
    if (ret != ERROR_SUCCESS) {
      // check `ret` from `for` loop
      break;
    }

    if (!ldr->VirtualProtect_F(thunk_head, size, old_protect, &old_protect)) {
      ret = ERROR_ACCESS_DENIED;
      break;
    }
  }
  if (ret != ERROR_SUCCESS) {
    for (int i = 0; i != ldr->n_modules; i++)
      if (modules[i] != NULL) ldr->FreeLibrary_F(modules[i]);
    ldr->FreeLibraryAndExitThread_F(ldr->this_mod, ret);
    return 0;
  }

  // import functions are resolved, so it's safe to program as normal

  SetEvent(ldr->event_init);
  DWORD_PTR addr = pe->OptionalHeader.AddressOfEntryPoint;
  typedef int (*EntryPoint_T)();
  EntryPoint_T entry = (EntryPoint_T)(addr + base);
  entry();
  // never returns, but just in case...
  return InjecteeExit();
}

HANDLE RemoteModuleList(DWORD pid) {
  BOOL ok = FALSE;
  MODULEENTRY32 entity;
  HANDLE snapshot = NULL;
  for (int retry = 0; retry < 4 && !ok; retry++) {
    snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
    if (snapshot != INVALID_HANDLE_VALUE) {
      entity.dwSize = sizeof entity;
      if (Module32First(snapshot, &entity)) {
        ok = TRUE;
      } else {
        // got an empty snapshot, try again
        CloseHandle(snapshot);
      }
    }
    Sleep(32);  // no choice but to wait
  }
  return snapshot;
}

HMODULE RemoteModuleLocate(HANDLE snapshot, const TCHAR *path) {
  HMODULE result = NULL;
  HANDLE handle =
      CreateFile(path, 0, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (handle == INVALID_HANDLE_VALUE) return NULL;

  BY_HANDLE_FILE_INFORMATION ref;
  BOOL succ = GetFileInformationByHandle(handle, &ref);
  CloseHandle(handle);
  if (!succ) return NULL;

  MODULEENTRY32 entity;
  entity.dwSize = sizeof entity;
  if (Module32First(snapshot, &entity)) {
    do {
      BY_HANDLE_FILE_INFORMATION test;
      handle = CreateFile(entity.szExePath, 0, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                          FILE_ATTRIBUTE_NORMAL, NULL);
      if (handle != INVALID_HANDLE_VALUE) {
        succ = GetFileInformationByHandle(handle, &test);
        CloseHandle(handle);
        if (succ) {
          if (ref.dwVolumeSerialNumber == test.dwVolumeSerialNumber &&
              ref.nFileIndexLow == test.nFileIndexLow &&
              ref.nFileIndexHigh == test.nFileIndexHigh) {
            result = entity.hModule;
            break;
          }
        }
      }
      entity.dwSize = sizeof entity;
    } while (Module32Next(snapshot, &entity));
  }
  return result;
}

static HMODULE RemoteModuleLocateByModule(HANDLE snapshot, HMODULE this_mod) {
  TCHAR path[MAX_PATH];
  if (GetModuleFileName(this_mod, path, _countof(path))) {
    return RemoteModuleLocate(snapshot, path);
  } else {
    return NULL;
  }
}

static HMODULE LocateRemoteModule(DWORD pid, HMODULE hModule) {
  HANDLE snapshot = RemoteModuleList(pid);
  HMODULE ret = RemoteModuleLocateByModule(snapshot, hModule);
  CloseHandle(snapshot);
  return ret;
}

static DWORD_PTR PtrOffset(void *this_ptr, void *this_mod, void *remote_mod) {
  return (DWORD_PTR)this_ptr - (DWORD_PTR)this_mod + (DWORD_PTR)remote_mod;
}

DWORD InjectSelfToProcess(DWORD pid, InjectRemoteControl *ctrl, VOID *data, SIZE_T len,
                          DWORD timeout, HANDLE cancel) {
  BOOL succ = FALSE;
  InjecteeLdrCtx ldr;
  HANDLE event_init = NULL;
  HANDLE ThisProc = GetCurrentProcess();
  SIZE_T dwOut;
  DWORD thread_id;
  HANDLE thread_ldr = NULL;
  HANDLE hProcess =
      OpenProcess(SYNCHRONIZE | PROCESS_CREATE_THREAD | PROCESS_DUP_HANDLE |
                      PROCESS_QUERY_INFORMATION | PROCESS_SUSPEND_RESUME | PROCESS_TERMINATE |
                      PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE,
                  FALSE, pid);
  HANDLE snapshot = NULL;

  do {
    ZeroMemory(ctrl, sizeof(*ctrl));
    if (hProcess == NULL) break;
    ctrl->remote_proc = hProcess;

    // count import modules
    DWORD module_count = 0;
    if (1) {
      DWORD_PTR base = (DWORD_PTR)GetModuleHandle(NULL);
      IMAGE_NT_HEADERS *pe = (IMAGE_NT_HEADERS *)(base + ((PIMAGE_DOS_HEADER)base)->e_lfanew);
      IMAGE_IMPORT_DESCRIPTOR *pImportDesc =
          (IMAGE_IMPORT_DESCRIPTOR *)(base +
                                      pe->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT]
                                          .VirtualAddress);
      for (; pImportDesc->Name; pImportDesc++) {
        PSTR pszModName = (PSTR)(base + pImportDesc->Name);
        if (!pszModName) break;
        module_count++;
      }
    }
    // allocate buffer in current process
    SIZE_T total_size = sizeof(ldr) + sizeof(HMODULE) * module_count + len;

    ZeroMemory(&ldr, sizeof ldr);
    ctrl->event_term = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (ctrl->event_term == NULL) break;
    event_init = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (event_init == NULL) break;
    if (!DuplicateHandle(ThisProc, event_init, hProcess, &ldr.event_init, EVENT_MODIFY_STATE, FALSE,
                         0))
      break;
    if (!DuplicateHandle(ThisProc, ctrl->event_term, hProcess, &ldr.injectee.event_term,
                         SYNCHRONIZE, FALSE, 0))
      break;
    if (!DuplicateHandle(ThisProc, ThisProc, hProcess, &ldr.injectee.parent_proc, SYNCHRONIZE,
                         FALSE, 0))
      break;
    ctrl->remote_data =
        VirtualAllocEx(hProcess, NULL, total_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (ctrl->remote_data == NULL) break;
    InjecteeLdrCtx *remote_ldr = (InjecteeLdrCtx *)ctrl->remote_data;

    // fetch pre-init func ptr
    HMODULE this_mod = GetModuleHandle(NULL);
    if (this_mod == NULL) break;
    HMODULE hKernel32 = GetModuleHandle(TEXT("KERNEL32"));
    if (hKernel32 == NULL) break;
    ldr.hKernel32 = LocateRemoteModule(pid, hKernel32);
    if (ldr.hKernel32 == NULL) break;

    GetModuleFileName(this_mod, ldr.path, MAX_PATH);
    if (!WriteProcessMemory(hProcess, remote_ldr, &ldr, sizeof ldr, &dwOut)) break;
    // ==== remote thread phrase 1 ====
    // load the pe file with LoadLibraryW
    LPTHREAD_START_ROUTINE remote_load =
        (LPTHREAD_START_ROUTINE)PtrOffset(LoadLibrary, hKernel32, ldr.hKernel32);
    thread_ldr =
        CreateRemoteThread(hProcess, NULL, 0, remote_load, &remote_ldr->path, 0, &thread_id);
    if (thread_ldr == NULL) break;
    HANDLE handles1[] = {thread_ldr, cancel};
    if (WaitForMultipleObjects(_countof(handles1) - (cancel ? 0 : 1), handles1, FALSE, timeout) !=
        WAIT_OBJECT_0)
      break;

    // resolve essential functions for remote loader
    ldr.GetProcAddr_F = (GetProcAddress_T)PtrOffset(GetProcAddress, hKernel32, ldr.hKernel32);
    ldr.FreeLibraryAndExitThread_F =
        (FreeLibraryAndExitThread_T)PtrOffset(FreeLibraryAndExitThread, hKernel32, ldr.hKernel32);
    ldr.FreeLibrary_F = (FreeLibrary_T)PtrOffset(FreeLibrary, hKernel32, ldr.hKernel32);
    ldr.LoadLibraryA_F = (LoadLibraryA_T)PtrOffset(LoadLibraryA, hKernel32, ldr.hKernel32);
    ldr.VirtualProtect_F = (VirtualProtect_T)PtrOffset(VirtualProtect, hKernel32, ldr.hKernel32);
    // and the remote loader itself
    ldr.this_mod = LocateRemoteModule(pid, this_mod);
    if (ldr.this_mod == NULL) break;
    LPTHREAD_START_ROUTINE remote_init =
        (LPTHREAD_START_ROUTINE)PtrOffset(InjecteeLdrEntry, this_mod, ldr.this_mod);

    // prepare user data
    void *remote_userdata = NULL;
    if (data) {
      char *base = (char *)remote_ldr;
      base += sizeof *remote_ldr;
      base += module_count * sizeof HMODULE;
      remote_userdata = base;
    }
    ldr.injectee.data = remote_userdata;
    ldr.injectee.len = len;
    ldr.n_modules = module_count;

    // write to remote process
    if (!WriteProcessMemory(hProcess, remote_ldr, &ldr, sizeof ldr, &dwOut)) break;
    if (len != 0 && !WriteProcessMemory(hProcess, remote_userdata, data, len, &dwOut)) break;
    // ==== remote thread phrase 2 ====
    // resolve import, call entry point and exit, for the whole life circle
    ctrl->remote_thread =
        CreateRemoteThread(hProcess, NULL, 0, remote_init, remote_ldr, 0, &thread_id);
    if (ctrl->remote_thread == NULL) break;

    HANDLE handles2[] = {event_init, ctrl->remote_thread, cancel};
    if (WaitForMultipleObjects(_countof(handles2) - (cancel ? 0 : 1), handles2, FALSE, timeout) !=
        WAIT_OBJECT_0)
      break;
    succ = TRUE;
  } while (0);

  if (!succ) {
    CloseHandle(ctrl->event_term);
    CloseHandle(ctrl->remote_thread);
    CloseHandle(ctrl->remote_proc);
  }
  CloseHandle(event_init);
  CloseHandle(thread_ldr);
  return succ ? ERROR_SUCCESS : -1;
}

InjecteeCtx *InjecteeGetCtx() { return g_injectee_ldr != NULL ? &g_injectee_ldr->injectee : NULL; }

DWORD InjecteeExit() {
  InjecteeLdrCtx *rs = g_injectee_ldr;
  if (rs != NULL) {
    g_injectee_ldr = NULL;
    HMODULE *modules = (HMODULE *)(&rs[1]);
    for (int i = 0; i != rs->n_modules; i++) FreeLibrary(modules[i]);
    CloseHandle(rs->injectee.event_term);
    CloseHandle(rs->injectee.parent_proc);
    CloseHandle(rs->event_init);
    FreeLibraryAndExitThread(rs->this_mod, ERROR_SUCCESS);
  }
  return 0;
}
