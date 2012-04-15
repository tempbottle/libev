/** @file
* @brief signal reactor
* @author zhangyafeikimi@gmail.com
* @date
* @version
*
*/
#include "ev.h"
#include "header.h"

namespace libev {

  void SignalReactor::AddSignalRef(int signum)
  {
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

  void SignalReactor::ReleaseSignalRef(int signum)
  {
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

  void SignalReactor::CancelAll()
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

  void SignalReactor::InvokeCallback(Event * ev)
  {
    EV_ASSERT(ev);
    EV_ASSERT(ev->callback);
    EventInternal * internal = &ev->internal;
    EV_ASSERT(internal);
    EV_ASSERT(internal->IsInList());

    //EV_LOG(kDebug, "Signal Event(%p)'s callback is to be invoked", ev);
    int perist_before = ev->event & kEvPersist;
    internal->real_event |= kInCallback;
    ev->callback(ev->fd, internal->real_event, ev->user_data);
    internal->real_event &= ~kInCallback;
    int perist_after = ev->event & kEvPersist;
    internal->sig_times--;
    //EV_LOG(kDebug, "Signal Event(%p)'s callback has been invoked", ev);

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

  int SignalReactor::PollImpl(int limit)
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

  int SignalReactor::RunImpl(int limit)
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

  SignalReactor::SignalReactor() : sigfd_(-1)
  {
    EV_VERIFY(sigprocmask(0, NULL, &old_sigset_) == 0);
  }

  SignalReactor::~SignalReactor()
  {
    UnInit();
    EV_VERIFY(sigprocmask(SIG_SETMASK, &old_sigset_, NULL) == 0);
  }

  int SignalReactor::Init()
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

  void SignalReactor::UnInit()
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

  int SignalReactor::Add(Event * ev)
  {
    EV_ASSERT(ev);
    EV_ASSERT(CheckInputEventFlag(ev->event) == kEvOK);
    EV_ASSERT(ev->event & kEvSignal);
    EV_ASSERT(ev->callback);

    if (ev->internal.IsInList())
    {
      EV_LOG(kError, "Signal Event(%p) is added before", ev);
      return kEvExists;
    }
    EV_ASSERT(!ev->internal.IsActive());
    ev->internal.sig_times = 0;

    AddSignalRef(ev->fd);
    ev->internal.AddToList(&sig_ev_);
    EV_LOG(kDebug, "Signal Event(%p) has been added", ev);
    return kEvOK;
  }

  int SignalReactor::Del(Event * ev)
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

  int SignalReactor::PollOne()
  {
    return PollImpl(1);
  }

  int SignalReactor::Poll()
  {
    return PollImpl(0);
  }

  int SignalReactor::RunOne()
  {
    return RunImpl(1);
  }

  int SignalReactor::Run()
  {
    return RunImpl(0);
  }

  int SignalReactor::Stop()
  {
    return interrupter_.Interrupt();
  }

  int SignalReactor::GetReadability(int immediate)
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

  void SignalReactor::OnReadable()
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

}
