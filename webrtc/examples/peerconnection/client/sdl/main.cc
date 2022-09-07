#include <SDL.h>
#include <stdio.h>

#include "webrtc/examples/peerconnection/client/conductor.h"
#include "webrtc/examples/peerconnection/client/flagdefs.h"
#include "webrtc/examples/peerconnection/client/sdl/main_wnd.h"
#include "webrtc/examples/peerconnection/client/peer_connection_client.h"
#include "webrtc/base/ssladapter.h"
#include "webrtc/base/win32socketinit.h"
#include "webrtc/base/win32socketserver.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/profiler.h"

#pragma comment(lib, "sdl2.lib")
#pragma comment(lib, "sdl2main.lib")

class CustomSocketServer : public rtc::PhysicalSocketServer {
    public:
  explicit CustomSocketServer(rtc::Thread* thread, SdlMainWnd* wnd)
         : message_queue_(thread),wnd_(wnd), conductor_(NULL), client_(NULL) {}

        virtual ~CustomSocketServer(){}

        //void SetMessageQueue(rtc::Thread *queue) override { message_queue_ = queue; }

        void SetClient(PeerConnectionClient *client){client_ = client;}
        void SetConductor(Conductor *conductor){conductor_ = conductor;}

        //loop
        bool Wait(int cms, bool process_io) override {
            // SK_TRACE_FUNC()
            //sdl event
            SDL_Event event;
            SDL_WaitEvent(&event);
            switch (event.type)
            {
            case SDL_USEREVENT+1:
            case SDL_QUIT:
                std::cout << "quit" << std::endl;
                SDL_Quit();
                wnd_->ReleaseResource();
                break;
            case SDL_USEREVENT:
                std::cout << "user event" <<std::endl;
                // DrawBitmap(window);
                break;
            default:
                break;
            }

            if(!wnd_->IsWindow() && !conductor_->connection_active() &&
                client_ != NULL && !client_->is_connected()){
                    message_queue_->Quit();
                    std::cout << "message_queue quit" << std::endl;
            }
            
            return rtc::PhysicalSocketServer::Wait(0, process_io);
        }

    protected:
        rtc::Thread *message_queue_;
        SdlMainWnd  *wnd_;
        Conductor   *conductor_;
        PeerConnectionClient *client_; 
};

void DrawBitmap(SDL_Window* wnd)
{
    SDL_UpdateWindowSurface(wnd);
}

void DrawButton(SDL_Rect *btn, int flag)
{

}

int draw_thread(void*){
    SDL_Event event;
	event.type = SDL_USEREVENT;
	event.user.data1 = NULL;
	SDL_PushEvent(&event);

    return 0;
}

int main(int argc, char* argv[])
{
    std::ofstream ofs;
    LoggerFiler logf(ofs);
    ofs.open("sdl" + logf.GetFileName());
    
    rtc::LogMessage::AddLogToStream(&logf, rtc::LS_VERBOSE);
    rtc::LogMessage::LogToDebug(rtc::LS_VERBOSE);
    // rtc::LogMessage::LogToDebug(rtc::WARNING);
    rtc::LogMessage::LogThreads(true);
    rtc::LogMessage::LogTimestamps(true);

    LOG(INFO) << "log level:" << rtc::LogMessage::GetMinLogSeverity();

    LOG(INFO) << "test log output";
    // halgo::Trace trace(__FUNCTION__, halgo::Channel::Instance());
    SK_TRACE_FUNC(0)
    rtc::EnsureWinsockInit();
    rtc::Win32Thread w32_thread;
    rtc::ThreadManager::Instance()->SetCurrentThread(&w32_thread);

    //rtc::WindowsCommandLineArguments win_args;
    //int argc = win_args.argc();
    //char** argv = win_args.argv();

    rtc::FlagList::SetFlagsFromCommandLine(&argc, argv, true);
    if (FLAG_help) {
      rtc::FlagList::Print(NULL, false);
      return 0;
    }


    LOG(INFO) << "server ip:" << FLAG_server
              << " port:" << FLAG_port
              << " autoconn:" << FLAG_autoconnect
    << " autocall:" << FLAG_autocall;
    SdlMainWnd wnd(FLAG_server, FLAG_port,
                   FLAG_autoconnect,
                   FLAG_autocall);


    //rtc::AutoThread auto_thread;
    rtc::Thread* thread = rtc::Thread::Current();
    CustomSocketServer socket_server(thread, &wnd);
    thread->set_socketserver(&socket_server);

    rtc::InitializeSSL();

    PeerConnectionClient client;
    rtc::scoped_refptr<Conductor> conductor(new rtc::RefCountedObject<Conductor>(&client, &wnd));

    wnd.Create();

    socket_server.SetClient(&client);
    socket_server.SetConductor(conductor);

    // SDL_CreateThread(draw_thread, "", NULL);

    thread->Run();

    wnd.Destroy();

    thread->set_socketserver(NULL);

    rtc::CleanupSSL();
    ofs.close();
    return 0;
} 