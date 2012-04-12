/** @file
* @brief log
* @author zhangyafeikimi@gmail.com
* @date
* @version
*
*/
#include "log.h"
#include "scoped_ptr.h"
#include "header.h"

namespace libev {

  static int s_log_level_map[] =
  {
    LOG_ALERT,
    LOG_CRIT,
    LOG_ERR,
    LOG_WARNING,
    LOG_NOTICE,
    LOG_INFO,
    LOG_DEBUG,
  };

  static const char * s_log_level_string[] =
  {
    "Alert",
    "Critical",
    "Error",
    "Warning",
    "Notice",
    "Info",
    "Debug",
  };


  class Log::Impl
  {
  private:
    int level_;
    int flags_;
    FILE * fp_;

  public:
    Impl(int level, int flags, const char * logfile, const char * syslog_ident)
      :level_(level), flags_(flags), fp_(NULL)
    {
      if (flags_ & kSysLog)
      {
        if (syslog_ident)
          openlog(syslog_ident, LOG_PID | LOG_NDELAY | LOG_NOWAIT, LOG_USER);
        else
          flags_ &= ~kSysLog;
      }

      if ((flags_ & kLogFile) && logfile)
        fp_ = fopen(logfile, "a");
    }

    ~Impl()
    {
      if (fp_)
        fclose(fp_);

      if (flags_ & kSysLog)
        closelog();
    }

    int Printf(int level, const char * content, int size)
    {
      if (flags_ & kSysLog)
        syslog(s_log_level_map[level], "%.*s\n", size, content);

      if (flags_ & kStdout)
      {
        fprintf(stdout, "%.*s\n", size, content);
        fflush(stdout);
      }

      if (flags_ & kStderr)
      {
        fprintf(stderr, "%.*s\n", size, content);
        fflush(stderr);
      }

      if (fp_)
      {
        fprintf(fp_, "%.*s\n", size, content);
        fflush(fp_);
      }

      return 0;
    }

    int GetLevel()const
    {
      return level_;
    }

    void SetLevel(int level)
    {
      level_ = level;
    }
  };


  Log::Log(int level, int flags, const char * logfile, const char * syslog_ident)
  {
    impl_ = new Impl(level, flags, logfile, syslog_ident);
  }

  Log::~Log()
  {
    delete impl_;
  }

  int Log::Printf(int level, const char * format, ...)
  {
    static int pid = 0;
    if (pid == 0)
      pid = static_cast<int>(getpid());

    if (impl_->GetLevel() < level)
      return 0;

    char buf[32768], * p = buf, * large_buf = NULL;
    int buf_size = sizeof(buf);
    int bytes_left = buf_size;
    int n;
    int ret = -1;
    struct timeval tv;
    struct tm tm_s;


    gettimeofday(&tv, NULL);
    localtime_r(&tv.tv_sec, &tm_s);

    for (;;)
    {
      //like '2011-04-18 16:21:30'
      n = strftime(p, bytes_left, "%Y-%m-%d %H:%M:%S", &tm_s);

      bytes_left -= n;
      p += n;
      if (bytes_left <= 0) goto extend_buf;

      n = snprintf(p, bytes_left, ".%06ld [%d/%d] [%s]: ",
        tv.tv_usec, pid, (int)syscall(SYS_gettid), s_log_level_string[level]);

      if (n < 0) goto format_error;
      bytes_left -= n;
      p += n;
      if (bytes_left <= 0) goto extend_buf;

      //var args
      va_list arglist;
      va_start(arglist, format);
      n = vsnprintf(p, bytes_left, format, arglist);
      va_end(arglist);

      if (n < 0) goto format_error;
      bytes_left -= n;
      p += n;
      if (bytes_left <= 0) goto extend_buf;

      //format log OK
      break;

extend_buf:
      buf_size = buf_size * 2;
      bytes_left = buf_size;

      if (large_buf)
        free(large_buf);
      large_buf = static_cast<char *>(malloc(buf_size));
      if (large_buf == NULL)
        goto format_error;

      p = large_buf;
    }

    if (large_buf)
    {
      n = p - large_buf;
      p = large_buf;
    }
    else
    {
      n = p - buf;
      p = buf;
    }

    if (impl_->Printf(level, p, n) < 0)
      ret = -1;
    else
      ret = 1;

format_error:
    if (large_buf)
      free(large_buf);
    return ret;
  }

  int Log::GetLevel()const
  {
    return impl_->GetLevel();
  }

  void Log::SetLevel(int level)
  {
    impl_->SetLevel(level);
  }


  static ScopedPtr<Log> s_log;

  void InitGlobalLog(int level, int flags,
    const char * logfile, const char * syslog_ident)
  {
    s_log.reset(new Log(level, flags, logfile, syslog_ident));
  }

  void UnInitGlobalLog()
  {
    s_log.reset();
  }

  Log& GlobalLog()
  {
    if (!s_log)
      InitGlobalLog();

    return *s_log;
  }
}
