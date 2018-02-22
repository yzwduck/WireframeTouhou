#include <Windows.h>
#include <Windowsx.h>
#include <CommCtrl.h>
#include <TlHelp32.h>
#include <Uxtheme.h>
#include <algorithm>
#include <cctype>
#include <vector>

#include "inject.h"
#include "render_d3d9.h"
#include "remote_wireframe.h"
#include "state_reader.h"
#include "th08.h"
#include "th10.h"
#include "th11.h"
#include "th12.h"
#include "th13.h"
#include "th14.h"
#include "th15.h"
#include "th16.h"

#include "res/resource.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "Uxtheme.lib")

class DemoApp {
 public:
  DemoApp(HINSTANCE hInst);
  ~DemoApp();

  HRESULT Initialize();

  void Run();

 private:
  const TCHAR *GameTitleString(int id) const;
  static INT_PTR CALLBACK DialogProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
  const TCHAR *ResLoadString(UINT id) const;
  INT_PTR DlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
  INT_PTR OnInitDialog(HWND hWnd);
  INT_PTR OnTimer(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
  INT_PTR OnActivate(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
  INT_PTR OnProcessListNotify(HWND hWnd, LPARAM lParam);
  void RefreshProcessList(HWND hWnd);
  INT_PTR OnOk(HWND hWnd);

  HINSTANCE m_hInst;
  std::vector<PROCESSENTRY32> vec_proc;
  bool has_th_running = false;
  TCHAR m_buffer[8];
  int cur_game_idx;
};

BOOL WINAPI DllEntryPoint(HINSTANCE hInstDLL) {
  RenderBase::gInit(hInstDLL);
  RemoteApp *app = RemoteApp::GetApp(true);
  app->Init();
  app->Run();
  InjecteeExit();
  return TRUE;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
  HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, NULL, 0);
  if (InjecteeGetCtx() != NULL) return DllEntryPoint(hInstance);

  DWORD_PTR base = (DWORD_PTR)GetModuleHandle(NULL);
  IMAGE_NT_HEADERS *pe = (IMAGE_NT_HEADERS *)(base + ((PIMAGE_DOS_HEADER)base)->e_lfanew);

  if (SUCCEEDED(CoInitialize(NULL))) {
    DemoApp app(hInstance);
    if (SUCCEEDED(app.Initialize())) {
      app.Run();
    }
    CoUninitialize();
  }

  return 0;
}

DemoApp::DemoApp(HINSTANCE hInst) : m_hInst(hInst) {}

DemoApp::~DemoApp() {}

HRESULT DemoApp::Initialize() {
  InitCommonControls();
  return S_OK;
}

void DemoApp::Run() {
  DialogBoxParam(m_hInst, MAKEINTRESOURCE(IDD_LAUNCHER), NULL, &DialogProc, (LPARAM)this);
}

const TCHAR *DemoApp::GameTitleString(int id) const {
  switch (id) {
    case th::Th08Reader::kGameId:
      return ResLoadString(IDS_TITLE_TH08);
    case th::Th10Reader::kGameId:
      return ResLoadString(IDS_TITLE_TH10);
    case th::Th11Reader::kGameId:
      return ResLoadString(IDS_TITLE_TH11);
    case th::Th12Reader::kGameId:
      return ResLoadString(IDS_TITLE_TH12);
    case th::Th13Reader::kGameId:
      return ResLoadString(IDS_TITLE_TH13);
    case th::Th14Reader::kGameId:
      return ResLoadString(IDS_TITLE_TH14);
    case th::Th15Reader::kGameId:
      return ResLoadString(IDS_TITLE_TH15);
    case th::Th16Reader::kGameId:
      return ResLoadString(IDS_TITLE_TH16);
    default:
      return TEXT("");
  }
}

INT_PTR DemoApp::DialogProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  DemoApp *ctx = NULL;
  if (uMsg == WM_INITDIALOG) {
    ctx = (DemoApp *)lParam;
    SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)ctx);
  } else {
    ctx = (DemoApp *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
  }
  if (ctx == NULL) return FALSE;
  return ctx->DlgProc(hWnd, uMsg, wParam, lParam);
}

const TCHAR *DemoApp::ResLoadString(UINT id) const {
  const TCHAR *textptr = NULL;
  int res = LoadString(m_hInst, id, (TCHAR *)&textptr, 0);
  if (res == 0 || textptr == NULL) {
    textptr = TEXT("");  // failback
  }
  return textptr;
}

INT_PTR DemoApp::DlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  switch (uMsg) {
    case WM_INITDIALOG:
      return OnInitDialog(hWnd);
    case WM_NOTIFY:
      return OnProcessListNotify(hWnd, lParam);
    // case WM_TIMER:
    // return OnTimer(hWnd, uMsg, wParam, lParam);
    case WM_ACTIVATE:
      return OnActivate(hWnd, uMsg, wParam, lParam);
    case WM_COMMAND: {
      switch (LOWORD(wParam)) {
        case IDOK:
          return OnOk(hWnd);
        case IDCANCEL:
          EndDialog(hWnd, uMsg);
          return TRUE;
      }
    }
    default:
      return FALSE;
  }
}

INT_PTR DemoApp::OnInitDialog(HWND hWnd) {
  HWND combo = GetDlgItem(hWnd, IDC_COMBO_GAME);
  for (th::StateReader **iter = th::StateReader::instances; *iter != nullptr; iter++) {
    const TCHAR *title = GameTitleString((*iter)->GetId());
    ComboBox_AddString(combo, title);
  }

  HWND proc = GetDlgItem(hWnd, IDC_LIST_PROCESS);
  LVCOLUMN column;
  // DWORD dwStyle = GetWindowLong(hWnd, GWL_STYLE);
  // dwStyle |= LVS_SHOWSELALWAYS;
  // SetWindowLong(hWnd, GWL_STYLE, dwStyle);
  ListView_SetExtendedListViewStyle(proc, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
  column.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
  column.iSubItem = 0;
  column.pszText = const_cast<TCHAR *>(TEXT("PID"));
  column.cx = 45;
  column.fmt = LVCFMT_LEFT;
  ListView_InsertColumn(proc, 0, &column);
  column.iSubItem = 1;
  column.pszText = NULL;
  column.cx = 90;
  column.fmt = LVCFMT_LEFT;
  ListView_InsertColumn(proc, 1, &column);

  SetWindowTheme(proc, L"Explorer", NULL);
  Button_SetCheck(GetDlgItem(hWnd, IDC_RADIO_MODE_HOOK), BST_CHECKED);
  Button_SetCheck(GetDlgItem(hWnd, IDC_RADIO_SCALE_AUTO), BST_CHECKED);
  // SetTimer(hWnd, IDT_REFRESH_PROC, 3000, NULL);
  // RefreshProcessList(hWnd);
  return FALSE;
}

INT_PTR DemoApp::OnTimer(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  if (wParam == IDT_REFRESH_PROC) {
    RefreshProcessList(hWnd);
    return TRUE;
  }
  return FALSE;
}

INT_PTR DemoApp::OnActivate(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  if (wParam != WA_INACTIVE) {
    int a = 0;
    RefreshProcessList(hWnd);
  }
  return TRUE;
}

INT_PTR DemoApp::OnProcessListNotify(HWND hWnd, LPARAM lParam) {
  NMHDR *nmhdr = (NMHDR *)lParam;
  if (nmhdr->idFrom == IDC_LIST_PROCESS) {
    if (nmhdr->code == LVN_GETDISPINFO) {
      NMLVDISPINFO *plvdi = (NMLVDISPINFO *)lParam;
      size_t idx = plvdi->item.iItem;
      if (idx >= vec_proc.size()) return FALSE;
      switch (plvdi->item.iSubItem) {
        case 0:
          wsprintf(m_buffer, TEXT("%d"), vec_proc[idx].th32ProcessID);
          plvdi->item.pszText = m_buffer;
          break;
        case 1:
          plvdi->item.pszText = vec_proc[idx].szExeFile;
          break;
      }
      return TRUE;
    } else if (nmhdr->code == LVN_ITEMCHANGED) {
      NMLISTVIEW *lv = (NMLISTVIEW *)lParam;
      size_t idx = lv->iItem;
      if ((lv->uNewState & LVIS_SELECTED) && idx < vec_proc.size()) {
        HANDLE hProc = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION | SYNCHRONIZE, FALSE,
                                   vec_proc[idx].th32ProcessID);
        bool ok = false;
        HWND game = GetDlgItem(hWnd, IDC_COMBO_GAME);
        th::Th16Reader tmpl;
        tmpl.Init(hProc);
        for (th::StateReader **iter = th::StateReader::instances; *iter != nullptr && hProc && !ok;
             iter++) {
          th::StateReader *r = *iter;
          r->Init(tmpl);
          if (r->Check() == 0) {
            ok = true;
            cur_game_idx = iter - th::StateReader::instances;
            ComboBox_SetCurSel(game, cur_game_idx);
          }
        }
        if (!ok) {
          ComboBox_SetCurSel(game, -1);
          cur_game_idx = -1;
        }
      }
    }
  }
  return FALSE;
}

static bool ProcessEntryNameCmp(const PROCESSENTRY32 &a, const PROCESSENTRY32 &b) {
  return lstrcmpi(a.szExeFile, b.szExeFile) < 0;
}

void DemoApp::RefreshProcessList(HWND hWnd) {
  HWND proc = GetDlgItem(hWnd, IDC_LIST_PROCESS);
  bool last_has_th = has_th_running;
  DWORD last_pid = 0;
  int last_idx = ListView_GetNextItem(proc, -1, LVNI_SELECTED);
  if (last_idx >= 0 && (size_t)last_idx < vec_proc.size()) {
    last_pid = vec_proc[last_idx].th32ProcessID;
  }

  last_idx = -1;
  int th_idx = -1;
  has_th_running = false;
  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  PROCESSENTRY32 p;
  ZeroMemory(&p, sizeof p);
  p.dwSize = sizeof p;
  if (Process32First(snapshot, &p)) {
    vec_proc.clear();
    do {
      vec_proc.push_back(p);
    } while (Process32Next(snapshot, &p));
  }
  CloseHandle(snapshot);
  std::sort(vec_proc.begin(), vec_proc.end(), ProcessEntryNameCmp);
  ListView_SetItemCountEx(proc, vec_proc.size(), LVSICF_NOINVALIDATEALL);
  HWND game = GetDlgItem(hWnd, IDC_COMBO_GAME);
  ComboBox_SetCurSel(game, -1);

  for (auto iter = vec_proc.cbegin(); iter != vec_proc.cend(); ++iter) {
    if ((iter->szExeFile[0] == 't' || iter->szExeFile[0] == 'T') &&
        (iter->szExeFile[1] == 'h' || iter->szExeFile[1] == 'H') &&
        std::isdigit(iter->szExeFile[2])) {
      th_idx = iter - vec_proc.cbegin();
    }
    if (iter->th32ProcessID == last_pid && last_pid != 0) last_idx = iter - vec_proc.cbegin();
  }
  int sel = (th_idx != -1) ? th_idx : last_idx;
  ListView_SetItemState(proc, -1, LVIF_STATE, LVIS_SELECTED);
  if (sel != -1) {
    ListView_SetItemState(proc, sel, LVIS_FOCUSED | LVIS_SELECTED, 0x000f);
    ListView_EnsureVisible(proc, sel, FALSE);
  }
  ListView_RedrawItems(proc, 0, vec_proc.size() - 1);
}

static BOOL CALLBACK EnumSwitchWindowCb(HWND hWnd, LPARAM lParam) {
  DWORD pid = (DWORD)lParam;
  DWORD w_pid;
  if (GetWindowThreadProcessId(hWnd, &w_pid)) {
    if (w_pid == pid) {
      if (GetWindow(hWnd, GW_OWNER) == (HWND)0) {
        ShowWindow(hWnd, SW_SHOWDEFAULT);
        ShowWindow(hWnd, SW_RESTORE);
        SetForegroundWindow(hWnd);
        return FALSE;
      }
    }
  }
  return TRUE;
}

INT_PTR DemoApp::OnOk(HWND hWnd) {
  HWND proc = GetDlgItem(hWnd, IDC_LIST_PROCESS);
  HWND game = GetDlgItem(hWnd, IDC_COMBO_GAME);
  int proc_idx = ListView_GetNextItem(proc, -1, LVNI_SELECTED);
  if (0 <= proc_idx && (size_t)proc_idx < vec_proc.size()) {
    bool ok = false;
    int game_idx = ComboBox_GetCurSel(game);
    if (game_idx == cur_game_idx && game_idx != -1) {
      ok = true;
    }
    if (ok) {
      InjectRemoteControl ctrl;
      RunConfig c = {0};
      c.app = RemoteWireframe::getId();
      c.reader = th::Th11Reader::instances[game_idx]->GetId();
      c.render = RenderD3D9::getId();
      c.hook = 1;
      c.detach_parent = 1;
      if (Button_GetCheck(GetDlgItem(hWnd, IDC_RADIO_SCALE_10)) == BST_CHECKED)
        c.render_scale_10x = 1;
      else if (Button_GetCheck(GetDlgItem(hWnd, IDC_RADIO_SCALE_15)) == BST_CHECKED)
        c.render_scale_15x = 1;
      else if (Button_GetCheck(GetDlgItem(hWnd, IDC_RADIO_SCALE_20)) == BST_CHECKED)
        c.render_scale_20x = 1;
      DWORD ret =
          InjectSelfToProcess(vec_proc[proc_idx].th32ProcessID, &ctrl, &c, sizeof c, 1000, NULL);
      if (ret != ERROR_SUCCESS) {
        MessageBox(hWnd, ResLoadString(IDS_WARN_INJECT), NULL, MB_ICONERROR);
      } else {
        DWORD pid = vec_proc[proc_idx].th32ProcessID;
        EnumWindows(EnumSwitchWindowCb, (LPARAM)pid);
        // EndDialog(hWnd, IDOK);
      }
    }
    if (!ok && game_idx != -1) {
      MessageBox(hWnd, ResLoadString(IDS_WARN_MISMATCH), NULL, MB_ICONWARNING);
    }
  }
  return FALSE;
}

/*
notes for rendering
Remote DX: issue render at MsgLoop
Remote GDI: post WM_PAINT
Local DX at Present: present too
Local GDI: UpdateWindow()
*/
