/** @file
* @brief test timer events
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

static void Test0_Callback(int fd, int event, void * user_data)
{
  EV_LOG(kInfo, "Test0_Callback");

  Event * ev = static_cast<Event *>(user_data);
  delete ev;
}

static void Test0()
{
  EV_LOG(kInfo, "Test 0: timer");

  ScopedPtr<Reactor> reactor(new Reactor);
  Event * ev[2];
  timespec timeout;
  EV_VERIFY(clock_gettime(CLOCK_MONOTONIC, &timeout) != -1);

  timeout.tv_sec += 1;
  ev[0] = new Event;
  ev[0]->timeout = timeout;
  ev[0]->event = kEvTimer;
  ev[0]->callback = Test0_Callback;
  ev[0]->user_data = ev[0];

  timeout.tv_sec += 1;
  ev[1] = new Event;
  ev[1]->timeout = timeout;
  ev[1]->event = kEvTimer;
  ev[1]->callback = Test0_Callback;
  ev[1]->user_data = ev[1];

  EV_VERIFY(reactor->Init() == kEvOK);
  EV_VERIFY(reactor->Add(ev[0]) == kEvOK);
  EV_VERIFY(reactor->Add(ev[1]) == kEvOK);

  reactor->Run();
  reactor.reset();

  EV_LOG(kInfo, "\n\n");
}

int main()
{
  Test0();
  return 0;
}
