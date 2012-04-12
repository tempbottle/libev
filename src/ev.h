/** @file
* @brief libev header
* @author zhangyafeikimi@gmail.com
* @date
* @version
*
* external header
*/
#ifndef LIBEV_EV_H
#define LIBEV_EV_H

#include <time.h>//timespec

namespace libev {

  // if not commented, the 'int' return value means this 'ErrorCode'
  enum ErrorCode
  {
    kEvOK = 0,                // OK
    kEvFailure = -1,          // common or system failures(refer to errno)
    kEvExists = -2,           // something that should not exist exists
    kEvNotExists = -3,        // something that should exist does not exist
    kEvNotSupport = -4,       // something is not supported
  };

  enum EventFlag
  {
    // 1.The following event flag need to be set in Event.event,
    //   and check kEvIn and kEvOut in callback,
    //   because kEvIn and kEvOut, which can be set to an event simultaneously,
    //   may not triggered simultaneously.
    // 2.kEvIO, kEvSignal, kEvTimer are mutual exclusive.
    // 3.all timers use a monotonic clock, timeouts are relative,
    //   please use clock_gettime(CLOCK_MONOTONIC, ...) to get the clock time.
    // 4.EPOLLPRI is not practical, which is not included.
    kEvIn = 0x01,             // fd/socket event: fd can be read(EPOLLIN)
    kEvOut = 0x02,            // fd/socket event: fd can be write(EPOLLOUT)
    kEvIO = kEvIn|kEvOut,
    kEvSignal = 0x04,         // signal event
    kEvTimer = 0x08,          // timer event

    kEvPersist = 0x10,        // persistent event
    kEvET = 0x20,             // use edge trigger in epoll(the default is level trigger)

    // the following event flag must not be set in Event.event,
    //   and check them in callback
    kEvErr = 0x1000,          // error(EPOLLERR|EPOLLHUP)
    kEvCanceled = 0x2000,     // deleted by the user, or canceled by libev's cleanup
  };


  typedef void (*ev_callback)(int fd, int event, void * user_data);

  struct EventInternal;
  struct Event
  {
  private:
    Event(const Event&);
    Event& operator=(const Event&);

  public:
    int fd;                   // fd(kEvIn, kEvOut, kEvTimer) or signal(kEvSignal)
    timespec timeout;         // relative timeout(kEvTimer)
    int event;                // event flags(bit or of EventFlag)
    ev_callback callback;     // callback
    void * user_data;         // user data
    EventInternal * internal; // internal data(DO NOT modify it!!!)

    Event();
    Event(int _fd, int _event, ev_callback _callback, void * _udata);
    ~Event();
  };


  class Reactor
  {
  public:
    // delete the return value if it is no longer used
    static Reactor * CreateIOReactor();// for test // may throw
    static Reactor * CreateSignalReactor();// for test // may throw
    static Reactor * CreateTimerReactor();// for test // may throw
    static Reactor * CreateReactor();// may throw
  public:
    virtual ~Reactor() {}

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
  };
}

#endif
