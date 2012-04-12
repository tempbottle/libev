/** @file
* @brief libev implementation
* @author zhangyafeikimi@gmail.com
* @date
* @version
*
*/
#include "ev.h"
#include "interrupter.h"
#include "list.h"
#include "log.h"
#include "scoped_ptr.h"
#include "header.h"
#include <vector>

#define my_offsetof(type, member) (size_t)((char *)&(((type *)1)->member) - 1)
#define my_container_of(ptr, type, member) ((type *)((char *)ptr - my_offsetof(type, member)))

namespace libev {

  static int CheckInputEventFlag(int flags)
  {
    int count = 0;

    if (flags & kEvErr)
    {
      EV_LOG(kError, "kEvErr must not be set");
      errno = EINVAL;
      return kEvFailure;
    }

    if (flags & kEvCanceled)
    {
      EV_LOG(kError, "kEvCanceled must not be set");
      errno = EINVAL;
      return kEvFailure;
    }

    if (flags & kEvIO) count++;
    if (flags & kEvSignal) count++;
    if (flags & kEvTimer) count++;

    if (count > 1)
    {
      EV_LOG(kError, "kEvIO, kEvSignal, kEvTimer are mutual exclusive");
      errno = EINVAL;
      return kEvFailure;
    }
    else if (count < 1)
    {
      EV_LOG(kError, "No valid event flags is set");
      errno = EINVAL;
      return kEvFailure;
    }
    else
    {
      return kEvOK;
    }
  }

  /************************************************************************/
  enum EventInternalFlag
  {
    kInAllList = 0x01,
    kInActiveList = 0x02,
  };

  struct EventInternal
  {
    Event * const parent;

    ListNode all;// node in all event list
    ListNode active;// node in active event list

    int real_event;// the real event to be passed to callback
    int sig_times;// the times that the signal is triggered but pending(kEvSignal)
    int flags;// EventInternalFlag

    explicit EventInternal(Event * _parent)
      : parent(_parent), real_event(0), sig_times(0), flags(0)
    {
      EV_ASSERT(parent);
    }

    bool IsInList()const
    {
      return flags & kInAllList;
    }

    void AddToList(List * list)
    {
      EV_ASSERT(list);
      EV_ASSERT(!IsInList());
      list->push_back(&all);
      flags |= kInAllList;
    }

    void DelFromList(List * list)
    {
      EV_ASSERT(list);
      EV_ASSERT(IsInList());
      list->erase(&all);
      flags &= ~kInAllList;
    }

    bool IsActive()const
    {
      return flags & kInActiveList;
    }

    void AddToActive(List * active_list)
    {
      EV_ASSERT(active_list);
      EV_ASSERT(!IsActive());
      active_list->push_back(&active);
      flags |= kInActiveList;
    }

    void DelFromActive(List * active_list)
    {
      EV_ASSERT(active_list);
      EV_ASSERT(IsActive());
      active_list->erase(&active);
      flags &= ~kInActiveList;
    }
  };


  /************************************************************************/
  Event::Event() : internal(NULL)
  {
  }

  Event::Event(int _fd, int _event, ev_callback _callback, void * _udata)
    : fd(_fd), event(_event), callback(_callback), user_data(_udata), internal(NULL)
  {
  }

  Event::~Event()
  {
    if (internal)
      delete internal;
  }


  /************************************************************************/
  class SignalReactor : public Reactor
  {
  private:
    int sigfd_;
    sigset_t sigset_;
    sigset_t old_sigset_;
    List sig_ev_;
    List active_sig_ev_;
    Interrupter interrupter_;

  private:
    // cancel all events
    void CancelAll()
    {
      ListNode * node;
      EventInternal * internal;
      Event * ev;

      for (node = sig_ev_.front(); node != sig_ev_.end() ; node = node->next)
      {
        EV_ASSERT(node);

        internal = my_container_of(node, EventInternal, all);
        ev = internal->parent;

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

    void InvokeCallback(Event * ev)
    {
      EV_ASSERT(ev);
      EV_ASSERT(ev->callback);
      EventInternal * internal = ev->internal;
      EV_ASSERT(internal);
      EV_ASSERT(internal->IsInList());

      EV_LOG(kDebug, "Signal Event(%p)'s callback is to be invoked", ev);
      ev->callback(ev->fd, internal->real_event, ev->user_data);
      internal->sig_times--;
      EV_LOG(kDebug, "Signal Event(%p)'s callback has been invoked", ev);

      if (!(internal->real_event & kEvPersist)
        || (internal->real_event & kEvCanceled))
      {
        internal->DelFromList(&sig_ev_);
        EV_LOG(kDebug, "Signal Event(%p) is not valid any more", ev);
        return;
      }

      if (internal->sig_times)
      {
        internal->AddToActive(&active_sig_ev_);
        EV_LOG(kDebug, "Signal Event(%p) is still active", ev);
      }
    }

    // if 'limit' > 0, execute at most 'limit' ready event
    // if 'limit' == 0, execute all ready events
    // return the number of executed event
    int PollImpl(int limit)
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

          internal = my_container_of(node, EventInternal, active);
          internal->DelFromActive(&active_sig_ev_);
          ev = internal->parent;

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

    // if 'limit' > 0, execute at most 'limit' event, or until interrupted
    // if 'limit' == 0, execute all events, or until interrupted
    // return the number of executed event
    int RunImpl(int limit)
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

          internal = my_container_of(node, EventInternal, active);
          internal->DelFromActive(&active_sig_ev_);
          ev = internal->parent;

          InvokeCallback(ev);
          number++;

          if (limit > 0 && number == limit)
            return number;
        }

        result = GetReadability(0);
        if (result == -1)
          return number;
        else if (result == 1)
          OnReadable();
        else
        {
          if (sig_ev_.empty())
            return number;
        }
      }
    }

  public:
    SignalReactor() : sigfd_(-1)
    {
      EV_VERIFY(sigprocmask(0, NULL, &old_sigset_) == 0);
    }

    ~SignalReactor()
    {
      UnInit();
      EV_VERIFY(sigprocmask(SIG_SETMASK, &old_sigset_, NULL) == 0);
    }

    int Init()
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

      return kEvOK;
    }

    void UnInit()
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

    int Add(Event * ev)
    {
      EV_ASSERT(ev);

      int result;
      result = CheckInputEventFlag(ev->event);
      if (result != kEvOK)
        return result;

      EV_ASSERT(ev->event & kEvSignal);
      EV_ASSERT(ev->callback);

      if (ev->internal == NULL)
      {
        ev->internal = new EventInternal(ev);// may throw
      }
      else
      {
        if (!ev->internal->IsInList())
        {
          EV_LOG(kError, "Signal Event(%p) is added before", ev);
          return kEvExists;
        }
        EV_ASSERT(!ev->internal->IsActive());
        ev->internal->sig_times = 0;
      }

      int signum = ev->fd;
      int sigfd;
      sigaddset(&sigset_, signum);
      sigfd = signalfd(sigfd_, &sigset_, SFD_CLOEXEC|SFD_NONBLOCK);
      if (sigfd == -1)
      {
        sigdelset(&sigset_, signum);
        EV_LOG(kError, "signalfd: %s", strerror(errno));
        return kEvFailure;
      }
      EV_ASSERT(sigfd == sigfd_);

      sigset_t tmp_sigset;
      sigemptyset(&tmp_sigset);
      sigaddset(&tmp_sigset, signum);
      EV_VERIFY(sigprocmask(SIG_BLOCK, &tmp_sigset, NULL) == 0);

      ev->internal->AddToList(&sig_ev_);
      EV_LOG(kDebug, "Signal Event(%p) has been added", ev);
      return kEvOK;
    }

    int Del(Event * ev)
    {
      EV_ASSERT(ev);

      EventInternal * internal = ev->internal;
      if (internal == NULL || !internal->IsInList())
      {
        EV_LOG(kError, "Signal Event(%p) is not added before", ev);
        return kEvNotExists;
      }

      int signum = ev->fd;
      int sigfd;
      sigdelset(&sigset_, signum);
      sigfd = signalfd(sigfd_, &sigset_, SFD_CLOEXEC|SFD_NONBLOCK);
      if (sigfd == -1)
      {
        sigaddset(&sigset_, signum);
        EV_LOG(kError, "signalfd: %s", strerror(errno));
        return kEvFailure;
      }
      EV_ASSERT(sigfd == sigfd_);

      sigset_t tmp_sigset;
      sigemptyset(&tmp_sigset);
      sigaddset(&tmp_sigset, signum);
      EV_VERIFY(sigprocmask(SIG_UNBLOCK, &tmp_sigset, NULL) == 0);

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
      return kEvOK;
    }

    int PollOne()
    {
      return PollImpl(1);
    }

    int Poll()
    {
      return PollImpl(0);
    }

    int RunOne()
    {
      return RunImpl(1);
    }

    int Run()
    {
      return RunImpl(0);
    }

    int Stop()
    {
      return interrupter_.Interrupt();
    }

    // return 1, readable
    // return 0, unreadable
    // return -1, interrupted
    int GetReadability(int immediate)
    {
      if (immediate)
      {
        struct pollfd poll_fds[1];
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
        struct pollfd poll_fds[2];
        poll_fds[0].fd = sigfd_;
        poll_fds[0].events = POLLIN;
        poll_fds[1].fd = interrupter_.fd();
        poll_fds[1].events = POLLIN;

        interrupter_.Reset();
        int result = poll(poll_fds, 2, -1);
        if (result <= 0)
          return 0;

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

    void OnReadable()
    {
      signalfd_siginfo siginfo;
      int result;
      int signum;
      ListNode * node;
      EventInternal * internal;
      Event * ev;

      for (;;)
      {
        result = read(sigfd_, &siginfo, sizeof(siginfo));
        if (result == -1)
        {
          if (errno == EAGAIN)
            return;
          if (errno == EINTR)
            continue;
        }
        EV_ASSERT(result == sizeof(siginfo));

        signum = siginfo.ssi_signo;

        // traverse 'sig_ev_' to match all events targeting 'signum'
        for (node = sig_ev_.front(); node != sig_ev_.end() ; node = node->next)
        {
          EV_ASSERT(node);

          internal = my_container_of(node, EventInternal, all);
          ev = internal->parent;

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
  };


  ///************************************************************************/
  //class EvImpl
  //{
  //private:
  //  /************************************************************************/
  //  struct EpollReactor
  //  {
  //    struct IOEvent
  //    {
  //      Event * event_in;
  //      Event * event_out;

  //      IOEvent() : event_in(NULL), event_out(NULL) {}
  //    };

  //    EvImpl * const ev_impl;
  //    SignalReactor * const signal_data;
  //    int epfd;
  //    std::vector<IOEvent> io_events;// fd to IOEvent array
  //    std::vector<epoll_event> ep_events;// for epoll_wait

  //    EpollReactor(EvImpl * _ev_impl, SignalReactor * _signal_data)
  //      : ev_impl(_ev_impl), signal_data(_signal_data), epfd(-1)
  //    {
  //    }

  //    ~EpollReactor()
  //    {
  //      UnInit();
  //    }

  //    void Resize(size_t fd)
  //    {
  //      size_t new_size = io_events.size();
  //      if (fd < new_size)
  //        return;

  //      while (new_size <= fd)
  //        new_size <<= 1;
  //      io_events.resize(new_size);// may throw
  //    }

  //    int Init()
  //    {
  //      epoll_event sig_ep_event;

  //      UnInit();

  //      io_events.resize(32);// magic // may throw
  //      ep_events.resize(32);// magic // may throw
  //      epfd = epoll_create(10000);// magic
  //      if (epfd == -1)
  //      {
  //        EV_LOG(kError, "epoll_create: %s", strerror(errno));
  //        return kEvFailure;
  //      }

  //      // close after execve
  //      if (fcntl(epfd, F_SETFD, 1) == -1)
  //      {
  //        EV_LOG(kWarning, "fcntl: %s", strerror(errno));
  //        // ignore the failure of fcntl
  //      }

  //      sig_ep_event.data.fd = signal_data->sigfd_;
  //      sig_ep_event.events = EPOLLIN|EPOLLET;// enough
  //      if (epoll_ctl(epfd, EPOLL_CTL_ADD, signal_data->sigfd_, &sig_ep_event) == -1)
  //      {
  //        EV_LOG(kError, "epoll_ctl: %s", strerror(errno));
  //        return kEvFailure;
  //      }

  //      return kEvOK;
  //    }

  //    void UnInit()
  //    {
  //      if (epfd != -1)
  //      {
  //        EV_ASSERT(signal_data->sigfd_ != -1);
  //        epoll_ctl(epfd, EPOLL_CTL_DEL, signal_data->sigfd_, NULL);

  //        safe_close(epfd);
  //        epfd = -1;
  //      }

  //      std::vector<IOEvent>().swap(io_events);
  //      std::vector<epoll_event>().swap(ep_events);
  //    }

  //    int Mod(Event * ev)
  //    {
  //      int fd;
  //      IOEvent * io_event;
  //      int op = EPOLL_CTL_ADD, events = EPOLLET;
  //      epoll_event ep_event;

  //      EV_ASSERT(ev->event & (kEvIn|kEvInPri|kEvOut));

  //      fd = ev->fd;
  //      Resize(fd);
  //      io_event = &io_events[fd];

  //      if (io_event->event_in)
  //      {
  //        if ((ev->event & (kEvIn|kEvInPri)) && io_event->event_in != ev)
  //          return kEvExists;

  //        if (io_event->event_in->event & kEvIn)
  //          events |= EPOLLIN;
  //        if (io_event->event_in->event & kEvInPri)
  //          events |= EPOLLPRI;
  //        op = EPOLL_CTL_MOD;
  //      }

  //      if (io_event->event_out)
  //      {
  //        if ((ev->event & kEvOut) && io_event->event_out != ev)
  //          return kEvExists;

  //        events |= EPOLLOUT;
  //        op = EPOLL_CTL_MOD;
  //      }

  //      if (ev->event & kEvIn)
  //        events |= EPOLLIN;
  //      if (ev->event & kEvInPri)
  //        events |= EPOLLPRI;
  //      if (ev->event & kEvOut)
  //        events |= EPOLLOUT;

  //      ep_event.data.fd = fd;
  //      ep_event.events = events;
  //      if (epoll_ctl(epfd, op, fd, &ep_event) == -1)
  //      {
  //        EV_LOG(kError, "epoll_ctl: %s", strerror(errno));
  //        return kEvFailure;
  //      }

  //      if (ev->event & (kEvIn|kEvInPri))
  //        io_event->event_in = ev;
  //      if (ev->event & kEvOut)
  //        io_event->event_out = ev;

  //      return kEvOK;
  //    }

  //    int Del(Event * ev)
  //    {
  //      int fd;
  //      IOEvent * io_event;

  //      EV_ASSERT(ev->event & (kEvIn|kEvInPri|kEvOut));

  //      fd = ev->fd;
  //      if (fd >= static_cast<int>(io_events.size()))
  //        return kEvNotExists;

  //      io_event = &io_events[fd];

  //      if (epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL) == -1)
  //      {
  //        EV_LOG(kError, "epoll_ctl: %s", strerror(errno));
  //        return kEvFailure;
  //      }

  //      if (ev->event & (kEvIn|kEvInPri))
  //        io_event->event_in = NULL;
  //      if (ev->event & kEvOut)
  //        io_event->event_out = NULL;

  //      return kEvOK;
  //    }

  //    int RunOne()
  //    {
  //      int i, result;
  //      int epevents_size = static_cast<int>(ep_events.size());

  //      for (;;)
  //      {
  //        result = epoll_wait(epfd, &ep_events[0], epevents_size, -1);

  //        if (result == -1)
  //        {
  //          if (errno == EINTR)
  //            continue;
  //          else
  //            return kEvFailure;
  //        }
  //        else if (result == 0)
  //        {
  //          return kEvOK;
  //        }

  //        for (i=0; i<result; i++)
  //        {
  //          int fd = ep_events[i].data.fd;
  //          int events = ep_events[i].events;

  //          if (fd == signal_data->sigfd_)
  //          {
  //            // signal
  //            EV_ASSERT(events & EPOLLIN);
  //            signal_data->OnReadable();
  //          }
  //          else
  //          {
  //            // socket
  //            IOEvent * io_event;
  //            Event * event_in = NULL;
  //            Event * event_out = NULL;

  //            EV_ASSERT(fd > 0 && fd < static_cast<int>(io_events.size()));
  //            io_event = &io_events[fd];

  //            if (events & (EPOLLERR|EPOLLHUP))
  //            {
  //              event_in = io_event->event_in;
  //              event_out = io_event->event_out;
  //            }
  //            else
  //            {
  //              if (events & (EPOLLIN|EPOLLPRI))
  //              {
  //                event_in = io_event->event_in;
  //                EV_ASSERT(event_in);
  //              }
  //              if (events & EPOLLOUT)
  //              {
  //                event_out = io_event->event_out;
  //                EV_ASSERT(event_out);
  //              }
  //            }

  //            if (event_in)
  //            {
  //              event_in->internal->real_event = event_in->event;
  //              event_in->internal->canceled = 0;
  //              ev_impl->Activate(event_in);
  //            }
  //            if (event_out)
  //            {
  //              event_out->internal->real_event = event_in->event;
  //              event_out->internal->canceled = 0;
  //              ev_impl->Activate(event_out);
  //            }
  //          }
  //        }

  //        if (result == epevents_size && epevents_size < 4096)// magic
  //        {
  //          epevents_size <<= 1;
  //          ep_events.resize(epevents_size);// may throw
  //        }
  //      }

  //      return kEvOK;
  //    }
  //  };


  //  /************************************************************************/
  //private:
  //  List all_ev_;
  //  List active_sig_ev_;
  //  ScopedPtr<SignalReactor> signal_;
  //  ScopedPtr<EpollReactor> epoll_;

  //public:
  //  EvImpl() {}
  //  ~EvImpl() {}

  //  // TODO
  //  int Init() {return kEvFailure;}
  //  int Run() {return kEvFailure;}
  //  int RunOne() {return kEvFailure;}
  //  int Stop() {return kEvFailure;}

  //  int Add(Event * ev) {return kEvFailure;}
  //  int Del(Event * ev) {return kEvFailure;}
  //  int Cancel(Event * ev) {return kEvFailure;}

  //  void Activate(Event * ev)
  //  {
  //    EventInternal * internal = ev->internal;
  //    EV_ASSERT(internal->all.prev && internal->all.next);
  //    EV_ASSERT(internal->active.prev == NULL && internal->active.next == NULL);
  //    active_sig_ev_.push_back(&internal->active);
  //  }

  //  int SafeActivate(Event * ev)
  //  {
  //    EventInternal * internal = ev->internal;
  //    if (!(internal->all.prev && internal->all.next)
  //      || !(internal->active.prev == NULL && internal->active.next == NULL))
  //    {
  //      errno = EINVAL;
  //      return kEvFailure;
  //    }

  //    active_sig_ev_.push_back(&internal->active);
  //    return kEvOK;
  //  }
  //};


  /************************************************************************/
  Reactor * Reactor::CreateSignalReactor()
  {
    return new SignalReactor;
  }
  //Reactor::Reactor() {impl_ = new EvImpl;}
  //Reactor::~Reactor() {delete impl_;}

  //int Reactor::Init() {return impl_->Init();}
  //int Reactor::Run() {return impl_->Run();}
  //int Reactor::RunOne() {return impl_->RunOne();}
  //int Reactor::Stop() {return impl_->Stop();}

  //int Reactor::Add(Event * ev) {return impl_->Add(ev);}
  //int Reactor::Del(Event * ev) {return impl_->Del(ev);}
  //int Reactor::Cancel(Event * ev) {return impl_->Cancel(ev);}
}
