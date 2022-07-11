/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/desktop_capture/screen_capturer.h"

#include <memory>
#include <utility>

#include "webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "webrtc/modules/desktop_capture/win/screen_capturer_win_directx.h"
#include "webrtc/modules/desktop_capture/win/screen_capturer_win_gdi.h"
#include "webrtc/modules/desktop_capture/win/screen_capturer_win_magnifier.h"

#include "webrtc/modules/desktop_capture/win/dxgi_duplicator_controller.h"
#include "webrtc/modules/desktop_capture/blank_detector_desktop_capturer_wrapper.h"
#include "webrtc/modules/desktop_capture/fallback_desktop_capturer_wrapper.h"

namespace webrtc {

	namespace {
		std::unique_ptr<DesktopCapturer> CreateScreenCapturerWinDirectx(const DesktopCaptureOptions& options) {
			std::unique_ptr<DesktopCapturer> capturer(
				new ScreenCapturerWinDirectx(options));
			capturer.reset(new BlankDetectorDesktopCapturerWrapper(
				std::move(capturer), RgbaColor(0, 0, 0, 0)));
			return capturer;
		}

	}  // namespace
// static
ScreenCapturer* ScreenCapturer::Create(const DesktopCaptureOptions& options) {
  std::unique_ptr<ScreenCapturer> capturer;
  if (options.allow_directx_capturer() &&
      ScreenCapturerWinDirectx::Initialize()) {
    capturer.reset(new ScreenCapturerWinDirectx(options));
  } else {
    capturer.reset(new ScreenCapturerWinGdi(options));
  }

  if (options.allow_use_magnification_api())
    return new ScreenCapturerWinMagnifier(std::move(capturer));

  return capturer.release();
}

// static
std::unique_ptr<DesktopCapturer> DesktopCapturer::CreateRawScreenCapturer(
	const DesktopCaptureOptions& options) {
	std::unique_ptr<DesktopCapturer> capturer(new ScreenCapturerWinGdi(options));
	if (options.allow_directx_capturer()) {
		// |dxgi_duplicator_controller| should be alive in this scope to ensure it
		// won't unload DxgiDuplicatorController.
		auto dxgi_duplicator_controller = DxgiDuplicatorController::Instance();
		if (ScreenCapturerWinDirectx::IsSupported()) {
			capturer.reset(new FallbackDesktopCapturerWrapper(
				CreateScreenCapturerWinDirectx(options), std::move(capturer)));
		}
	}

	if (options.allow_use_magnification_api()) {
		// ScreenCapturerWinMagnifier cannot work on Windows XP or earlier, as well
		// as 64-bit only Windows, and it may randomly crash on multi-screen
		// systems. So we may need to fallback to use original capturer.
		capturer.reset(new FallbackDesktopCapturerWrapper(
			std::unique_ptr<DesktopCapturer>(new ScreenCapturerWinMagnifier()),
			std::move(capturer)));
	}

	return capturer;
}

}  // namespace webrtc
