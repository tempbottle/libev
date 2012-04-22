/** @file
* @brief reactor
* @author zhangyafeikimi@gmail.com
* @date
* @version
*
*/
#include "ev.h"
#include "log.h"
#include "heap.h"
#include "interrupter.h"
#include "header.h"
#include <vector>

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

    // members about timer events
    int timerfd_;
    Heap min_time_heap_;

    // members about io events
    struct IOEvent
    {
      Event * event_in;
      Event * event_out;
      IOEvent() : event_in(0), event_out(0) {}
    };
    int epfd_;
    std::vector<IOEvent> fd_2_io_ev_;// fd to IOEvent array
    std::vector<epoll_event> ep_ev_;// for epoll_wait

    // common members
    List ev_list_;// event list
    List sig_ev_list_;// signal event list
    List active_ev_list_;// active event list
    Interrupter interrupter_;

    // temporary members helping invoke callback
    int ev_cleaned_;// the event is cleaned
    int ev_canceled_;// the event is canceled by user inside its callback

  private:
    void AddSignalRef(int signum);
    void ReleaseSignalRef(int signum);
    void ScheduleTimer();
    void ResizeIOEvent(int fd);

    // add/del 'ev' to/from 'ev_list_' or 'sig_ev_list_' according to its type
    void AddToList(Event * ev);
    void DelFromList(Event * ev);
    // setup/cleanup 'ev'(mainly affairs about system) according to its type
    int Setup(Event * ev);
    void CleanUp(Event * ev);
    // cancel 'ev' inside callback
    void CancelInsideCB(Event * ev);
    // cancel 'ev' outside callback
    void CancelOutsideCB(Event * ev);
    // cancel all events
    void CancelAll();

    // invoke callback of 'ev'
    void InvokeCallback(Event * ev);

    void OnSignalReadable();
    void OnTimerReadable();

    // poll and execute ready events
    // if 'limit' > 0, execute at most 'limit' ready events
    // if 'limit' == 0, execute all ready events
    // if 'blocking' == 1, run until no new ready events, or until interrupted
    // if 'blocking' == 0, run until no events, or until interrupted
    // return the number of executed event
    int PollImpl(int limit, int blocking);

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
      EV_VERIFY(sigprocmask(SIG_BLOCK, &tmp_sigset, 0) != -1);

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
      EV_VERIFY(sigprocmask(SIG_UNBLOCK, &tmp_sigset, 0) != -1);

      EV_LOG(kDebug, "Signal(%d) has been unblocked and deleted", signum);
    }
  }

  void ReactorImpl::ScheduleTimer()
  {
    if (min_time_heap_.empty())
      return;

    const Event * ev = min_time_heap_.top();

    itimerspec timerspec;
    timerspec.it_value = ev->timeout;
    timerspec.it_interval.tv_sec = 0;
    timerspec.it_interval.tv_nsec = 0;

    EV_LOG(kDebug, "timerfd_settime: seconds=%ld nanoseconds=%ld",
      static_cast<long>(timerspec.it_value.tv_sec), timerspec.it_value.tv_nsec);
    EV_VERIFY(timerfd_settime(timerfd_, TFD_TIMER_ABSTIME, &timerspec, 0) != -1);
  }

  void ReactorImpl::ResizeIOEvent(int fd)
  {
    EV_ASSERT(fd >= 0);

    size_t sfd = static_cast<size_t>(fd);
    size_t new_size = fd_2_io_ev_.size();
    EV_ASSERT(new_size != 0);
    if (sfd < new_size)
      return;

    while (new_size <= sfd)
      new_size <<= 1;//double the size of 'fd_2_io_ev_'
    fd_2_io_ev_.resize(new_size);// may throw
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

  int ReactorImpl::Setup(Event * ev)
  {
    if (ev->event & kEvSignal)
    {
      AddSignalRef(ev->fd);
    }
    else if (ev->event & kEvTimer)
    {
      ev->fd = -1;
      min_time_heap_.push(ev);

      if (ev->fd == 0)
        ScheduleTimer();
    }
    else if (ev->event & kEvIO)
    {
      int fd = ev->fd;
      ResizeIOEvent(fd);
      IOEvent * io_event = &fd_2_io_ev_[fd];

      if ((ev->event & kEvIn) && io_event->event_in)
      {
        EV_LOG(kError, "(Another) IO Event(%p) has been added before", io_event->event_in);
        return kEvExists;
      }
      if ((ev->event & kEvOut) && io_event->event_out)
      {
        EV_LOG(kError, "(Another) IO Event(%p) has been added before", io_event->event_out);
        return kEvExists;
      }

      int op = EPOLL_CTL_ADD;
      int events = 0;
      if (ev->event & kEvET)
        events |= EPOLLET;

      if (io_event->event_in)
      {
        events |= EPOLLIN;
        op = EPOLL_CTL_MOD;
      }
      if (io_event->event_out)
      {
        events |= EPOLLOUT;
        op = EPOLL_CTL_MOD;
      }
      if (ev->event & kEvIn)
        events |= EPOLLIN;
      if (ev->event & kEvOut)
        events |= EPOLLOUT;

      epoll_event epev;
      epev.data.u64 = 0;// suppress valgrind warnings
      epev.data.fd = fd;
      epev.events = events;
      EV_LOG(kDebug, "epoll_ctl: op=%d fd=%d", op, fd);
      if (epoll_ctl(epfd_, op, fd, &epev) == -1)
      {
        EV_LOG(kError, "epoll_ctl: %s", strerror(errno));
        return kEvFailure;
      }

      if (ev->event & kEvIn)
        io_event->event_in = ev;
      if (ev->event & kEvOut)
        io_event->event_out = ev;
    }

    ev->real_event = 0;
    ev->triggered_times = 0;
    ev->flags = 0;
    ev->reactor = this;
    return kEvOK;
  }

  void ReactorImpl::CleanUp(Event * ev)
  {
    int fd = ev->fd;

    if (ev->event & kEvSignal)
    {
      ReleaseSignalRef(fd);
    }
    else if (ev->event & kEvTimer)
    {
      // nothing to do
    }
    else if (ev->event & kEvIO)
    {
      IOEvent * io_event = &fd_2_io_ev_[fd];
      int op = EPOLL_CTL_DEL;
      int events = 0;
      int del_in = 1, del_out = 1;

      if (ev->event & kEvIn)
        events |= EPOLLIN;
      if (ev->event & kEvOut)
        events |= EPOLLOUT;

      // 'ev->event' has only one flag: kEvIn or kEvOut
      if ((events & (EPOLLIN|EPOLLOUT)) != (EPOLLIN|EPOLLOUT))
      {
        if ((events & EPOLLIN) && io_event->event_out)
        {
          del_out = 0;
          events = EPOLLOUT;
          op = EPOLL_CTL_MOD;
        }
        else if ((events & EPOLLOUT) && io_event->event_in)
        {
          del_in = 0;
          events = EPOLLIN;
          op = EPOLL_CTL_MOD;
        }
      }

      epoll_event epev;
      epev.data.u64 = 0;// suppress valgrind warnings
      epev.events = events;
      epev.data.fd = fd;
      EV_LOG(kDebug, "epoll_ctl: op=%d fd=%d", op, fd);
      EV_VERIFY(epoll_ctl(epfd_, op, fd, &epev) != -1);

      if (del_in)
        io_event->event_in = 0;
      if (del_out)
        io_event->event_out = 0;
    }

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
      ev = ev_container_of(node, Event, _all);
      CancelOutsideCB(ev);
    }

    for (node = sig_ev_list_.front(); node != sig_ev_list_.end() ; node = node->next)
    {
      ev = ev_container_of(node, Event, _all);
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

    if (ev->event & (kEvSignal|kEvTimer))
    {
      if (--ev->triggered_times > 0)
      {
        ev->AddToActive(&active_ev_list_);
        EV_LOG(kDebug, "Event(%p) is still active, times=%d", ev, ev->triggered_times);
      }
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
        ev = ev_container_of(node, Event, _all);

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

          EV_LOG(kDebug, "Signal Event(%p) is active", ev);
        }
      }
    }// for
  }

  void ReactorImpl::OnTimerReadable()
  {
    int result;
    uint64_t expire;

    for (;;)
    {
      do result = read(timerfd_, &expire, sizeof(expire));
      while (result == -1 && errno == EINTR);

      if (result == -1 && errno == EAGAIN)
        break;
    }

    timespec now;
    EV_VERIFY(clock_gettime(CLOCK_MONOTONIC, &now) != -1);

    Event * ev;
    while (!min_time_heap_.empty())
    {
      ev = min_time_heap_.top();

      if (timespec_le(&ev->timeout, &now))
      {
        min_time_heap_.pop();

        ev->real_event = ev->event;
        ev->AddToActive(&active_ev_list_);

        EV_LOG(kDebug, "Timer Event(%p) is active", ev);
      }
      else
      {
        break;
      }
    }

    ScheduleTimer();
  }

  int ReactorImpl::PollImpl(int limit, int blocking)
  {
    EV_ASSERT(limit >= 0);

    int number = 0;
    int i, result;
    int epevents_size = static_cast<int>(ep_ev_.size());
    ListNode * node;
    Event * ev;
    int timeout = (blocking)?(-1):(0);

    for (;;)
    {
      // 1.handle active events in 'active_ev_list_'
      while (!active_ev_list_.empty())
      {
        node = active_ev_list_.front();
        ev = ev_container_of(node, Event, _active);
        ev->DelFromActive(&active_ev_list_);

        InvokeCallback(ev);
        number++;

        if (limit > 0 && number == limit)
          return number;
      }

      // 2.check to quit in blocking mode
      if (blocking && ev_list_.empty() && sig_ev_list_.empty())
      {
        EV_LOG(kDebug, "Event loop quits for no events");
        return number;
      }

      // 3.epoll_wait
      EV_LOG(kDebug, "epoll_wait");
      do result = epoll_wait(epfd_, &ep_ev_[0], epevents_size, timeout);
      while (result == -1 && errno == EINTR);
      EV_LOG(kDebug, "after epoll_wait");

      if (result == -1)
        return kEvFailure;

      EV_ASSERT(result >= 0);

      for (i=0; i<result; i++)
      {
        int fd = ep_ev_[i].data.fd;
        int events = ep_ev_[i].events;

        if (fd == interrupter_.fd())
        {
          // interrupted
          interrupter_.Reset();
          return number;
        }
        else if (fd == sigfd_)
        {
          // signal
          EV_ASSERT(events & EPOLLIN);
          OnSignalReadable();
        }
        else if (fd == timerfd_)
        {
          // timer
          EV_ASSERT(events & EPOLLIN);
          OnTimerReadable();
        }
        else
        {
          // io
          IOEvent * io_event;
          Event * event_in = 0;
          Event * event_out = 0;
          int real_event = 0;

          io_event = &fd_2_io_ev_[fd];

          if (events & (EPOLLERR|EPOLLHUP))
          {
            event_in = io_event->event_in;
            event_out = io_event->event_out;
            real_event |= kEvErr;
            EV_ASSERT(event_in || event_out);
          }
          else
          {
            if (events & EPOLLIN)
            {
              event_in = io_event->event_in;
              real_event |= kEvIn;
              EV_ASSERT(event_in);
            }
            if (events & EPOLLOUT)
            {
              event_out = io_event->event_out;
              real_event |= kEvOut;
              EV_ASSERT(event_out);
            }
          }

          if (event_in)
          {
            event_in->real_event = real_event;
            event_in->AddToActive(&active_ev_list_);
            EV_LOG(kDebug, "IO Event(%p) is active", event_in);
          }
          if (event_out && event_out != event_in)
          {
            event_out->real_event = real_event;
            event_out->AddToActive(&active_ev_list_);
            EV_LOG(kDebug, "IO Event(%p) is active", event_out);
          }
        }
      }

      // 4.check to quit in non-blocking mode
      if (!blocking && result == 0 && active_ev_list_.empty())
      {
        EV_LOG(kDebug, "Event loop quits for no new ready events");
        return number;
      }

      // 5.check and resize 'ep_ev_' to make epoll_wait get more results
      if (result == epevents_size && epevents_size < 4096)// magic
      {
        epevents_size <<= 1;
        ep_ev_.resize(epevents_size);// may throw
      }
    }// for
  }

  ReactorImpl::ReactorImpl() : sigfd_(-1), timerfd_(-1), epfd_(-1)
  {
    EV_VERIFY(sigprocmask(0, 0, &old_sigset_) != -1);
  }

  ReactorImpl::~ReactorImpl()
  {
    UnInit();
    EV_VERIFY(sigprocmask(SIG_SETMASK, &old_sigset_, 0) != -1);
  }

  int ReactorImpl::Init()
  {
    // initialize members that may throw first
    fd_2_io_ev_.resize(32);// magic // may throw
    ep_ev_.resize(32);// magic // may throw


    sigemptyset(&sigset_);
    sigfd_ = signalfd(-1, &sigset_, SFD_CLOEXEC|SFD_NONBLOCK);
    if (sigfd_ == -1)
    {
      EV_LOG(kError, "signalfd: %s", strerror(errno));
      return kEvFailure;
    }
    for (int i=0; i<_NSIG; i++)
    {
      sig_ev_refcount_[i] = 0;
    }


    timerfd_ = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC|TFD_NONBLOCK);
    if (timerfd_ == -1)
    {
      safe_close(sigfd_);
      sigfd_ = -1;
      EV_LOG(kError, "timerfd_create: %s", strerror(errno));
      return kEvFailure;
    }


    epfd_ = epoll_create(20000);// magic
    if (epfd_ == -1)
    {
      safe_close(timerfd_);
      timerfd_ = -1;
      safe_close(sigfd_);
      sigfd_ = -1;
      EV_LOG(kError, "epoll_create: %s", strerror(errno));
      return kEvFailure;
    }
    // close after execve
    if (fcntl(epfd_, F_SETFD, 1) == -1)
    {
      EV_LOG(kWarning, "fcntl: %s", strerror(errno));
      // ignore the failure of fcntl
    }


    if (interrupter_.Init() != kEvOK)
    {
      safe_close(epfd_);
      epfd_ = -1;
      safe_close(timerfd_);
      timerfd_ = -1;
      safe_close(sigfd_);
      sigfd_ = -1;
      return kEvFailure;
    }
    interrupter_.Reset();


    epoll_event epev[3];
    epev[0].data.u64 = 0;// suppress valgrind warnings
    epev[0].data.fd = sigfd_;
    epev[1].data.u64 = 0;
    epev[1].data.fd = timerfd_;
    epev[2].data.u64 = 0;
    epev[2].data.fd = interrupter_.fd();

    for (int i=0; i<3; i++)
    {
      epev[i].events = EPOLLIN|EPOLLET;
      if (epoll_ctl(epfd_, EPOLL_CTL_ADD, epev[i].data.fd, &epev[i]) == -1)
      {
        interrupter_.UnInit();
        safe_close(epfd_);
        epfd_ = -1;
        safe_close(timerfd_);
        timerfd_ = -1;
        safe_close(sigfd_);
        sigfd_ = -1;
        EV_LOG(kError, "epoll_ctl: %s", strerror(errno));
        return kEvFailure;
      }
    }

    return kEvOK;
  }

  void ReactorImpl::UnInit()
  {
    CancelAll();
    Poll(0);

    interrupter_.UnInit();

    if (epfd_ != -1)
    {
      safe_close(epfd_);
      epfd_ = -1;
    }

    if (timerfd_ != -1)
    {
      safe_close(timerfd_);
      timerfd_ = -1;
    }

    if (sigfd_ != -1)
    {
      safe_close(sigfd_);
      sigfd_ = -1;
    }

    std::vector<IOEvent>().swap(fd_2_io_ev_);
    std::vector<epoll_event>().swap(ep_ev_);
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

    if ((ret = Setup(ev)) != kEvOK)
      return ret;

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

    if (ev->reactor != this)
    {
      EV_LOG(kError, "Event(%p) is not added before", ev);
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
      if (ev->IsActive())
        ev->DelFromActive(&active_ev_list_);
      CleanUp(ev);
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

    if (ev->reactor != this)
    {
      EV_LOG(kError, "Event(%p) is not added before", ev);
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

  int ReactorImpl::Poll(int limit)
  {
    return PollImpl(limit, 0);
  }

  int ReactorImpl::Run(int limit)
  {
    return PollImpl(limit, 1);
  }

  int ReactorImpl::Stop()
  {
    return interrupter_.Interrupt();
  }


  /************************************************************************/
  Reactor::Reactor() {impl_ = new ReactorImpl;}// may throw
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
