#ifndef WEBRTC_DESKTOP_CAPTURER_FACTORY_H_
#define WEBRTC_DESKTOP_CAPTURER_FACTORY_H_

#include "webrtc/media/base/videocapturerfactory.h"
#include "webrtc/base/logging.h"

namespace cricket {
	class WebRtcScreenCapturerFactory : public ScreenCapturerFactory {
	public:
		//virtual VideoCapturer* Create(const webrtc::WindowId& wndid);
		virtual VideoCapturer* Create(const intptr_t& wndid) {
			LOG(INFO) << "WebRtcScreenCapturerFactory";
		};
	};
}
#endif
