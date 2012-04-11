/** @file
* @brief include all external headers,
*        declare eventfd, signalfd, timerfd API
* @author zhangyafeikimi@gmail.com
* @date
* @version
*
*/
#ifndef LIBEV_HEADER_H
#define LIBEV_HEADER_H

/************************************************************************/
/*C headers*/
/************************************************************************/
#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif
#include <unistd.h>
#include <sys/syscall.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <syslog.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/epoll.h>
#include <sys/uio.h>
#include <sys/un.h>


/************************************************************************/
/*including or declarations of eventfd, signalfd, timerfd API*/
/************************************************************************/
#if defined HAVE_SYS_SIGNALFD
# include <sys/signalfd.h>
#else/*HAVE_SYS_SIGNALFD*/
struct signalfd_siginfo
{
  uint32_t ssi_signo;
  int32_t ssi_errno;
  int32_t ssi_code;
  uint32_t ssi_pid;
  uint32_t ssi_uid;
  int32_t ssi_fd;
  uint32_t ssi_tid;
  uint32_t ssi_band;
  uint32_t ssi_overrun;
  uint32_t ssi_trapno;
  int32_t ssi_status;
  int32_t ssi_int;
  uint64_t ssi_ptr;
  uint64_t ssi_utime;
  uint64_t ssi_stime;
  uint64_t ssi_addr;
  uint8_t __pad[48];
};

enum
{
  SFD_CLOEXEC = 02000000,
  SFD_NONBLOCK = 04000,
};

inline int signalfd(int fd, const sigset_t *mask, int flags)
{
# if defined SYS_signalfd4
  return syscall(SYS_signalfd4, fd, mask, _NSIG/8, flags);
# else
  return syscall(SYS_signalfd, fd, mask, _NSIG/8);
# endif
}
#endif/*HAVE_SYS_SIGNALFD*/


#if defined HAVE_SYS_TIMERFD
# include <sys/timerfd.h>
#else/*HAVE_SYS_TIMERFD*/
enum
{
  TFD_CLOEXEC = 02000000,
  TFD_NONBLOCK = 04000,
  TFD_TIMER_ABSTIME = 1,
};

inline int timerfd_create(int clockid, int flags)
{
  return syscall(SYS_timerfd_create, clockid, flags);
}

inline int timerfd_settime(int fd, int flags,
                           const struct itimerspec *new_value, struct itimerspec *old_value)
{
  return syscall(SYS_timerfd_settime, fd, flags, new_value, old_value);
}

inline int timerfd_gettime(int fd, struct itimerspec *curr_value)
{
  return syscall(SYS_timerfd_gettime, fd, curr_value);
}
#endif


#if defined HAVE_SYS_EVENTFD
# include <sys/eventfd.h>
#else/*HAVE_SYS_EVENTFD*/
enum
{
  EFD_CLOEXEC = 02000000,
  EFD_NONBLOCK = 04000,
  EFD_SEMAPHORE = 1,
};

inline int eventfd(unsigned int initval, int flags)
{
  return syscall(SYS_eventfd, initval, flags);
}
#endif/*HAVE_SYS_EVENTFD*/

inline int safe_close(int fd)
{
  int ret;
  do ret = close(fd);
  while (ret == -1 && errno == EINTR);
  return ret;
}

#endif
