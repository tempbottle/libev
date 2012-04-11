/** @file
* @brief libev implementation
* @author zhangyafeikimi@gmail.com
* @date
* @version
*
*/
#include "ev.h"
#include "log.h"
#include "scoped_ptr.h"
#include "header.h"
#include <new>

namespace libev {

  class InternalEvent : public Event
  {
  public:
    Ev::Impl * ev;

    InternalEvent() : Event(), ev(0) {}
    InternalEvent(int fd, int event, ev_callback callback, void * udata)
      : Event(fd, event, callback, udata), ev(0) {}

    virtual ~InternalEvent() {}
  };

  Event * CreateSignalEvent(int signum, int event, ev_callback callback, void * udata)
  {
    event |= kEvSignal;

    Event * ev = new (std::nothrow) InternalEvent(signum, event, callback, udata);
    if (ev == 0)
      return 0;

    return ev;
  }


  class SignalData
  {
  public:
    int sigfd;
    sigset_t sigset;
    InternalEvent * sigev[_NSIG];

    SignalData() : sigfd(-1) {}

    ~SignalData()
    {
      if (sigfd != -1)
        safe_close(sigfd);
    }

    int Init()
    {
      sigfd = signalfd(-1, &sigset, SFD_CLOEXEC|SFD_NONBLOCK);
      if (sigfd == -1)
        return kEvFailure;

      sigemptyset(&sigset);
      for (size_t i=0; i<_NSIG; i++)
        sigev[i] = 0;

      return kEvOK;
    }
  };


  class Ev::Impl
  {
  private:
    ScopedPtr<SignalData> signal_;
    // TODO reactor

  public:
    Impl() {}
    ~Impl() {}

    // TODO
    int Init() {return kEvFailure;}
    int Run() {return kEvFailure;}
    int RunOne() {return kEvFailure;}
    int Stop() {return kEvFailure;}

    int AddSignal(Event * ev) {return kEvFailure;}
    int DelSignal(Event * ev) {return kEvFailure;}
  };


  Ev::Ev() {impl_ = new Impl;}
  Ev::~Ev() {delete impl_;}

  int Ev::Init() {return impl_->Init();}
  int Ev::Run() {return impl_->Run();}
  int Ev::RunOne() {return impl_->RunOne();}
  int Ev::Stop() {return impl_->Stop();}

  int Ev::AddSignal(Event * ev) {return impl_->AddSignal(ev);}
  int Ev::DelSignal(Event * ev) {return impl_->DelSignal(ev);}

}
