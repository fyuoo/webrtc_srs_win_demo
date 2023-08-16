#ifndef STRTC_ENGINE_INTERFACE_H_
#define STRTC_ENGINE_INTERFACE_H_

#include <functional>
#include <iostream>

#include "strtc_common_define.h"

namespace strtc {
class StrtcEngineInterface {
 public:
  virtual ~StrtcEngineInterface() = default;

  static StrtcEngineInterface* create(StrtcEngineObserver* observer);

  virtual bool init() = 0;
  virtual bool startStream(StreamOptions& options) = 0;
  virtual void stopStream() = 0;
  virtual void setLocalVideoRender(HWND wnd) = 0;
  virtual void setRemoteVideoRender(int channel_id, HWND wnd) = 0;
  virtual void muteLocalAudio(bool mute) = 0;
  virtual void muteLocalVideo(bool mute) = 0;
  virtual int createChannel(ChannelType type) = 0;
  virtual void start(int channel_id, const std::string& url,
                     std::function<void()> on_success,
                     std::function<void(std::string error)> on_failure) = 0;
  virtual void stop(int channel_id) = 0;
};
}  // namespace strtc
#endif  // STRTC_ENGINE_INTERFACE_H_