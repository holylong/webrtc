/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/desktop_capture/cropping_window_capturer.h"

#include "webrtc/modules/desktop_capture/cropped_desktop_frame.h"
#include "webrtc/system_wrappers/include/logging.h"
//#include "webrtc/base/profiler.h"
//#include "webrtc/base/logging.h"
#include <iostream>

namespace webrtc {

CroppingWindowCapturer::CroppingWindowCapturer(
    const DesktopCaptureOptions& options)
    : options_(options),
      callback_(NULL),
      window_capturer_(WindowCapturer::Create(options)),
      selected_window_(kNullWindowId),
      excluded_window_(kNullWindowId) {
	std::cout << "create WindowCapturer" << std::endl;
}

CroppingWindowCapturer::~CroppingWindowCapturer() {}

void CroppingWindowCapturer::Start(DesktopCapturer::Callback* callback) {
  callback_ = callback;
  window_capturer_->Start(callback);
}

void CroppingWindowCapturer::SetSharedMemoryFactory(
    std::unique_ptr<SharedMemoryFactory> shared_memory_factory) {
  window_capturer_->SetSharedMemoryFactory(std::move(shared_memory_factory));
}

void CroppingWindowCapturer::Capture(const DesktopRegion& region) {
  if (ShouldUseScreenCapturer()) {
    if (!screen_capturer_.get()) {
      screen_capturer_.reset(ScreenCapturer::Create(options_));
      if (excluded_window_) {
        screen_capturer_->SetExcludedWindow(excluded_window_);
      }
      screen_capturer_->Start(this);
    }
    screen_capturer_->Capture(region);
  } else {
    window_capturer_->Capture(region);
  }
}

void CroppingWindowCapturer::SetExcludedWindow(WindowId window) {
  excluded_window_ = window;
  if (screen_capturer_.get()) {
    screen_capturer_->SetExcludedWindow(window);
  }
}

bool CroppingWindowCapturer::GetWindowList(WindowList* windows) {
  return window_capturer_->GetWindowList(windows);
}

bool CroppingWindowCapturer::SelectWindow(WindowId id) {
  if (window_capturer_->SelectWindow(id)) {
    selected_window_ = id;
    return true;
  }
  return false;
}

bool CroppingWindowCapturer::BringSelectedWindowToFront() {
  return window_capturer_->BringSelectedWindowToFront();
}

void CroppingWindowCapturer::OnCaptureResult(
    DesktopCapturer::Result result,
    std::unique_ptr<DesktopFrame> screen_frame) {
  if (!ShouldUseScreenCapturer()) {
    LOG(LS_INFO) << "Window no longer on top when ScreenCapturer finishes";
    window_capturer_->Capture(DesktopRegion());
    return;
  }

  if (result != Result::SUCCESS) {
    LOG(LS_WARNING) << "ScreenCapturer failed to capture a frame";
    callback_->OnCaptureResult(result, nullptr);
    return;
  }

  DesktopRect window_rect = GetWindowRectInVirtualScreen();
  if (window_rect.is_empty()) {
    LOG(LS_WARNING) << "Window rect is empty";
    callback_->OnCaptureResult(Result::ERROR_TEMPORARY, nullptr);
    return;
  }

  callback_->OnCaptureResult(
      Result::SUCCESS,
      CreateCroppedDesktopFrame(std::move(screen_frame), window_rect));
}

#if !defined(WEBRTC_WIN)
// static
WindowCapturer*
CroppingWindowCapturer::Create(const DesktopCaptureOptions& options) {
  return WindowCapturer::Create(options);
}
#endif

}  // namespace webrtc
