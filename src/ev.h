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
#include "list.h"
#include <time.h>//struct timespec

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
    // 3.all timers use a monotonic clock, timeouts are absolute and monotonic,
    //   please use clock_gettime(CLOCK_MONOTONIC, ...) to get the clock time.
    // 4.EPOLLPRI is not practical, which is not included and implemented.
    // 5.Check kEvIn and kEvOut in callback(2nd parameter) if kEvIO is set,
    //   because kEvIn and kEvOut, which can be set to poll simultaneously,
    //   may not be triggered simultaneously.
    //   kEvSignal and kEvTimer need not to be checked in callback(2nd parameter).
    kEvIn = 0x01,             // fd/socket event: fd can be read(EPOLLIN)
    kEvOut = 0x02,            // fd/socket event: fd can be write(EPOLLOUT)
    kEvIO = kEvIn|kEvOut,
    kEvSignal = 0x04,         // signal event
    kEvTimer = 0x08,          // timer event(on which kEvPersist is ignored)
    kEvPersist = 0x10,        // persistent event
    kEvET = 0x20,             // use edge trigger(EPOLLET)


    // The following event flag must not be set in Event.event,
    // they are to be checked in callback(2nd parameter).
    kEvErr = 0x1000,          // error(EPOLLERR|EPOLLHUP) only for kEvIO
    kEvCanceled = 0x2000,     // canceled by the user or library cleanup


    // The following event flag are private. No attention please.
    kInAllList = 0x01,
    kInActiveList = 0x02,
    kInCallback = 0x04,
  };

  typedef void (*ev_callback)(int fd, int event, void * user_data);


  class ReactorImpl;

  struct Event
  {
  public:
    // The following members are required fields,
    // but they can not be modified after being added to reactor.
    int fd;                   // fd(kEvIn, kEvOut), signal number(kEvSignal), heap index(kEvTimer)
    timespec timeout;         // absolute and monotonic timeout(kEvTimer)
    int event;                // event flags(bit or of EventFlag)
    ev_callback callback;     // callback
    void * user_data;         // user data

  public:
    Event();
    Event(int _fd, int _event, ev_callback _callback, void * _udata);
    Event(timespec * _timeout, ev_callback _callback, void * _udata);
    int Del();
    int Cancel();

  private:
    DISALLOW_COPY_AND_ASSIGN(Event);
    friend class ReactorImpl;

    ListNode _all;// node in all event list
    ListNode _active;// node in active event list

    int real_event;// the real event to be passed to callback
    int triggered_times;// the times that the signal is triggered but pending(signal events)
    int flags;// bit or of EventInternalFlag
    ReactorImpl * reactor;

    // return 1, in
    // return 0, not in
    int IsInList()const {return flags & kInAllList;}
    void AddToList(List * list);
    void DelFromList(List * list);

    // return 1, active
    // return 0, not active
    int IsActive()const {return flags & kInActiveList;}
    void AddToActive(List * active_list);
    void DelFromActive(List * active_list);
  };


  // check Event.event
  int CheckInputEventFlag(int flags);
  int CheckEvent(const Event * ev);


  /************************************************************************/
  class Reactor
  {
  private:
    DISALLOW_COPY_AND_ASSIGN(Reactor);
    ReactorImpl * impl_;

  public:
    Reactor();
    ~Reactor();

    int Init();
    void UnInit();

    int Add(Event * ev);
    int Del(Event * ev);
    int Cancel(Event * ev);

    // execute at most one ready event
    // return the number of executed event
    int PollOne();
    // execute all ready events
    // return the number of executed event
    int Poll();
    // if 'limit' > 0, execute at most 'limit' ready events
    // if 'limit' == 0, execute all ready events
    // return the number of executed event
    int Poll(int limit);
    // execute at most one event, or until 'Stop' is called
    // return the number of executed event
    int RunOne();
    // execute all ready events, or until 'Stop' is called
    // return the number of executed event
    int Run();
    // if 'limit' > 0, execute at most 'limit' events, or until interrupted
    // if 'limit' == 0, execute all events, or until interrupted
    // return the number of executed event
    int Run(int limit);
    int Stop();
  };
}

#endif
