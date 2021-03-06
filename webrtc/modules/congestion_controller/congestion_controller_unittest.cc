/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/modules/pacing/mock/mock_paced_sender.h"
#include "webrtc/modules/congestion_controller/include/congestion_controller.h"
#include "webrtc/modules/congestion_controller/include/mock/mock_congestion_controller.h"
#include "webrtc/modules/remote_bitrate_estimator/include/mock/mock_remote_bitrate_observer.h"
#include "webrtc/system_wrappers/include/clock.h"

using testing::_;
using testing::NiceMock;
using testing::Return;
using testing::SaveArg;
using testing::StrictMock;

namespace webrtc {
namespace test {

class CongestionControllerTest : public ::testing::Test {
 protected:
  CongestionControllerTest() : clock_(123456) {}
  ~CongestionControllerTest() override {}

  void SetUp() override {
    pacer_ = new NiceMock<MockPacedSender>();
    std::unique_ptr<PacedSender> pacer(pacer_);  // Passes ownership.
    std::unique_ptr<PacketRouter> packet_router(new PacketRouter());
    controller_.reset(
        new CongestionController(&clock_, &observer_, &remote_bitrate_observer_,
                                 std::move(packet_router), std::move(pacer)));
    bandwidth_observer_.reset(
        controller_->GetBitrateController()->CreateRtcpBandwidthObserver());

    // Set the initial bitrate estimate and expect the |observer| and |pacer_|
    // to be updated.
    EXPECT_CALL(observer_, OnNetworkChanged(kInitialBitrateBps, _, _));
    EXPECT_CALL(*pacer_, SetEstimatedBitrate(kInitialBitrateBps));
    controller_->SetBweBitrates(0, kInitialBitrateBps, 5 * kInitialBitrateBps);
  }

  SimulatedClock clock_;
  StrictMock<MockCongestionObserver> observer_;
  NiceMock<MockPacedSender>* pacer_;
  NiceMock<MockRemoteBitrateObserver> remote_bitrate_observer_;
  std::unique_ptr<RtcpBandwidthObserver> bandwidth_observer_;
  std::unique_ptr<CongestionController> controller_;
  const uint32_t kInitialBitrateBps = 60000;
};

TEST_F(CongestionControllerTest, OnNetworkChanged) {
  // Test no change.
  clock_.AdvanceTimeMilliseconds(25);
  controller_->Process();

  EXPECT_CALL(observer_, OnNetworkChanged(kInitialBitrateBps * 2, _, _));
  EXPECT_CALL(*pacer_, SetEstimatedBitrate(kInitialBitrateBps * 2));
  bandwidth_observer_->OnReceivedEstimatedBitrate(kInitialBitrateBps * 2);
  clock_.AdvanceTimeMilliseconds(25);
  controller_->Process();

  EXPECT_CALL(observer_, OnNetworkChanged(kInitialBitrateBps, _, _));
  EXPECT_CALL(*pacer_, SetEstimatedBitrate(kInitialBitrateBps));
  bandwidth_observer_->OnReceivedEstimatedBitrate(kInitialBitrateBps);
  clock_.AdvanceTimeMilliseconds(25);
  controller_->Process();
}

TEST_F(CongestionControllerTest, OnSendQueueFull) {
  EXPECT_CALL(*pacer_, ExpectedQueueTimeMs())
      .WillOnce(Return(PacedSender::kMaxQueueLengthMs + 1));

  EXPECT_CALL(observer_, OnNetworkChanged(0, _, _));
  controller_->Process();

  // Let the pacer not be full next time the controller checks.
  EXPECT_CALL(*pacer_, ExpectedQueueTimeMs())
      .WillOnce(Return(PacedSender::kMaxQueueLengthMs - 1));

  EXPECT_CALL(observer_, OnNetworkChanged(kInitialBitrateBps, _, _));
  controller_->Process();
}

TEST_F(CongestionControllerTest, OnSendQueueFullAndEstimateChange) {
  EXPECT_CALL(*pacer_, ExpectedQueueTimeMs())
      .WillOnce(Return(PacedSender::kMaxQueueLengthMs + 1));
  EXPECT_CALL(observer_, OnNetworkChanged(0, _, _));
  controller_->Process();

  // Receive new estimate but let the queue still be full.
  bandwidth_observer_->OnReceivedEstimatedBitrate(kInitialBitrateBps * 2);
  EXPECT_CALL(*pacer_, ExpectedQueueTimeMs())
      .WillOnce(Return(PacedSender::kMaxQueueLengthMs + 1));
  //  The send pacer should get the new estimate though.
  EXPECT_CALL(*pacer_, SetEstimatedBitrate(kInitialBitrateBps * 2));
  clock_.AdvanceTimeMilliseconds(25);
  controller_->Process();

  // Let the pacer not be full next time the controller checks.
  // |OnNetworkChanged| should be called with the new estimate.
  EXPECT_CALL(*pacer_, ExpectedQueueTimeMs())
      .WillOnce(Return(PacedSender::kMaxQueueLengthMs - 1));
  EXPECT_CALL(observer_, OnNetworkChanged(kInitialBitrateBps * 2, _, _));
  clock_.AdvanceTimeMilliseconds(25);
  controller_->Process();
}

TEST_F(CongestionControllerTest, SignalNetworkState) {
  EXPECT_CALL(observer_, OnNetworkChanged(0, _, _));
  controller_->SignalNetworkState(kNetworkDown);

  EXPECT_CALL(observer_, OnNetworkChanged(kInitialBitrateBps, _, _));
  controller_->SignalNetworkState(kNetworkUp);

  EXPECT_CALL(observer_, OnNetworkChanged(0, _, _));
  controller_->SignalNetworkState(kNetworkDown);
}

TEST_F(CongestionControllerTest,
       SignalNetworkStateAndQueueIsFullAndEstimateChange) {
  // Send queue is full
  EXPECT_CALL(*pacer_, ExpectedQueueTimeMs())
      .WillRepeatedly(Return(PacedSender::kMaxQueueLengthMs + 1));
  EXPECT_CALL(observer_, OnNetworkChanged(0, _, _));
  controller_->Process();

  // Queue is full and network is down. Expect no bitrate change.
  controller_->SignalNetworkState(kNetworkDown);
  controller_->Process();

  // Queue is full but network is up. Expect no bitrate change.
  controller_->SignalNetworkState(kNetworkUp);
  controller_->Process();

  // Receive new estimate but let the queue still be full.
  EXPECT_CALL(*pacer_, SetEstimatedBitrate(kInitialBitrateBps * 2));
  bandwidth_observer_->OnReceivedEstimatedBitrate(kInitialBitrateBps * 2);
  clock_.AdvanceTimeMilliseconds(25);
  controller_->Process();

  // Let the pacer not be full next time the controller checks.
  EXPECT_CALL(*pacer_, ExpectedQueueTimeMs())
      .WillOnce(Return(PacedSender::kMaxQueueLengthMs - 1));
  EXPECT_CALL(observer_, OnNetworkChanged(kInitialBitrateBps * 2, _, _));
  controller_->Process();
}

}  // namespace test
}  // namespace webrtc
