#include "strtc_peer_connection_channel.h"

#include "modules/audio_device/include/audio_device.h"
#include "modules/video_capture/video_capture_factory.h"
#include "pc/video_track_source.h"
#include "rtc_base/thread.h"
#include "strtc_srs_signal.h"
#include "strtc_video_render.h"

namespace strtc {
class DummySetSessionDescriptionObserver
    : public webrtc::SetSessionDescriptionObserver {
 public:
  static rtc::scoped_refptr<DummySetSessionDescriptionObserver> Create(
      std::function<void()> on_success,
      std::function<void(const std::string&)> on_failure) {
    return rtc::make_ref_counted<DummySetSessionDescriptionObserver>(
        on_success, on_failure);
  }

  virtual void OnSuccess() {
    if (on_success_) {
      on_success_();
    }
  }

  virtual void OnFailure(webrtc::RTCError error) {
    if (on_failure_) {
      on_failure_(error.message());
    }
  }

 protected:
  DummySetSessionDescriptionObserver(
      std::function<void()> on_success,
      std::function<void(const std::string&)> on_failure)
      : on_success_(on_success), on_failure_(on_failure) {}

 private:
  std::function<void()> on_success_;
  std::function<void(const std::string&)> on_failure_;
};

StrtcPeerConnectionChannel::StrtcPeerConnectionChannel(
    rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory,
    rtc::scoped_refptr<webrtc::MediaStreamInterface> media_stream,
    ChannelType channel_type, int channel_id,
    StrtcPeerConnectionChannelObserver* observer)
    : factory_(factory),
      media_stream_(media_stream),
      channel_type_(channel_type),
      channel_id_(channel_id),
      observer_(observer) {
  srs_signaling_.reset(new StrtcSrsSignal());
}

StrtcPeerConnectionChannel::~StrtcPeerConnectionChannel() {
  RTC_LOG(LS_INFO) << __FUNCTION__;
  if (peer_connection_) {
    std::vector<rtc::scoped_refptr<webrtc::RtpSenderInterface>> senders =
        peer_connection_->GetSenders();
    for (const auto& sender : senders) {
      peer_connection_->RemoveTrack(sender);
    }
    peer_connection_->Close();
  }
}

void StrtcPeerConnectionChannel::start(
    const std::string& url, std::function<void()> on_success,
    std::function<void(std::string error)> on_failure) {
  url_ = url;
  on_success_ = on_success;
  on_failure_ = on_failure;
  if (!createPeerConnection()) {
    if (on_failure_) {
      std::string error("create peer connection failed");
      on_failure_(error);
      on_failure_ = nullptr;
    }
    return;
  }
}

void StrtcPeerConnectionChannel::stop() {}

void StrtcPeerConnectionChannel::setRemoteVideoRender(HWND wnd) {
  video_renderer_.reset(new VideoRenderer(wnd));
  std::vector<rtc::scoped_refptr<webrtc::RtpReceiverInterface>> receiver =
      peer_connection_->GetReceivers();
  if (video_renderer_.get()) {
    for (int index = 0; index < receiver.size(); index++) {
      auto* track = reinterpret_cast<webrtc::MediaStreamTrackInterface*>(
          (void*)receiver[index]->track());
      if (track->kind() == webrtc::MediaStreamTrackInterface::kVideoKind) {
        auto* video_track = static_cast<webrtc::VideoTrackInterface*>(track);
        video_track->AddOrUpdateSink(video_renderer_.get(),
                                     rtc::VideoSinkWants());
      }
    }
  }
}

bool StrtcPeerConnectionChannel::createPeerConnection() {
  if (!factory_) {
    RTC_LOG(LS_ERROR) << __FUNCTION__ << " peer connection factroy is nullptr";
    return false;
  }

  webrtc::PeerConnectionInterface::RTCConfiguration config;
  config.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
  webrtc::PeerConnectionInterface::IceServer server;
  server.uri = "stun:stun.l.google.com:19302";
  config.servers.push_back(server);
  config.disable_link_local_networks = true;
  auto error_or_peer_connection = factory_->CreatePeerConnectionOrError(
      config, webrtc::PeerConnectionDependencies(this));
  if (error_or_peer_connection.ok()) {
    peer_connection_ = std::move(error_or_peer_connection.value());
  }

  if (!peer_connection_) {
    RTC_LOG(LS_ERROR) << __FUNCTION__ << " peer connection is nullptr";
    return false;
  }

  if (channel_type_ == ChannelType::PUBLISH) {
    if (media_stream_) {
      for (const auto& audioTrack : media_stream_->GetAudioTracks()) {
        peer_connection_->AddTrack(audioTrack, {});
      }
      for (const auto& videoTrack : media_stream_->GetVideoTracks()) {
        peer_connection_->AddTrack(videoTrack, {});
      }
    }
  } else if (channel_type_ == ChannelType::SUBSCRIBE) {
    webrtc::RtpTransceiverInit init;
    init.direction = webrtc::RtpTransceiverDirection::kRecvOnly;
    peer_connection_->AddTransceiver(cricket::MediaType::MEDIA_TYPE_AUDIO,
                                     init);
    peer_connection_->AddTransceiver(cricket::MediaType::MEDIA_TYPE_VIDEO,
                                     init);
  }

  createOffer();

  return true;
}

void StrtcPeerConnectionChannel::createOffer() {
  webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;
  options.offer_to_receive_audio = true;
  options.offer_to_receive_video = true;
  peer_connection_->CreateOffer(this, options);
}

void StrtcPeerConnectionChannel::createAnswer() {
  peer_connection_->CreateAnswer(
      this, webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());
}

void StrtcPeerConnectionChannel::sendOffer(const std::string& offer) {
  std::string answer;
  if (srs_signaling_->post(url_, offer, &answer, channel_type_) == 0) {
    RTC_LOG(LS_INFO) << __FUNCTION__ << " " << answer;
    webrtc::SdpParseError error;
    std::unique_ptr<webrtc::SessionDescriptionInterface> desc =
        webrtc::CreateSessionDescription(webrtc::SdpType::kAnswer, answer,
                                         &error);
    peer_connection_->SetRemoteDescription(
        DummySetSessionDescriptionObserver::Create(
            std::bind(&StrtcPeerConnectionChannel::
                          OnSetRemoteSessionDescriptionSuccess,
                      this),
            std::bind(&StrtcPeerConnectionChannel::
                          OnSetRemoteSessionDescriptionFailure,
                      this, std::placeholders::_1))
            .get(),
        desc.release());
  } else {
    if (on_failure_) {
      std::string error("requeset signaling server failed");
      on_failure_(error);
      on_failure_ = nullptr;
    }
  }
}

void StrtcPeerConnectionChannel::OnSuccess(
    webrtc::SessionDescriptionInterface* desc) {
  if (peer_connection_) {
    peer_connection_->SetLocalDescription(
        DummySetSessionDescriptionObserver::Create(
            std::bind(&StrtcPeerConnectionChannel::
                          OnSetLocalSessionDescriptionSuccess,
                      this),
            std::bind(&StrtcPeerConnectionChannel::
                          OnSetLocalSessionDescriptionFailure,
                      this, std::placeholders::_1))
            .get(),
        desc);
    std::string sdp;
    desc->ToString(&sdp);
    sendOffer(sdp);
  }
}

void StrtcPeerConnectionChannel::OnFailure(webrtc::RTCError error) {
  RTC_LOG(LS_ERROR) << __FUNCTION__ << " create session failure "
                    << ToString(error.type()) << ": " << error.message();
  if (on_failure_) {
    std::string error("create session failed");
    on_failure_(error);
    on_failure_ = nullptr;
  }
}

void StrtcPeerConnectionChannel::OnSetLocalSessionDescriptionSuccess() {
  RTC_LOG(LS_ERROR) << __FUNCTION__;
}

void StrtcPeerConnectionChannel::OnSetLocalSessionDescriptionFailure(
    const std::string& error) {
  RTC_LOG(LS_ERROR) << __FUNCTION__ << " " << error;
  if (on_failure_) {
    std::string error("set local session failed");
    on_failure_(error);
    on_failure_ = nullptr;
  }
}

void StrtcPeerConnectionChannel::OnSetRemoteSessionDescriptionSuccess() {
  RTC_LOG(LS_ERROR) << __FUNCTION__;
  if (on_success_) {
    on_success_();
    on_success_ = nullptr;
  }
}

void StrtcPeerConnectionChannel::OnSetRemoteSessionDescriptionFailure(
    const std::string& error) {
  RTC_LOG(LS_ERROR) << __FUNCTION__ << " " << error;
  if (on_failure_) {
    std::string error("set remote session failed");
    on_failure_(error);
    on_failure_ = nullptr;
  }
}

void StrtcPeerConnectionChannel::OnSignalingChange(
    webrtc::PeerConnectionInterface::SignalingState new_state) {
  RTC_LOG(LS_INFO) << __FUNCTION__ << " " << new_state;
}

void StrtcPeerConnectionChannel::OnAddStream(
    rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) {
  RTC_LOG(LS_INFO) << __FUNCTION__;
}

void StrtcPeerConnectionChannel::OnRemoveStream(
    rtc::scoped_refptr<webrtc::MediaStreamInterface> strea) {
  RTC_LOG(LS_INFO) << __FUNCTION__;
}

void StrtcPeerConnectionChannel::OnAddTrack(
    rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver,
    const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>&
        streams) {
  RTC_LOG(LS_INFO) << __FUNCTION__ << " " << receiver->id();
}

void StrtcPeerConnectionChannel::OnRemoveTrack(
    rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver) {
  RTC_LOG(LS_INFO) << __FUNCTION__ << " " << receiver->id();
}

void StrtcPeerConnectionChannel::OnDataChannel(
    rtc::scoped_refptr<webrtc::DataChannelInterface> channel) {
  RTC_LOG(LS_INFO) << __FUNCTION__;
}

void StrtcPeerConnectionChannel::OnRenegotiationNeeded() {
  RTC_LOG(LS_INFO) << __FUNCTION__;
}

void StrtcPeerConnectionChannel::OnIceConnectionChange(
    webrtc::PeerConnectionInterface::IceConnectionState new_state) {
  RTC_LOG(LS_INFO) << __FUNCTION__ << " new state: " << new_state;
}

void StrtcPeerConnectionChannel::OnIceGatheringChange(
    webrtc::PeerConnectionInterface::IceGatheringState new_state) {}

void StrtcPeerConnectionChannel::OnIceCandidate(
    const webrtc::IceCandidateInterface* candidate) {
  RTC_LOG(LS_INFO) << __FUNCTION__;
}

void StrtcPeerConnectionChannel::OnIceConnectionReceivingChange(
    bool receiving) {}

void StrtcPeerConnectionChannel::OnConnectionChange(
    webrtc::PeerConnectionInterface::PeerConnectionState new_state) {
  RTC_LOG(LS_INFO) << __FUNCTION__ << " new state: " << new_state;
  if (observer_) {
    if (new_state ==
        webrtc::PeerConnectionInterface::PeerConnectionState::kConnected) {
    } else if (new_state == webrtc::PeerConnectionInterface::
                                PeerConnectionState::kConnected) {
    } else if (new_state ==
               webrtc::PeerConnectionInterface::PeerConnectionState::kClosed) {
    } else if (new_state == webrtc::PeerConnectionInterface::
                                PeerConnectionState::kDisconnected) {
    } else if (new_state ==
               webrtc::PeerConnectionInterface::PeerConnectionState::kFailed) {
      std::string error_msg("peer connection failed");
      observer_->on_stream_failure(channel_id_, 0, error_msg);
    }
  }
}
}  // namespace strtc
