/** @file
* @brief interrupter
* @author zhangyafeikimi@gmail.com
* @date
* @version
*
*/
#ifndef LIBEV_INTERRUPTER_H
#define LIBEV_INTERRUPTER_H

#include "ev-internal.h"

namespace libev {

  class Interrupter
  {
  private:
    DISALLOW_COPY_AND_ASSIGN(Interrupter);
  public:
    Interrupter();
    ~Interrupter();

    int Init();
    void UnInit();

    int Interrupt();
    // after an 'Interrupt', we must 'Reset' the inner status
    void Reset();

    // get the read fd to be passed to select/poll/epoll/...
    int fd() const
    {
      return fd_;
    }

  private:
    int fd_;
  };
}

#endif
