#include "strtc_video_render.h"

#include "api/video/i420_buffer.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/logging.h"
#include "third_party/libyuv/include/libyuv/convert.h"
#include "third_party/libyuv/include/libyuv/convert_argb.h"
#include "third_party/libyuv/include/libyuv/convert_from.h"

namespace strtc {
VideoRenderer::VideoRenderer(HWND& wnd) {
  RTC_LOG(LS_INFO) << "Create VideoRenderer";
  ::InitializeCriticalSection(&buffer_lock_);
  ZeroMemory(&bmi_, sizeof(bmi_));
  bmi_.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi_.bmiHeader.biPlanes = 1;
  bmi_.bmiHeader.biBitCount = 32;
  bmi_.bmiHeader.biCompression = BI_RGB;
  bmi_.bmiHeader.biWidth = 1;
  bmi_.bmiHeader.biHeight = -1;
  bmi_.bmiHeader.biSizeImage = 1 * 1 * (bmi_.bmiHeader.biBitCount >> 3);
  wnd_ = wnd;
}

VideoRenderer::~VideoRenderer() {
  RTC_LOG(LS_INFO) << ("~VideoRenderer");
  ::DeleteCriticalSection(&buffer_lock_);
}

void VideoRenderer::SetSize(int width, int height) {
  AutoLock<VideoRenderer> lock(this);

  if (width == bmi_.bmiHeader.biWidth && -height == bmi_.bmiHeader.biHeight) {
    return;
  }

  bmi_.bmiHeader.biWidth = width;
  bmi_.bmiHeader.biHeight = -height;
  bmi_.bmiHeader.biSizeImage =
      width * height * (bmi_.bmiHeader.biBitCount >> 3);
  image_.reset(new uint8_t[bmi_.bmiHeader.biSizeImage]);
}

void VideoRenderer::OnFrame(const webrtc::VideoFrame& video_frame) {
  // RTC_LOG(LS_INFO) << "OnFrame";
  {
    AutoLock<VideoRenderer> lock(this);

    rtc::scoped_refptr<webrtc::I420BufferInterface> buffer(
        video_frame.video_frame_buffer()->ToI420());
    if (video_frame.rotation() != webrtc::kVideoRotation_0) {
      buffer = webrtc::I420Buffer::Rotate(*buffer, video_frame.rotation());
    }

    if (wnd_) {
      SetSize(buffer->width(), buffer->height());

      RTC_DCHECK(image_.get() != NULL);
      libyuv::I420ToARGB(buffer->DataY(), buffer->StrideY(), buffer->DataU(),
                         buffer->StrideU(), buffer->DataV(), buffer->StrideV(),
                         image_.get(),
                         bmi_.bmiHeader.biWidth * bmi_.bmiHeader.biBitCount / 8,
                         buffer->width(), buffer->height());

      if (IsWindow(wnd_)) {
        OnPaint();
      } else {
        RTC_LOG(LS_ERROR) << "wnd_ is not window!";
      }
    }
  }
}

void VideoRenderer::OnPaint() {
  // PAINTSTRUCT ps;
  // ::BeginPaint(wnd_, &ps);

  RECT rc;
  ::GetClientRect(wnd_, &rc);

  const BITMAPINFO& bmi = this->bmi();
  int height = abs(bmi.bmiHeader.biHeight);
  int width = bmi.bmiHeader.biWidth;

  const uint8_t* image = this->image();
  if (image != NULL) {
    auto mWindowDC = ::GetDC(wnd_);
    HDC dc_mem = ::CreateCompatibleDC(mWindowDC);
    ::SetStretchBltMode(dc_mem, HALFTONE);

    // Set the map mode so that the ratio will be maintained for us.
    HDC all_dc[] = {mWindowDC, dc_mem};
    for (int i = 0; i < arraysize(all_dc); ++i) {
      SetMapMode(all_dc[i], MM_ISOTROPIC);
      SetWindowExtEx(all_dc[i], width, height, NULL);
      SetViewportExtEx(all_dc[i], rc.right, rc.bottom, NULL);
    }

    HBITMAP bmp_mem = ::CreateCompatibleBitmap(mWindowDC, rc.right, rc.bottom);
    HGDIOBJ bmp_old = ::SelectObject(dc_mem, bmp_mem);

    POINT logical_area = {rc.right, rc.bottom};
    DPtoLP(mWindowDC, &logical_area, 1);

    HBRUSH brush = ::CreateSolidBrush(RGB(0, 0, 0));
    RECT logical_rect = {0, 0, logical_area.x, logical_area.y};
    ::FillRect(dc_mem, &logical_rect, brush);
    ::DeleteObject(brush);

    int x = (logical_area.x / 2) - (width / 2);
    int y = (logical_area.y / 2) - (height / 2);

    StretchDIBits(dc_mem, x, y, width, height, 0, 0, width, height, image, &bmi,
                  DIB_RGB_COLORS, SRCCOPY);

    BitBlt(mWindowDC, 0, 0, logical_area.x, logical_area.y, dc_mem, 0, 0,
           SRCCOPY);

    // Cleanup.
    ::SelectObject(dc_mem, bmp_old);
    ::DeleteObject(bmp_mem);
    ::DeleteDC(dc_mem);
    ::ReleaseDC(wnd_, mWindowDC);
  }
  // ::EndPaint(wnd_, &ps);
}
}  // namespace strtc