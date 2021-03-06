/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCPeerConnection+Private.h"

#import "NSString+StdString.h"
#import "RTCConfiguration+Private.h"
#import "RTCDataChannel+Private.h"
#import "RTCIceCandidate+Private.h"
#import "RTCMediaConstraints+Private.h"
#import "RTCMediaStream+Private.h"
#import "RTCPeerConnectionFactory+Private.h"
#import "RTCRtpReceiver+Private.h"
#import "RTCRtpSender+Private.h"
#import "RTCSessionDescription+Private.h"
#import "RTCStatsReport+Private.h"
#import "WebRTC/RTCLogging.h"

#include <memory>

#include "webrtc/api/jsepicecandidate.h"
#include "webrtc/base/checks.h"

NSString * const kRTCPeerConnectionErrorDomain =
    @"org.webrtc.RTCPeerConnection";
int const kRTCPeerConnnectionSessionDescriptionError = -1;

namespace webrtc {

class CreateSessionDescriptionObserverAdapter
    : public CreateSessionDescriptionObserver {
 public:
  CreateSessionDescriptionObserverAdapter(
      void (^completionHandler)(RTCSessionDescription *sessionDescription,
                                NSError *error)) {
    completion_handler_ = completionHandler;
  }

  ~CreateSessionDescriptionObserverAdapter() {
    completion_handler_ = nil;
  }

  void OnSuccess(SessionDescriptionInterface *desc) override {
    RTC_DCHECK(completion_handler_);
    std::unique_ptr<webrtc::SessionDescriptionInterface> description =
        std::unique_ptr<webrtc::SessionDescriptionInterface>(desc);
    RTCSessionDescription* session =
        [[RTCSessionDescription alloc] initWithNativeDescription:
            description.get()];
    completion_handler_(session, nil);
    completion_handler_ = nil;
  }

  void OnFailure(const std::string& error) override {
    RTC_DCHECK(completion_handler_);
    NSString* str = [NSString stringForStdString:error];
    NSError* err =
        [NSError errorWithDomain:kRTCPeerConnectionErrorDomain
                            code:kRTCPeerConnnectionSessionDescriptionError
                        userInfo:@{ NSLocalizedDescriptionKey : str }];
    completion_handler_(nil, err);
    completion_handler_ = nil;
  }

 private:
  void (^completion_handler_)
      (RTCSessionDescription *sessionDescription, NSError *error);
};

class SetSessionDescriptionObserverAdapter :
    public SetSessionDescriptionObserver {
 public:
  SetSessionDescriptionObserverAdapter(void (^completionHandler)
      (NSError *error)) {
    completion_handler_ = completionHandler;
  }

  ~SetSessionDescriptionObserverAdapter() {
    completion_handler_ = nil;
  }

  void OnSuccess() override {
    RTC_DCHECK(completion_handler_);
    completion_handler_(nil);
    completion_handler_ = nil;
  }

  void OnFailure(const std::string& error) override {
    RTC_DCHECK(completion_handler_);
    NSString* str = [NSString stringForStdString:error];
    NSError* err =
        [NSError errorWithDomain:kRTCPeerConnectionErrorDomain
                            code:kRTCPeerConnnectionSessionDescriptionError
                        userInfo:@{ NSLocalizedDescriptionKey : str }];
    completion_handler_(err);
    completion_handler_ = nil;
  }

 private:
  void (^completion_handler_)(NSError *error);
};

PeerConnectionDelegateAdapter::PeerConnectionDelegateAdapter(
    RTCPeerConnection *peerConnection) {
  peer_connection_ = peerConnection;
}

PeerConnectionDelegateAdapter::~PeerConnectionDelegateAdapter() {
  peer_connection_ = nil;
}

void PeerConnectionDelegateAdapter::OnSignalingChange(
    PeerConnectionInterface::SignalingState new_state) {
  RTCSignalingState state =
      [[RTCPeerConnection class] signalingStateForNativeState:new_state];
  RTCPeerConnection *peer_connection = peer_connection_;
  [peer_connection.delegate peerConnection:peer_connection
                   didChangeSignalingState:state];
}

void PeerConnectionDelegateAdapter::OnAddStream(
    rtc::scoped_refptr<MediaStreamInterface> stream) {
  RTCMediaStream *mediaStream =
      [[RTCMediaStream alloc] initWithNativeMediaStream:stream];
  RTCPeerConnection *peer_connection = peer_connection_;
  [peer_connection.delegate peerConnection:peer_connection
                              didAddStream:mediaStream];
}

void PeerConnectionDelegateAdapter::OnRemoveStream(
    rtc::scoped_refptr<MediaStreamInterface> stream) {
  RTCMediaStream *mediaStream =
      [[RTCMediaStream alloc] initWithNativeMediaStream:stream];
  RTCPeerConnection *peer_connection = peer_connection_;
  [peer_connection.delegate peerConnection:peer_connection
                           didRemoveStream:mediaStream];
}

void PeerConnectionDelegateAdapter::OnDataChannel(
    rtc::scoped_refptr<DataChannelInterface> data_channel) {
  RTCDataChannel *dataChannel =
      [[RTCDataChannel alloc] initWithNativeDataChannel:data_channel];
  RTCPeerConnection *peer_connection = peer_connection_;
  [peer_connection.delegate peerConnection:peer_connection
                        didOpenDataChannel:dataChannel];
}

void PeerConnectionDelegateAdapter::OnRenegotiationNeeded() {
  RTCPeerConnection *peer_connection = peer_connection_;
  [peer_connection.delegate peerConnectionShouldNegotiate:peer_connection];
}

void PeerConnectionDelegateAdapter::OnIceConnectionChange(
    PeerConnectionInterface::IceConnectionState new_state) {
  RTCIceConnectionState state =
      [[RTCPeerConnection class] iceConnectionStateForNativeState:new_state];
  RTCPeerConnection *peer_connection = peer_connection_;
  [peer_connection.delegate peerConnection:peer_connection
               didChangeIceConnectionState:state];
}

void PeerConnectionDelegateAdapter::OnIceGatheringChange(
    PeerConnectionInterface::IceGatheringState new_state) {
  RTCIceGatheringState state =
      [[RTCPeerConnection class] iceGatheringStateForNativeState:new_state];
  RTCPeerConnection *peer_connection = peer_connection_;
  [peer_connection.delegate peerConnection:peer_connection
                didChangeIceGatheringState:state];
}

void PeerConnectionDelegateAdapter::OnIceCandidate(
    const IceCandidateInterface *candidate) {
  RTCIceCandidate *iceCandidate =
      [[RTCIceCandidate alloc] initWithNativeCandidate:candidate];
  RTCPeerConnection *peer_connection = peer_connection_;
  [peer_connection.delegate peerConnection:peer_connection
                   didGenerateIceCandidate:iceCandidate];
}

void PeerConnectionDelegateAdapter::OnIceCandidatesRemoved(
    const std::vector<cricket::Candidate>& candidates) {
  NSMutableArray* ice_candidates =
      [NSMutableArray arrayWithCapacity:candidates.size()];
  for (const auto& candidate : candidates) {
    std::unique_ptr<JsepIceCandidate> candidate_wrapper(
        new JsepIceCandidate(candidate.transport_name(), -1, candidate));
    RTCIceCandidate* ice_candidate = [[RTCIceCandidate alloc]
        initWithNativeCandidate:candidate_wrapper.get()];
    [ice_candidates addObject:ice_candidate];
  }
  RTCPeerConnection* peer_connection = peer_connection_;
  [peer_connection.delegate peerConnection:peer_connection
                    didRemoveIceCandidates:ice_candidates];
}

}  // namespace webrtc


@implementation RTCPeerConnection {
  NSMutableArray *_localStreams;
  std::unique_ptr<webrtc::PeerConnectionDelegateAdapter> _observer;
  rtc::scoped_refptr<webrtc::PeerConnectionInterface> _peerConnection;
}

@synthesize delegate = _delegate;

- (instancetype)initWithFactory:(RTCPeerConnectionFactory *)factory
                  configuration:(RTCConfiguration *)configuration
                    constraints:(RTCMediaConstraints *)constraints
                       delegate:(id<RTCPeerConnectionDelegate>)delegate {
  NSParameterAssert(factory);
  std::unique_ptr<webrtc::PeerConnectionInterface::RTCConfiguration> config(
      [configuration createNativeConfiguration]);
  if (!config) {
    return nil;
  }
  if (self = [super init]) {
    _observer.reset(new webrtc::PeerConnectionDelegateAdapter(self));
    std::unique_ptr<webrtc::MediaConstraints> nativeConstraints =
        constraints.nativeConstraints;
    _peerConnection =
        factory.nativeFactory->CreatePeerConnection(*config,
                                                    nativeConstraints.get(),
                                                    nullptr,
                                                    nullptr,
                                                    _observer.get());
    _localStreams = [[NSMutableArray alloc] init];
    _delegate = delegate;
  }
  return self;
}

- (NSArray *)localStreams {
  return [_localStreams copy];
}

- (RTCSessionDescription *)localDescription {
  const webrtc::SessionDescriptionInterface *description =
      _peerConnection->local_description();
  return description ?
      [[RTCSessionDescription alloc] initWithNativeDescription:description]
          : nil;
}

- (RTCSessionDescription *)remoteDescription {
  const webrtc::SessionDescriptionInterface *description =
      _peerConnection->remote_description();
  return description ?
      [[RTCSessionDescription alloc] initWithNativeDescription:description]
          : nil;
}

- (RTCSignalingState)signalingState {
  return [[self class]
      signalingStateForNativeState:_peerConnection->signaling_state()];
}

- (RTCIceConnectionState)iceConnectionState {
  return [[self class] iceConnectionStateForNativeState:
      _peerConnection->ice_connection_state()];
}

- (RTCIceGatheringState)iceGatheringState {
  return [[self class] iceGatheringStateForNativeState:
      _peerConnection->ice_gathering_state()];
}

- (BOOL)setConfiguration:(RTCConfiguration *)configuration {
  std::unique_ptr<webrtc::PeerConnectionInterface::RTCConfiguration> config(
      [configuration createNativeConfiguration]);
  if (!config) {
    return NO;
  }
  return _peerConnection->SetConfiguration(*config);
}

- (void)close {
  _peerConnection->Close();
}

- (void)addIceCandidate:(RTCIceCandidate *)candidate {
  std::unique_ptr<const webrtc::IceCandidateInterface> iceCandidate(
      candidate.nativeCandidate);
  _peerConnection->AddIceCandidate(iceCandidate.get());
}

- (void)removeIceCandidates:(NSArray<RTCIceCandidate *> *)iceCandidates {
  std::vector<cricket::Candidate> candidates;
  for (RTCIceCandidate *iceCandidate in iceCandidates) {
    std::unique_ptr<const webrtc::IceCandidateInterface> candidate(
        iceCandidate.nativeCandidate);
    if (candidate) {
      candidates.push_back(candidate->candidate());
      // Need to fill the transport name from the sdp_mid.
      candidates.back().set_transport_name(candidate->sdp_mid());
    }
  }
  if (!candidates.empty()) {
    _peerConnection->RemoveIceCandidates(candidates);
  }
}

- (void)addStream:(RTCMediaStream *)stream {
  if (!_peerConnection->AddStream(stream.nativeMediaStream)) {
    RTCLogError(@"Failed to add stream: %@", stream);
    return;
  }
  [_localStreams addObject:stream];
}

- (void)removeStream:(RTCMediaStream *)stream {
  _peerConnection->RemoveStream(stream.nativeMediaStream);
  [_localStreams removeObject:stream];
}

- (void)offerForConstraints:(RTCMediaConstraints *)constraints
          completionHandler:
    (void (^)(RTCSessionDescription *sessionDescription,
              NSError *error))completionHandler {
  rtc::scoped_refptr<webrtc::CreateSessionDescriptionObserverAdapter>
      observer(new rtc::RefCountedObject
          <webrtc::CreateSessionDescriptionObserverAdapter>(completionHandler));
  _peerConnection->CreateOffer(observer, constraints.nativeConstraints.get());
}

- (void)answerForConstraints:(RTCMediaConstraints *)constraints
           completionHandler:
    (void (^)(RTCSessionDescription *sessionDescription,
              NSError *error))completionHandler {
  rtc::scoped_refptr<webrtc::CreateSessionDescriptionObserverAdapter>
      observer(new rtc::RefCountedObject
          <webrtc::CreateSessionDescriptionObserverAdapter>(completionHandler));
  _peerConnection->CreateAnswer(observer, constraints.nativeConstraints.get());
}

- (void)setLocalDescription:(RTCSessionDescription *)sdp
          completionHandler:(void (^)(NSError *error))completionHandler {
  rtc::scoped_refptr<webrtc::SetSessionDescriptionObserverAdapter> observer(
      new rtc::RefCountedObject<webrtc::SetSessionDescriptionObserverAdapter>(
          completionHandler));
  _peerConnection->SetLocalDescription(observer, sdp.nativeDescription);
}

- (void)setRemoteDescription:(RTCSessionDescription *)sdp
           completionHandler:(void (^)(NSError *error))completionHandler {
  rtc::scoped_refptr<webrtc::SetSessionDescriptionObserverAdapter> observer(
      new rtc::RefCountedObject<webrtc::SetSessionDescriptionObserverAdapter>(
          completionHandler));
  _peerConnection->SetRemoteDescription(observer, sdp.nativeDescription);
}

- (RTCRtpSender *)senderWithKind:(NSString *)kind
                        streamId:(NSString *)streamId {
  std::string nativeKind = [NSString stdStringForString:kind];
  std::string nativeStreamId = [NSString stdStringForString:streamId];
  rtc::scoped_refptr<webrtc::RtpSenderInterface> nativeSender(
      _peerConnection->CreateSender(nativeKind, nativeStreamId));
  return nativeSender ?
      [[RTCRtpSender alloc] initWithNativeRtpSender:nativeSender]
      : nil;
}

- (NSArray<RTCRtpSender *> *)senders {
  std::vector<rtc::scoped_refptr<webrtc::RtpSenderInterface>> nativeSenders(
      _peerConnection->GetSenders());
  NSMutableArray *senders = [[NSMutableArray alloc] init];
  for (const auto &nativeSender : nativeSenders) {
    RTCRtpSender *sender =
        [[RTCRtpSender alloc] initWithNativeRtpSender:nativeSender];
    [senders addObject:sender];
  }
  return senders;
}

- (NSArray<RTCRtpReceiver *> *)receivers {
  std::vector<rtc::scoped_refptr<webrtc::RtpReceiverInterface>> nativeReceivers(
      _peerConnection->GetReceivers());
  NSMutableArray *receivers = [[NSMutableArray alloc] init];
  for (const auto &nativeReceiver : nativeReceivers) {
    RTCRtpReceiver *receiver =
        [[RTCRtpReceiver alloc] initWithNativeRtpReceiver:nativeReceiver];
    [receivers addObject:receiver];
  }
  return receivers;
}

#pragma mark - Private

+ (webrtc::PeerConnectionInterface::SignalingState)nativeSignalingStateForState:
    (RTCSignalingState)state {
  switch (state) {
    case RTCSignalingStateStable:
      return webrtc::PeerConnectionInterface::kStable;
    case RTCSignalingStateHaveLocalOffer:
      return webrtc::PeerConnectionInterface::kHaveLocalOffer;
    case RTCSignalingStateHaveLocalPrAnswer:
      return webrtc::PeerConnectionInterface::kHaveLocalPrAnswer;
    case RTCSignalingStateHaveRemoteOffer:
      return webrtc::PeerConnectionInterface::kHaveRemoteOffer;
    case RTCSignalingStateHaveRemotePrAnswer:
      return webrtc::PeerConnectionInterface::kHaveRemotePrAnswer;
    case RTCSignalingStateClosed:
      return webrtc::PeerConnectionInterface::kClosed;
  }
}

+ (RTCSignalingState)signalingStateForNativeState:
    (webrtc::PeerConnectionInterface::SignalingState)nativeState {
  switch (nativeState) {
    case webrtc::PeerConnectionInterface::kStable:
      return RTCSignalingStateStable;
    case webrtc::PeerConnectionInterface::kHaveLocalOffer:
      return RTCSignalingStateHaveLocalOffer;
    case webrtc::PeerConnectionInterface::kHaveLocalPrAnswer:
      return RTCSignalingStateHaveLocalPrAnswer;
    case webrtc::PeerConnectionInterface::kHaveRemoteOffer:
      return RTCSignalingStateHaveRemoteOffer;
    case webrtc::PeerConnectionInterface::kHaveRemotePrAnswer:
      return RTCSignalingStateHaveRemotePrAnswer;
    case webrtc::PeerConnectionInterface::kClosed:
      return RTCSignalingStateClosed;
  }
}

+ (NSString *)stringForSignalingState:(RTCSignalingState)state {
  switch (state) {
    case RTCSignalingStateStable:
      return @"STABLE";
    case RTCSignalingStateHaveLocalOffer:
      return @"HAVE_LOCAL_OFFER";
    case RTCSignalingStateHaveLocalPrAnswer:
      return @"HAVE_LOCAL_PRANSWER";
    case RTCSignalingStateHaveRemoteOffer:
      return @"HAVE_REMOTE_OFFER";
    case RTCSignalingStateHaveRemotePrAnswer:
      return @"HAVE_REMOTE_PRANSWER";
    case RTCSignalingStateClosed:
      return @"CLOSED";
  }
}

+ (webrtc::PeerConnectionInterface::IceConnectionState)
    nativeIceConnectionStateForState:(RTCIceConnectionState)state {
  switch (state) {
    case RTCIceConnectionStateNew:
      return webrtc::PeerConnectionInterface::kIceConnectionNew;
    case RTCIceConnectionStateChecking:
      return webrtc::PeerConnectionInterface::kIceConnectionChecking;
    case RTCIceConnectionStateConnected:
      return webrtc::PeerConnectionInterface::kIceConnectionConnected;
    case RTCIceConnectionStateCompleted:
      return webrtc::PeerConnectionInterface::kIceConnectionCompleted;
    case RTCIceConnectionStateFailed:
      return webrtc::PeerConnectionInterface::kIceConnectionFailed;
    case RTCIceConnectionStateDisconnected:
      return webrtc::PeerConnectionInterface::kIceConnectionDisconnected;
    case RTCIceConnectionStateClosed:
      return webrtc::PeerConnectionInterface::kIceConnectionClosed;
    case RTCIceConnectionStateCount:
      return webrtc::PeerConnectionInterface::kIceConnectionMax;
  }
}

+ (RTCIceConnectionState)iceConnectionStateForNativeState:
    (webrtc::PeerConnectionInterface::IceConnectionState)nativeState {
  switch (nativeState) {
    case webrtc::PeerConnectionInterface::kIceConnectionNew:
      return RTCIceConnectionStateNew;
    case webrtc::PeerConnectionInterface::kIceConnectionChecking:
      return RTCIceConnectionStateChecking;
    case webrtc::PeerConnectionInterface::kIceConnectionConnected:
      return RTCIceConnectionStateConnected;
    case webrtc::PeerConnectionInterface::kIceConnectionCompleted:
      return RTCIceConnectionStateCompleted;
    case webrtc::PeerConnectionInterface::kIceConnectionFailed:
      return RTCIceConnectionStateFailed;
    case webrtc::PeerConnectionInterface::kIceConnectionDisconnected:
      return RTCIceConnectionStateDisconnected;
    case webrtc::PeerConnectionInterface::kIceConnectionClosed:
      return RTCIceConnectionStateClosed;
    case webrtc::PeerConnectionInterface::kIceConnectionMax:
      return RTCIceConnectionStateCount;
  }
}

+ (NSString *)stringForIceConnectionState:(RTCIceConnectionState)state {
  switch (state) {
    case RTCIceConnectionStateNew:
      return @"NEW";
    case RTCIceConnectionStateChecking:
      return @"CHECKING";
    case RTCIceConnectionStateConnected:
      return @"CONNECTED";
    case RTCIceConnectionStateCompleted:
      return @"COMPLETED";
    case RTCIceConnectionStateFailed:
      return @"FAILED";
    case RTCIceConnectionStateDisconnected:
      return @"DISCONNECTED";
    case RTCIceConnectionStateClosed:
      return @"CLOSED";
    case RTCIceConnectionStateCount:
      return @"COUNT";
  }
}

+ (webrtc::PeerConnectionInterface::IceGatheringState)
    nativeIceGatheringStateForState:(RTCIceGatheringState)state {
  switch (state) {
    case RTCIceGatheringStateNew:
      return webrtc::PeerConnectionInterface::kIceGatheringNew;
    case RTCIceGatheringStateGathering:
      return webrtc::PeerConnectionInterface::kIceGatheringGathering;
    case RTCIceGatheringStateComplete:
      return webrtc::PeerConnectionInterface::kIceGatheringComplete;
  }
}

+ (RTCIceGatheringState)iceGatheringStateForNativeState:
    (webrtc::PeerConnectionInterface::IceGatheringState)nativeState {
  switch (nativeState) {
    case webrtc::PeerConnectionInterface::kIceGatheringNew:
      return RTCIceGatheringStateNew;
    case webrtc::PeerConnectionInterface::kIceGatheringGathering:
      return RTCIceGatheringStateGathering;
    case webrtc::PeerConnectionInterface::kIceGatheringComplete:
      return RTCIceGatheringStateComplete;
  }
}

+ (NSString *)stringForIceGatheringState:(RTCIceGatheringState)state {
  switch (state) {
    case RTCIceGatheringStateNew:
      return @"NEW";
    case RTCIceGatheringStateGathering:
      return @"GATHERING";
    case RTCIceGatheringStateComplete:
      return @"COMPLETE";
  }
}

+ (webrtc::PeerConnectionInterface::StatsOutputLevel)
    nativeStatsOutputLevelForLevel:(RTCStatsOutputLevel)level {
  switch (level) {
    case RTCStatsOutputLevelStandard:
      return webrtc::PeerConnectionInterface::kStatsOutputLevelStandard;
    case RTCStatsOutputLevelDebug:
      return webrtc::PeerConnectionInterface::kStatsOutputLevelDebug;
  }
}

- (rtc::scoped_refptr<webrtc::PeerConnectionInterface>)nativePeerConnection {
  return _peerConnection;
}

@end
