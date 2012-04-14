/** @file
* @brief signal reactor test
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

// non-persistent event
static void Test1_Callback(int signum, int event, void * user_data)
{
  EV_LOG(kInfo, "Test1_Callback %d", signum);
}

static void Test1()
{
  EV_LOG(kInfo, "Test 1");
  EV_LOG(kInfo, "press Ctrl+C and Ctrl+\\(unordered), then quit");

  Event ev[2];
  ScopedPtr<Reactor> reactor;

  reactor.reset(Reactor::CreateSignalReactor());

  ev[0].fd = SIGINT;
  ev[0].event = kEvSignal;
  ev[0].callback = Test1_Callback;
  ev[0].user_data = &ev[0];

  ev[1].fd = SIGQUIT;
  ev[1].event = kEvSignal;
  ev[1].callback = Test1_Callback;
  ev[1].user_data = &ev[1];

  EV_VERIFY(reactor->Init() == kEvOK);
  EV_VERIFY(reactor->Add(&ev[0]) == kEvOK);
  EV_VERIFY(reactor->Add(&ev[1]) == kEvOK);

  reactor->Run();

  reactor->UnInit();
  reactor.reset();
  EV_LOG(kInfo, "\n");
}


// delete another persistent event
struct Test2_Helper
{
  Event * ev[2];
  Reactor * reactor;
};

static void Test2_Callback(int signum, int event, void * user_data)
{
  if (signum == SIGINT)
  {
    if (event & kEvCanceled)
      EV_LOG(kInfo, "Test2_Callback SIGINT has been canceled");
    else
      EV_LOG(kInfo, "Test2_Callback SIGINT");
  }
  else if (signum == SIGQUIT)
  {
    EV_LOG(kInfo, "Test2_Callback SIGQUIT");
    Test2_Helper * helper = (Test2_Helper *)user_data;
    helper->reactor->Del(helper->ev[0]);
  }
}

static void Test2()
{
  EV_LOG(kInfo, "Test 2");
  EV_LOG(kInfo, "Ctrl+C is persistent; press Ctrl+\\, then quit");

  Event ev[2];
  Test2_Helper helper;
  ScopedPtr<Reactor> reactor;

  reactor.reset(Reactor::CreateSignalReactor());

  helper.ev[0] = &ev[0];
  helper.ev[1] = &ev[1];
  helper.reactor = reactor.get();

  ev[0].fd = SIGINT;
  ev[0].event = kEvSignal|kEvPersist;
  ev[0].callback = Test2_Callback;
  ev[0].user_data = &helper;

  ev[1].fd = SIGQUIT;
  ev[1].event = kEvSignal;
  ev[1].callback = Test2_Callback;
  ev[1].user_data = &helper;

  EV_VERIFY(reactor->Init() == kEvOK);
  EV_VERIFY(reactor->Add(&ev[0]) == kEvOK);
  EV_VERIFY(reactor->Add(&ev[1]) == kEvOK);

  reactor->Run();

  reactor->UnInit();
  reactor.reset();
  EV_LOG(kInfo, "\n");
}


// two methods to delete persistent event inside callback
struct Test3_Helper
{
  Event * ev;
  int times;
  Reactor * reactor;
};

static void Test3_Callback(int signum, int event, void * user_data)
{
  EV_LOG(kInfo, "Test3_Callback %d", signum);

  Test3_Helper * helper = (Test3_Helper *)user_data;

  if (--helper->times == 0)
  {
    if (signum == SIGINT)
    {
      EV_LOG(kInfo, "Test3_Callback %d will be canceled", signum);
      helper->reactor->Del(helper->ev);
    }
    else if (signum == SIGQUIT)
    {
      helper->ev->event &= ~kEvPersist;
    }
  }
}

static void Test3()
{
  EV_LOG(kInfo, "Test 3");
  EV_LOG(kInfo, "press Ctrl+C 3 times and Ctrl+\\ 3 times(unordered), then quit");

  Event ev[2];
  ScopedPtr<Reactor> reactor;

  reactor.reset(Reactor::CreateSignalReactor());

  Test3_Helper helper[2] = {{&ev[0], 3, reactor.get()}, {&ev[1], 3, reactor.get()}};

  ev[0].fd = SIGINT;
  ev[0].event = kEvSignal|kEvPersist;
  ev[0].callback = Test3_Callback;
  ev[0].user_data = &helper[0];

  ev[1].fd = SIGQUIT;
  ev[1].event = kEvSignal|kEvPersist;
  ev[1].callback = Test3_Callback;
  ev[1].user_data = &helper[1];

  EV_VERIFY(reactor->Init() == kEvOK);
  EV_VERIFY(reactor->Add(&ev[0]) == kEvOK);
  EV_VERIFY(reactor->Add(&ev[1]) == kEvOK);

  reactor->Run();

  reactor->UnInit();
  reactor.reset();
  EV_LOG(kInfo, "\n");
}


// same signal events
static void Test4_Callback(int signum, int event, void * user_data)
{
  EV_LOG(kInfo, "Test4_Callback %d", signum);
}

static void Test4()
{
  EV_LOG(kInfo, "Test 4");
  EV_LOG(kInfo, "press Ctrl+C, then quit");

  Event ev[2];
  ScopedPtr<Reactor> reactor;

  reactor.reset(Reactor::CreateSignalReactor());

  ev[0].fd = SIGINT;
  ev[0].event = kEvSignal;
  ev[0].callback = Test4_Callback;
  ev[0].user_data = &ev[0];

  ev[1].fd = SIGINT;
  ev[1].event = kEvSignal;
  ev[1].callback = Test4_Callback;
  ev[1].user_data = &ev[1];

  EV_VERIFY(reactor->Init() == kEvOK);
  EV_VERIFY(reactor->Add(&ev[0]) == kEvOK);
  EV_VERIFY(reactor->Add(&ev[1]) == kEvOK);

  reactor->Run();

  reactor->UnInit();
  reactor.reset();
  EV_LOG(kInfo, "\n");
}


// same signal events persistent
struct Test5_Helper
{
  Event * ev;
  int times;
  Reactor * reactor;
};

static void Test5_Callback(int signum, int event, void * user_data)
{
  EV_LOG(kInfo, "Test5_Callback %d", signum);

  Test3_Helper * helper = (Test3_Helper *)user_data;

  if (--helper->times == 0)
  {
    EV_LOG(kInfo, "Test5_Callback %d will be canceled", signum);
    helper->reactor->Del(helper->ev);
  }
}

static void Test5()
{
  EV_LOG(kInfo, "Test 5");
  EV_LOG(kInfo, "press Ctrl+C 3 times, then quit");

  Event ev[2];
  ScopedPtr<Reactor> reactor;

  reactor.reset(Reactor::CreateSignalReactor());

  Test5_Helper helper[2] = {{&ev[0], 3, reactor.get()}, {&ev[1], 3, reactor.get()}};

  ev[0].fd = SIGINT;
  ev[0].event = kEvSignal|kEvPersist;
  ev[0].callback = Test5_Callback;
  ev[0].user_data = &helper[0];

  ev[1].fd = SIGINT;
  ev[1].event = kEvSignal|kEvPersist;
  ev[1].callback = Test5_Callback;
  ev[1].user_data = &helper[1];

  EV_VERIFY(reactor->Init() == kEvOK);
  EV_VERIFY(reactor->Add(&ev[0]) == kEvOK);
  EV_VERIFY(reactor->Add(&ev[1]) == kEvOK);

  reactor->Run();

  reactor->UnInit();
  reactor.reset();
  EV_LOG(kInfo, "\n");
}

int main()
{
  Test1();
  Test2();
  Test3();
  Test4();
  Test5();

  return 0;
}
