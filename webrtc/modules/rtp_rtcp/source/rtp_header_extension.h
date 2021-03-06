/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_HEADER_EXTENSION_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_HEADER_EXTENSION_H_

#include <map>

#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "webrtc/typedefs.h"

namespace webrtc {

const uint16_t kRtpOneByteHeaderExtensionId = 0xBEDE;

const size_t kRtpOneByteHeaderLength = 4;
const size_t kTransmissionTimeOffsetLength = 4;
const size_t kAudioLevelLength = 2;
const size_t kAbsoluteSendTimeLength = 4;
const size_t kVideoRotationLength = 2;
const size_t kTransportSequenceNumberLength = 3;
const size_t kPlayoutDelayLength = 4;

// Playout delay in milliseconds. A playout delay limit (min or max)
// has 12 bits allocated. This allows a range of 0-4095 values which translates
// to a range of 0-40950 in milliseconds.
const int kPlayoutDelayGranularityMs = 10;
// Maximum playout delay value in milliseconds.
const int kPlayoutDelayMaxMs = 40950;

struct HeaderExtension {
  explicit HeaderExtension(RTPExtensionType extension_type)
      : type(extension_type), length(0), active(true) {
    Init();
  }

  HeaderExtension(RTPExtensionType extension_type, bool active)
      : type(extension_type), length(0), active(active) {
    Init();
  }

  void Init() {
    // TODO(solenberg): Create handler classes for header extensions so we can
    // get rid of switches like these as well as handling code spread out all
    // over.
    switch (type) {
      case kRtpExtensionTransmissionTimeOffset:
        length = kTransmissionTimeOffsetLength;
        break;
      case kRtpExtensionAudioLevel:
        length = kAudioLevelLength;
        break;
      case kRtpExtensionAbsoluteSendTime:
        length = kAbsoluteSendTimeLength;
        break;
      case kRtpExtensionVideoRotation:
        length = kVideoRotationLength;
        break;
      case kRtpExtensionTransportSequenceNumber:
        length = kTransportSequenceNumberLength;
        break;
      case kRtpExtensionPlayoutDelay:
        length = kPlayoutDelayLength;
        break;
      default:
        assert(false);
    }
  }

  const RTPExtensionType type;
  uint8_t length;
  bool active;
};

class RtpHeaderExtensionMap {
 public:
  static constexpr RTPExtensionType kInvalidType = kRtpExtensionNone;
  static constexpr uint8_t kInvalidId = 0;
  RtpHeaderExtensionMap();
  ~RtpHeaderExtensionMap();

  void Erase();

  int32_t Register(const RTPExtensionType type, const uint8_t id);

  // Active on an extension indicates whether it is currently being added on
  // on the RTP packets. The active/inactive status on an extension can change
  // dynamically depending on the need to convey new information.
  int32_t RegisterInactive(const RTPExtensionType type, const uint8_t id);
  bool SetActive(const RTPExtensionType type, bool active);

  int32_t Deregister(const RTPExtensionType type);

  bool IsRegistered(RTPExtensionType type) const;

  int32_t GetType(const uint8_t id, RTPExtensionType* type) const;
  // Return kInvalidType if not found.
  RTPExtensionType GetType(uint8_t id) const;

  int32_t GetId(const RTPExtensionType type, uint8_t* id) const;
  // Return kInvalidId if not found.
  uint8_t GetId(RTPExtensionType type) const;

  //
  // Methods below ignore any inactive rtp header extensions.
  //

  size_t GetTotalLengthInBytes() const;

  int32_t GetLengthUntilBlockStartInBytes(const RTPExtensionType type) const;

  void GetCopy(RtpHeaderExtensionMap* map) const;

  int32_t Size() const;

  RTPExtensionType First() const;

  RTPExtensionType Next(RTPExtensionType type) const;

 private:
  int32_t Register(const RTPExtensionType type, const uint8_t id, bool active);
  std::map<uint8_t, HeaderExtension*> extensionMap_;
};
}  // namespace webrtc

#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_HEADER_EXTENSION_H_

