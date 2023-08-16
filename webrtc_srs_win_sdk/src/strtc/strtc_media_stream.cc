#include "strtc_media_stream.h"

#include "api/audio_codecs/audio_decoder_factory.h"
#include "api/audio_codecs/audio_encoder_factory.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/audio_options.h"
#include "api/create_peerconnection_factory.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "modules/audio_device/include/audio_device.h"
#include "modules/video_capture/video_capture_factory.h"
#include "pc/video_track_source.h"
#include "rtc_base/logging.h"
#include "rtc_base/ref_counted_object.h"
#include "strtc_vcm_capturer.h"

namespace strtc {
class CapturerTrackSource : public webrtc::VideoTrackSource {
 public:
  static rtc::scoped_refptr<CapturerTrackSource> Create(int width, int height,
                                                        int fps) {
    std::unique_ptr<strtc::VcmCapturer> capturer;
    std::unique_ptr<webrtc::VideoCaptureModule::DeviceInfo> info(
        webrtc::VideoCaptureFactory::CreateDeviceInfo());
    if (!info) {
      return nullptr;
    }
    int num_devices = info->NumberOfDevices();
    for (int i = 0; i < num_devices; ++i) {
      capturer =
          absl::WrapUnique(strtc::VcmCapturer::Create(width, height, fps, i));
      if (capturer) {
        return rtc::make_ref_counted<CapturerTrackSource>(std::move(capturer));
      }
    }

    return nullptr;
  }

 protected:
  explicit CapturerTrackSource(std::unique_ptr<strtc::VcmCapturer> capturer)
      : VideoTrackSource(/*remote=*/false), capturer_(std::move(capturer)) {}

 private:
  rtc::VideoSourceInterface<webrtc::VideoFrame>* source() override {
    return capturer_.get();
  }

 private:
  std::unique_ptr<strtc::VcmCapturer> capturer_;
};

StrtcMediaStream::StrtcMediaStream(
    rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory,
    StreamOptions& options)
    : factory_(factory),
      has_audio_(options.hasAudio),
      has_video_(options.hasVideo),
      width_(options.width),
      height_(options.height),
      fps_(options.fps) {}

StrtcMediaStream::~StrtcMediaStream() {
  RTC_LOG(LS_INFO) << __FUNCTION__;
  stopStream();
}

bool StrtcMediaStream::startStream() {
  if (!factory_) {
    RTC_LOG(LS_ERROR) << __FUNCTION__ << " peerconnection factroy nullptr";
    return false;
  }
  media_stream_ = factory_->CreateLocalMediaStream("STONElms");
  if (!media_stream_.get()) {
    RTC_LOG(LS_ERROR) << __FUNCTION__ << " create local media stream failed";
    return false;
  }

  if (has_audio_) {
    cricket::AudioOptions options;
    rtc::scoped_refptr<webrtc::AudioSourceInterface> source =
        factory_->CreateAudioSource(options);
    rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track(
        factory_->CreateAudioTrack("STONEaudio", source.get()));
    media_stream_->AddTrack(audio_track);
  }

  if (has_video_) {
    video_device_ = CapturerTrackSource::Create(width_, height_, fps_);
    if (video_device_) {
      rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track(
          factory_->CreateVideoTrack("STONEvideo", video_device_.get()));
      media_stream_->AddTrack(video_track);
    } else {
      RTC_LOG(LS_ERROR) << __FUNCTION__ << " create capturer source failed";
      return false;
    }
  }
  RTC_LOG(LS_INFO) << __FUNCTION__ << " capture audio: " << has_audio_
                   << " video: " << has_video_ << " success";

  return true;
}

void StrtcMediaStream::setVideoRender(HWND wnd) {
  if (media_stream_) {
    video_renderer_ = std::make_unique<VideoRenderer>(wnd);
    auto videoTracks = media_stream_->GetVideoTracks();
    for (auto track : videoTracks) {
      track->AddOrUpdateSink(video_renderer_.get(), rtc::VideoSinkWants());
    }
  }
}
rtc::scoped_refptr<webrtc::MediaStreamInterface>
StrtcMediaStream::getMediaStream() {
  return media_stream_;
}

void StrtcMediaStream::stopStream() {
  if (media_stream_) {
    auto audioTracks = media_stream_->GetAudioTracks();
    for (auto track : audioTracks) {
      media_stream_->RemoveTrack(track);
    }
    auto videoTracks = media_stream_->GetVideoTracks();
    for (auto track : videoTracks) {
      if (video_renderer_) {
        track->RemoveSink(video_renderer_.get());
      }
      media_stream_->RemoveTrack(track);
    }
  }
}
}  // namespace strtc
