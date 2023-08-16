#ifndef STRTC_PEER_CONNECTION_CHANNEL_H_
#define STRTC_PEER_CONNECTION_CHANNEL_H_

#include "api/peer_connection_interface.h"
#include "strtc_common_define.h"
#include "strtc_srs_signal.h"

namespace strtc {
class StrtcPeerConnectionChannelObserver {
 public:
  virtual ~StrtcPeerConnectionChannelObserver() = default;
  virtual void on_stream_failure(int channel_id, int code,
                                 std::string& error) = 0;
};

class StrtcPeerConnectionChannel
    : public webrtc::CreateSessionDescriptionObserver,
      public webrtc::PeerConnectionObserver {
 public:
  StrtcPeerConnectionChannel(
      rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory,
      rtc::scoped_refptr<webrtc::MediaStreamInterface> media_stream,
      ChannelType channel_type, int channel_id,
      StrtcPeerConnectionChannelObserver* observer);

  ~StrtcPeerConnectionChannel();

  void start(const std::string& url, std::function<void()> on_success,
             std::function<void(std::string error)> on_failure);
  void stop();
  void setRemoteVideoRender(HWND wnd);

  ChannelType getChannelType() { return channel_type_; }

 private:
  bool createPeerConnection();
  void createOffer();
  void createAnswer();

  void sendOffer(const std::string& message);

  // PeerConnectionObserver implementation
  void OnSignalingChange(
      webrtc::PeerConnectionInterface::SignalingState new_state) override;
  void OnAddStream(
      rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override;
  void OnRemoveStream(
      rtc::scoped_refptr<webrtc::MediaStreamInterface> strea) override;
  void OnAddTrack(
      rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver,
      const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>&
          streams) override;
  void OnRemoveTrack(
      rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver) override;
  void OnDataChannel(
      rtc::scoped_refptr<webrtc::DataChannelInterface> channel) override;
  void OnRenegotiationNeeded() override;
  void OnIceConnectionChange(
      webrtc::PeerConnectionInterface::IceConnectionState new_state) override;
  void OnIceGatheringChange(
      webrtc::PeerConnectionInterface::IceGatheringState new_state) override;
  void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override;
  void OnIceConnectionReceivingChange(bool receiving) override;
  void OnConnectionChange(
      webrtc::PeerConnectionInterface::PeerConnectionState new_state) override;

  // CreateSessionDescriptionObserver implementation
  void OnSuccess(webrtc::SessionDescriptionInterface* desc) override;
  void OnFailure(webrtc::RTCError error) override;
  void OnSetLocalSessionDescriptionSuccess();
  void OnSetLocalSessionDescriptionFailure(const std::string& error);
  void OnSetRemoteSessionDescriptionSuccess();
  void OnSetRemoteSessionDescriptionFailure(const std::string& error);

 private:
  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory_;

  rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;
  rtc::scoped_refptr<webrtc::MediaStreamInterface> media_stream_;
  std::unique_ptr<rtc::VideoSinkInterface<webrtc::VideoFrame>> video_renderer_;

  ChannelType channel_type_;
  int channel_id_;
  std::string url_;
  std::unique_ptr<StrtcSrsSignal> srs_signaling_;

  StrtcPeerConnectionChannelObserver* observer_;

  std::function<void()> on_success_;
  std::function<void(std::string error)> on_failure_;
};
}  // namespace strtc
#endif  // STRTC_PEER_CONNECTION_CHANNEL_H_