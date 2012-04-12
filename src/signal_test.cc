/** @file
* @brief interrupter test
* @author zhangyafeikimi@gmail.com
* @date
* @version
*
*/
#include "ev.h"
#include "log.h"
#include "scoped_ptr.h"
#include "header.h"

using namespace libev;

const int times = 10;

static void SIGINT_Callback(int signum, int event, void * user_data)
{
  printf("SIGINT_Callback %d\n", signum);
}

int main()
{
  Event ev[2];
  ScopedPtr<Reactor> signal_reactor;
  signal_reactor.reset(Reactor::CreateSignalReactor());

  ev[0].fd = SIGINT;
  ev[0].event = kEvSignal|kEvPersist;
  ev[0].callback = SIGINT_Callback;
  ev[0].user_data = &ev[0];

  ev[1].fd = SIGQUIT;
  ev[1].event = kEvSignal;
  ev[1].callback = SIGINT_Callback;
  ev[1].user_data = &ev[1];

  EV_VERIFY(signal_reactor->Init() == kEvOK);
  EV_VERIFY(signal_reactor->Add(&ev[0]) == kEvOK);
  //EV_VERIFY(signal_reactor->Add(&ev[1]) == kEvOK);

  signal_reactor->Run();

  //signal_reactor->UnInit();
  //signal_reactor.reset();

  return 0;
}
