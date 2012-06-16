/** @file
 * @brief test signal events
 * @author zhangyafeikimi@gmail.com
 * @date
 * @version
 *
 */
#include "ev.h"
#include "log.h"
#include "scoped_ptr.h"
#include "header.h"
#include <map>

using namespace libev;

static int counter;

static void Test0_Callback(int signum, int event, void * user_data)
{
  EV_LOG(kInfo, "Test0_Callback %d", signum);

  if (event & kEvCanceled)
  {
    counter++;
    Event * ev = static_cast<Event *>(user_data);
    delete ev;
  }
}

static void Test0()
{
  EV_LOG(kInfo, "Test 0: delete/cancel/cleanup");

  counter = 0;

  Event * ev[4];
  ScopedPtr<Reactor> reactor(new Reactor);

  ev[0] = new Event;
  ev[0]->fd = SIGINT;
  ev[0]->event = kEvSignal;
  ev[0]->callback = Test0_Callback;
  ev[0]->user_data = ev[0];

  ev[1] = new Event;
  ev[1]->fd = SIGINT;
  ev[1]->event = kEvSignal;
  ev[1]->callback = Test0_Callback;
  ev[1]->user_data = ev[1];

  ev[2] = new Event;
  ev[2]->fd = SIGINT;
  ev[2]->event = kEvSignal;
  ev[2]->callback = Test0_Callback;
  ev[2]->user_data = ev[2];

  ev[3] = new Event;
  ev[3]->fd = SIGINT;
  ev[3]->event = kEvSignal|kEvPersist;
  ev[3]->callback = Test0_Callback;
  ev[3]->user_data = ev[3];

  EV_VERIFY(reactor->Init() == kEvOK);
  EV_VERIFY(reactor->Add(ev[0]) == kEvOK);
  EV_VERIFY(reactor->Add(ev[1]) == kEvOK);
  EV_VERIFY(reactor->Add(ev[2]) == kEvOK);
  EV_VERIFY(reactor->Add(ev[3]) == kEvOK);

  EV_VERIFY(reactor->Cancel(ev[0]) == kEvOK);
  EV_VERIFY(reactor->Del(ev[1]) == kEvOK);
  delete ev[1];

  reactor.reset();

  EV_VERIFY(counter == 3);

  EV_LOG(kInfo, "\n\n");
}


static void Test1_Callback(int signum, int event, void * user_data)
{
  EV_LOG(kInfo, "Test1_Callback %d", signum);

  counter++;
  Event * ev = static_cast<Event *>(user_data);
  delete ev;
}

static void Test1()
{
  EV_LOG(kInfo, "Test 1: non-persistent events");

  counter = 0;

  Event * ev[2];
  ScopedPtr<Reactor> reactor(new Reactor);

  ev[0] = new Event;
  ev[0]->fd = SIGINT;
  ev[0]->event = kEvSignal;
  ev[0]->callback = Test1_Callback;
  ev[0]->user_data = ev[0];

  ev[1] = new Event;
  ev[1]->fd = SIGQUIT;
  ev[1]->event = kEvSignal;
  ev[1]->callback = Test1_Callback;
  ev[1]->user_data = ev[1];

  EV_VERIFY(reactor->Init() == kEvOK);
  EV_VERIFY(reactor->Add(ev[0]) == kEvOK);
  EV_VERIFY(reactor->Add(ev[1]) == kEvOK);

  kill(getpid(), SIGINT);
  kill(getpid(), SIGQUIT);

  reactor->Run();
  reactor.reset();

  EV_VERIFY(counter == 2);

  EV_LOG(kInfo, "\n\n");
}


struct Test2_Helper
{
  Event * ev;
  Event * ev_other;
};

static void Test2_Callback(int signum, int event, void * user_data)
{
  EV_LOG(kInfo, "Test2_Callback %d", signum);

  if (signum == SIGINT)
  {
    counter++;

    Test2_Helper * helper = static_cast<Test2_Helper *>(user_data);
    helper->ev_other->Del();
    delete helper->ev;
    delete helper->ev_other;
  }
  else if (signum == SIGQUIT)
  {
    counter++;

    Test2_Helper * helper = static_cast<Test2_Helper *>(user_data);
    helper->ev_other->Cancel();
    delete helper->ev;
  }
  else if (signum == SIGUSR2 && (event & kEvCanceled))
  {
    counter++;

    Event * ev = static_cast<Event *>(user_data);
    delete ev;
  }
}

static void Test2()
{
  EV_LOG(kInfo, "Test 2: delete/cancel other events");
  EV_LOG(kInfo, "press Ctrl+C and Ctrl+\\(unordered), then quit");

  counter = 0;

  Event * ev[4];
  Test2_Helper helper[2];
  ScopedPtr<Reactor> reactor(new Reactor);

  ev[0] = new Event;
  ev[1] = new Event;
  ev[2] = new Event;
  ev[3] = new Event;

  helper[0].ev = ev[0];
  helper[0].ev_other = ev[2];

  helper[1].ev = ev[1];
  helper[1].ev_other = ev[3];

  ev[0]->fd = SIGINT;
  ev[0]->event = kEvSignal;
  ev[0]->callback = Test2_Callback;
  ev[0]->user_data = &helper[0];

  ev[1]->fd = SIGQUIT;
  ev[1]->event = kEvSignal;
  ev[1]->callback = Test2_Callback;
  ev[1]->user_data = &helper[1];

  ev[2]->fd = SIGUSR1;
  ev[2]->event = kEvSignal|kEvPersist;
  ev[2]->callback = Test2_Callback;
  ev[2]->user_data = ev[2];

  ev[3]->fd = SIGUSR2;
  ev[3]->event = kEvSignal|kEvPersist;
  ev[3]->callback = Test2_Callback;
  ev[3]->user_data = ev[3];

  EV_VERIFY(reactor->Init() == kEvOK);
  EV_VERIFY(reactor->Add(ev[0]) == kEvOK);
  EV_VERIFY(reactor->Add(ev[1]) == kEvOK);
  EV_VERIFY(reactor->Add(ev[2]) == kEvOK);
  EV_VERIFY(reactor->Add(ev[3]) == kEvOK);

  kill(getpid(), SIGINT);
  kill(getpid(), SIGQUIT);

  reactor->Run();
  reactor.reset();

  EV_VERIFY(counter == 3);

  EV_LOG(kInfo, "\n\n");
}


static void Test3_Callback(int signum, int event, void * user_data)
{
  EV_LOG(kInfo, "Test3_Callback %d", signum);

  counter++;

  Event * ev = static_cast<Event *>(user_data);
  if (signum == SIGINT || signum == SIGQUIT)
  {
    ev->Cancel();
    delete ev;
  }
  else
  {
    ev->Del();
    delete ev;
  }
}

static void Test3()
{
  EV_LOG(kInfo, "Test 3: delete/cancel self's events");

  counter = 0;

  Event * ev[4];
  ScopedPtr<Reactor> reactor(new Reactor);

  ev[0] = new Event;
  ev[1] = new Event;
  ev[2] = new Event;
  ev[3] = new Event;

  ev[0]->fd = SIGINT;
  ev[0]->event = kEvSignal;
  ev[0]->callback = Test3_Callback;
  ev[0]->user_data = ev[0];

  ev[1]->fd = SIGQUIT;
  ev[1]->event = kEvSignal|kEvPersist;
  ev[1]->callback = Test3_Callback;
  ev[1]->user_data = ev[1];

  ev[2]->fd = SIGUSR1;
  ev[2]->event = kEvSignal;
  ev[2]->callback = Test3_Callback;
  ev[2]->user_data = ev[2];

  ev[3]->fd = SIGUSR2;
  ev[3]->event = kEvSignal|kEvPersist;
  ev[3]->callback = Test3_Callback;
  ev[3]->user_data = ev[3];

  EV_VERIFY(reactor->Init() == kEvOK);
  EV_VERIFY(reactor->Add(ev[0]) == kEvOK);
  EV_VERIFY(reactor->Add(ev[1]) == kEvOK);
  EV_VERIFY(reactor->Add(ev[2]) == kEvOK);
  EV_VERIFY(reactor->Add(ev[3]) == kEvOK);

  kill(getpid(), SIGINT);
  kill(getpid(), SIGQUIT);
  kill(getpid(), SIGUSR1);
  kill(getpid(), SIGUSR2);

  reactor->Run();
  reactor.reset();

  EV_VERIFY(counter == 4);

  EV_LOG(kInfo, "\n\n");
}


static void Test4_Callback(int signum, int event, void * user_data)
{
  EV_LOG(kInfo, "Test4_Callback %d", signum);

  counter++;

  Event * ev = static_cast<Event *>(user_data);
  delete ev;
}

static void Test4()
{
  EV_LOG(kInfo, "Test 4: non-persistent events with the same signal");

  counter = 0;

  Event * ev[2];
  ScopedPtr<Reactor> reactor(new Reactor);

  ev[0] = new Event;
  ev[0]->fd = SIGINT;
  ev[0]->event = kEvSignal;
  ev[0]->callback = Test4_Callback;
  ev[0]->user_data = ev[0];

  ev[1] = new Event;
  ev[1]->fd = SIGINT;
  ev[1]->event = kEvSignal;
  ev[1]->callback = Test4_Callback;
  ev[1]->user_data = ev[1];

  EV_VERIFY(reactor->Init() == kEvOK);
  EV_VERIFY(reactor->Add(ev[0]) == kEvOK);
  EV_VERIFY(reactor->Add(ev[1]) == kEvOK);

  kill(getpid(), SIGINT);

  reactor->Run();
  reactor.reset();

  EV_VERIFY(counter == 2);

  EV_LOG(kInfo, "\n\n");
}


static void Test5_Callback(int signum, int event, void * user_data)
{
  EV_LOG(kInfo, "Test5_Callback %d", signum);

  counter++;

  Event * ev = static_cast<Event *>(user_data);

  static std::map<Event *, int> time_map;

  if (++time_map[ev] == 3)
  {
    ev->Del();
    delete ev;
  }
  else
  {
    kill(getpid(), SIGINT);
  }
}

static void Test5()
{
  EV_LOG(kInfo, "Test 5: persistent events with the same signal");

  counter = 0;

  Event * ev[2];
  ScopedPtr<Reactor> reactor(new Reactor);

  ev[0] = new Event;
  ev[0]->fd = SIGINT;
  ev[0]->event = kEvSignal|kEvPersist;
  ev[0]->callback = Test5_Callback;
  ev[0]->user_data = ev[0];

  ev[1] = new Event;
  ev[1]->fd = SIGINT;
  ev[1]->event = kEvSignal|kEvPersist;
  ev[1]->callback = Test5_Callback;
  ev[1]->user_data = ev[1];

  EV_VERIFY(reactor->Init() == kEvOK);
  EV_VERIFY(reactor->Add(ev[0]) == kEvOK);
  EV_VERIFY(reactor->Add(ev[1]) == kEvOK);

  kill(getpid(), SIGINT);

  reactor->Run();
  reactor.reset();

  EV_VERIFY(counter == 6);

  EV_LOG(kInfo, "\n\n");
}


int main()
{
  Test0();
  Test1();
  Test2();
  Test3();
  Test4();
  Test5();

  return 0;
}
