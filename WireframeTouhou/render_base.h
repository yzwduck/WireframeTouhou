#ifndef RENDER_BASE_H
#define RENDER_BASE_H

#include <Windows.h>
#include "game_state.h"

template <class Interface>
inline void SafeRelease(Interface **ppInterfaceToRelease) {
  if (*ppInterfaceToRelease != NULL) {
    (*ppInterfaceToRelease)->Release();
    (*ppInterfaceToRelease) = NULL;
  }
}

class RenderBase {
 public:
  virtual ~RenderBase() = default;
  virtual int Render(th::GameState *s) = 0;
  virtual int ExitRequest() = 0;
  static void gInit(HINSTANCE hInst) { s_hInst = hInst; }
  static RenderBase *BuildRender(int id);
  HANDLE RegEvent(HANDLE hEvent);

 protected:
  static HINSTANCE s_hInst;
  HANDLE m_hEvent = NULL;
  static BOOL m_bWindowClassReg;
  static BOOL RegisterWindowClass();
  static CONST TCHAR *GetWindowClassName();
  static LRESULT CALLBACK BaseWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
  virtual LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) = 0;
};

#endif
