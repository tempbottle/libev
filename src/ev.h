/** @file
* @brief libev
* @author zhangyafeikimi@gmail.com
* @date
* @version
*
*/
#ifndef LIBEV_EV_H
#define LIBEV_EV_H

#include "ev-internal.h"
#include "interrupter.h"
#include "scoped_ptr.h"
#include <time.h>
#include <signal.h>

namespace libev {

  // if not commented, the 'int' return value means this 'ErrorCode'
  enum ErrorCode
  {
    kEvOK = 0,                // OK
    kEvFailure = -1,          // common or system failures(refer to errno)
    kEvExists = -2,           // something that should not exist exists
    kEvNotExists = -3,        // something that should exist does not exist
  };

  enum EventFlag
  {
    // 1.The following event flag need to be set in Event.event.
    // 2.kEvIO, kEvSignal, kEvTimer are mutual exclusive.
    // 3.all timers use a monotonic clock, timeouts are relative,
    //   please use clock_gettime(CLOCK_MONOTONIC, ...) to get the clock time.
    // 4.EPOLLPRI is not practical, which is not included and implemented.
    // 5.Check kEvIn and kEvOut in callback if kEvIO is set, because kEvIn and kEvOut,
    //   which can be set to poll simultaneously, may not be triggered simultaneously.
    kEvIn = 0x01,             // fd/socket event: fd can be read(EPOLLIN)
    kEvOut = 0x02,            // fd/socket event: fd can be write(EPOLLOUT)
    kEvIO = kEvIn|kEvOut,
    kEvSignal = 0x04,         // signal event
    kEvTimer = 0x08,          // timer event
    kEvPersist = 0x10,        // persistent event
    kEvET = 0x20,             // use edge trigger in epoll(the default is level trigger)

    // 1.The following event flag must not be set in Event.event,
    //   they are to be checked in callback.
    // 2.kEvErr is only for kEvIO.
    // 3.kEvCanceled is due to a cancellation by user or the reactor.
    kEvErr = 0x1000,          // error(EPOLLERR|EPOLLHUP)
    kEvCanceled = 0x2000,     // deleted by the user, or canceled by libev's cleanup
  };

  typedef void (*ev_callback)(int fd_or_signum, int event, void * user_data);

  // check Event.event
  int CheckInputEventFlag(int flags);


  struct Event
  {
  private:
    DISALLOW_COPY_AND_ASSIGN(Event);

  public:
    int fd;                   // fd(kEvIn, kEvOut, kEvTimer) or signal(kEvSignal)
    timespec timeout;         // relative timeout(kEvTimer)
    int event;                // event flags(bit or of EventFlag)
    ev_callback callback;     // callback
    void * user_data;         // user data
    EventInternal internal;// internal data(DO NOT modify it!!!)

    Event();
    Event(int _fd, int _event, ev_callback _callback, void * _udata);
  };


  class Reactor
  {
  public:
    // delete the return value if it is no longer used
    //static Reactor * CreateIOReactor();// for test
    static Reactor * CreateSignalReactor();// for test
    //static Reactor * CreateTimerReactor();// for test
    //static Reactor * CreateReactor();
  public:
    virtual ~Reactor();

    virtual int Init() = 0;
    virtual void UnInit() = 0;

    virtual int Add(Event * ev) = 0;
    virtual int Del(Event * ev) = 0;

    // execute at most one ready event
    // return the number of executed event
    virtual int PollOne() = 0;
    // execute all ready events
    // return the number of executed event
    virtual int Poll() = 0;
    // execute at most one event, or until 'Stop' is called
    // return the number of executed event
    virtual int RunOne() = 0;
    // execute all ready events, or until 'Stop' is called
    // return the number of executed event
    virtual int Run() = 0;
    virtual int Stop() = 0;

    // return 1, readable
    // return 0, unreadable
    // return -1, interrupted
    virtual int GetReadability(int immediate) = 0;
    virtual void OnReadable();
  };


  /************************************************************************/
  class SignalReactor : public Reactor
  {
  private:
    DISALLOW_COPY_AND_ASSIGN(SignalReactor);

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
    SignalReactor();
    virtual ~SignalReactor();

    virtual int Init();
    virtual void UnInit();

    virtual int Add(Event * ev);
    virtual int Del(Event * ev);

    virtual int PollOne();
    virtual int Poll();
    virtual int RunOne();
    virtual int Run();
    virtual int Stop();

    virtual int GetReadability(int immediate);
    virtual void OnReadable();
  };

}

#endif
