/** @file
* @brief interrupter
* @author zhangyafeikimi@gmail.com
* @date
* @version
*
*/
#ifndef LIBEV_INTERRUPTER_H
#define LIBEV_INTERRUPTER_H

namespace libev {

  class Interrupter
  {
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
