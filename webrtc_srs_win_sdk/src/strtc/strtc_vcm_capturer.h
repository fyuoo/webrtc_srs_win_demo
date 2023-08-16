#ifndef STRTC_VCM_CAPTURER_H_
#define STRTC_VCM_CAPTURER_H_

#include <memory>
#include <vector>

#include "api/scoped_refptr.h"
#include "modules/video_capture/video_capture.h"
#include "rtc_base/thread.h"
#include "test/test_video_capturer.h"

namespace strtc {
class VcmCapturer : public webrtc::test::TestVideoCapturer,
                    public rtc::VideoSinkInterface<webrtc::VideoFrame> {
 public:
  static VcmCapturer* Create(size_t width, size_t height, size_t target_fps,
                             size_t capture_device_index);
  virtual ~VcmCapturer();

  void OnFrame(const webrtc::VideoFrame& frame) override;

 private:
  VcmCapturer();
  bool Init(size_t width, size_t height, size_t target_fps,
            size_t capture_device_index);
  void Destroy();

  rtc::scoped_refptr<webrtc::VideoCaptureModule> CreateDeviceOnCurrentThread(
      const char* unique_device_utf8);
  int32_t StartCaptureOnCurrentThread(webrtc::VideoCaptureCapability);
  int32_t StopCaptureOnCurrentThread();
  void ReleaseOnCurrentThread();

 private:
  rtc::scoped_refptr<webrtc::VideoCaptureModule> vcm_;
  webrtc::VideoCaptureCapability capability_;

  std::unique_ptr<rtc::Thread> thread_;
};
}  // namespace strtc
#endif  // STRTC_VCM_CAPTURER_H_