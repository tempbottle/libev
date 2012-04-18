/** @file
* @brief signal reactor
* @author zhangyafeikimi@gmail.com
* @date
* @version
*
*/
#include "ev.h"
#include "interrupter.h"
#include "header.h"

namespace libev {

  class SignalReactor::Impl : public Reactor
  {
  private:
    DISALLOW_COPY_AND_ASSIGN(Impl);

    int sigfd_;
    sigset_t sigset_;
    sigset_t old_sigset_;
    List sig_ev_;
    List active_sig_ev_;
    Interrupter interrupter_;
    int sig_ev_refcount_[_NSIG];

  private:
    void AddSignalRef(int signum);
    void ReleaseSignalRef(int signum);

    // cancel all events
    void CancelAll();
    void InvokeCallback(Event * ev);

    // if 'limit' > 0, execute at most 'limit' ready event
    // if 'limit' == 0, execute all ready events
    // return the number of executed event
    int PollImpl(int limit);

    // if 'limit' > 0, execute at most 'limit' event, or until interrupted
    // if 'limit' == 0, execute all events, or until interrupted
    // return the number of executed event
    int RunImpl(int limit);

  public:
    Impl();
    virtual ~Impl();

    virtual int Init();
    virtual void UnInit();

    virtual int Add(Event * ev);
    virtual int Del(Event * ev);

    virtual int PollOne();
    virtual int Poll();
    virtual int RunOne();
    virtual int Run();
    virtual int Stop();

    int fd()const;
    int GetReadability(int immediate);
    void OnReadable();
  };


  /************************************************************************/
  void SignalReactor::Impl::AddSignalRef(int signum)
  {
    EV_ASSERT(signum >= 0 && signum < _NSIG);
    EV_ASSERT(sig_ev_refcount_[signum] >= 0);

    if (sig_ev_refcount_[signum] == 0)
    {
      sigaddset(&sigset_, signum);
      EV_VERIFY(signalfd(sigfd_, &sigset_, SFD_CLOEXEC|SFD_NONBLOCK) != -1);

      sigset_t tmp_sigset;
      sigemptyset(&tmp_sigset);
      sigaddset(&tmp_sigset, signum);
      EV_VERIFY(sigprocmask(SIG_BLOCK, &tmp_sigset, NULL) == 0);

      EV_LOG(kDebug, "Signal(%d) has been blocked and added", signum);
    }
    sig_ev_refcount_[signum]++;
  }

  void SignalReactor::Impl::ReleaseSignalRef(int signum)
  {
    EV_ASSERT(signum >= 0 && signum < _NSIG);
    EV_ASSERT(sig_ev_refcount_[signum] > 0);

    if (--sig_ev_refcount_[signum] == 0)
    {
      sigdelset(&sigset_, signum);
      EV_VERIFY(signalfd(sigfd_, &sigset_, SFD_CLOEXEC|SFD_NONBLOCK) != -1);

      sigset_t tmp_sigset;
      sigemptyset(&tmp_sigset);
      sigaddset(&tmp_sigset, signum);
      EV_VERIFY(sigprocmask(SIG_UNBLOCK, &tmp_sigset, NULL) == 0);

      EV_LOG(kDebug, "Signal(%d) has been unblocked and deleted", signum);
    }
  }

  void SignalReactor::Impl::CancelAll()
  {
    ListNode * node;
    EventInternal * internal;
    Event * ev;

    for (node = sig_ev_.front(); node != sig_ev_.end() ; node = node->next)
    {
      EV_ASSERT(node);

      internal = ev_container_of(node, EventInternal, all);
      ev = ev_container_of(internal, Event, internal);

      if (!internal->IsActive())
      {
        internal->real_event = kEvCanceled;// only kEvCanceled
        internal->AddToActive(&active_sig_ev_);
      }
      else
      {
        internal->real_event |= kEvCanceled;// previous event with kEvCanceled
      }

      EV_LOG(kDebug, "Signal Event(%p) has been canceled", ev);
    }
  }

  void SignalReactor::Impl::InvokeCallback(Event * ev)
  {
    EV_ASSERT(ev);
    EV_ASSERT(ev->callback);
    EventInternal * internal = &ev->internal;
    EV_ASSERT(internal);
    EV_ASSERT(internal->IsInList());

    int perist_before = ev->event & kEvPersist;
    internal->real_event |= kInCallback;
    ev->callback(ev->fd, internal->real_event, ev->user_data);
    internal->real_event &= ~kInCallback;
    int perist_after = ev->event & kEvPersist;
    internal->sig_times--;

    int still_in_list = internal->IsInList();
    int remove_persist = (perist_before && !perist_after);
    int unpersist = !(internal->real_event & kEvPersist);
    int canceled = (internal->real_event & kEvCanceled);

    if (!still_in_list || remove_persist || unpersist || canceled)
    {
      if (still_in_list)
        internal->DelFromList(&sig_ev_);

      if (!still_in_list)
        EV_LOG(kDebug, "Signal Event(%p) has been canceled by user inside its callback", ev);
      else if (remove_persist)
        EV_LOG(kDebug, "Signal Event(%p)'s kEvPersist is turned off by user", ev);
      else if (unpersist)
        EV_LOG(kDebug, "Signal Event(%p) has no kEvPersist", ev);
      else if (canceled)
        EV_LOG(kDebug, "Signal Event(%p) has been canceled", ev);

      ReleaseSignalRef(ev->fd);

      return;
    }

    if (internal->sig_times)
    {
      internal->AddToActive(&active_sig_ev_);
      EV_LOG(kDebug, "Signal Event(%p) is still active, times=%d", ev, internal->sig_times);
    }
  }

  int SignalReactor::Impl::PollImpl(int limit)
  {
    EV_ASSERT(limit >= 0);

    int number = 0;
    ListNode * node;
    EventInternal * internal;
    Event * ev;

    for (;;)
    {
      while (!active_sig_ev_.empty())
      {
        node = active_sig_ev_.front();
        EV_ASSERT(node);

        internal = ev_container_of(node, EventInternal, active);
        internal->DelFromActive(&active_sig_ev_);
        ev = ev_container_of(internal, Event, internal);

        InvokeCallback(ev);
        number++;

        if (limit > 0 && number == limit)
          return number;
      }

      if (GetReadability(1) == 1)
        OnReadable();
      else
        return number;
    }
  }

  int SignalReactor::Impl::RunImpl(int limit)
  {
    EV_ASSERT(limit >= 0);

    int number = 0;
    ListNode * node;
    EventInternal * internal;
    Event * ev;
    int result;

    for (;;)
    {
      while (!active_sig_ev_.empty())
      {
        node = active_sig_ev_.front();
        EV_ASSERT(node);

        internal = ev_container_of(node, EventInternal, active);
        internal->DelFromActive(&active_sig_ev_);
        ev = ev_container_of(internal, Event, internal);

        InvokeCallback(ev);
        number++;

        if (limit > 0 && number == limit)
          return number;
      }

      if (sig_ev_.empty())
      {
        EV_LOG(kDebug, "Signal event loop quits for no events");
        return number;
      }

      result = GetReadability(0);
      if (result == -1)
      {
        EV_LOG(kDebug, "Signal event loop quits for an interruption");
        return number;
      }
      else if (result == 1)
        OnReadable();
      else
        EV_ASSERT(0);
    }
  }

  SignalReactor::Impl::Impl() : sigfd_(-1)
  {
    EV_VERIFY(sigprocmask(0, NULL, &old_sigset_) == 0);
  }

  SignalReactor::Impl::~Impl()
  {
    UnInit();
    EV_VERIFY(sigprocmask(SIG_SETMASK, &old_sigset_, NULL) == 0);
  }

  int SignalReactor::Impl::Init()
  {
    UnInit();

    sigemptyset(&sigset_);
    sigfd_ = signalfd(-1, &sigset_, SFD_CLOEXEC|SFD_NONBLOCK);
    if (sigfd_ == -1)
    {
      EV_LOG(kError, "signalfd: %s", strerror(errno));
      return kEvFailure;
    }

    if (interrupter_.Init() != kEvOK)
    {
      safe_close(sigfd_);
      sigfd_ = -1;
      return kEvFailure;
    }

    for (int i=0; i<_NSIG; i++)
      sig_ev_refcount_[i] = 0;

    return kEvOK;
  }

  void SignalReactor::Impl::UnInit()
  {
    interrupter_.UnInit();

    if (sigfd_ != -1)
    {
      safe_close(sigfd_);
      sigfd_ = -1;
    }

    CancelAll();
    Poll();
  }

  int SignalReactor::Impl::Add(Event * ev)
  {
    EV_ASSERT(ev);
    EV_ASSERT(ev->fd >= 0 && ev->fd < _NSIG);
    EV_ASSERT(CheckInputEventFlag(ev->event) == kEvOK);
    EV_ASSERT(ev->event & kEvSignal);
    EV_ASSERT(ev->callback);

    if (ev->internal.IsInList())
    {
      EV_LOG(kError, "Signal Event(%p) has been added before", ev);
      return kEvExists;
    }
    EV_ASSERT(!ev->internal.IsActive());
    ev->internal.sig_times = 0;

    AddSignalRef(ev->fd);
    ev->internal.AddToList(&sig_ev_);
    EV_LOG(kDebug, "Signal Event(%p) has been added", ev);
    return kEvOK;
  }

  int SignalReactor::Impl::Del(Event * ev)
  {
    EV_ASSERT(ev);

    EventInternal * internal = &ev->internal;
    if (!internal->IsInList())
    {
      EV_LOG(kError, "Signal Event(%p) is not added before", ev);
      return kEvNotExists;
    }

    if (internal->real_event & kInCallback)
    {
      internal->DelFromList(&sig_ev_);
      EV_LOG(kDebug, "Signal Event(%p) is being canceled by user inside its callback", ev);
      return kEvOK;
    }
    else
    {
      if (!internal->IsActive())
      {
        internal->real_event = kEvCanceled;// only kEvCanceled
        internal->AddToActive(&active_sig_ev_);
      }
      else
      {
        internal->real_event |= kEvCanceled;// previous event with kEvCanceled
      }

      EV_LOG(kDebug, "Signal Event(%p) is being canceled by user outside its callback", ev);
      return kEvOK;
    }
  }

  int SignalReactor::Impl::PollOne()
  {
    return PollImpl(1);
  }

  int SignalReactor::Impl::Poll()
  {
    return PollImpl(0);
  }

  int SignalReactor::Impl::RunOne()
  {
    return RunImpl(1);
  }

  int SignalReactor::Impl::Run()
  {
    return RunImpl(0);
  }

  int SignalReactor::Impl::Stop()
  {
    return interrupter_.Interrupt();
  }

  int SignalReactor::Impl::fd()const
  {
    return sigfd_;
  }

  int SignalReactor::Impl::GetReadability(int immediate)
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

  void SignalReactor::Impl::OnReadable()
  {
    signalfd_siginfo siginfo;
    int result;
    int signum;
    ListNode * node;
    EventInternal * internal;
    Event * ev;

    for (;;)
    {
      do result = read(sigfd_, &siginfo, sizeof(siginfo));
      while (result == -1 && errno == EINTR);

      if (result == -1 && errno == EAGAIN)
        return;

      EV_ASSERT(result == sizeof(siginfo));

      signum = siginfo.ssi_signo;

      // traverse 'sig_ev_' to match all events targeting 'signum'
      for (node = sig_ev_.front(); node != sig_ev_.end() ; node = node->next)
      {
        EV_ASSERT(node);

        internal = ev_container_of(node, EventInternal, all);
        ev = ev_container_of(internal, Event, internal);

        if (ev->fd == signum)
        {
          if (!internal->IsActive())
          {
            internal->real_event = ev->event;
            internal->sig_times = 1;
            internal->AddToActive(&active_sig_ev_);
          }
          else
          {
            internal->sig_times++;
          }

          EV_LOG(kDebug, "Signal Event(%p) is active", ev);
        }
      }
    }// for
  }// OnReadable


  /************************************************************************/
  SignalReactor::SignalReactor() {impl_ = new Impl;}
  SignalReactor::~SignalReactor() {delete impl_;}
  int SignalReactor::Init() {return impl_->Init();}
  void SignalReactor::UnInit() {impl_->UnInit();}
  int SignalReactor::Add(Event * ev) {return impl_->Add(ev);}
  int SignalReactor::Del(Event * ev) {return impl_->Del(ev);}
  int SignalReactor::PollOne() {return impl_->PollOne();}
  int SignalReactor::Poll() {return impl_->Poll();}
  int SignalReactor::RunOne() {return impl_->RunOne();}
  int SignalReactor::Run() {return impl_->Run();}
  int SignalReactor::Stop() {return impl_->Stop();}
  int SignalReactor::fd()const {return impl_->fd();}
  int SignalReactor::GetReadability(int immediate) {return impl_->GetReadability(immediate);}
  void SignalReactor::OnReadable() {impl_->OnReadable();}

}
