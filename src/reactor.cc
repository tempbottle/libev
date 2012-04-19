/** @file
* @brief reactor
* @author zhangyafeikimi@gmail.com
* @date
* @version
*
*/
#include "ev.h"
#include "log.h"
#include "interrupter.h"
#include "header.h"

namespace libev {

  class ReactorImpl
  {
  private:
    DISALLOW_COPY_AND_ASSIGN(ReactorImpl);

    // members about signal events
    int sigfd_;
    sigset_t sigset_;
    sigset_t old_sigset_;
    int sig_ev_refcount_[_NSIG];

    // common members
    List ev_list_;// event list (timer and IO)
    List sig_ev_list_;// signal event list
    List active_ev_list_;// active event list
    Interrupter interrupter_;

    // temporary members helping invoke callback
    int ev_cleaned_;// the event is cleaned
    int ev_canceled_;// the event is canceled by user inside its callback

  private:
    void AddSignalRef(int signum);
    void ReleaseSignalRef(int signum);

    // add/del 'ev' to/from 'ev_list_' or 'sig_ev_list_' according to its type
    void AddToList(Event * ev);
    void DelFromList(Event * ev);
    // setup/cleanup 'ev'(mainly affairs about system) according to its type
    void Setup(Event * ev);
    void CleanUp(Event * ev);
    void CancelInsideCB(Event * ev);
    void CancelOutsideCB(Event * ev);
    void CancelAll();
    void InvokeCallback(Event * ev);

    // return 1, readable
    // return 0, unreadable
    // return -1, interrupted(if immediate is non-zero)
    int GetReadability(int immediate);
    void OnSignalReadable();

  public:
    ReactorImpl();
    ~ReactorImpl();

    int Init();
    void UnInit();

    int Add(Event * ev);
    int Del(Event * ev);
    int Cancel(Event * ev);

    int Poll(int limit);
    int Run(int limit);
    int Stop();
  };


  /************************************************************************/
  void ReactorImpl::AddSignalRef(int signum)
  {
    EV_ASSERT(sig_ev_refcount_[signum] >= 0);

    if (sig_ev_refcount_[signum] == 0)
    {
      sigaddset(&sigset_, signum);
      EV_VERIFY(signalfd(sigfd_, &sigset_, SFD_CLOEXEC|SFD_NONBLOCK) != -1);

      sigset_t tmp_sigset;
      sigemptyset(&tmp_sigset);
      sigaddset(&tmp_sigset, signum);
      EV_VERIFY(sigprocmask(SIG_BLOCK, &tmp_sigset, 0) == 0);

      EV_LOG(kDebug, "Signal(%d) has been blocked and added", signum);
    }
    sig_ev_refcount_[signum]++;
  }

  void ReactorImpl::ReleaseSignalRef(int signum)
  {
    EV_ASSERT(sig_ev_refcount_[signum] > 0);

    if (--sig_ev_refcount_[signum] == 0)
    {
      sigdelset(&sigset_, signum);
      EV_VERIFY(signalfd(sigfd_, &sigset_, SFD_CLOEXEC|SFD_NONBLOCK) != -1);

      sigset_t tmp_sigset;
      sigemptyset(&tmp_sigset);
      sigaddset(&tmp_sigset, signum);
      EV_VERIFY(sigprocmask(SIG_UNBLOCK, &tmp_sigset, 0) == 0);

      EV_LOG(kDebug, "Signal(%d) has been unblocked and deleted", signum);
    }
  }

  void ReactorImpl::AddToList(Event * ev)
  {
    if (ev->event & kEvSignal)
      ev->AddToList(&sig_ev_list_);
    else
      ev->AddToList(&ev_list_);
  }

  void ReactorImpl::DelFromList(Event * ev)
  {
    if (ev->event & kEvSignal)
      ev->DelFromList(&sig_ev_list_);
    else
      ev->DelFromList(&ev_list_);
  }

  void ReactorImpl::Setup(Event * ev)
  {
    if (ev->event & kEvSignal)
      AddSignalRef(ev->fd);
    // TODO

    ev->reactor = this;
  }

  void ReactorImpl::CleanUp(Event * ev)
  {
    if (ev->event & kEvSignal)
      ReleaseSignalRef(ev->fd);
    // TODO

    ev->reactor = 0;
  }

  void ReactorImpl::CancelInsideCB(Event * ev)
  {
    EV_ASSERT(ev->flags & kInCallback);
    EV_ASSERT(!ev->IsInList());

    if (ev_cleaned_ == 0)
    {
      ev_cleaned_ = 1;
      CleanUp(ev);
    }
    ev_canceled_ = 1;

    EV_LOG(kDebug, "Event(%p) has been canceled inside its callback", ev);
  }

  void ReactorImpl::CancelOutsideCB(Event * ev)
  {
    EV_ASSERT((ev->flags & kInCallback) == 0);
    EV_ASSERT(ev->IsInList());

    if (!ev->IsActive())
    {
      ev->real_event = kEvCanceled;// only kEvCanceled
      ev->AddToActive(&active_ev_list_);
    }
    else
    {
      ev->real_event |= kEvCanceled;// previous event with kEvCanceled
    }

    EV_LOG(kDebug, "Event(%p) has been canceled", ev);
  }

  void ReactorImpl::CancelAll()
  {
    ListNode * node;
    Event * ev;

    for (node = ev_list_.front(); node != ev_list_.end() ; node = node->next)
    {
      ev = ev_container_of(node, Event, all);
      CancelOutsideCB(ev);
    }

    for (node = sig_ev_list_.front(); node != sig_ev_list_.end() ; node = node->next)
    {
      ev = ev_container_of(node, Event, all);
      CancelOutsideCB(ev);
    }
  }

  void ReactorImpl::InvokeCallback(Event * ev)
  {
    EV_ASSERT(ev->IsInList());
    // invoke callback after removing event from 'active_ev_list_'
    EV_ASSERT(!ev->IsActive());

    DelFromList(ev);
    // now 'ev' is a free event, which can be destroyed

    int perist = ev->event & kEvPersist;
    int canceled = (ev->real_event & kEvCanceled);
    int put_back = perist && !canceled;

    if (!put_back)
    {
      if (!perist)
        EV_LOG(kDebug, "Event(%p) has no kEvPersist", ev);
      if (canceled)
        EV_LOG(kDebug, "Event(%p) has been canceled", ev);

      CleanUp(ev);
      ev_cleaned_ = 1;
    }
    else
    {
      ev_cleaned_ = 0;
    }
    ev_canceled_ = 0;

    ev->flags |= kInCallback;
    ev->callback(ev->fd, ev->real_event, ev->user_data);
    if (!put_back || ev_canceled_)
      return;
    ev->flags &= ~kInCallback;

    // put back 'ev' that should be put back and also 'ev' is not canceled
    AddToList(ev);

    if (--ev->triggered_times)
    {
      ev->AddToActive(&active_ev_list_);
      EV_LOG(kDebug, "Event(%p) is still active, times=%d", ev, ev->triggered_times);
    }
  }

  int ReactorImpl::GetReadability(int immediate)// TODO
  {
    if (immediate)
    {
      pollfd poll_fds[1];
      poll_fds[0].fd = sigfd_;
      poll_fds[0].events = POLLIN;
      int result = poll(poll_fds, 1, 0);

      if (result == 0)
      {
        return 0;
      }
      else
      {
        EV_ASSERT(poll_fds[0].revents & POLLIN);
        return 1;
      }
    }
    else
    {
      pollfd poll_fds[2];
      poll_fds[0].fd = sigfd_;
      poll_fds[0].events = POLLIN;
      poll_fds[1].fd = interrupter_.fd();
      poll_fds[1].events = POLLIN;

      interrupter_.Reset();
      int result;

      do result = poll(poll_fds, 2, -1);
      while (result == -1 && errno == EINTR);

      EV_ASSERT(result != 0);
      EV_ASSERT(result != -1);

      if (poll_fds[0].revents & POLLIN)
      {
        // readable
        return 1;
      }

      if (poll_fds[1].revents & POLLIN)
      {
        // interrupted
        interrupter_.Reset();
        return -1;
      }

      return 0;
    }
  }

  void ReactorImpl::OnSignalReadable()
  {
    signalfd_siginfo siginfo;
    int result;
    int signum;
    ListNode * node;
    Event * ev;

    for (;;)
    {
      do result = read(sigfd_, &siginfo, sizeof(siginfo));
      while (result == -1 && errno == EINTR);

      if (result == -1 && errno == EAGAIN)
        return;

      EV_ASSERT(result == sizeof(siginfo));

      signum = siginfo.ssi_signo;

      // traverse 'sig_ev_list_' to match all events targeting 'signum'
      for (node = sig_ev_list_.front(); node != sig_ev_list_.end() ; node = node->next)
      {
        EV_ASSERT(node);

        ev = ev_container_of(node, Event, all);

        if (ev->fd == signum)
        {
          if (!ev->IsActive())
          {
            ev->real_event = ev->event;
            ev->triggered_times = 1;
            ev->AddToActive(&active_ev_list_);
          }
          else
          {
            ev->triggered_times++;
          }

          EV_LOG(kDebug, "Event(%p) is active", ev);
        }
      }
    }// for
  }

  ReactorImpl::ReactorImpl() : sigfd_(-1)
  {
    EV_VERIFY(sigprocmask(0, 0, &old_sigset_) == 0);
  }

  ReactorImpl::~ReactorImpl()
  {
    UnInit();
    EV_VERIFY(sigprocmask(SIG_SETMASK, &old_sigset_, 0) == 0);
  }

  int ReactorImpl::Init()
  {
    UnInit();

    sigemptyset(&sigset_);
    sigfd_ = signalfd(-1, &sigset_, SFD_CLOEXEC|SFD_NONBLOCK);
    if (sigfd_ == -1)
    {
      EV_LOG(kError, "signalfd: %s", strerror(errno));
      return kEvFailure;
    }

    for (int i=0; i<_NSIG; i++)
      sig_ev_refcount_[i] = 0;

    if (interrupter_.Init() != kEvOK)
    {
      safe_close(sigfd_);
      sigfd_ = -1;
      return kEvFailure;
    }

    return kEvOK;
  }

  void ReactorImpl::UnInit()
  {
    interrupter_.UnInit();

    if (sigfd_ != -1)
    {
      safe_close(sigfd_);
      sigfd_ = -1;
    }

    CancelAll();
    Poll(0);
  }

  int ReactorImpl::Add(Event * ev)
  {
    if (ev == 0)
    {
      errno = EINVAL;
      return kEvFailure;
    }

    int ret;
    if ((ret = CheckEvent(ev)) != kEvOK)
      return ret;

    if (ev->IsInList() || ev->IsActive() || ev->reactor)
    {
      EV_LOG(kError, "Event(%p) has been added before", ev);
      return kEvExists;
    }

    ev->real_event = 0;
    ev->triggered_times = 0;
    ev->flags = 0;

    Setup(ev);
    AddToList(ev);
    EV_LOG(kDebug, "Event(%p) has been added", ev);
    return kEvOK;
  }

  int ReactorImpl::Del(Event * ev)
  {
    if (ev == 0)
    {
      errno = EINVAL;
      return kEvFailure;
    }

    if (ev->flags & kInCallback)
    {
      EV_LOG(kDebug, "Event(%p) is being deleted(canceled) by user inside its callback", ev);
      // deletion inside callback degenerates into a cancellation
      CancelInsideCB(ev);
    }
    else
    {
      EV_LOG(kDebug, "Event(%p) is being deleted by user", ev);

      DelFromList(ev);
      CleanUp(ev);
      if (ev->IsActive())
        ev->DelFromActive(&active_ev_list_);
    }

    EV_LOG(kDebug, "Event(%p) has been deleted", ev);
    return kEvOK;
  }

  int ReactorImpl::Cancel(Event * ev)
  {
    if (ev == 0)
    {
      errno = EINVAL;
      return kEvFailure;
    }

    if (ev->flags & kInCallback)
    {
      CancelInsideCB(ev);
      return kEvOK;
    }
    else
    {
      if (!ev->IsInList())
      {
        EV_LOG(kError, "Event(%p) is not added before", ev);
        return kEvNotExists;
      }
      CancelOutsideCB(ev);
      return kEvOK;
    }
  }

  int ReactorImpl::Poll(int limit)// TODO
  {
    EV_ASSERT(limit >= 0);

    int number = 0;
    ListNode * node;
    Event * ev;

    for (;;)
    {
      while (!active_ev_list_.empty())
      {
        node = active_ev_list_.front();
        EV_ASSERT(node);

        ev = ev_container_of(node, Event, active);
        ev->DelFromActive(&active_ev_list_);

        InvokeCallback(ev);
        number++;

        if (limit > 0 && number == limit)
          return number;
      }

      if (GetReadability(1) == 1)
        OnSignalReadable();
      else
        return number;
    }
  }

  int ReactorImpl::Run(int limit)// TODO
  {
    EV_ASSERT(limit >= 0);

    int number = 0;
    ListNode * node;
    Event * ev;
    int result;

    for (;;)
    {
      while (!active_ev_list_.empty())
      {
        node = active_ev_list_.front();
        EV_ASSERT(node);

        ev = ev_container_of(node, Event, active);
        ev->DelFromActive(&active_ev_list_);

        InvokeCallback(ev);
        number++;

        if (limit > 0 && number == limit)
          return number;
      }

      if (ev_list_.empty() && sig_ev_list_.empty())
      {
        EV_LOG(kDebug, "Event loop quits for no events");
        return number;
      }

      result = GetReadability(0);
      if (result == -1)
      {
        EV_LOG(kDebug, "Event loop quits for an interruption");
        return number;
      }
      else if (result == 1)
        OnSignalReadable();
      else
        EV_ASSERT(0);
    }
  }

  int ReactorImpl::Stop()
  {
    return interrupter_.Interrupt();
  }


  /************************************************************************/
  Reactor::Reactor() {impl_ = new ReactorImpl;}
  Reactor::~Reactor() {delete impl_;}
  int Reactor::Init() {return impl_->Init();}
  void Reactor::UnInit() {impl_->UnInit();}
  int Reactor::Add(Event * ev) {return impl_->Add(ev);}
  int Reactor::Del(Event * ev) {return impl_->Del(ev);}
  int Reactor::Cancel(Event * ev) {return impl_->Cancel(ev);}
  int Reactor::PollOne() {return Poll(1);}
  int Reactor::Poll() {return Poll(0);}
  int Reactor::Poll(int limit) {return impl_->Poll(limit);}
  int Reactor::RunOne() {return Run(1);}
  int Reactor::Run() {return Run(0);}
  int Reactor::Run(int limit) {return impl_->Run(limit);}
  int Reactor::Stop() {return impl_->Stop();}


  /************************************************************************/
  int Event::Del()
  {
    if (reactor == 0)
    {
      errno = EINVAL;
      return kEvFailure;
    }

    return reactor->Del(this);
  }

  int Event::Cancel()
  {
    if (reactor == 0)
    {
      errno = EINVAL;
      return kEvFailure;
    }

    return reactor->Cancel(this);
  }
}
