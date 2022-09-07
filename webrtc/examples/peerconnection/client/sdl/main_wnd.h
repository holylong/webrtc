#ifndef _SDL_PEERCONNECTION_CLINET_MAIN_WND_H_
#define _SDL_PEERCONNECTION_CLINET_MAIN_WND_H_

#include <stdint.h>
#include <memory>
#include <string>
#include <SDL.h>
#include <map>

#include "webrtc/api/mediastreaminterface.h"
#include "webrtc/base/win32.h"
#include "webrtc/examples/peerconnection/client/peer_connection_client.h"
#include "webrtc/media/base/mediachannel.h"
#include "webrtc/media/base/videocommon.h"
#include "webrtc/media/base/videoframe.h"
#include "third_party/libyuv/include/libyuv/convert_from.h"
#include "examples/peerconnection//client/main_wnd.h"
#include "examples/peerconnection/client/tracer.h"
#include "examples/peerconnection/client/rtc_logger.h"
#include "webrtc/api/datachannelinterface.h"

#include <mutex>

//HAVE_LIBC

#ifdef _WIN32
#undef main /* We don't want SDL to override our main() */
#endif

class SdlMainWnd : public MainWindow
{
    public:
        SdlMainWnd(const char* server, int port, bool autoconnect, bool autocall);
        ~SdlMainWnd();

    //ui
    void Create()
    {
          SK_TRACE_FUNC(frame_index_)
        RTC_CHECK(window_ == NULL);
        SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_TIMER);
        // SDL_Surface* background_surface = SDL_LoadBMP("third_party/sdl/test/testyuv.bmp");
        // screen_width_ = background_surface->w;
        // screen_height_ = background_surface->h;
        std::cout << "background width:" << screen_width_  << " height:" << screen_height_ << std::endl;
        window_ = SDL_CreateWindow("PeerConnection", 
                                            SDL_WINDOWPOS_UNDEFINED, 
                                            SDL_WINDOWPOS_UNDEFINED, 
                                            screen_width_, screen_height_, 
                                            SDL_WINDOW_SHOWN);
        // SDL_Surface *surface = SDL_GetWindowSurface(window_);
        // SDL_BlitSurface(background_surface, NULL, surface, NULL);
        renderer_ = SDL_CreateRenderer(window_, -1, 0);

        LOG(INFO) << "create remote texture";
        remotetex_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB32, SDL_TEXTUREACCESS_STREAMING, screen_width_/2, screen_height_/2);
        remoterect_ = new SDL_Rect();
        remoterect_->x = 0;
        remoterect_->y = 0;
        remoterect_->w = screen_width_/2;
        remoterect_->h = screen_height_/2;

        SDL_SetTextureBlendMode(remotetex_, SDL_BLENDMODE_BLEND);

        LOG(INFO) << "create local texture";
        // localtex_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB32, SDL_TEXTUREACCESS_STREAMING, screen_width_/2, screen_height_/2);
        // localrect_ = new SDL_Rect();
        // localrect_->x = screen_width_/2;
        // localrect_->y = screen_height_/2;
        // localrect_->w = screen_width_/2;
        // localrect_->h = screen_height_/2;

        // SDL_SetTextureBlendMode(localtex_, SDL_BLENDMODE_BLEND);

        LOG(INFO) << "update background texture";
        // SDL_UpdateWindowSurface(window_);
        SwitchToConnectUI();
    }

    void ReleaseResource(){
      SK_TRACE_FUNC(frame_index_)
        callback_->Close();
        window_ = NULL;
        renderer_ = NULL;
        vbox_ = NULL;
        peer_list_ = NULL;
    }

    void Destroy()
    {
      SK_TRACE_FUNC(frame_index_)
        // SDL_Delay(3000);
        SDL_DestroyWindow(window_);
        window_ = NULL;
    }

    //override
    virtual void RegisterObserver(MainWndCallback* callback) { callback_ = callback;}
    virtual bool IsWindow()
    {
        // SK_TRACE_FUNC()
        return window_ != NULL;
    }

    virtual void MessageBox(const char* caption, const char* text, bool is_error);
    virtual MainWindow::UI current_ui()
    {
      SK_TRACE_FUNC(frame_index_)
        if(vbox_)return CONNECT_TO_SERVER;
        if(peer_list_)return LIST_PEERS;
        return STREAMING;
    }
    virtual void SwitchToConnectUI();
    virtual void SwitchToPeerList(const Peers& peers);
    virtual void SwitchToStreamingUI();

    virtual void StartLocalRenderer(webrtc::VideoTrackInterface* local_video);
    virtual void StopLocalRenderer();

    virtual void StartRemoteRenderer(webrtc::VideoTrackInterface* remote_video);
    virtual void StopRemoteRenderer();

    virtual void QueueUIThreadCallback(int msg_id, void* data);


    virtual void OnStateChange() {}
    //  A data buffer was successfully received.
    virtual void OnMessage(const webrtc::DataBuffer& buffer){

    };

    void DrawFrame();

    protected:
    class VideoRenderer : public rtc::VideoSinkInterface<cricket::VideoFrame> {
            public:
                VideoRenderer(SdlMainWnd* main_wnd, webrtc::VideoTrackInterface* track_to_render): width_(0),
                    height_(0),
                    main_wnd_(main_wnd),
                    rendered_track_(track_to_render)
                {
               SK_TRACE_FUNC(index_)
                  ::InitializeCriticalSection(&buffer_lock_);
                    rendered_track_->AddOrUpdateSink(this, rtc::VideoSinkWants());
                }

                void Lock() { ::EnterCriticalSection(&buffer_lock_); }

                void Unlock() { ::LeaveCriticalSection(&buffer_lock_); }

                virtual ~VideoRenderer()
                {
                  SK_TRACE_FUNC(index_)
                    rendered_track_->RemoveSink(this);
                    ::DeleteCriticalSection(&buffer_lock_);
                }

                void OnFrame(const cricket::VideoFrame& video_frame) override
                {
                  AutoLock<VideoRenderer> lock(this);
                    // SK_TRACE_FUNC()
                  const cricket::VideoFrame* frame =
                      video_frame.GetCopyWithRotationApplied();

                  SetSize(frame->width(), frame->height());

                  int size = width_ * height_ * 4;
                  // TODO(henrike): Convert directly to RGBA
                  frame->ConvertToRgbBuffer(cricket::FOURCC_ARGB, image_.get(),
                                            size, width_ * 4);
                  // Convert the B,G,R,A frame to R,G,B,A, which is accepted by
                  // GTK. The 'A' is just padding for GTK, so we can use it as
                  // temp.
                  uint8_t* pix = image_.get();
                  uint8_t* end = image_.get() + size;
                  while (pix < end) {
                    pix[3] = pix[0];  // Save B to A.
                    pix[0] = pix[2];  // Set Red.
                    pix[2] = pix[3];  // Set Blue.
                    pix[3] = 0xFF;    // Fixed Alpha.
                    pix += 4;
                  }
                    main_wnd_->DrawFrame();
                }

                const uint8_t* image() const 
                {
                    // SK_TRACE_FUNC()
                    return image_.get();
                }

                int width() const{
                    // SK_TRACE_FUNC() 
                    return width_;
                }
                int height() const{
                    // SK_TRACE_FUNC() 
                    return height_;
                }

            protected:
                void SetSize(int width, int height)
                {
                    AutoLock<VideoRenderer> lock(this);
                    if (width_ == width && height_ == height) {
                        return;
                    }

                    width_ = width;
                    height_ = height;
                    image_.reset(new uint8_t[width * height * 4]); 
                }
                int width_{0};
                int height_{0};
                CRITICAL_SECTION buffer_lock_;
                std::unique_ptr<uint8_t[]> image_;
                SdlMainWnd* main_wnd_;
                unsigned short index_;
                rtc::scoped_refptr<webrtc::VideoTrackInterface> rendered_track_;
        };

      // A little helper class to make sure we always to proper locking and
        // unlocking when working with VideoRenderer buffers.
        template <typename T>
        class AutoLock {
         public:
          explicit AutoLock(T* obj) : obj_(obj) { obj_->Lock(); }
          ~AutoLock() { obj_->Unlock(); }

         protected:
          T* obj_;
        };

    protected:
        MainWndCallback *callback_{NULL};
        std::unique_ptr<VideoRenderer> local_renderer_;
        std::unique_ptr<VideoRenderer> remote_renderer_;

        bool            autoconnect_{false};
        bool            autocall_{false};
        std::string     server_{"127.0.0.1"};
        std::string     port_{"9999"};
        int             frame_width_;
        int             frame_height_;
        short           frame_index_{0};
        // std::unique_ptr<uint8_t[]> draw_buffer_;
        std::shared_ptr<uint8_t> draw_buffer_;
        int draw_buffer_size_;

        //ui
        SDL_Window      *window_{NULL};
        SDL_Renderer    *renderer_{NULL};
        SDL_Texture     *localtex_{NULL};
        SDL_Rect        *localrect_{NULL};
        SDL_Texture     *remotetex_{NULL};
        SDL_Rect        *remoterect_{NULL};
        SDL_Surface     *vbox_{NULL};
        SDL_Surface     *peer_list_{NULL};
        int             screen_width_{1280};
        int             screen_height_{720};
        bool            already_login_{false};

        std::mutex      locker;
};


#endif //_SDL_PEERCONNECTION_CLINET_MAIN_WND_H_