#define _CRT_SECURE_NO_WARNINGS
#include "render_d3d9.h"

#include <stdio.h>
#include <cmath>
#include "remote_app.h"

#pragma comment(lib, "d3d9.lib")

RenderD3D9::RenderD3D9() : m_hWnd(NULL), m_bDeviceLost(FALSE), m_pD3D(NULL), m_pD3DDev(NULL) {
  ZeroMemory(&m_D3Dpp, sizeof m_D3Dpp);
  m_D3Dpp.Windowed = TRUE;
  m_D3Dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
  m_D3Dpp.BackBufferFormat = D3DFMT_UNKNOWN;
  m_D3Dpp.EnableAutoDepthStencil = TRUE;
  m_D3Dpp.AutoDepthStencilFormat = D3DFMT_D16;
  if (RemoteApp::GetConfig() != nullptr)
    m_D3Dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
}

LARGE_INTEGER g_ts_last_present;
FILE *g_fp;

int RenderD3D9::Render(th::GameState *s) {
  int succ = 0;
  HRESULT hr = S_OK;
  if (s == nullptr) return 1;

  do {
    LARGE_INTEGER ts_start;
    QueryPerformanceCounter(&ts_start);
    if (FAILED(PreRenderSetup())) break;
    if (FAILED(CreateDeviceResources())) break;

    hr = m_pD3DDev->BeginScene();

    hr = m_pD3DDev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    hr = m_pD3DDev->SetRenderState(D3DRS_LIGHTING, FALSE);
    hr = m_pD3DDev->SetRenderState(D3DRS_ZENABLE, TRUE);
    // hr = m_pD3DDev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    hr = m_pD3DDev->SetRenderState(D3DRS_SRCBLENDALPHA, D3DRS_DESTBLENDALPHA);
    hr = m_pD3DDev->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_XRGB(255, 255, 255),
                          1.0f, 0);

    D3DVIEWPORT9 vp;
    hr = m_pD3DDev->GetViewport(&vp);
    if (FAILED(hr)) break;
    RunConfig *cfg = RemoteApp::GetConfig();
    if (cfg->render_scale_10x) {
      scale = 1.0f;
    } else if (cfg->render_scale_15x) {
      scale = 1.5f;
    } else if (cfg->render_scale_20x) {
      scale = 2.0f;
    } else {
      const float white = 1.05f;
      float scale_x = 1.0f;
      float scale_y = 1.0f;
      if (vp.Width > (s->config.right - s->config.left) * (2.0f * white))
        scale_x = 2.0f;
      else if (vp.Width > (s->config.right - s->config.left) * (1.5f * white))
        scale_x = 1.5f;
      if (vp.Height > (s->config.bottom - s->config.top) * (2.0f * white))
        scale_y = 2.0f;
      else if (vp.Height > (s->config.bottom - s->config.top) * (1.5f * white))
        scale_y = 1.5f;
      scale = min(scale_x, scale_y);
    }
    offset.x = std::floorf((vp.Width - (s->config.right + s->config.left) * scale) / 2);
    offset.y = std::floorf((vp.Height - (s->config.bottom + s->config.top) * scale) / 2);

    DrawPlayer(s);
    if (s->config.circle_bullet) {
      for (size_t i = 0; i != s->bullet.size(); i++) DrawCircle(&s->bullet[i], 0);
    } else {
      for (size_t i = 0; i != s->bullet.size(); i++) DrawBox(&s->bullet[i], 0);
    }
    if (s->config.circle_enemy) {
      for (size_t i = 0; i != s->enemy.size(); i++) DrawCircle(&s->enemy[i], 0);
    } else {
      for (size_t i = 0; i != s->enemy.size(); i++) DrawBox(&s->enemy[i], 0);
    }
    for (size_t i = 0; i != s->laser_line.size(); i++) {
      DrawLaserLine(&s->laser_line[i], 0);
    }
    for (size_t i = 0; i != s->laser_curve.size(); i++) {
      DrawLaserCurve(s, i, 0);
    }
    DrawBorder(s, 0);
    DrawQueueReserve(0, 0);

    hr = m_pD3DDev->EndScene();
    LARGE_INTEGER ts_render;
    QueryPerformanceCounter(&ts_render);
    hr = m_pD3DDev->Present(NULL, NULL, NULL, NULL);
    LARGE_INTEGER ts_present;
    QueryPerformanceCounter(&ts_present);
    if (g_fp == NULL) {
      // g_fp = fopen("D:\\fps_inner.txt", "w");
    }
    if (0) {
      LARGE_INTEGER ms, freq;
      QueryPerformanceFrequency(&freq);
      ms.QuadPart = ts_render.QuadPart - ts_start.QuadPart;
      ms.QuadPart *= 1000000;
      ms.QuadPart /= freq.QuadPart;
      fprintf(g_fp, "%lld,", ms.QuadPart);
      ms.QuadPart = ts_present.QuadPart - ts_render.QuadPart;
      ms.QuadPart *= 1000000;
      ms.QuadPart /= freq.QuadPart;
      fprintf(g_fp, "%lld,", ms.QuadPart);
      ms.QuadPart = ts_present.QuadPart - g_ts_last_present.QuadPart;
      ms.QuadPart *= 1000000;
      ms.QuadPart /= freq.QuadPart;
      fprintf(g_fp, "%lld\n", g_ts_last_present.QuadPart ? ms.QuadPart : 0);
      g_ts_last_present.QuadPart = ts_present.QuadPart;
    }
    if (hr == D3DERR_DEVICELOST) {
      hr = S_OK;
      DiscardDeviceResources();
    }
    succ = 1;
  } while (0);
  if (!succ) {
  }
  return 0;
}

int RenderD3D9::ExitRequest() {
  // DestroyWindow(m_hWnd);
  // fclose(g_fp);
  g_fp = NULL;
  if (m_hWnd != NULL)
    PostMessage(m_hWnd, WM_CLOSE, 0, 0);
  else if (m_hEvent != NULL)
    SetEvent(m_hEvent);
  return 0;
}

HRESULT RenderD3D9::PreRenderSetup() {
  int succ = 0;
  do {
    if (FAILED(CreateDeviceIndependentResources())) break;
    if (!RegisterWindowClass()) break;
    if (m_hWnd == NULL) {
      RECT rect = {0};
      RunConfig *cfg = RemoteApp::GetConfig();
      if (cfg->render_scale_15x) {
        rect.right = 720;  // 960
        rect.bottom = 720;
      } else if (cfg->render_scale_20x) {
        rect.right = 960;  // 1280
        rect.bottom = 960;
      } else {
        rect.right = 640;
        rect.bottom = 640;  // 480
      }
      AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
      m_hWnd = CreateWindow(GetWindowClassName(), TEXT("D3D9 render"), WS_OVERLAPPEDWINDOW,
                            CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left,
                            rect.bottom - rect.top, NULL, NULL, s_hInst, this);
      if (m_hWnd == NULL) break;
      ShowWindow(m_hWnd, SW_SHOWNORMAL);
      UpdateWindow(m_hWnd);
    }
    if (m_pD3DDev == NULL) {
      if (FAILED(m_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, m_hWnd,
                                      D3DCREATE_HARDWARE_VERTEXPROCESSING, &m_D3Dpp, &m_pD3DDev)))
        break;
    }
    succ = 1;
  } while (0);
  return succ ? S_OK : E_FAIL;
}

HRESULT RenderD3D9::CreateDeviceIndependentResources() {
  if (m_pD3D == NULL) {
    m_pD3D = Direct3DCreate9(D3D_SDK_VERSION);
  }
  if (m_pD3D)
    return S_OK;
  else
    return E_FAIL;
}

HRESULT RenderD3D9::CreateDeviceResources() {
  HRESULT hr = S_OK;
  if (m_bDeviceLost) {
    if (FAILED(hr = m_pD3DDev->TestCooperativeLevel())) {
      if (hr == D3DERR_DEVICELOST) {
        // return repaint and exit
        // InvalidateRect(m_hWnd, NULL, TRUE);
        return hr;
      }
      if (hr == D3DERR_DEVICENOTRESET) {
        // OnLostDevice();
        hr = m_pD3DDev->Reset(&m_D3Dpp);
        if (FAILED(hr)) {
          // reset failed, try again later
          // InvalidateRect(m_hWnd, NULL, TRUE);
          return hr;
        }
        // OnResetDevice();
      }
    }
    m_bDeviceLost = FALSE;
  }
  // if (SUCCEEDED(hr) && m_pVB == NULL) {
  //   hr = m_pD3DDev->CreateVertexBuffer(kMaxBox * 8 * sizeof(CUSTOMVERTEX),
  //     D3DUSAGE_WRITEONLY | D3DUSAGE_DYNAMIC, D3DFVF_CUSTOMVERTEX,
  //     D3DPOOL_DEFAULT, &m_pVB, NULL);
  // }
  return S_OK;
}

void RenderD3D9::DiscardDeviceResources() {
  m_bDeviceLost = TRUE;
  // SafeRelease(&m_pVB);
}

void RenderD3D9::ReleaseResource() {
  SafeRelease(&m_pD3DDev);
  SafeRelease(&m_pD3D);
}

void RenderD3D9::DrawQueueReserve(int vec, int idx) {
  bool do_flush = false;
  if (vec <= 0 && idx <= 0) do_flush = true;
  if (buf_vec.size() + vec >= buf_vec.capacity() || buf_idx.size() + idx >= buf_idx.capacity())
    do_flush = true;

  if (do_flush) {
    if (1) {
      // check
      for (size_t i = 0; i < buf_idx.size(); i += 2) {
        CUSTOMVERTEX p1 = buf_vec[buf_idx[i]];
        CUSTOMVERTEX p2 = buf_vec[buf_idx[i + 1]];
        if (p1.x != p2.x && p1.y != p2.y && std::fabs(p1.x - p2.x) > 10.0f &&
            std::fabs(p1.y - p2.y) > 10.0f) {
          int trigger_me = 0;
        }
      }
    }
    HRESULT hr;
    hr = m_pD3DDev->SetFVF(D3DFVF_CUSTOMVERTEX);
    hr = m_pD3DDev->DrawIndexedPrimitiveUP(D3DPT_LINELIST, 0, buf_vec.size(), buf_idx.size() / 2,
                                           buf_idx.data(), D3DFMT_INDEX16, buf_vec.data(),
                                           sizeof buf_vec[0]);
    buf_vec.clear();
    buf_idx.clear();
  }
  if (buf_vec.size() + vec >= buf_vec.capacity() || buf_idx.size() + idx >= buf_idx.capacity()) {
    int pinch = 1;
  }
}

void RenderD3D9::DrawPlayer(th::GameState *s) {
  DrawQueueReserve(8, 12);
  short idx = static_cast<short>(buf_vec.size());
  CUSTOMVERTEX *vbuf = buf_vec.alloc(4);
  short *ibuf = buf_idx.alloc(4);
  vbuf[0].x = offset.x + s->config.left * scale - 1.0f;
  vbuf[0].y = offset.y + s->player.pos.y * scale;
  vbuf[1].x = offset.x + s->config.right * scale;
  vbuf[1].y = vbuf[0].y;

  vbuf[2].x = offset.x + s->player.pos.x * scale;
  vbuf[2].y = offset.y + s->config.top * scale - 1.0f;
  vbuf[3].x = vbuf[2].x;
  vbuf[3].y = offset.y + s->config.bottom * scale;

  for (int i = 0; i < 4; i++) {
    vbuf[i].z = 0.0f;
    vbuf[i].w = 0.0f;
    vbuf[i].color = 0xcccccccc;
    ibuf[i] = idx + i;
  }

  DrawBox(&s->player, 0);
}

void RenderD3D9::DrawBorder(th::GameState *s, DWORD color) {
  DrawQueueReserve(4, 8);
  short idx = static_cast<short>(buf_vec.size());
  CUSTOMVERTEX *vbuf = buf_vec.alloc(4);
  short *ibuf = buf_idx.alloc(8);

  // 0 1
  // 3 2
  vbuf[0].x = offset.x + s->config.left * scale - 1.0f;
  vbuf[0].y = offset.y + s->config.top * scale - 1.0f;
  vbuf[1].x = offset.x + s->config.right * scale;
  vbuf[1].y = vbuf[0].y;
  vbuf[2].x = vbuf[1].x;
  vbuf[2].y = offset.y + s->config.bottom * scale;
  vbuf[3].x = vbuf[0].x;
  vbuf[3].y = vbuf[2].y;

  for (int i = 0; i < 4; i++) {
    vbuf[i].z = vbuf[i].w = 0.0f;
    vbuf[i].color = color;
    ibuf[i * 2] = i + idx;
    ibuf[i * 2 + 1] = (i + 1) % 4 + idx;
  }
}

void RenderD3D9::DrawBox(th::HitBox *box, DWORD color) {
  DrawQueueReserve(4, 8);
  short idx = static_cast<short>(buf_vec.size());
  CUSTOMVERTEX *vbuf = buf_vec.alloc(4);
  short *ibuf = buf_idx.alloc(8);

  // 0 1
  // 3 2
  vbuf[0].x = std::roundf(offset.x + (box->pos.x - box->size.x / 2) * scale);
  vbuf[0].y = std::roundf(offset.y + (box->pos.y - box->size.y / 2) * scale);
  vbuf[1].x = vbuf[0].x + box->size.x * scale;
  vbuf[1].y = vbuf[0].y;
  vbuf[2].x = vbuf[1].x;
  vbuf[2].y = vbuf[1].y + box->size.y * scale;
  vbuf[3].x = vbuf[0].x;
  vbuf[3].y = vbuf[2].y;

  for (int i = 0; i < 4; i++) {
    vbuf[i].z = vbuf[i].w = 0.0f;
    vbuf[i].color = color;
    ibuf[i * 2] = i + idx;
    ibuf[i * 2 + 1] = (i + 1) % 4 + idx;
  }
}

void RenderD3D9::DrawCircle(th::HitBox *box, DWORD color) {
  // return;
  float rad = box->size.x / 2 * scale;
  if (rad < 5.0f) return DrawBox(box, color);

  int n_vec;
  if (rad < 25.0f)
    n_vec = 4 * 3;
  else
    n_vec = (int)rad / 8 * 4;
  n_vec = 4 * 6;
  DrawQueueReserve(n_vec, n_vec * 2);
  short idx = (short)buf_vec.size();
  CUSTOMVERTEX *vbuf = buf_vec.alloc(n_vec);
  short *ibuf = buf_idx.alloc(n_vec * 2);
  th::Point2f center = offset + box->pos * scale;
  for (int i = 0; i < n_vec; i++) {
    th::Point2f p = center + th::Point2f::rotate(rad, 3.14159265358979323846f * 2 / n_vec * i);
    vbuf[i].x = p.x;
    vbuf[i].y = p.y;
    vbuf[i].z = 0.0f;
    vbuf[i].w = 0.0f;
    vbuf[i].color = color;
  }
  short *iout = ibuf;
  *iout++ = idx;
  for (int i = 1; i < n_vec; i++) {
    *iout++ = idx + i;
    *iout++ = idx + i;
  }
  *iout++ = idx;
}

void RenderD3D9::DrawLaserLine(th::RotateBox *box, DWORD color) {
  DrawQueueReserve(4, 8);
  short idx = static_cast<short>(buf_vec.size());
  CUSTOMVERTEX *vbuf = buf_vec.alloc(4);
  short *ibuf = buf_idx.alloc(8);

  th::Point2f v_mid =
      offset + box->origin * scale +
      th::Point2f::rotate((box->len_start + box->len_end) * (scale / 2), box->angle);
  th::Point2f v1 = th::Point2f::rotate((box->len_end - box->len_start) * (scale / 2), box->angle);
  th::Point2f v2;
  v2.x = (box->width * (scale / 2)) * std::sin(box->angle) * -1;
  v2.y = (box->width * (scale / 2)) * std::cos(box->angle);
  th::Point2f pt[4];
  pt[0] = v_mid + v1 + v2;
  pt[1] = v_mid + v1 - v2;
  pt[2] = v_mid - v1 - v2;
  pt[3] = v_mid - v1 + v2;
  for (int i = 0; i < 4; i++) {
    vbuf[i].x = pt[i].x;
    vbuf[i].y = pt[i].y;
    vbuf[i].z = vbuf[i].w = 0.0f;
    vbuf[i].color = color;
    ibuf[i * 2] = i + idx;
    ibuf[i * 2 + 1] = (i + 1) % 4 + idx;
  }
}

void RenderD3D9::DrawLaserCurve(th::GameState *s, int idx, DWORD color) {
  th::LaserCurve *curve = &s->laser_curve[idx];
  int n_seg = s->laser_curve[idx].segments;
  if (n_seg == 0) return;
  DrawQueueReserve(n_seg * 2 + 2, n_seg * 4 + 4);
  short v_idx = static_cast<short>(buf_vec.size());
  CUSTOMVERTEX *vbuf = buf_vec.alloc(n_seg * 2 + 2);
  short *ibuf = buf_idx.alloc(n_seg * 4 + 4);
  // CUSTOMVERTEX vbuf[512];
  // short ibuf[1024];

  for (int i = 0; i < n_seg; i++) {
    th::LaserCurveSeg *seg = &s->laser_curve_seg[curve->idx_start + i];
    th::Point2f vhalf;
    vhalf.x = std::sinf(seg->angle) * (curve->width * scale / 2) * -1;
    vhalf.y = std::cosf(seg->angle) * (curve->width * scale / 2);
    th::Point2f p1 = seg->pos * scale + offset + vhalf;
    th::Point2f p2 = seg->pos * scale + offset - vhalf;
    vbuf[i].x = p1.x;
    vbuf[i].y = p1.y;
    vbuf[2 * n_seg + 1 - i].x = p2.x;
    vbuf[2 * n_seg + 1 - i].y = p2.y;
    if (i == n_seg - 1) {
      // the ending
      th::Point2f hhalf = th::Point2f::rotate(seg->len * scale, seg->angle);
      p1 = p1 + hhalf;
      p2 = p2 + hhalf;
      vbuf[i + 1].x = p1.x;
      vbuf[i + 1].y = p1.y;
      vbuf[i + 2].x = p2.x;
      vbuf[i + 2].y = p2.y;
    }
  }
  for (int i = 0; i < n_seg * 2 + 2; i++) {
    vbuf[i].z = 0.0f;
    vbuf[i].color = color;
  }
  ibuf[0] = v_idx;
  for (int i = 1; i < n_seg * 2 + 2; i++) {
    ibuf[i * 2 - 1] = i + v_idx;
    ibuf[i * 2] = i + v_idx;
  }
  ibuf[n_seg * 4 + 3] = v_idx;
}

LRESULT RenderD3D9::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  bool wasHandled = false;
  LRESULT result = 0;

  switch (uMsg) {
    case WM_CREATE:
      result = 1;
      wasHandled = true;
      break;
    case WM_SIZE: {
      UINT width = LOWORD(lParam);
      UINT height = HIWORD(lParam);
      if (m_pD3DDev) {
        ZeroMemory(&m_D3Dpp, sizeof m_D3Dpp);
        m_D3Dpp.Windowed = TRUE;
        m_D3Dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
        m_D3Dpp.BackBufferFormat = D3DFMT_UNKNOWN;
        m_D3Dpp.EnableAutoDepthStencil = TRUE;
        m_D3Dpp.AutoDepthStencilFormat = D3DFMT_D16;
        HRESULT hr = m_pD3DDev->Reset(&m_D3Dpp);
        int break_here = 0;
      }
      result = 0;
      wasHandled = true;
      break;
    }
    case WM_DESTROY: {
      if (m_hEvent == NULL) PostQuitMessage(0);
      result = 1;
      wasHandled = false;
      break;
    }
    case WM_NCDESTROY: {
      // m_hWnd = NULL;
      if (m_hEvent != NULL) SetEvent(m_hEvent);
      ReleaseResource();
      break;
    }
  }
  if (!wasHandled) result = DefWindowProc(hWnd, uMsg, wParam, lParam);
  return result;
}
