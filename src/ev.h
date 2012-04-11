/** @file
* @brief libev header
* @author zhangyafeikimi@gmail.com
* @date
* @version
*
*/
#ifndef LIBEV_EV_H
#define LIBEV_EV_H

namespace libev {

  enum ErrorCode
  {
    kEvOK = 0,
    kEvFailure = -1,
  };

  enum EventFlag
  {
    kEvIn = 1,
    kEvOut = 2,
    kEvErr = 4,
    kEvInPri = 8,
    kEvSignal = 16,
    kEvTimer = 32,
    kEvPersist = 64,
  };

  typedef void (*ev_callback)(int fd, int event, void * udata);


  class Event
  {
  private:
    Event(const Event&);
    Event& operator=(const Event&);

  protected:
    Event() {}
    Event(int fd, int event, ev_callback callback, void * udata)
      : fd_(fd), event_(event), callback_(callback), udata_(udata) {}

    int fd_;// fd or signum
    int event_;
    ev_callback callback_;
    void * udata_;

  public:
    virtual ~Event() {}

    int fd() {return fd_;}
    int event() {return event_;}
    ev_callback callback() {return callback_;}
    void * udata() {return udata_;}
  };

  // return a null pointer, when memory is not enough
  // delete the return value, when it is no longer used
  Event * CreateSignalEvent(int signum, int event, ev_callback callback, void * udata);


  class Ev
  {
  private:
    Ev(const Ev&);
    Ev& operator=(const Ev&);

  public:
    class Impl;
  private:
    Impl * impl_;

  public:
    Ev();
    ~Ev();

    int Init();
    int Run();
    int RunOne();
    int Stop();

    int AddSignal(Event * ev);
    int DelSignal(Event * ev);
  };
}

#endif
