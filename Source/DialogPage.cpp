#include "DialogPage.h"
#include <map>

// Improvise. Adapt. Overcome.
static std::map<HWND, DialogPage*> pageByHwnd;
static BOOL CALLBACK RoutingDialogProc(HWND hWndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  if (uMsg == DialogPage::WM_USERINIT) {
    pageByHwnd[hWndDlg] = reinterpret_cast<DialogPage*>(lParam);
  }
  auto pageByHwndIter = pageByHwnd.find(hWndDlg);
  if (pageByHwndIter != pageByHwnd.end()) {
    return pageByHwndIter->second->DialogProc(hWndDlg, uMsg, wParam, lParam);
  }
  return FALSE;
}

DialogPage::DialogPage(HINSTANCE hInstance, HWND hWndParent, uint32 resourceId)
  : resourceId(resourceId) {
  hWnd = CreateDialog(hInstance, MAKEINTRESOURCE(resourceId), hWndParent, RoutingDialogProc);
}

