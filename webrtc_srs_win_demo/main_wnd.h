/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef EXAMPLES_PEERCONNECTION_CLIENT_MAIN_WND_H_
#define EXAMPLES_PEERCONNECTION_CLIENT_MAIN_WND_H_

#include <map>
#include <memory>
#include <string>
#if defined(WEBRTC_WIN)
#include "rtc_base/win32.h"
#endif  // WEBRTC_WIN

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef WIN32

class MainWnd {
 public:
  static const wchar_t kClassName[];

  MainWnd();
  ~MainWnd();

  HWND handle() const { return wnd_; }
  bool Create();
  bool Destroy();

 protected:
  enum ChildWindowID {
    EDIT_ID = 1,
    BUTTON_ID,
    LABEL1_ID,
    LABEL2_ID,
    LISTBOX_ID,
  };

  static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
  static bool RegisterWindowClass();

 private:
  HWND wnd_;
  DWORD ui_thread_id_;
  HWND edit1_;
  HWND edit2_;
  HWND label1_;
  HWND label2_;
  HWND button_;
  HWND listbox_;
  static ATOM wnd_class_;
};
#endif  // WIN32

#endif  // EXAMPLES_PEERCONNECTION_CLIENT_MAIN_WND_H_
