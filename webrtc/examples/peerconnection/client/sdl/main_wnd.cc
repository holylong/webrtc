#include "webrtc/examples/peerconnection/client/sdl/main_wnd.h"

#include <math.h>

#include "webrtc/examples/peerconnection/client/defaults.h"
//#include "webrtc/examples/peerconnection/client/easylogging++.h"
#include "webrtc/base/arraysize.h"
#include "webrtc/base/common.h"
#include "webrtc/base/logging.h"

namespace {
    struct UIThreadCallbackData{

    };
}//namespace


SdlMainWnd::SdlMainWnd(const char* server, int port, bool autoconnect, bool autocall)
    :callback_(NULL),autoconnect_(autoconnect),autocall_(autocall),server_(server),window_(NULL)
{
  SK_TRACE_FUNC(frame_index_)
    port_ = std::to_string(port);
}

void SdlMainWnd::QueueUIThreadCallback(int msg_id, void* data)
{
  SK_TRACE_FUNC(frame_index_)
    LOG(INFO) << "==>> msg_id:" <<  msg_id;
    Sleep(100);
    callback_->UIThreadCallback(msg_id, data);
}

SdlMainWnd::~SdlMainWnd()
{
  SK_TRACE_FUNC(frame_index_)
    RTC_DCHECK(!IsWindow());
}

void SdlMainWnd::MessageBox(const char* caption, const char* text, bool is_error)
{
  SK_TRACE_FUNC(frame_index_)
    LOG(INFO) << "caption:" << caption << " text:" << text << " is_error:" << is_error;
} 

void SdlMainWnd::DrawFrame()
{
    locker.lock();
  SK_TRACE_FUNC(frame_index_)
    VideoRenderer* remote_renderer = remote_renderer_.get();
    if (remote_renderer && remote_renderer->image() != NULL && renderer_ != NULL) 
    {
        frame_width_ = remote_renderer->width();
        frame_height_ = remote_renderer->height();
        
        // if (!draw_buffer_.get()) {
            draw_buffer_size_ = (frame_width_ * frame_height_ * 4) * 4;
            // draw_buffer_.reset(new uint8_t[draw_buffer_size_]);
        uint8_t* mybuf = new uint8_t[draw_buffer_size_];    
            // draw_buffer_ = std::shared_ptr<uint8_t[]>(new uint8_t[draw_buffer_size_]);
        // }

        std::cout << "index:" << frame_index_++ <<  " remote width:" << frame_width_ << " height:" << frame_height_ 
        << " buffer size:" << draw_buffer_size_ << std::endl;

        const uint32_t* image = reinterpret_cast<const uint32_t*>(remote_renderer->image());
        // uint32_t* scaled = reinterpret_cast<uint32_t*>(draw_buffer_.get());
        uint32_t* scaled = reinterpret_cast<uint32_t*>(mybuf);
        for (int r = 0; r < frame_height_; ++r) {
            for (int c = 0; c < frame_width_; ++c) {
                int x = c * 2;
                scaled[x] = scaled[x + 1] = image[c];
            }

            uint32_t* prev_line = scaled;
            scaled += frame_width_ * 2;
            memcpy(scaled, prev_line, (frame_width_ * 2) * 4);

            image += frame_width_;
            scaled += frame_width_ * 2;
        }
        LOG(WARNING) << " draw renderer start";
        // SDL_Rect rect{0,0, frame_width_,frame_height_};
        static FILE* fp = fopen("abc_rgb.yuv", "wb");
        if(fp != NULL){
            fwrite(scaled, 1, (frame_width_ * frame_height_ * 4)*4, fp);
            fflush(fp);
        }
        // SDL_UpdateTexture(remotetex_, &rect, scaled, frame_width_*4);
        LOG(WARNING) << " draw renderer 2";
        // SDL_RenderClear(renderer_);
        LOG(WARNING) << " draw renderer 3";
		// SDL_RenderCopy(renderer_, remotetex_, &rect, remoterect_);
        LOG(WARNING) << " draw renderer ok";
        VideoRenderer* local_renderer = local_renderer_.get();
        if (local_renderer && local_renderer->image()) 
        {
            image = reinterpret_cast<const uint32_t*>(local_renderer->image());
            // SDL_UpdateTexture(localtex_, localrect_, image, frame_width_);
		    // SDL_RenderCopy(renderer_, localtex_, localrect_, localrect_);
        }
        SDL_RenderPresent(renderer_);
    }
    locker.unlock();
}

void SdlMainWnd::SwitchToConnectUI()
{
  SK_TRACE_FUNC(frame_index_)
    LOG(INFO) << __PRETTY_FUNCTION__;
    if (peer_list_) {
        peer_list_ = NULL;
    }

    vbox_ = SDL_GetWindowSurface(window_);
    
    if(callback_!= NULL){
        if(already_login_)return;
        LOG(INFO) << "start login";
        callback_->StartLogin(server_, port_.length() ? atoi(port_.c_str()) : 0);
        already_login_ = true;
    }
    else{
        LOG(INFO) << "callback_ is null";
    }
}

void SdlMainWnd::SwitchToPeerList(const Peers& peers)
{
  SK_TRACE_FUNC(frame_index_)
    if (!peer_list_) {
        if (vbox_) {
            vbox_ = NULL;
        }
    peer_list_ = SDL_GetWindowSurface(window_);
    } else {
    }
}

void SdlMainWnd::SwitchToStreamingUI()
{
  SK_TRACE_FUNC(frame_index_)
    if (peer_list_) {
        peer_list_ = NULL;
    }
}

void SdlMainWnd::StartRemoteRenderer(webrtc::VideoTrackInterface* remote_video)
{
  SK_TRACE_FUNC(frame_index_)
    remote_renderer_.reset(new VideoRenderer(this, remote_video));
}

void SdlMainWnd::StopRemoteRenderer()
{
  SK_TRACE_FUNC(frame_index_)
    remote_renderer_.reset();
}

void SdlMainWnd::StartLocalRenderer(webrtc::VideoTrackInterface* local_video)
{
  SK_TRACE_FUNC(frame_index_)
    local_renderer_.reset(new VideoRenderer(this, local_video));
}

void SdlMainWnd::StopLocalRenderer()
{
  SK_TRACE_FUNC(frame_index_)
    local_renderer_.reset();
}