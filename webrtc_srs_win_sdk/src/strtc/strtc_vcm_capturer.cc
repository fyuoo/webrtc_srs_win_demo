#include "strtc_vcm_capturer.h"

#include "modules/video_capture/video_capture_factory.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace strtc {
VcmCapturer::VcmCapturer()
    : vcm_(nullptr), thread_(rtc::Thread::CreateWithSocketServer()) {
  thread_->Start();
}

bool VcmCapturer::Init(size_t width, size_t height, size_t target_fps,
                       size_t capture_device_index) {
  std::unique_ptr<webrtc::VideoCaptureModule::DeviceInfo> device_info(
      webrtc::VideoCaptureFactory::CreateDeviceInfo());

  char device_name[256];
  char unique_name[256];
  if (device_info->GetDeviceName(static_cast<uint32_t>(capture_device_index),
                                 device_name, sizeof(device_name), unique_name,
                                 sizeof(unique_name)) != 0) {
    Destroy();
    return false;
  }

  vcm_ = thread_->Invoke<rtc::scoped_refptr<webrtc::VideoCaptureModule>>(
      RTC_FROM_HERE, [this, &unique_name] {
        return CreateDeviceOnCurrentThread(unique_name);
      });
  if (!vcm_) {
    return false;
  }
  vcm_->RegisterCaptureDataCallback(this);

  device_info->GetCapability(vcm_->CurrentDeviceName(), 0, capability_);

  capability_.width = static_cast<int32_t>(width);
  capability_.height = static_cast<int32_t>(height);
  capability_.maxFPS = static_cast<int32_t>(target_fps);
  capability_.videoType = webrtc::VideoType::kI420;

  if (thread_->Invoke<int32_t>(RTC_FROM_HERE, [this] {
        return StartCaptureOnCurrentThread(capability_);
      }) != 0) {
    Destroy();
    return false;
  }

  return true;
}

rtc::scoped_refptr<webrtc::VideoCaptureModule>
VcmCapturer::CreateDeviceOnCurrentThread(const char* deviceUniqueIdUTF8) {
  return webrtc::VideoCaptureFactory::Create(deviceUniqueIdUTF8);
}

int32_t VcmCapturer::StartCaptureOnCurrentThread(
    webrtc::VideoCaptureCapability capability) {
  return vcm_->StartCapture(capability);
}

int32_t VcmCapturer::StopCaptureOnCurrentThread() {
  return vcm_->StopCapture();
}

void VcmCapturer::ReleaseOnCurrentThread() { vcm_ = nullptr; }

VcmCapturer* VcmCapturer::Create(size_t width, size_t height, size_t target_fps,
                                 size_t capture_device_index) {
  std::unique_ptr<VcmCapturer> vcm_capturer(new VcmCapturer());
  if (!vcm_capturer->Init(width, height, target_fps, capture_device_index)) {
    RTC_LOG(LS_WARNING) << "Failed to create VcmCapturer(w = " << width
                        << ", h = " << height << ", fps = " << target_fps
                        << ")";
    return nullptr;
  }
  return vcm_capturer.release();
}

void VcmCapturer::Destroy() {
  if (!vcm_) {
    return;
  }
  thread_->Invoke<int32_t>(RTC_FROM_HERE,
                           [this] { return StopCaptureOnCurrentThread(); });
  vcm_->DeRegisterCaptureDataCallback();
  thread_->Invoke<void>(RTC_FROM_HERE,
                        [this] { return ReleaseOnCurrentThread(); });
}

VcmCapturer::~VcmCapturer() { Destroy(); }

void VcmCapturer::OnFrame(const webrtc::VideoFrame& frame) {
  webrtc::test::TestVideoCapturer::OnFrame(frame);
}
}  // namespace strtc