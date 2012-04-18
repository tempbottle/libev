/** @file
* @brief epoll reactor
* @author zhangyafeikimi@gmail.com
* @date
* @version
*
*/
#include "ev.h"
#include "interrupter.h"
#include "header.h"
#include <vector>

namespace libev {

  class EpollReactor::Impl : public Reactor
  {
  private:
    DISALLOW_COPY_AND_ASSIGN(Impl);

    struct IOEvent
    {
      Event * event_in;
      Event * event_out;
      IOEvent() : event_in(0), event_out(0) {}
    };

    SignalReactor * const signal_;
    epoll_event signal_event_;

    int epfd_;
    std::vector<IOEvent> io_ev_;// fd to IOEvent array
    List active_io_ev_;
    std::vector<epoll_event> ep_ev_;// for epoll_wait
    Interrupter interrupter_;

  private:
    void ResizeIOEvent(int fd);
    // return 1, has 'ev'
    // return 0, has not'ev'
    int HasEvent(Event * ev);
    int DelEvent(Event * ev);

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
    explicit Impl(SignalReactor * signal);
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
  };


  /************************************************************************/
  void EpollReactor::Impl::ResizeIOEvent(int fd)
  {
    EV_ASSERT(fd >= 0);

    size_t sfd = static_cast<size_t>(fd);
    size_t new_size = io_ev_.size();
    if (sfd < new_size)
      return;

    while (new_size <= sfd)
      new_size <<= 1;
    io_ev_.resize(new_size);// may throw
  }


  int EpollReactor::Impl::HasEvent(Event * ev)
  {
    EV_ASSERT(ev);

    int fd = ev->fd;
    if (fd >= static_cast<int>(io_ev_.size()))
      return 0;

    IOEvent * io_event = &io_ev_[fd];

    if ((ev->event & kEvIn) && io_event->event_in == ev)
      return 1;

    if ((ev->event & kEvOut) && io_event->event_out == ev)
      return 1;

    return 0;
  }

  int EpollReactor::Impl::DelEvent(Event * ev)
  {
    //int fd;
    //IOEvent * io_event;

    //EV_ASSERT(ev->event & (kEvIn|kEvOut));

    //fd = ev->fd;
    //if (fd >= static_cast<int>(io_ev_.size()))
    //  return kEvNotExists;

    //io_event = &io_ev_[fd];

    //if (epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, 0) == -1)
    //{
    //  EV_LOG(kError, "epoll_ctl: %s", strerror(errno));
    //  return kEvFailure;
    //}

    //if (ev->event & (kEvIn|kEvInPri))
    //  io_event->event_in = 0;
    //if (ev->event & kEvOut)
    //  io_event->event_out = 0;
  }

  void EpollReactor::Impl::CancelAll()
  {
    //  ListNode * node;
    //  EventInternal * internal;
    //  Event * ev;

    //  for (node = sig_ev_.front(); node != sig_ev_.end() ; node = node->next)
    //  {
    //    EV_ASSERT(node);

    //    internal = ev_container_of(node, EventInternal, all);
    //    ev = ev_container_of(internal, Event, internal);

    //    if (!internal->IsActive())
    //    {
    //      internal->real_event = kEvCanceled;// only kEvCanceled
    //      internal->AddToActive(&active_sig_ev_);
    //    }
    //    else
    //    {
    //      internal->real_event |= kEvCanceled;// previous event with kEvCanceled
    //    }

    //    EV_LOG(kDebug, "IO Event(%p) has been canceled", ev);
    //  }
  }

  void EpollReactor::Impl::InvokeCallback(Event * ev)
  {
    //  EV_ASSERT(ev);
    //  EV_ASSERT(ev->callback);
    //  EventInternal * internal = &ev->internal;
    //  EV_ASSERT(internal);
    //  EV_ASSERT(internal->IsInList());

    //  //EV_LOG(kDebug, "IO Event(%p)'s callback is to be invoked", ev);
    //  int perist_before = ev->event & kEvPersist;
    //  internal->real_event |= kInCallback;
    //  ev->callback(ev->fd, internal->real_event, ev->user_data);
    //  internal->real_event &= ~kInCallback;
    //  int perist_after = ev->event & kEvPersist;
    //  internal->sig_times--;
    //  //EV_LOG(kDebug, "IO Event(%p)'s callback has been invoked", ev);

    //  int still_in_list = internal->IsInList();
    //  int remove_persist = (perist_before && !perist_after);
    //  int unpersist = !(internal->real_event & kEvPersist);
    //  int canceled = (internal->real_event & kEvCanceled);

    //  if (!still_in_list || remove_persist || unpersist || canceled)
    //  {
    //    if (still_in_list)
    //      internal->DelFromList(&sig_ev_);

    //    if (!still_in_list)
    //      EV_LOG(kDebug, "IO Event(%p) has been canceled by user inside its callback", ev);
    //    else if (remove_persist)
    //      EV_LOG(kDebug, "IO Event(%p)'s kEvPersist is turned off by user", ev);
    //    else if (unpersist)
    //      EV_LOG(kDebug, "IO Event(%p) has no kEvPersist", ev);
    //    else if (canceled)
    //      EV_LOG(kDebug, "IO Event(%p) has been canceled", ev);

    //    ReleaseSignalRef(ev->fd);

    //    return;
    //  }

    //  if (internal->sig_times)
    //  {
    //    internal->AddToActive(&active_sig_ev_);
    //    EV_LOG(kDebug, "IO Event(%p) is still active, times=%d", ev, internal->sig_times);
    //  }
  }

  //int EpollReactor::Impl::PollImpl(int limit)
  //{
  //  EV_ASSERT(limit >= 0);

  //  int number = 0;
  //  ListNode * node;
  //  EventInternal * internal;
  //  Event * ev;

  //  for (;;)
  //  {
  //    while (!active_sig_ev_.empty())
  //    {
  //      node = active_sig_ev_.front();
  //      EV_ASSERT(node);

  //      internal = ev_container_of(node, EventInternal, active);
  //      internal->DelFromActive(&active_sig_ev_);
  //      ev = ev_container_of(internal, Event, internal);

  //      InvokeCallback(ev);
  //      number++;

  //      if (limit > 0 && number == limit)
  //        return number;
  //    }

  //    if (GetReadability(1) == 1)
  //      OnReadable();
  //    else
  //      return number;
  //  }
  //}

  //int EpollReactor::Impl::RunImpl(int limit)
  //{
  //  EV_ASSERT(limit >= 0);

  //  int number = 0;
  //  ListNode * node;
  //  EventInternal * internal;
  //  Event * ev;
  //  int result;

  //  for (;;)
  //  {
  //    while (!active_sig_ev_.empty())
  //    {
  //      node = active_sig_ev_.front();
  //      EV_ASSERT(node);

  //      internal = ev_container_of(node, EventInternal, active);
  //      internal->DelFromActive(&active_sig_ev_);
  //      ev = ev_container_of(internal, Event, internal);

  //      InvokeCallback(ev);
  //      number++;

  //      if (limit > 0 && number == limit)
  //        return number;
  //    }

  //    if (sig_ev_.empty())
  //    {
  //      EV_LOG(kDebug, "Signal event loop quits for no events");
  //      return number;
  //    }

  //    result = GetReadability(0);
  //    if (result == -1)
  //    {
  //      EV_LOG(kDebug, "Signal event loop quits for an interruption");
  //      return number;
  //    }
  //    else if (result == 1)
  //      OnReadable();
  //    else
  //      EV_ASSERT(0);
  //  }
  //}

  EpollReactor::Impl::Impl()
    : signal_(0), epfd_(-1)
  {
  }

  EpollReactor::Impl::Impl(SignalReactor * signal)
    : signal_(signal), epfd_(-1)
  {
  }

  EpollReactor::Impl::~Impl()
  {
    UnInit();
  }

  int EpollReactor::Impl::Init()
  {
    UnInit();

    io_ev_.resize(32);// magic // may throw
    ep_ev_.resize(32);// magic // may throw
    epfd_ = epoll_create(20000);// magic
    if (epfd_ == -1)
    {
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
      return kEvFailure;
    }

    if (signal_)
    {
      EV_ASSERT(signal_->fd() != -1);
      signal_event_.data.fd = signal_->fd();
      signal_event_.events = EPOLLIN|EPOLLET;// enough
      if (epoll_ctl(epfd_, EPOLL_CTL_ADD, signal_event_.data.fd, &signal_event_) == -1)
      {
        interrupter_.UnInit();
        safe_close(epfd_);
        epfd_ = -1;
        EV_LOG(kError, "epoll_ctl: %s", strerror(errno));
        return kEvFailure;
      }
    }

    return kEvOK;
  }

  void EpollReactor::Impl::UnInit()
  {
    interrupter_.UnInit();

    if (epfd_ != -1)
    {
      if (signal_)
      {
        EV_ASSERT(signal_event_.data.fd != -1);
        epoll_ctl(epfd_, EPOLL_CTL_DEL, signal_event_.data.fd, 0);
      }

      safe_close(epfd_);
      epfd_ = -1;
    }

    CancelAll();
    Poll();

    std::vector<IOEvent>().swap(io_ev_);
    std::vector<epoll_event>().swap(ep_ev_);
  }

  int EpollReactor::Impl::Add(Event * ev)
  {
    EV_ASSERT(ev);
    EV_ASSERT(ev->fd >= 0);
    EV_ASSERT(CheckInputEventFlag(ev->event) == kEvOK);
    EV_ASSERT(ev->event & kEvIO);
    EV_ASSERT(ev->callback);

    if (ev->internal.IsInList())// TODO
    {
      EV_LOG(kError, "IO Event(%p) has been added before", ev);
      return kEvExists;
    }
    EV_ASSERT(!ev->internal.IsActive());

    int fd = ev->fd;
    ResizeIOEvent(fd);// TODO ¸ºÖµ

    IOEvent * io_event = &io_ev_[fd];
    int op = EPOLL_CTL_ADD, events = (ev->event & kEvET)?EPOLLET:0;
    epoll_event ep_event;

    if (io_event->event_in)
    {
      if ((ev->event & kEvIn) && io_event->event_in != ev)
      {
        EV_LOG(kError, "Another IO Event(%p) has been added before", io_event->event_in);
        return kEvExists;
      }

      if (io_event->event_in->event & kEvIn)
        events |= EPOLLIN;
      op = EPOLL_CTL_MOD;
    }

    if (io_event->event_out)
    {
      if ((ev->event & kEvOut) && io_event->event_out != ev)
      {
        EV_LOG(kError, "Another IO Event(%p) has been added before", io_event->event_out);
        return kEvExists;
      }

      events |= EPOLLOUT;
      op = EPOLL_CTL_MOD;
    }

    if (ev->event & kEvIn)
      events |= EPOLLIN;
    if (ev->event & kEvOut)
      events |= EPOLLOUT;

    ep_event.data.fd = fd;
    ep_event.events = events;
    if (epoll_ctl(epfd_, op, fd, &ep_event) == -1)
    {
      EV_LOG(kError, "epoll_ctl: %s", strerror(errno));
      return kEvFailure;
    }

    if (ev->event & kEvIn)
      io_event->event_in = ev;
    if (ev->event & kEvOut)
      io_event->event_out = ev;

    EV_LOG(kDebug, "IO Event(%p) has been added", ev);
    return kEvOK;
  }

  int EpollReactor::Impl::Del(Event * ev)
  {
    EV_ASSERT(ev);

    EventInternal * internal = &ev->internal;
    if (!internal->IsInList())// TODO
    {
      EV_LOG(kError, "IO Event(%p) is not added before", ev);
      return kEvNotExists;
    }


    if (internal->real_event & kInCallback)
    {
      //int fd = ev->fd; TODO
      //IOEvent * io_event = &io_ev_[fd];

      //if (epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, 0) == -1)
      //{
      //  EV_LOG(kError, "epoll_ctl: %s", strerror(errno));
      //  return kEvFailure;
      //}

      //if (ev->event & kEvIn)
      //  io_event->event_in = 0;
      //if (ev->event & kEvOut)
      //  io_event->event_out = 0;

      EV_LOG(kDebug, "IO Event(%p) is being canceled by user inside its callback", ev);
      return kEvOK;
    }
    else
    {
      if (!internal->IsActive())
      {
        internal->real_event = kEvCanceled;// only kEvCanceled
        internal->AddToActive(&active_io_ev_);
      }
      else
      {
        internal->real_event |= kEvCanceled;// previous event with kEvCanceled
      }

      EV_LOG(kDebug, "IO Event(%p) is being canceled by user outside its callback", ev);
      return kEvOK;
    }

    return kEvOK;
  }


  //int EpollReactor::Impl::PollOne()
  //{
  //  return PollImpl(1);
  //}

  //int EpollReactor::Impl::Poll()
  //{
  //  return PollImpl(0);
  //}




  //    int RunOne()
  //    {
  //      int i, result;
  //      int epevents_size = static_cast<int>(ep_ev_.size());

  //      for (;;)
  //      {
  //        result = epoll_wait(epfd_, &ep_ev_[0], epevents_size, -1);

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
  //          int fd = ep_ev_[i].data.fd;
  //          int events = ep_ev_[i].events;

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
  //            Event * event_in = 0;
  //            Event * event_out = 0;

  //            EV_ASSERT(fd > 0 && fd < static_cast<int>(io_ev_.size()));
  //            io_event = &io_ev_[fd];

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
  //          ep_ev_.resize(epevents_size);// may throw
  //        }
  //      }

  //      return kEvOK;
  //    }
  //  };




  //int EpollReactor::Impl::RunOne()
  //{
  //  return RunImpl(1);
  //}

  //int EpollReactor::Impl::Run()
  //{
  //  return RunImpl(0);
  //}

  //int EpollReactor::Impl::Stop()
  //{
  //  return interrupter_.Interrupt();
  //}

  /************************************************************************/
  EpollReactor::EpollReactor() {impl_ = new Impl;}
  EpollReactor::EpollReactor(SignalReactor * signal) {impl_ = new Impl(signal);}
  EpollReactor::~EpollReactor() {delete impl_;}
  int EpollReactor::Init() {return impl_->Init();}
  void EpollReactor::UnInit() {impl_->UnInit();}
  int EpollReactor::Add(Event * ev) {return impl_->Add(ev);}
  int EpollReactor::Del(Event * ev) {return impl_->Del(ev);}
  int EpollReactor::PollOne() {return impl_->PollOne();}
  int EpollReactor::Poll() {return impl_->Poll();}
  int EpollReactor::RunOne() {return impl_->RunOne();}
  int EpollReactor::Run() {return impl_->Run();}
  int EpollReactor::Stop() {return impl_->Stop();}

}
