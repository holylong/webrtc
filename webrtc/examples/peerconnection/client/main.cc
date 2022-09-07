/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/examples/peerconnection/client/conductor.h"
#include "webrtc/examples/peerconnection/client/flagdefs.h"
#include "webrtc/examples/peerconnection/client/main_wnd.h"
#include "webrtc/examples/peerconnection/client/peer_connection_client.h"
#include "webrtc/base/ssladapter.h"
#include "webrtc/base/win32socketinit.h"
#include "webrtc/base/win32socketserver.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/profiler.h"
#include "examples/peerconnection/client/rtc_logger.h"

//bool rtc::LogMessage::thread_ = true, rtc::LogMessage::timestamp_ = true;

int PASCAL wWinMain(HINSTANCE instance, HINSTANCE prev_instance,
                    wchar_t* cmd_line, int cmd_show) {
//int main(int argc, char* argv[]){
  //rtc::LogMessage* log = new rtc::LogMessage("webrtc-message-b.log", 10000, rtc::LS_INFO);
  //std::ofstream fp("webrtc-message-a.log", std::ios::app);

std::ofstream ofs;
LoggerFiler logf(ofs);
ofs.open("win" + logf.GetFileName());

rtc::LogMessage::AddLogToStream(&logf, rtc::LS_VERBOSE);
rtc::LogMessage::LogToDebug(rtc::LS_VERBOSE);
// rtc::LogMessage::LogToDebug(rtc::WARNING);
rtc::LogMessage::LogThreads(true);
rtc::LogMessage::LogTimestamps(true);

  //PROFILE_DUMP_ALL(rtc::LS_INFO);
LOG(INFO) << __FUNCTION__ << " "
          << "test peerconnection";
  rtc::EnsureWinsockInit();
  rtc::Win32Thread w32_thread;
  rtc::ThreadManager::Instance()->SetCurrentThread(&w32_thread);

  rtc::WindowsCommandLineArguments win_args;
  int argcl = win_args.argc();
  char **argvl = win_args.argv();

  rtc::FlagList::SetFlagsFromCommandLine(&argcl, argvl, true);
  if (FLAG_help) {
    rtc::FlagList::Print(NULL, false);
    return 0;
  }

  // Abort if the user specifies a port that is outside the allowed
  // range [1, 65535].
  if ((FLAG_port < 1) || (FLAG_port > 65535)) {
    printf("Error: %i is not a valid port.\n", FLAG_port);
    return -1;
  }

  MainWnd wnd(FLAG_server, FLAG_port, FLAG_autoconnect, FLAG_autocall);
  if (!wnd.Create()) {
    ASSERT(false);
    return -1;
  }

  rtc::InitializeSSL();
  PeerConnectionClient client;
  rtc::scoped_refptr<Conductor> conductor(
        new rtc::RefCountedObject<Conductor>(&client, &wnd));

  // Main loop.
  MSG msg;
  BOOL gm;
  while ((gm = ::GetMessage(&msg, NULL, 0, 0)) != 0 && gm != -1) {
    if (!wnd.PreTranslateMessage(&msg)) {
      ::TranslateMessage(&msg);
      ::DispatchMessage(&msg);
    }
  }

  if (conductor->connection_active() || client.is_connected()) {
    while ((conductor->connection_active() || client.is_connected()) &&
           (gm = ::GetMessage(&msg, NULL, 0, 0)) != 0 && gm != -1) {
      if (!wnd.PreTranslateMessage(&msg)) {
        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
      }
    }
  }

  rtc::CleanupSSL();
  ofs.close();
  return 0;
}
