/*
 *  Copyright (c) 2010 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MEDIA_ENGINE_FAKEWEBRTCVIDEOENGINE_H_
#define WEBRTC_MEDIA_ENGINE_FAKEWEBRTCVIDEOENGINE_H_

#include <map>
#include <set>
#include <vector>

#include "webrtc/base/basictypes.h"
#include "webrtc/base/criticalsection.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/stringutils.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/media/base/codec.h"
#include "webrtc/media/engine/webrtcvideodecoderfactory.h"
#include "webrtc/media/engine/webrtcvideoencoderfactory.h"
#include "webrtc/modules/video_coding/include/video_error_codes.h"
#include "webrtc/video_decoder.h"
#include "webrtc/video_encoder.h"

namespace cricket {

static const int kMinVideoBitrate = 100;
static const int kStartVideoBitrate = 300;
static const int kMaxVideoBitrate = 1000;

// WebRtc channel id and capture id share the same number space.
// This is how AddRenderer(renderId, ...) is able to tell if it is adding a
// renderer for a channel or it is adding a renderer for a capturer.
static const int kViEChannelIdBase = 0;
static const int kViEChannelIdMax = 1000;
static const int kEventTimeoutMs = 10000;

// Fake class for mocking out webrtc::VideoDecoder
class FakeWebRtcVideoDecoder : public webrtc::VideoDecoder {
 public:
  FakeWebRtcVideoDecoder() : num_frames_received_(0) {}

  virtual int32_t InitDecode(const webrtc::VideoCodec*, int32_t) {
    return WEBRTC_VIDEO_CODEC_OK;
  }

  virtual int32_t Decode(const webrtc::EncodedImage&,
                         bool,
                         const webrtc::RTPFragmentationHeader*,
                         const webrtc::CodecSpecificInfo*,
                         int64_t) {
    num_frames_received_++;
    return WEBRTC_VIDEO_CODEC_OK;
  }

  virtual int32_t RegisterDecodeCompleteCallback(
      webrtc::DecodedImageCallback*) {
    return WEBRTC_VIDEO_CODEC_OK;
  }

  virtual int32_t Release() { return WEBRTC_VIDEO_CODEC_OK; }

  int GetNumFramesReceived() const {
    return num_frames_received_;
  }

 private:
  int num_frames_received_;
};

// Fake class for mocking out WebRtcVideoDecoderFactory.
class FakeWebRtcVideoDecoderFactory : public WebRtcVideoDecoderFactory {
 public:
  FakeWebRtcVideoDecoderFactory()
      : num_created_decoders_(0) {
  }

  virtual webrtc::VideoDecoder* CreateVideoDecoder(
      webrtc::VideoCodecType type) {
    if (supported_codec_types_.count(type) == 0) {
      return NULL;
    }
    FakeWebRtcVideoDecoder* decoder = new FakeWebRtcVideoDecoder();
    decoders_.push_back(decoder);
    num_created_decoders_++;
    return decoder;
  }

  virtual void DestroyVideoDecoder(webrtc::VideoDecoder* decoder) {
    decoders_.erase(
        std::remove(decoders_.begin(), decoders_.end(), decoder),
        decoders_.end());
    delete decoder;
  }

  void AddSupportedVideoCodecType(webrtc::VideoCodecType type) {
    supported_codec_types_.insert(type);
  }

  int GetNumCreatedDecoders() {
    return num_created_decoders_;
  }

  const std::vector<FakeWebRtcVideoDecoder*>& decoders() {
    return decoders_;
  }

 private:
  std::set<webrtc::VideoCodecType> supported_codec_types_;
  std::vector<FakeWebRtcVideoDecoder*> decoders_;
  int num_created_decoders_;
};

// Fake class for mocking out webrtc::VideoEnoder
class FakeWebRtcVideoEncoder : public webrtc::VideoEncoder {
 public:
  FakeWebRtcVideoEncoder()
      : init_encode_event_(false, false), num_frames_encoded_(0) {}

  virtual int32_t InitEncode(const webrtc::VideoCodec* codecSettings,
                             int32_t numberOfCores,
                             size_t maxPayloadSize) {
    rtc::CritScope lock(&crit_);
    codec_settings_ = *codecSettings;
    init_encode_event_.Set();
    return WEBRTC_VIDEO_CODEC_OK;
  }

  bool WaitForInitEncode() { return init_encode_event_.Wait(kEventTimeoutMs); }

  webrtc::VideoCodec GetCodecSettings() {
    rtc::CritScope lock(&crit_);
    return codec_settings_;
  }

  virtual int32_t Encode(const webrtc::VideoFrame& inputImage,
                         const webrtc::CodecSpecificInfo* codecSpecificInfo,
                         const std::vector<webrtc::FrameType>* frame_types) {
    rtc::CritScope lock(&crit_);
    ++num_frames_encoded_;
    init_encode_event_.Set();
    return WEBRTC_VIDEO_CODEC_OK;
  }

  virtual int32_t RegisterEncodeCompleteCallback(
      webrtc::EncodedImageCallback* callback) {
    return WEBRTC_VIDEO_CODEC_OK;
  }

  virtual int32_t Release() { return WEBRTC_VIDEO_CODEC_OK; }

  virtual int32_t SetChannelParameters(uint32_t packetLoss, int64_t rtt) {
    return WEBRTC_VIDEO_CODEC_OK;
  }

  virtual int32_t SetRates(uint32_t newBitRate, uint32_t frameRate) {
    return WEBRTC_VIDEO_CODEC_OK;
  }

  int GetNumEncodedFrames() {
    rtc::CritScope lock(&crit_);
    return num_frames_encoded_;
  }

 private:
  rtc::CriticalSection crit_;
  rtc::Event init_encode_event_;
  int num_frames_encoded_ GUARDED_BY(crit_);
  webrtc::VideoCodec codec_settings_ GUARDED_BY(crit_);
};

// Fake class for mocking out WebRtcVideoEncoderFactory.
class FakeWebRtcVideoEncoderFactory : public WebRtcVideoEncoderFactory {
 public:
  FakeWebRtcVideoEncoderFactory()
      : created_video_encoder_event_(false, false),
        num_created_encoders_(0),
        encoders_have_internal_sources_(false) {}

  webrtc::VideoEncoder* CreateVideoEncoder(
      webrtc::VideoCodecType type) override {
    rtc::CritScope lock(&crit_);
    if (supported_codec_types_.count(type) == 0) {
      return NULL;
    }
    FakeWebRtcVideoEncoder* encoder = new FakeWebRtcVideoEncoder();
    encoders_.push_back(encoder);
    num_created_encoders_++;
    created_video_encoder_event_.Set();
    return encoder;
  }

  bool WaitForCreatedVideoEncoders(int num_encoders) {
    while (created_video_encoder_event_.Wait(kEventTimeoutMs)) {
      if (GetNumCreatedEncoders() >= num_encoders)
        return true;
    }
    return false;
  }

  void DestroyVideoEncoder(webrtc::VideoEncoder* encoder) override {
    rtc::CritScope lock(&crit_);
    encoders_.erase(
        std::remove(encoders_.begin(), encoders_.end(), encoder),
        encoders_.end());
    delete encoder;
  }

  const std::vector<WebRtcVideoEncoderFactory::VideoCodec>& codecs()
      const override {
    return codecs_;
  }

  bool EncoderTypeHasInternalSource(
      webrtc::VideoCodecType type) const override {
    return encoders_have_internal_sources_;
  }

  void set_encoders_have_internal_sources(bool internal_source) {
    encoders_have_internal_sources_ = internal_source;
  }

  void AddSupportedVideoCodecType(webrtc::VideoCodecType type,
                                  const std::string& name) {
    supported_codec_types_.insert(type);
    codecs_.push_back(
        WebRtcVideoEncoderFactory::VideoCodec(type, name, 1280, 720, 30));
  }

  int GetNumCreatedEncoders() {
    rtc::CritScope lock(&crit_);
    return num_created_encoders_;
  }

  const std::vector<FakeWebRtcVideoEncoder*> encoders() {
    rtc::CritScope lock(&crit_);
    return encoders_;
  }

 private:
  rtc::CriticalSection crit_;
  rtc::Event created_video_encoder_event_;
  std::set<webrtc::VideoCodecType> supported_codec_types_;
  std::vector<WebRtcVideoEncoderFactory::VideoCodec> codecs_;
  std::vector<FakeWebRtcVideoEncoder*> encoders_ GUARDED_BY(crit_);
  int num_created_encoders_ GUARDED_BY(crit_);
  bool encoders_have_internal_sources_;
};

}  // namespace cricket

#endif  // WEBRTC_MEDIA_ENGINE_FAKEWEBRTCVIDEOENGINE_H_
