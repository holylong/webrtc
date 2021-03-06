/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/desktop_capture/window_capturer.h"

#include <assert.h>

#include "webrtc/base/constructormagic.h"
#include "webrtc/modules/desktop_capture/desktop_frame.h"

namespace webrtc {

namespace {

class WindowCapturerNull : public WindowCapturer {
 public:
  WindowCapturerNull();
  virtual ~WindowCapturerNull();

  // WindowCapturer interface.
  bool GetWindowList(WindowList* windows) override;
  bool SelectWindow(WindowId id) override;
  bool BringSelectedWindowToFront() override;
  void CaptureFrame() {};
  // DesktopCapturer interface.
  void Start(Callback* callback) override;
  void Capture(const DesktopRegion& region) override;

 private:
  Callback* callback_ = nullptr;

  RTC_DISALLOW_COPY_AND_ASSIGN(WindowCapturerNull);
};

WindowCapturerNull::WindowCapturerNull() {}
WindowCapturerNull::~WindowCapturerNull() {}

bool WindowCapturerNull::GetWindowList(WindowList* windows) {
  // Not implemented yet.
  return false;
}

bool WindowCapturerNull::SelectWindow(WindowId id) {
  // Not implemented yet.
  return false;
}

bool WindowCapturerNull::BringSelectedWindowToFront() {
  // Not implemented yet.
  return false;
}

void WindowCapturerNull::Start(Callback* callback) {
  assert(!callback_);
  assert(callback);

  callback_ = callback;
}

void WindowCapturerNull::Capture(const DesktopRegion& region) {
  // Not implemented yet.
  callback_->OnCaptureResult(Result::ERROR_TEMPORARY, nullptr);
}

}  // namespace

// static
WindowCapturer* WindowCapturer::Create(const DesktopCaptureOptions& options) {
  return new WindowCapturerNull();
}

// static
std::unique_ptr<DesktopCapturer> DesktopCapturer::CreateRawWindowCapturer(
	const DesktopCaptureOptions& options) {
	return std::unique_ptr<DesktopCapturer>(new WindowCapturerNull());
}

}  // namespace webrtc
