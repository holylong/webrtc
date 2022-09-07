/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_EXAMPLES_PEERCONNECTION_CLIENT_CONDUCTOR_H_
#define WEBRTC_EXAMPLES_PEERCONNECTION_CLIENT_CONDUCTOR_H_
#pragma once

#include <deque>
#include <map>
#include <set>
#include <string>

#include "webrtc/api/mediastreaminterface.h"
#include "webrtc/api/peerconnectioninterface.h"
#include "webrtc/examples/peerconnection/client/main_wnd.h"
#include "webrtc/examples/peerconnection/client/peer_connection_client.h"
#include "webrtc/api/datachannelinterface.h"
#include <mutex>

//#define HAVE_WEBRTC_VIDEO

namespace webrtc {
class VideoCaptureModule;
}  // namespace webrtc

namespace cricket {
class VideoRenderer;
}  // namespace cricket

class Conductor
  : public webrtc::PeerConnectionObserver,
                  public webrtc::DataChannelObserver,
    public webrtc::CreateSessionDescriptionObserver,
    public PeerConnectionClientObserver,
    public MainWndCallback {
 public:
  enum CallbackID {
    MEDIA_CHANNELS_INITIALIZED = 1,
    PEER_CONNECTION_CLOSED,
    SEND_MESSAGE_TO_PEER,
    NEW_STREAM_ADDED,
    STREAM_REMOVED,
  };

  Conductor(PeerConnectionClient* client, MainWindow* main_wnd);

  bool connection_active() const;


  virtual void Close();

 protected:
  ~Conductor();
  bool InitializePeerConnection();
  bool ReinitializePeerConnectionForLoopback();
  bool CreatePeerConnection(bool dtls);
  bool CreateDataChannel();
  
  void DeletePeerConnection();
  void EnsureStreamingUI();
  void AddStreams();
  cricket::VideoCapturer* OpenVideoCaptureDevice();

  //
  // PeerConnectionObserver implementation.
  //

  void OnSignalingChange(
      webrtc::PeerConnectionInterface::SignalingState new_state) override{};
  void OnAddStream(
      rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override;
  void OnRemoveStream(
      rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override;
  void OnDataChannel(
      rtc::scoped_refptr<webrtc::DataChannelInterface> channel) override {
      //用来接收datachannel消息
    channel->RegisterObserver(this);
  }
  void OnRenegotiationNeeded() override {}
  void OnIceConnectionChange(
      webrtc::PeerConnectionInterface::IceConnectionState new_state) override{};
  void OnIceGatheringChange(
      webrtc::PeerConnectionInterface::IceGatheringState new_state) override{};
  void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override;
  void OnIceConnectionReceivingChange(bool receiving) override {}

  //
  // PeerConnectionClientObserver implementation.
  //

  virtual void OnSignedIn();

  virtual void OnDisconnected();

  virtual void OnPeerConnected(int id, const std::string& name);

  virtual void OnPeerDisconnected(int id);

  virtual void OnMessageFromPeer(int peer_id, const std::string& message);

  virtual void OnMessageSent(int err);

  virtual bool SendDataChannelMessage(const webrtc::DataBuffer& buffer);

  virtual void OnServerConnectionFailure();

    virtual void OnStateChange() {
    main_wnd_->OnStateChange();
    
    }
  //  A data buffer was successfully received.
    virtual void OnMessage(const webrtc::DataBuffer& buffer){
      main_wnd_->OnMessage(buffer);
    };

  //
  // MainWndCallback implementation.
  //

  virtual void StartLogin(const std::string& server, int port);

  virtual void DisconnectFromServer();

  virtual void ConnectToPeer(int peer_id);

  virtual void DisconnectFromCurrentPeer();

  virtual void UIThreadCallback(int msg_id, void* data);

  // CreateSessionDescriptionObserver implementation.
  virtual void OnSuccess(webrtc::SessionDescriptionInterface* desc);
  virtual void OnFailure(const std::string& error);

  void Lock() { ::EnterCriticalSection(&buffer_lock_); }

  void Unlock() { ::LeaveCriticalSection(&buffer_lock_); }

template <typename T>
  class RtcAutoLock {
   public:
  explicit RtcAutoLock(T* obj) : obj_(obj) { obj_->Lock(); }
    ~RtcAutoLock() { obj_->Unlock(); }

   protected:
    T* obj_;
  };

 protected:
  // Send a message to the remote peer. 代码定位老是跟windows的SendMessage搞混，改个名字
  void SelfSendMessage(const std::string& json_object);

  int peer_id_;
  bool loopback_;
  //std::mutex locker_;
  CRITICAL_SECTION buffer_lock_;
  rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;
  //用来发送datachannel消息
  rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel_;
  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface>
      peer_connection_factory_;
  PeerConnectionClient* client_;
  MainWindow* main_wnd_;
  std::deque<std::string*> pending_messages_;
  std::map<std::string, rtc::scoped_refptr<webrtc::MediaStreamInterface> >
      active_streams_;
  std::string server_;

  unsigned short step_{0};
};

#endif  // WEBRTC_EXAMPLES_PEERCONNECTION_CLIENT_CONDUCTOR_H_
