/** @file
* @brief interrupter
* @author zhangyafeikimi@gmail.com
* @date
* @version
*
*/
#include "interrupter.h"
#include "ev.h"
#include "log.h"
#include "header.h"

namespace libev {

  Interrupter::Interrupter() : fd_(-1) {}

  Interrupter:: ~Interrupter()
  {
    UnInit();
  }

  int Interrupter::Init()
  {
    UnInit();

    fd_ = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (fd_ == -1)
    {
      EV_LOG(kError, "eventfd: %s", strerror(errno));
      return kEvFailure;
    }
    return kEvOK;
  }

  void Interrupter::UnInit()
  {
    if (fd_ != -1)
    {
      safe_close(fd_);
      fd_ = -1;
    }
  }

  int Interrupter::Interrupt()
  {
    uint64_t counter = 1;
    int result;

    for (;;)
    {
      result = write(fd_, &counter, sizeof(counter));
      if (result == -1 && (errno == EAGAIN || errno == EINTR))
      {
        continue;
      }
      else if (result == -1)
      {
        EV_LOG(kError, "write: %s", strerror(errno));
        return kEvFailure;
      }

      EV_VERIFY(result == sizeof(counter));
      return kEvOK;
    }
  }

  void Interrupter::Reset()
  {
    uint64_t counter;
    int result;
    for (;;)
    {
      result = read(fd_, &counter, sizeof(counter));
      if (result != -1)
      {
        continue;
      }
      else
      {
        if (errno == EINTR)
          continue;
        if (errno == EAGAIN)
          return;// read until nothing to read
      }
    }
  }
}
