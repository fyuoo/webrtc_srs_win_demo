/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "main_wnd.h"

#include <math.h>
#include <winnt.h>
//#include "rtc_base/arraysize.h"
//#include "rtc_base/checks.h"
//#include "rtc_base/logging.h"

ATOM MainWnd::wnd_class_ = 0;
const wchar_t MainWnd::kClassName[] = L"WebRTC_MainWnd";

namespace {

const char kConnecting[] = "Connecting... ";
const char kNoVideoStreams[] = "(no video streams either way)";
const char kNoIncomingStream[] = "(no incoming video)";

void CalculateWindowSizeForText(HWND wnd, const wchar_t* text, size_t* width,
                                size_t* height) {
  HDC dc = ::GetDC(wnd);
  RECT text_rc = {0};
  ::DrawTextW(dc, text, -1, &text_rc, DT_CALCRECT | DT_SINGLELINE);
  ::ReleaseDC(wnd, dc);
  RECT client, window;
  ::GetClientRect(wnd, &client);
  ::GetWindowRect(wnd, &window);

  *width = text_rc.right - text_rc.left;
  *width += (window.right - window.left) - (client.right - client.left);
  *height = text_rc.bottom - text_rc.top;
  *height += (window.bottom - window.top) - (client.bottom - client.top);
}

HFONT GetDefaultFont() {
  static HFONT font = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
  return font;
}

std::string GetWindowText(HWND wnd) {
  char text[MAX_PATH] = {0};
  ::GetWindowTextA(wnd, &text[0], ARRAYSIZE(text));
  return text;
}

}  // namespace

MainWnd::MainWnd() : wnd_(NULL) {}

MainWnd::~MainWnd() {}

bool MainWnd::Create() {
  // RTC_DCHECK(wnd_ == NULL);
  if (!RegisterWindowClass()) return false;
  wnd_ =
      ::CreateWindowExW(WS_EX_OVERLAPPEDWINDOW, kClassName, L"WebRTC",
                        WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN,
                        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                        CW_USEDEFAULT, NULL, NULL, GetModuleHandle(NULL), this);

  ::SendMessage(wnd_, WM_SETFONT, reinterpret_cast<WPARAM>(GetDefaultFont()),
                TRUE);

  return wnd_ != NULL;
}

bool MainWnd::Destroy() {
  bool ret = ::DestroyWindow(wnd_);

  return ret != FALSE;
}

LRESULT CALLBACK MainWnd::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  switch (msg) {
    case WM_COMMAND: {
      int wmId = LOWORD(wp);
      // 分析菜单选择:
      switch (wmId) {
        case 104:
          // DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
          break;
        case 105:
          // DestroyWindow(hwnd);
          break;
        default:
          break;
          // return DefWindowProc(hwnd, msg, wp, lp);
      }
    } break;
    case WM_PAINT: {
      // PAINTSTRUCT ps;
      // HDC hdc = BeginPaint(hwnd, &ps);
      // TODO: 在此处添加使用 hdc 的任何绘图代码...
      // EndPaint(hwnd, &ps);
    } break;
    case WM_DESTROY:
      PostQuitMessage(0);
      break;
    default:
      return DefWindowProc(hwnd, msg, wp, lp);
      break;
  }
  return 0;
}

// static
bool MainWnd::RegisterWindowClass() {
  if (wnd_class_) return true;

  WNDCLASSEXW wcex = {sizeof(WNDCLASSEX)};
  wcex.style = CS_DBLCLKS;
  wcex.hInstance = GetModuleHandle(NULL);
  wcex.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  wcex.hCursor = ::LoadCursor(NULL, IDC_ARROW);
  wcex.lpfnWndProc = &WndProc;
  wcex.lpszClassName = kClassName;
  wnd_class_ = ::RegisterClassExW(&wcex);
  // RTC_DCHECK(wnd_class_ != 0);
  return wnd_class_ != 0;
}
