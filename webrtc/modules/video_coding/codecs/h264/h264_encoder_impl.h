/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_CODECS_H264_H264_ENCODER_IMPL_H_
#define WEBRTC_MODULES_VIDEO_CODING_CODECS_H264_H264_ENCODER_IMPL_H_

#include <memory>
#include <vector>

#include "webrtc/modules/video_coding/codecs/h264/include/h264.h"
#include "webrtc/modules/video_coding/utility/h264_bitstream_parser.h"
#include "webrtc/modules/video_coding/utility/quality_scaler.h"

#include "third_party/openh264/src/codec/api/svc/codec_app_def.h"

class ISVCEncoder;

namespace webrtc {

class H264EncoderImpl : public H264Encoder {
 public:
  H264EncoderImpl();
  ~H264EncoderImpl() override;

  // |max_payload_size| is ignored.
  // The following members of |codec_settings| are used. The rest are ignored.
  // - codecType (must be kVideoCodecH264)
  // - targetBitrate
  // - maxFramerate
  // - width
  // - height
  int32_t InitEncode(const VideoCodec* codec_settings,
                     int32_t number_of_cores,
                     size_t /*max_payload_size*/) override;
  int32_t Release() override;

  int32_t RegisterEncodeCompleteCallback(
      EncodedImageCallback* callback) override;
  int32_t SetRates(uint32_t bitrate, uint32_t framerate) override;

  // The result of encoding - an EncodedImage and RTPFragmentationHeader - are
  // passed to the encode complete callback.
  int32_t Encode(const VideoFrame& frame,
                 const CodecSpecificInfo* codec_specific_info,
                 const std::vector<FrameType>* frame_types) override;

  // Unsupported / Do nothing.
  int32_t SetChannelParameters(uint32_t packet_loss, int64_t rtt) override;
  int32_t SetPeriodicKeyFrames(bool enable) override;
  void OnDroppedFrame() override;

 private:
  bool IsInitialized() const;
  SEncParamExt CreateEncoderParams() const;

  webrtc::H264BitstreamParser h264_bitstream_parser_;
  QualityScaler quality_scaler_;
  // Reports statistics with histograms.
  void ReportInit();
  void ReportError();

  ISVCEncoder* openh264_encoder_;
  VideoCodec codec_settings_;
  int32_t number_of_cores_;

  EncodedImage encoded_image_;
  std::unique_ptr<uint8_t[]> encoded_image_buffer_;
  EncodedImageCallback* encoded_image_callback_;

  bool has_reported_init_;
  bool has_reported_error_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_CODECS_H264_H264_ENCODER_IMPL_H_
