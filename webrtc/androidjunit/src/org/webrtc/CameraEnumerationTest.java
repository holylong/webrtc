/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import static org.junit.Assert.assertEquals;
import static org.webrtc.CameraEnumerationAndroid.getClosestSupportedFramerateRange;

import org.webrtc.CameraEnumerationAndroid.CaptureFormat;
import org.webrtc.CameraEnumerationAndroid.CaptureFormat.FramerateRange;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;

import java.util.Arrays;

/**
 * Tests for CameraEnumerationAndroid.
 */
@RunWith(RobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class CameraEnumerationTest {
  @Test
  public void testGetClosestSupportedFramerateRange() {
    assertEquals(new FramerateRange(10000, 30000),
        getClosestSupportedFramerateRange(
            Arrays.asList(new FramerateRange(10000, 30000),
                          new FramerateRange(30000, 30000)),
            30 /* requestedFps */));

    assertEquals(new FramerateRange(10000, 20000),
        getClosestSupportedFramerateRange(
            Arrays.asList(new FramerateRange(0, 30000),
                          new FramerateRange(10000, 20000),
                          new FramerateRange(14000, 16000),
                          new FramerateRange(15000, 15000)),
            15 /* requestedFps */));

    assertEquals(new FramerateRange(10000, 20000),
        getClosestSupportedFramerateRange(
            Arrays.asList(new FramerateRange(15000, 15000),
                          new FramerateRange(10000, 20000),
                          new FramerateRange(10000, 30000)),
            10 /* requestedFps */));
  }
}
