#ifndef STRTC_ENGINE_H_
#define STRTC_ENGINE_H_

#include <map>

#include "rtc_base/thread.h"
#include "strtc_engine_interface.h"
#include "strtc_media_stream.h"
#include "strtc_peer_connection_channel.h"

namespace strtc {
class StrtcEngine : public StrtcEngineInterface,
                    public StrtcPeerConnectionChannelObserver {
 public:
  StrtcEngine(StrtcEngineObserver* observer);
  ~StrtcEngine();

  virtual bool init() override;

  virtual bool startStream(StreamOptions& options) override;
  virtual void stopStream() override;

  virtual void setLocalVideoRender(HWND wnd) override;
  virtual void setRemoteVideoRender(int channel_id, HWND wnd) override;
  virtual void muteLocalAudio(bool mute) override{};
  virtual void muteLocalVideo(bool mute) override{};
  virtual int createChannel(ChannelType type) override;

  virtual void start(
      int channel_id, const std::string& url, std::function<void()> on_success,
      std::function<void(std::string error)> on_failure) override;
  virtual void stop(int channel_id) override;

 private:
  bool createPeerConnectionFactory();

  virtual void on_stream_failure(int channel_id, int code,
                                 std::string& error) override;

 private:
  std::unique_ptr<rtc::Thread> task_thread_;

  std::unique_ptr<rtc::Thread> signaling_thread_;
  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory_;

  std::unique_ptr<StrtcMediaStream> local_stream_;

  int channel_id_;
  std::map<int, rtc::scoped_refptr<StrtcPeerConnectionChannel>> channel_map_;

  StrtcEngineObserver* observer_;
};
}  // namespace strtc
#endif  // STRTC_ENGINE_H_