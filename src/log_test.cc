/** @file
* @brief log test
* @author zhangyafeikimi@gmail.com
* @date
* @version
*
*/
#include "log.h"

using namespace libev;

void test1()
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

void test2()
{
  Log log(kDebug, kSysLog, 0, "log_test");
  log.Printf(kAlert, "kAlert");
  log.Printf(kError, "kError");
  log.Printf(kInfo, "kInfo");
  log.Printf(kDebug, "kDebug");
}

void test3()
{
  Log log(kDebug, kLogFile, "log_test.log", 0);
  log.Printf(kAlert, "kAlert");
  log.Printf(kError, "kError");
  log.Printf(kInfo, "kInfo");
  log.Printf(kDebug, "kDebug");
}

void test4()
{
  Log log(kDebug, kStdout | kSysLog | kLogFile, 0, 0);
  log.Printf(kAlert, "kAlert");
  log.Printf(kError, "kError");
  log.Printf(kInfo, "kInfo");
  log.Printf(kDebug, "kDebug");
}

int main()
{
  test1();
  test2();
  test3();
  test4();
  return 0;
}
