/** @file
* @brief log test
* @author zhangyafeikimi@gmail.com
* @date
* @version
*
*/
#include "log.h"
#include "header.h"

using namespace libev;

static void Test1()
{
  Log log(kDebug, kStdout);
  log.Printf(kAlert, "kAlert");
  log.Printf(kError, "kError");
  log.Printf(kInfo, "kInfo");
  log.Printf(kDebug, "kDebug");

  log.SetLevel(kError);

  log.Printf(kAlert, "kAlert");
  log.Printf(kError, "kError");
  log.Printf(kInfo, "kInfo(assert not printed)");
  log.Printf(kDebug, "kDebug(assert not printed)");
}

static void Test2()
{
  Log log(kDebug, kSysLog, NULL, "log_test");
  log.Printf(kAlert, "kAlert");
  log.Printf(kError, "kError");
  log.Printf(kInfo, "kInfo");
  log.Printf(kDebug, "kDebug");
}

static void Test3()
{
  Log log(kDebug, kLogFile, "log_test.log", NULL);
  log.Printf(kAlert, "kAlert");
  log.Printf(kError, "kError");
  log.Printf(kInfo, "kInfo");
  log.Printf(kDebug, "kDebug");
}

static void Test4()
{
  Log log(kDebug, kStdout | kSysLog | kLogFile, NULL, NULL);
  log.Printf(kAlert, "kAlert");
  log.Printf(kError, "kError");
  log.Printf(kInfo, "kInfo");
  log.Printf(kDebug, "kDebug");
}

static void Test5()
{
  EV_LOG(kDebug, "printed by EV_LOG");
  EV_LOG(kDebug, "printed by EV_LOG2 %d", 666);
  EV_LOG(kDebug, "printed by EV_LOG3 %s", "test");
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
