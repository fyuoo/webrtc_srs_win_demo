#include "strtc_engine.h"

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
#include "rtc_base/ssl_adapter.h"
#include "rtc_base/trace_event.h"

namespace strtc {
StrtcEngine::StrtcEngine(StrtcEngineObserver* observer)
    : channel_id_(0), observer_(observer) {}

StrtcEngine::~StrtcEngine() { rtc::CleanupSSL(); }

bool StrtcEngine::init() {
  rtc::LogMessage::LogToDebug(rtc::LoggingSeverity::LS_VERBOSE);

  rtc::InitializeSSL();

  task_thread_ = rtc::Thread::Create();
  if (!task_thread_->Start()) {
    return false;
  }

  if (!createPeerConnectionFactory()) {
    return false;
  }

  return true;
}

bool StrtcEngine::startStream(StreamOptions& options) {
  if (!task_thread_->IsCurrent()) {
    return task_thread_->Invoke<bool>(
        RTC_FROM_HERE, [this, &options]() { return startStream(options); });
  }
  local_stream_.reset(new StrtcMediaStream(factory_, options));
  if (local_stream_) {
    return local_stream_->startStream();
  }

  return false;
}

void StrtcEngine::stopStream() {
  if (!task_thread_->IsCurrent()) {
    return task_thread_->Invoke<void>(RTC_FROM_HERE,
                                      [this]() { return stopStream(); });
  }
  // 停止采集，停止推流
  for (auto it = channel_map_.begin(); it != channel_map_.end();) {
    if (it->second->getChannelType() == ChannelType::PUBLISH) {
      channel_map_.erase(it++);
    } else {
      ++it;
    }
  }
  local_stream_.reset();
}

bool StrtcEngine::createPeerConnectionFactory() {
  if (!signaling_thread_.get()) {
    signaling_thread_ = rtc::Thread::Create();
    if (!signaling_thread_->Start()) {
      return false;
    }
  }

  factory_ = webrtc::CreatePeerConnectionFactory(
      nullptr, nullptr, signaling_thread_.get(), nullptr,
      webrtc::CreateBuiltinAudioEncoderFactory(),
      webrtc::CreateBuiltinAudioDecoderFactory(),
      webrtc::CreateBuiltinVideoEncoderFactory(),
      webrtc::CreateBuiltinVideoDecoderFactory(), nullptr, nullptr);

  return factory_ != nullptr;
}

int StrtcEngine::createChannel(ChannelType type) {
  if (!task_thread_->IsCurrent()) {
    return task_thread_->Invoke<int>(
        RTC_FROM_HERE, [this, &type]() { return createChannel(type); });
  }

  rtc::scoped_refptr<StrtcPeerConnectionChannel> pc_channel;
  if (type == ChannelType::PUBLISH) {
    if (!local_stream_) {
      return -1;
    }

    channel_id_++;
    pc_channel = rtc::make_ref_counted<StrtcPeerConnectionChannel>(
        factory_, local_stream_->getMediaStream(), type, channel_id_, this);
  } else if (type == ChannelType::SUBSCRIBE) {
    channel_id_++;
    pc_channel = rtc::make_ref_counted<StrtcPeerConnectionChannel>(
        factory_, nullptr, type, channel_id_, this);
  } else {
  }

  channel_map_[channel_id_] = pc_channel;

  return channel_id_;
}

void StrtcEngine::start(int channel_id, const std::string& url,
                        std::function<void()> on_success,
                        std::function<void(std::string error)> on_failure) {
  task_thread_->PostTask(
      webrtc::ToQueuedTask([this, channel_id, url, on_success, on_failure]() {
        auto it = channel_map_.find(channel_id);
        if (it != channel_map_.end()) {
          if (it->second) {
            it->second->start(url, on_success, on_failure);
          }
        } else {
          RTC_LOG(LS_ERROR)
              << __FUNCTION__ << " channel id: " << channel_id << " not exist";
        }
      }));
}

void StrtcEngine::stop(int channel_id) {
  task_thread_->PostTask(webrtc::ToQueuedTask([this, channel_id]() {
    auto it = channel_map_.find(channel_id);
    if (it != channel_map_.end()) {
      if (it->second) {
        it->second->stop();
      }
    }
  }));
}

void StrtcEngine::setLocalVideoRender(HWND wnd) {
  task_thread_->PostTask(webrtc::ToQueuedTask([this, wnd]() {
    if (local_stream_) {
      local_stream_->setVideoRender(wnd);
    }
  }));
}

void StrtcEngine::setRemoteVideoRender(int channel_id, HWND wnd) {
  task_thread_->PostTask(webrtc::ToQueuedTask([this, channel_id, wnd]() {
    auto it = channel_map_.find(channel_id);
    if (it != channel_map_.end()) {
      if (it->second) {
        it->second->setRemoteVideoRender(wnd);
      }
    }
  }));
}

void StrtcEngine::on_stream_failure(int channel_id, int code,
                                    std::string& error) {
  RTC_LOG(LS_ERROR) << __FUNCTION__ << " channel id: " << channel_id
                    << "code: " << code << "error: " << error;
  if (observer_) {
    observer_->on_stream_error(channel_id, code, error);
  }
}
}  // namespace strtc
