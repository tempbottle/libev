/** @file
* @brief libev
* @author zhangyafeikimi@gmail.com
* @date
* @version
*
*/
#include "ev.h"
#include "header.h"

namespace libev {

  Event::Event() {}

  Event::Event(int _fd, int _event, ev_callback _callback, void * _udata)
    : fd(_fd), event(_event), callback(_callback), user_data(_udata) {}


  /************************************************************************/
  int CheckInputEventFlag(int flags)
  {
    int count = 0;

    if (flags & kEvErr)
    {
      EV_LOG(kError, "kEvErr must not be set");
      goto einval;
    }

    if (flags & kEvCanceled)
    {
      EV_LOG(kError, "kEvCanceled must not be set");
      goto einval;
    }

    if (flags & kEvIO) count++;
    if (flags & kEvSignal) count++;
    if (flags & kEvTimer) count++;

    if (count > 1)
    {
      EV_LOG(kError, "kEvIO, kEvSignal, kEvTimer are mutual exclusive");
      goto einval;
    }
    else if (count < 1)
    {
      EV_LOG(kError, "No valid event flags is set");
      goto einval;
    }

    return kEvOK;
einval:
    errno = EINVAL;
    return kEvFailure;
  }

  int CheckEvent(Event * ev)
  {
    if (ev == 0)
      goto einval;

    if (CheckInputEventFlag(ev->event) != kEvOK)
      goto einval;

    if (ev->event & kEvIO)
    {
      if (ev->fd < 0)
      {
        EV_LOG(kError, "Event(%p) has an invalid fd", ev, ev->fd);
        goto einval;
      }
    }
    else if (ev->event & kEvSignal)
    {
      if (ev->fd < 0 || ev->fd >= _NSIG)
      {
        EV_LOG(kError, "Event(%p) has an invalid signal number", ev, ev->fd);
        goto einval;
      }
    }
    else if (ev->event & kEvTimer)
    {
      // nothing to check now
    }

    EV_ASSERT(ev->fd >= 0 && ev->fd < _NSIG);
    
    if (ev->callback == 0)
    {
      EV_LOG(kError, "Event(%p) has no callback", ev);
      goto einval;
    }

    return kEvOK;
einval:
    errno = EINVAL;
    return kEvFailure;
  }


  /************************************************************************/
  Reactor::~Reactor() {}


  /************************************************************************/
  Reactor * Reactor::CreateSignalReactor()
  {
    return new SignalReactor;// may throw
  }

}
