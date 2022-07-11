/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_capture/video_capture_factory.h"

#include "webrtc/modules/video_capture/video_capture_impl.h"

#include "webrtc/base/logging.h"

namespace webrtc {

rtc::scoped_refptr<VideoCaptureModule> VideoCaptureFactory::Create(
    const int32_t id,
    const char* deviceUniqueIdUTF8) {
	LOG(LS_INFO) << "=======>>>>VideoCaptureImpl in VideoCaptureFactory in WebRtcVcmFactory Create VideoCaptureModule";
#if defined(ANDROID)
  return nullptr;
#else
	LOG(LS_INFO) << "=======>>>>VideoCaptureImpl is in videocapturemodule namespace";
  return videocapturemodule::VideoCaptureImpl::Create(id, deviceUniqueIdUTF8);
#endif
}

rtc::scoped_refptr<VideoCaptureModule> VideoCaptureFactory::Create(
    const int32_t id,
    VideoCaptureExternal*& externalCapture) {
  return videocapturemodule::VideoCaptureImpl::Create(id, externalCapture);
}

VideoCaptureModule::DeviceInfo* VideoCaptureFactory::CreateDeviceInfo(
    const int32_t id) {
#if defined(ANDROID)
  return nullptr;
#else
  return videocapturemodule::VideoCaptureImpl::CreateDeviceInfo(id);
#endif
}

}  // namespace webrtc
