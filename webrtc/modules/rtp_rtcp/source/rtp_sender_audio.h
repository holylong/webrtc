/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_SENDER_AUDIO_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_SENDER_AUDIO_H_

#include "webrtc/common_types.h"
#include "webrtc/base/criticalsection.h"
#include "webrtc/base/onetimeevent.h"
#include "webrtc/modules/rtp_rtcp/source/dtmf_queue.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_rtcp_config.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_sender.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"
#include "webrtc/typedefs.h"

namespace webrtc {
class RTPSenderAudio : public DTMFqueue {
 public:
  RTPSenderAudio(Clock* clock, RTPSender* rtpSender);
  virtual ~RTPSenderAudio();

  int32_t RegisterAudioPayload(const char payloadName[RTP_PAYLOAD_NAME_SIZE],
                               int8_t payloadType,
                               uint32_t frequency,
                               size_t channels,
                               uint32_t rate,
                               RtpUtility::Payload** payload);

  int32_t SendAudio(FrameType frameType,
                    int8_t payloadType,
                    uint32_t captureTimeStamp,
                    const uint8_t* payloadData,
                    size_t payloadSize,
                    const RTPFragmentationHeader* fragmentation);

  // set audio packet size, used to determine when it's time to send a DTMF
  // packet in silence (CNG)
  int32_t SetAudioPacketSize(uint16_t packetSizeSamples);

  // Store the audio level in dBov for
  // header-extension-for-audio-level-indication.
  // Valid range is [0,100]. Actual value is negative.
  int32_t SetAudioLevel(uint8_t level_dBov);

  // Send a DTMF tone using RFC 2833 (4733)
  int32_t SendTelephoneEvent(uint8_t key, uint16_t time_ms, uint8_t level);

  int AudioFrequency() const;

  // Set payload type for Redundant Audio Data RFC 2198
  int32_t SetRED(int8_t payloadType);

  // Get payload type for Redundant Audio Data RFC 2198
  int32_t RED(int8_t* payloadType) const;

 protected:
  int32_t SendTelephoneEventPacket(
      bool ended,
      int8_t dtmf_payload_type,
      uint32_t dtmfTimeStamp,
      uint16_t duration,
      bool markerBit);  // set on first packet in talk burst

  bool MarkerBit(const FrameType frameType, const int8_t payloadType);

 private:
  Clock* const _clock;
  RTPSender* const _rtpSender;

  rtc::CriticalSection _sendAudioCritsect;

  uint16_t _packetSizeSamples GUARDED_BY(_sendAudioCritsect);

  // DTMF
  bool _dtmfEventIsOn;
  bool _dtmfEventFirstPacketSent;
  int8_t _dtmfPayloadType GUARDED_BY(_sendAudioCritsect);
  uint32_t _dtmfTimestamp;
  uint8_t _dtmfKey;
  uint32_t _dtmfLengthSamples;
  uint8_t _dtmfLevel;
  int64_t _dtmfTimeLastSent;
  uint32_t _dtmfTimestampLastSent;

  int8_t _REDPayloadType GUARDED_BY(_sendAudioCritsect);

  // VAD detection, used for markerbit
  bool _inbandVADactive GUARDED_BY(_sendAudioCritsect);
  int8_t _cngNBPayloadType GUARDED_BY(_sendAudioCritsect);
  int8_t _cngWBPayloadType GUARDED_BY(_sendAudioCritsect);
  int8_t _cngSWBPayloadType GUARDED_BY(_sendAudioCritsect);
  int8_t _cngFBPayloadType GUARDED_BY(_sendAudioCritsect);
  int8_t _lastPayloadType GUARDED_BY(_sendAudioCritsect);

  // Audio level indication
  // (https://datatracker.ietf.org/doc/draft-lennox-avt-rtp-audio-level-exthdr/)
  uint8_t _audioLevel_dBov GUARDED_BY(_sendAudioCritsect);
  OneTimeEvent first_packet_sent_;
};
}  // namespace webrtc

#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_SENDER_AUDIO_H_
