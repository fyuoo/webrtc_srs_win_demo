#ifndef STRTC_COMMON_DEFINE_H_
#define STRTC_COMMON_DEFINE_H_

#ifdef _WIN32
#include <windows.h>
#endif

#include <iostream>

namespace strtc {
enum StreamType { STRREAM_TYPE_CAMERA, STRREAM_TYPE_SCREEN };

struct StreamOptions {
  StreamOptions()
      : streamType(StreamType::STRREAM_TYPE_CAMERA),
        hasAudio(true),
        hasVideo(true),
        width(640),
        height(480),
        fps(25) {}
  StreamType streamType;
  bool hasAudio;
  bool hasVideo;
  int width;
  int height;
  int fps;
};

enum ChannelType { PUBLISH, SUBSCRIBE };

class StrtcEngineObserver {
 public:
  virtual ~StrtcEngineObserver() = default;
  virtual void on_stream_error(int channel_id, int code, std::string error) = 0;
  // virtual void on_add_stream(int channel_id) = 0;
};
}  // namespace strtc
#endif  // STRTC_COMMON_DEFINE_H_
