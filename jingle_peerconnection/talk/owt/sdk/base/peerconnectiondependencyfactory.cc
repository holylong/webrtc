// Copyright (C) <2018> Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0
#include <iostream>
#include "talk/owt/sdk/base/customizedaudiodevicemodule.h"
#include "talk/owt/sdk/base/encodedvideoencoderfactory.h"
#include "talk/owt/sdk/base/peerconnectiondependencyfactory.h"
#include "webrtc/api/audio_codecs/builtin_audio_decoder_factory.h"
#include "webrtc/api/audio_codecs/builtin_audio_encoder_factory.h"
#include "webrtc/base/bind.h"
#include "webrtc/base/ssladapter.h"
#include "webrtc/base/thread.h"
#include "webrtc/media/base/mediachannel.h"
#include "webrtc/media/engine/webrtcvideodecoderfactory.h"
#include "webrtc/media/engine/webrtcvideoencoderfactory.h"
#include "webrtc/modules/audio_processing/include/audio_processing.h"
#include "webrtc/system_wrappers/include/field_trial_default.h"
#if defined(WEBRTC_WIN)
#include "talk/owt/sdk/base/win/msdkvideodecoderfactory.h"
#include "talk/owt/sdk/base/win/msdkvideoencoderfactory.h"
#elif defined(WEBRTC_IOS)
#include "talk/owt/sdk/base/ios/networkmonitorios.h"
#include "talk/owt/sdk/base/objc/ObjcVideoCodecFactory.h"
#endif
#if defined(WEBRTC_LINUX) || defined(WEBRTC_WIN)
#include "talk/owt/sdk/base/customizedvideodecoderfactory.h"
#endif
#include "owt/base/clientconfiguration.h"
#include "owt/base/globalconfiguration.h"
using namespace rtc;
namespace owt {
namespace base {
void PeerConnectionThread::Run() {
  ProcessMessages(kForever);
  SetAllowBlockingCalls(true);
}
PeerConnectionThread::~PeerConnectionThread() {
  LOG(LS_INFO) << "Quit a PeerConnectionThread.";
  Stop();
}
rtc::scoped_refptr<PeerConnectionDependencyFactory>
    PeerConnectionDependencyFactory::dependency_factory_;
std::mutex PeerConnectionDependencyFactory::get_pc_dependency_factory_mutex_;
PeerConnectionDependencyFactory::PeerConnectionDependencyFactory()
    : pc_thread_(new PeerConnectionThread),
      callback_thread_(new PeerConnectionThread),
      field_trial_("WebRTC-H264HighProfile/Enabled/") {
#if defined(WEBRTC_WIN)
  if (GlobalConfiguration::GetVideoHardwareAccelerationEnabled()) {
    render_hardware_acceleration_enabled_ = true;
    } else {
        render_hardware_acceleration_enabled_ = false;
  }
#endif
#if defined(WEBRTC_IOS)
  network_monitor_ = nullptr;
#endif
  encoded_frame_ = GlobalConfiguration::GetEncodedVideoFrameEnabled();
  pc_thread_->Start();
}
PeerConnectionDependencyFactory::~PeerConnectionDependencyFactory() {}
rtc::scoped_refptr<webrtc::PeerConnectionInterface>
PeerConnectionDependencyFactory::CreatePeerConnection(
    const webrtc::PeerConnectionInterface::RTCConfiguration& config,
    webrtc::PeerConnectionObserver* observer) {
  return pc_thread_
      ->Invoke<scoped_refptr<webrtc::PeerConnectionInterface>>(
          RTC_FROM_HERE, Bind(&PeerConnectionDependencyFactory::
                                  CreatePeerConnectionOnCurrentThread,
                              this, config, observer))
      .get();
}
PeerConnectionDependencyFactory* PeerConnectionDependencyFactory::Get() {
  std::lock_guard<std::mutex> lock(get_pc_dependency_factory_mutex_);
  if (!dependency_factory_.get()) {
    dependency_factory_ =
        new rtc::RefCountedObject<PeerConnectionDependencyFactory>();
    dependency_factory_->CreatePeerConnectionFactory();
  }
  return dependency_factory_.get();
}
const scoped_refptr<PeerConnectionFactoryInterface>&
PeerConnectionDependencyFactory::GetPeerConnectionFactory() {
  if (!pc_factory_.get())
    CreatePeerConnectionFactory();
  RTC_CHECK(pc_factory_.get());
  return pc_factory_;
}
void PeerConnectionDependencyFactory::
    CreatePeerConnectionFactoryOnCurrentThread() {
  LOG(LS_INFO) << "CreatePeerConnectionOnCurrentThread";
  if (GlobalConfiguration::GetAECEnabled()
      && GlobalConfiguration::GetAEC3Enabled()) {
    field_trial_ += "OWT-EchoCanceller3/Enabled/";
  }
  webrtc::field_trial::InitFieldTrialsFromString(field_trial_.c_str());
  if (!rtc::InitializeSSL()) {
    LOG(LS_ERROR) << "Failed to initialize SSL.";
    RTC_NOTREACHED();
    return;
  }
  rtc::Thread* worker_thread = new rtc::Thread();
  worker_thread->SetName("worker_thread", nullptr);
  rtc::Thread* signaling_thread = new rtc::Thread();
  signaling_thread->SetName("signaling_thread", nullptr);
  rtc::Thread* network_thread = new rtc::Thread();
  network_thread->SetName("network_thread", nullptr);
  RTC_CHECK(worker_thread->Start() && signaling_thread->Start() &&
            network_thread->Start())
      << "Failed to start threads";
#if defined(WEBRTC_IOS)
  // Use webrtc::VideoEn(De)coderFactory on iOS.
  std::unique_ptr<VideoEncoderFactory> encoder_factory;
  std::unique_ptr<VideoDecoderFactory> decoder_factory;
  encoder_factory = ObjcVideoCodecFactory::CreateObjcVideoEncoderFactory();
  decoder_factory = ObjcVideoCodecFactory::CreateObjcVideoDecoderFactory();
#elif defined(WEBRTC_WIN) || defined(WEBRTC_LINUX)
  std::unique_ptr<cricket::WebRtcVideoEncoderFactory> encoder_factory;
  std::unique_ptr<cricket::WebRtcVideoDecoderFactory> decoder_factory;
#if defined(WEBRTC_WIN)
  if (render_hardware_acceleration_enabled_) {
    if (!encoded_frame_) {
      encoder_factory.reset(new MSDKVideoEncoderFactory());
    }
    if (!GlobalConfiguration::GetCustomizedVideoDecoderEnabled()) {
      decoder_factory.reset(new MSDKVideoDecoderFactory());
    }
  }
#endif
  if (GlobalConfiguration::GetCustomizedVideoDecoderEnabled()) {
    decoder_factory.reset(new CustomizedVideoDecoderFactory(
        GlobalConfiguration::GetCustomizedVideoDecoder()));
  }
  // Encoded video frame enabled
  if (encoded_frame_) {
    encoder_factory.reset(new EncodedVideoEncoderFactory());
  }
#else
#error "Unsupported platform."
#endif
  // Raw audio frame
  // if adm is nullptr, voe_base will initilize it with the default internal
  // adm.
  rtc::scoped_refptr<AudioDeviceModule> adm;
  if (GlobalConfiguration::GetCustomizedAudioInputEnabled()) {
    // Create ADM on worker thred as RegisterAudioCallback is invoked there.
    adm = worker_thread->Invoke<rtc::scoped_refptr<AudioDeviceModule>>(
               RTC_FROM_HERE,
               Bind(&PeerConnectionDependencyFactory::
                    CreateCustomizedAudioDeviceModuleOnCurrentThread,
                    this));
  }
#if defined(WEBRTC_IOS)
  pc_factory_ = webrtc::CreatePeerConnectionFactory(
      network_thread, worker_thread, signaling_thread, adm,
      webrtc::CreateBuiltinAudioEncoderFactory(),
      webrtc::CreateBuiltinAudioDecoderFactory(), std::move(encoder_factory),
      std::move(decoder_factory), nullptr,
      nullptr);  // Decoder factory
#elif defined(WEBRTC_WIN) || defined(WEBRTC_LINUX)
  pc_factory_ = webrtc::CreatePeerConnectionFactory(
      network_thread, worker_thread, signaling_thread, adm,
      webrtc::CreateBuiltinAudioEncoderFactory(),
      webrtc::CreateBuiltinAudioDecoderFactory(),
      encoder_factory.release(),   // Encoder factory
      decoder_factory.release());  // Decoder factory
#else
#error "Unsupported platform."
#endif
  LOG(LS_INFO) << "CreatePeerConnectionOnCurrentThread finished.";
}
scoped_refptr<webrtc::PeerConnectionInterface>
PeerConnectionDependencyFactory::CreatePeerConnectionOnCurrentThread(
    const webrtc::PeerConnectionInterface::RTCConfiguration& config,
    webrtc::PeerConnectionObserver* observer) {
  return (pc_factory_->CreatePeerConnection(config, nullptr,
                                            nullptr, observer))
      .get();
}
void PeerConnectionDependencyFactory::CreatePeerConnectionFactory() {
  RTC_CHECK(!pc_factory_.get());
  LOG(LS_INFO)
      << "PeerConnectionDependencyFactory::CreatePeerConnectionFactory()";
  RTC_CHECK(pc_thread_);
  pc_thread_->Invoke<void>(RTC_FROM_HERE,
                           Bind(&PeerConnectionDependencyFactory::
                                    CreatePeerConnectionFactoryOnCurrentThread,
                                this));
  RTC_CHECK(pc_factory_.get());
}
scoped_refptr<webrtc::MediaStreamInterface>
PeerConnectionDependencyFactory::CreateLocalMediaStream(
    const std::string& label) {
  RTC_CHECK(pc_thread_);
  return pc_thread_->Invoke<scoped_refptr<webrtc::MediaStreamInterface>>(
      RTC_FROM_HERE,
      Bind(&PeerConnectionFactoryInterface::CreateLocalMediaStream,
           pc_factory_.get(), label));
}
scoped_refptr<webrtc::VideoTrackSourceInterface>
PeerConnectionDependencyFactory::CreateVideoSource(
    std::unique_ptr<cricket::VideoCapturer> capturer,
    const MediaConstraintsInterface* constraints) {
  return pc_thread_
      ->Invoke<scoped_refptr<webrtc::VideoTrackSourceInterface>>(
          RTC_FROM_HERE,
          Bind((rtc::scoped_refptr<VideoTrackSourceInterface>(
                   PeerConnectionFactoryInterface::*)(
                   cricket::VideoCapturer*,
                   const MediaConstraintsInterface*)) &
                   PeerConnectionFactoryInterface::CreateVideoSource,
               pc_factory_.get(), capturer.release(), constraints))
      .get();
}
scoped_refptr<VideoTrackInterface>
PeerConnectionDependencyFactory::CreateLocalVideoTrack(
    const std::string& id,
    webrtc::VideoTrackSourceInterface* video_source) {
  return pc_thread_->Invoke<scoped_refptr<VideoTrackInterface>>(RTC_FROM_HERE,
                       Bind(&PeerConnectionFactoryInterface::CreateVideoTrack,
                            pc_factory_.get(), id, video_source))
      .get();
}
scoped_refptr<AudioTrackInterface>
PeerConnectionDependencyFactory::CreateLocalAudioTrack(const std::string& id) {
  bool aec_enabled, agc_enabled, ns_enabled;
  aec_enabled = GlobalConfiguration::GetAECEnabled();
  agc_enabled = GlobalConfiguration::GetAGCEnabled();
  ns_enabled = GlobalConfiguration::GetNSEnabled();
  if (!aec_enabled || !agc_enabled || !ns_enabled) {
    cricket::AudioOptions options;
    options.echo_cancellation = absl::optional<bool>(aec_enabled ? true : false);
    options.auto_gain_control = absl::optional<bool>(agc_enabled ? true : false);
    options.noise_suppression = absl::optional<bool>(ns_enabled ? true : false);
    options.residual_echo_detector =
        absl::optional<bool>(aec_enabled ? true : false);
    scoped_refptr<webrtc::AudioSourceInterface> audio_source =
        CreateAudioSource(options);
    return pc_thread_
        ->Invoke<scoped_refptr<AudioTrackInterface>>(
            RTC_FROM_HERE,
            Bind(&PeerConnectionFactoryInterface::CreateAudioTrack,
                 pc_factory_.get(), id, audio_source.get()))
        .get();
  } else {
    return pc_thread_
        ->Invoke<scoped_refptr<AudioTrackInterface>>(
            RTC_FROM_HERE,
            Bind(&PeerConnectionFactoryInterface::CreateAudioTrack,
                 pc_factory_.get(), id, nullptr))
        .get();
  }
}
scoped_refptr<AudioTrackInterface>
PeerConnectionDependencyFactory::CreateLocalAudioTrack(
    const std::string& id,
    webrtc::AudioSourceInterface* audio_source) {
  return pc_thread_
      ->Invoke<scoped_refptr<AudioTrackInterface>>(
          RTC_FROM_HERE, Bind(&PeerConnectionFactoryInterface::CreateAudioTrack,
                              pc_factory_.get(), id, audio_source))
      .get();
}
rtc::scoped_refptr<AudioSourceInterface>
PeerConnectionDependencyFactory::CreateAudioSource(
    const cricket::AudioOptions& options) {
  return pc_thread_
      ->Invoke<scoped_refptr<webrtc::AudioSourceInterface>>(
          RTC_FROM_HERE,
          Bind((rtc::scoped_refptr<AudioSourceInterface>(
                   PeerConnectionFactoryInterface::*)(
                   const cricket::AudioOptions&)) &
                   PeerConnectionFactoryInterface::CreateAudioSource,
               pc_factory_.get(), options))
      .get();
}
rtc::scoped_refptr<PeerConnectionFactoryInterface>
PeerConnectionDependencyFactory::PeerConnectionFactory() const {
  return pc_factory_;
}
rtc::NetworkMonitorInterface* PeerConnectionDependencyFactory::NetworkMonitor(){
#if defined(WEBRTC_IOS)
  pc_thread_->Invoke<void>(
      RTC_FROM_HERE,
      Bind(
          &PeerConnectionDependencyFactory::CreateNetworkMonitorOnCurrentThread,
          this));
  return network_monitor_;
#else
  return nullptr;
#endif
}
void PeerConnectionDependencyFactory::CreateNetworkMonitorOnCurrentThread() {
#if defined(WEBRTC_IOS)
  if (!network_monitor_) {
     network_monitor_ = new NetworkMonitorIos();
     network_monitor_->Start();
  }
#endif
}
scoped_refptr<webrtc::AudioDeviceModule>
PeerConnectionDependencyFactory::CreateCustomizedAudioDeviceModuleOnCurrentThread() {
  return CustomizedAudioDeviceModule::Create(
     GlobalConfiguration::GetAudioFrameGenerator());
}
}
}
