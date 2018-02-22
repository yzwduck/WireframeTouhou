#ifndef INJECT_H
#define INJECT_H

#include <Windows.h>

// structure used on self
typedef struct {
  HANDLE event_term;     // signal from self to remote, ask remote to term
  HANDLE remote_thread;  // direct control over remote thread
  HANDLE remote_proc;
  void *remote_data;  // control data in remote process
} InjectRemoteControl;

// structure used on remote process
typedef struct {
  HANDLE event_term;   // receive term signal from self/parent
  HANDLE parent_proc;  // check whether self/parent is alive
  VOID *data;          // param from self/parent on initialization
  SIZE_T len;          // length of data
} InjecteeCtx;

/**
 * \brief Inject this program into remote process
 * \param pid remote process id
 * \param ctrl a structure to control remote process
 * \param data some context/parameters passing from self to remote
 * \param len length of data
 */
DWORD InjectSelfToProcess(DWORD pid, InjectRemoteControl *ctrl, VOID *data, SIZE_T len,
                          DWORD timeout, HANDLE cancel);

/**
 * \brief Check if running under remote process
 * If run as DLL, this will return non-NULL context.
 * Call this on WinMain/EntryPoint
 */
InjecteeCtx *InjecteeGetCtx();

/**
 * \brief Unload and exit
 */
DWORD InjecteeExit();

HANDLE RemoteModuleList(DWORD pid);

HMODULE RemoteModuleLocate(HANDLE snapshot, const TCHAR *path);

#endif  // !INJECT_H
