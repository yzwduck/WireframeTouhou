#include "render_base.h"
#include "render_d3d9.h"

HINSTANCE RenderBase::s_hInst;
BOOL RenderBase::m_bWindowClassReg;

RenderBase *RenderBase::BuildRender(int id) {
  if (id == RenderD3D9::getId()) return new RenderD3D9();
  return nullptr;
}

HANDLE RenderBase::RegEvent(HANDLE hEvent) {
  HANDLE old = m_hEvent;
  m_hEvent = hEvent;
  return old;
}

BOOL RenderBase::RegisterWindowClass() {
  if (m_bWindowClassReg) return TRUE;
  UnregisterClass(GetWindowClassName(), s_hInst);
  WNDCLASSEX wcex = {sizeof(WNDCLASSEX)};
  wcex.style = CS_HREDRAW | CS_VREDRAW;
  wcex.lpfnWndProc = BaseWndProc;
  wcex.cbClsExtra = 0;
  wcex.cbWndExtra = sizeof(LONG_PTR);
  wcex.hInstance = s_hInst;
  wcex.hbrBackground = NULL;
  wcex.lpszMenuName = NULL;
  wcex.hCursor = LoadCursor(NULL, IDI_APPLICATION);
  wcex.lpszClassName = GetWindowClassName();
  if (RegisterClassEx(&wcex)) {
    m_bWindowClassReg = TRUE;
    return TRUE;
  } else {
    return FALSE;
  }
}

CONST TCHAR *RenderBase::GetWindowClassName() { return TEXT("Wireframe Touhou"); }

LRESULT RenderBase::BaseWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  RenderBase *Render = nullptr;
  if (uMsg == WM_CREATE) {
    LPCREATESTRUCT pcs = reinterpret_cast<LPCREATESTRUCT>(lParam);
    Render = static_cast<RenderBase *>(pcs->lpCreateParams);
    SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(Render));
  } else {
    Render = reinterpret_cast<RenderBase *>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
  }

  if (Render != nullptr)
    return Render->WndProc(hWnd, uMsg, wParam, lParam);
  else
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}
