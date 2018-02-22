#ifndef RENDER_D3D9_H
#define RENDER_D3D9_H

#include <d3d9.h>
#include "render_base.h"

class RenderD3D9 : public RenderBase {
 public:
  RenderD3D9();
  int Render(th::GameState *s);
  int ExitRequest();

  static int getId() { return 1; }

 protected:
  typedef struct CUSTOMVERTEX {
    FLOAT x, y, z, w;
    DWORD color;
  } CUSTOMVERTEX;
  static const DWORD D3DFVF_CUSTOMVERTEX = (D3DFVF_XYZRHW | D3DFVF_DIFFUSE);

  th::Point2f offset;
  float scale;

  varray<CUSTOMVERTEX, 3072 * 4> buf_vec;
  varray<short, 3072 * 8> buf_idx;

  HWND m_hWnd;
  BOOL m_bDeviceLost;
  D3DPRESENT_PARAMETERS m_D3Dpp;
  LPDIRECT3D9 m_pD3D;
  LPDIRECT3DDEVICE9 m_pD3DDev;

  HRESULT PreRenderSetup();

  // Initialize device-independent resources.
  HRESULT CreateDeviceIndependentResources();

  // Initialize device-dependent resources.
  HRESULT CreateDeviceResources();

  // Release device-dependent resource.
  void DiscardDeviceResources();

  void ReleaseResource();

  void DrawQueueReserve(int vec, int idx);

  void DrawPlayer(th::GameState *s);

  void DrawBorder(th::GameState *s, DWORD color);

  void DrawBox(th::HitBox *box, DWORD color);

  void DrawCircle(th::HitBox *box, DWORD color);

  void DrawLaserLine(th::RotateBox *box, DWORD color);

  void DrawLaserCurve(th::GameState *s, int idx, DWORD color);

  LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
};

#endif
