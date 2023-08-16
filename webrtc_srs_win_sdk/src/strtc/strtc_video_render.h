#ifndef STRTC_VIDEO_RENDER_H_
#define STRTC_VIDEO_RENDER_H_

#include "api/video/video_frame.h"
#include "api/video/video_sink_interface.h"

#if defined(WEBRTC_WIN)
#include "rtc_base/win32.h"
#endif  // WEBRTC_WIN

namespace strtc {
class VideoRenderer : public rtc::VideoSinkInterface<webrtc::VideoFrame> {
 public:
  VideoRenderer(HWND& wnd);
  virtual ~VideoRenderer();

  void Lock() { ::EnterCriticalSection(&buffer_lock_); }
  void Unlock() { ::LeaveCriticalSection(&buffer_lock_); }

  void OnFrame(const webrtc::VideoFrame& frame) override;

  void OnPaint();

  const BITMAPINFO& bmi() const { return bmi_; }
  const uint8_t* image() const { return image_.get(); }

 protected:
  void SetSize(int width, int height);

  enum {
    SET_SIZE,
    RENDER_FRAME,
  };

  HWND wnd_;
  BITMAPINFO bmi_;
  std::unique_ptr<uint8_t[]> image_;
  CRITICAL_SECTION buffer_lock_;
};

template <typename T>
class AutoLock {
 public:
  explicit AutoLock(T* obj) : obj_(obj) { obj_->Lock(); }
  ~AutoLock() { obj_->Unlock(); }

 protected:
  T* obj_;
};
}  // namespace strtc
#endif  // STRTC_VIDEO_RENDER_H_