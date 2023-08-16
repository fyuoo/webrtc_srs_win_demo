#ifndef STRTC_MEDIA_STREAM_H_
#define STRTC_MEDIA_STREAM_H_

#include "api/media_stream_interface.h"
#include "api/peer_connection_interface.h"
#include "strtc_common_define.h"
#include "strtc_video_render.h"

namespace strtc {
class CapturerTrackSource;
class StrtcMediaStream {
 public:
  StrtcMediaStream(
      rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory,
      StreamOptions& options);
  ~StrtcMediaStream();

  bool startStream();
  void stopStream();

  void setVideoRender(HWND wnd);
  rtc::scoped_refptr<webrtc::MediaStreamInterface> getMediaStream();

 private:
  bool has_audio_;
  bool has_video_;
  int width_;
  int height_;
  int fps_;

  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory_;
  rtc::scoped_refptr<CapturerTrackSource> video_device_;
  rtc::scoped_refptr<webrtc::MediaStreamInterface> media_stream_;

  std::unique_ptr<VideoRenderer> video_renderer_;
};
}  // namespace strtc

#endif  // STRTC_MEDIA_STREAM_H_