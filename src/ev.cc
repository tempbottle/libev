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

  int CheckInputEventFlag(int flags)
  {
    int count = 0;

    if (flags & kEvErr)
    {
      errno = EINVAL;
      EV_LOG(kError, "kEvErr must not be set");
      return kEvFailure;
    }

    if (flags & kEvCanceled)
    {
      errno = EINVAL;
      EV_LOG(kError, "kEvCanceled must not be set");
      return kEvFailure;
    }

    if (flags & kEvIO) count++;
    if (flags & kEvSignal) count++;
    if (flags & kEvTimer) count++;

    if (count > 1)
    {
      errno = EINVAL;
      EV_LOG(kError, "kEvIO, kEvSignal, kEvTimer are mutual exclusive");
      return kEvFailure;
    }
    else if (count < 1)
    {
      errno = EINVAL;
      EV_LOG(kError, "No valid event flags is set");
      return kEvFailure;
    }
    else
    {
      return kEvOK;
    }
  }


  /************************************************************************/
  Event::Event() {}

  Event::Event(int _fd, int _event, ev_callback _callback, void * _udata)
    : fd(_fd), event(_event), callback(_callback), user_data(_udata) {}


  /************************************************************************/
  Reactor::~Reactor() {}

  void Reactor::OnReadable() {}


  /************************************************************************/
  Reactor * Reactor::CreateSignalReactor()
  {
    return new SignalReactor;// may throw
  }

}
