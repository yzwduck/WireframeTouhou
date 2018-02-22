#ifndef STATE_READER_H
#define STATE_READER_H

#include <Windows.h>

#include "game_state.h"

namespace th {

class RebaseUtil {
 public:
  RebaseUtil() : delta(0) {}

  RebaseUtil(DWORD_PTR delta) : delta(delta) {}

  void RebaseSet(DWORD_PTR delta) { this->delta = delta; }

  DWORD_PTR RebaseGet() const { return this->delta; }

  LPCVOID Rebase(LPCVOID addr) const { return (LPCVOID)((DWORD_PTR)addr + delta); }

  static BOOL ReadProcMem(HANDLE hProc, LPCVOID lpBaseAddress, LPVOID lpBuffer, SIZE_T nSize) {
    SIZE_T dwOut;
    BOOL ret = ReadProcessMemory(hProc, lpBaseAddress, lpBuffer, nSize, &dwOut);
    return ret && dwOut == nSize;
  }

  BOOL ReadProcMemRe(HANDLE hProc, LPCVOID lpBaseAddress, LPVOID lpBuffer, SIZE_T nSize) const {
    LPCVOID target = Rebase(lpBaseAddress);
    return ReadProcMem(hProc, target, lpBuffer, nSize);
  }

 private:
  /**
   * \brief rebase delta = new_base - old_base
   */
  DWORD_PTR delta;
};

class StateReader : public RebaseUtil {
 public:
  StateReader();

  virtual ~StateReader() = default;

  virtual int GetId() const = 0;

  virtual int Check() const = 0;

  int Init(HANDLE hProc);

  int Init(const StateReader &other);

  virtual int Read(GameState *state) const = 0;

  virtual StateReader *Clone() const = 0;

  virtual int Hook() const = 0;

  virtual int Unhook() const = 0;

  virtual DWORD_PTR GetImageBase() const = 0;

  static StateReader *BuildReader(int id);

  static StateReader *instances[];

  /**
   * \brief Ask hooked function to perform unhook by itself
   */
  static int unhook_req_;

  /**
   * \brief Event set after unhook is done
   */
  static HANDLE unhooked_event_;

 protected:
  HANDLE hProc;
};
}

#endif
