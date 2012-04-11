/** @file
* @brief log
* @author zhangyafeikimi@gmail.com
* @date
* @version
*
*/
#ifndef LIBEV_LOG_H
#define LIBEV_LOG_H

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
    int GetLevel()const;
    void SetLevel(int level);
  };
}

#endif
