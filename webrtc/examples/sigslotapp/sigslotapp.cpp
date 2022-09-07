// sigslotapp.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//
//#define _SIGSLOT_HAS_WIN32_THREADS
#include <map>
#include <memory>
#include <string>

#include "webrtc/base/nethelpers.h"
#include "webrtc/base/physicalsocketserver.h"
#include "webrtc/base/signalthread.h"
#include "webrtc/base/common.h"
#include "webrtc/base/sigslot.h"
#include <synchapi.h>

#include <iostream>

class AsyncPresenter {
 public:
  sigslot::signal1<AsyncPresenter*, sigslot::multi_threaded_local> SignalReadEvent;

  AsyncPresenter(const char* name) { name_ = name;}

  std::string GetName() const { return name_;}

  private:
  const char* name_;
};

class AsyncHelper : public sigslot::has_slots<> {
 public:
  AsyncHelper(AsyncPresenter* presenter) { hanging_get_.reset(presenter);
  }
  void InitSignalSlots() { 
      ASSERT(hanging_get_.get() != NULL);
    hanging_get_->SignalReadEvent.connect(this,
                                            &AsyncHelper::OnHangingGetRead);
  }
  void OnHangingGetRead(AsyncPresenter* socket) {
    std::cout << __FUNCTION__ << " " << socket->GetName().c_str() << std::endl;
  }

  private:
  std::unique_ptr<AsyncPresenter> hanging_get_;
};

int main()
{
    std::cout << "Hello World!\n";
  AsyncPresenter* presenter = new AsyncPresenter("abc");
  AsyncHelper helper(presenter);
  helper.InitSignalSlots();
  presenter->SignalReadEvent(presenter);
}