#pragma once

#include "BaseTypes.h"
#include "BaseWindows.h"
#include "SerializeFwd.h"

class DialogPage {
public:
  static constexpr uint32 WM_USERINIT = (WM_APP + 1);
protected:
  HWND hWnd = 0;
  uint32 resourceId = 0;
public:
  virtual bool DialogProc(HWND hWndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) = 0;

  DialogPage(HINSTANCE hInstance, HWND hWndParent, uint32 resourceId);

  inline HWND GetHandle() const {
    return hWnd;
  }

  virtual bool SerializeWrite(const WriteSerializer& serializer) = 0;
  virtual bool SerializeRead(const ReadSerializer& serializer) = 0;
};
