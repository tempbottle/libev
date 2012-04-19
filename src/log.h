/** @file
* @brief log
* @author zhangyafeikimi@gmail.com
* @date
* @version
*
*/
#ifndef LIBEV_LOG_H
#define LIBEV_LOG_H

#include <stdlib.h>

namespace libev {

  // flags used by Log::Log
  enum
  {
    kSysLog = 1,// use syslog from <syslog.h>
    kStdout = 2,
    kStderr = 4,
    kLogFile = 8,
  };

  // log/verbose level
  enum
  {
    kAlert = 0,
    kCritical,
    kError,
    kWarning,
    kNotice,
    kInfo,
    kDebug,
  };

  class Log
  {
  private:
    class Impl;
    Impl * impl_;

  public:
    // If 'flags' & kFile is non-zero, 'logfile' will be the filename of output log
    // If 'flags' & kSysLog is non-zero, 'syslog_ident' will be used to 'openlog'
    Log(int level, int flags, const char * logfile = 0, const char * syslog_ident = 0);
    ~Log();
    // return -1, the message is not logged for internal reasons
    // return 0, the message is not logged for 'level'
    // return 1, the message is successfully logged
    int Printf(int level, const char * format, ...);
    // return the log level
    int GetLevel()const;
    void SetLevel(int level);
  };

  void InitGlobalLog(int level = kDebug, int flags = kStderr,
    const char * logfile = 0, const char * syslog_ident = 0);
  void UnInitGlobalLog();
  Log& GlobalLog();
}

#define EV_LOG(level, format, args...) do { \
  GlobalLog().Printf(level, "[%s:%d] "format, __FILE__, __LINE__, ##args); \
} while(0)

#if defined NDEBUG
# define EV_ASSERT(exp) ((void)0)
#else
# define EV_ASSERT(exp) do { \
  if (!(exp)) { \
    GlobalLog().Printf(kError, "[%s:%d] expression(%s) failed", __FILE__, __LINE__, #exp); \
    abort(); \
  } \
} while(0)
#endif

#define EV_VERIFY(exp) do { \
  if (!(exp)) { \
    GlobalLog().Printf(kError, "[%s:%d] expression(%s) failed", __FILE__, __LINE__, #exp); \
    abort(); \
  } \
} while(0)

#endif
