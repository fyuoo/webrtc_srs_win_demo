/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// clang-format off
// clang formating would change include order.
#include <windows.h>
#include <shellapi.h>  // must come after windows.h
// clang-format on

#include <iostream>
#include <string>

#include "main_wnd.h"
#include "strtc_common_define.h"
#include "strtc_engine_interface.h"

#if 1
int PASCAL wWinMain(HINSTANCE instance, HINSTANCE prev_instance,
                    wchar_t* cmd_line, int cmd_show) {
#else
int main() {
#endif
  WSADATA wsaData;
  WORD wVersionRequested = MAKEWORD(1, 0);
  WSAStartup(wVersionRequested, &wsaData);

  class DummyObserver : public strtc::StrtcEngineObserver {
    virtual void on_stream_error(int channel_id, int code,
                                 std::string error) override {
      std::cout << "on stream error channel id: " << channel_id
                << " code: " << code << " error: " << error << std::endl;
    }
  };

  std::unique_ptr<DummyObserver> observer =
      std::unique_ptr<DummyObserver>(new DummyObserver());
  std::unique_ptr<strtc::StrtcEngineInterface> engine(
      strtc::StrtcEngineInterface::create(observer.get()));
  engine->init();

#if 1
  // PUBLISH
  MainWnd local_stream_wnd;
  local_stream_wnd.Create();

  strtc::StreamOptions options;
  engine->startStream(options);
  engine->setLocalVideoRender(local_stream_wnd.handle());

  int publish_channel_id = engine->createChannel(strtc::ChannelType::PUBLISH);
  engine->start(
      publish_channel_id,
      "webrtc://192.168.1.161:1985/live/livestream",
      [publish_channel_id]() {
        std::cout << "publish success id: " << publish_channel_id << std::endl;
      },
      [publish_channel_id](std::string error) {
        std::cout << "publish failed id: " << publish_channel_id
                  << "error: " << error << std::endl;
      });
#endif

#if 0
  // SUBSCRIBE 1
  MainWnd remote_stream_wnd;
  remote_stream_wnd.Create();
  int subscribe_channel_id =
      engine->createChannel(strtc::ChannelType::SUBSCRIBE);
  engine->start(
      subscribe_channel_id, "webrtc://192.168.1.161:1985/live/livestream",
      [subscribe_channel_id]() {
        std::cout << "subscribe success id: " << subscribe_channel_id
                  << std::endl;
      },
      [subscribe_channel_id](std::string error) {
        std::cout << "subscribe failed id: " << subscribe_channel_id
                  << "error: " << error << std::endl;
      });
  engine->setRemoteVideoRender(subscribe_channel_id,
                               remote_stream_wnd.handle());
#endif

#if 0
  // SUBSCRIBE 2
  MainWnd remote_stream_wnd2;
  remote_stream_wnd2.Create();
  int subscribe_channel_id_2 =
      engine->createChannel(strtc::ChannelType::SUBSCRIBE);
  engine->start(
      subscribe_channel_id_2, "webrtc://192.168.1.161:1985/live/livestream",
      [subscribe_channel_id_2]() {
        std::cout << "subscribe success id: " << subscribe_channel_id_2
                  << std::endl;
      },
      [subscribe_channel_id_2](std::string error) {
        std::cout << "subscribe failed id: " << subscribe_channel_id_2
                  << "error: " << error << std::endl;
      });
  engine->setRemoteVideoRender(subscribe_channel_id_2,
                               remote_stream_wnd2.handle());
#endif

  // Main loop
  MSG msg;
  BOOL gm;
  while ((gm = ::GetMessage(&msg, NULL, 0, 0)) != 0 && gm != -1) {
    ::TranslateMessage(&msg);
    ::DispatchMessage(&msg);
  }

  return 0;
}
