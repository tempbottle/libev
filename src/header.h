/** @file
* @brief include required system headers,
*        declare and implement eventfd, signalfd, timerfd API if needed
* @author zhangyafeikimi@gmail.com
* @date
* @version
*
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
/*some useful and short inline functions*/
/************************************************************************/
inline int safe_close(int fd)
{
  int ret;
  do ret = close(fd);
  while (ret == -1 && errno == EINTR);
  return ret;
}

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
  fd = syscall(SYS_signalfd, fd, mask, _NSIG/8);
  if (fd == -1)
    return -1;

  if (flags & SFD_NONBLOCK)
    fcntl(fd, F_SETFL, O_NONBLOCK);
  if (flags & SFD_CLOEXEC)
    fcntl(fd, F_SETFD, FD_CLOEXEC);
  return fd;
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
# if defined SYS_eventfd2
  return syscall(SYS_eventfd2, initval, flags);
# else
  int fd = syscall(SYS_eventfd, initval);
  if (fd == -1)
    return -1;

  if ((flags & EFD_NONBLOCK) && fcntl(fd, F_SETFL, O_NONBLOCK) == -1)
  {
    safe_close(fd);
    return -1;
  }
  if ((flags & EFD_CLOEXEC) && fcntl(fd, F_SETFD, FD_CLOEXEC) == -1)
  {
    safe_close(fd);
    return -1;
  }
  return fd;
# endif
}
#endif/*HAVE_SYS_EVENTFD*/

/************************************************************************/
/*timespec functions*/
/************************************************************************/
#define timespec_isset(tvp) ((tvp)->tv_sec || (tvp)->tv_nsec)
#define timespec_clear(tvp) (tvp)->tv_sec = (tvp)->tv_nsec = 0

#define timespec_cmp(tvp,uvp,cmp) \
  ((tvp)->tv_sec cmp (uvp)->tv_sec || \
  ((tvp)->tv_sec == (uvp)->tv_sec && (tvp)->tv_nsec cmp (uvp)->tv_nsec))
#define timespec_less(a, b) timespec_cmp(a, b, <)
#define timespec_greater(a, b) timespec_cmp(a, b, >)
#define timespec_equal(a, b) timespec_cmp(a, b, ==)
#define timespec_ge(a, b) (!timespec_less(a, b))
#define timespec_le(a, b) (!timespec_greater(a, b))
#define timespec_ne(a, b) (!timespec_equal(a, b))

#define timespec_subto_ms(a, b) (((a).tv_sec - (b).tv_sec)*1000 + ((a).tv_nsec - (b).tv_nsec)/1000)
#define timespec_subto_ns(a, b) (((a).tv_sec - (b).tv_sec)*1000000 + ((a).tv_nsec - (b).tv_nsec))

inline void timespec_fix(struct timespec * tv)
{
  while (tv->tv_nsec < 0)
  {
    tv->tv_sec--;
    tv->tv_nsec += 1000000000;
  }

  while (tv->tv_nsec >= 1000000000)
  {
    tv->tv_sec++;
    tv->tv_nsec -= 1000000000;
  }
}

inline void timespec_addto(struct timespec * a, const struct timespec * b)
{
  a->tv_sec += b->tv_sec;
  a->tv_nsec += b->tv_nsec;
  timespec_fix(a);
}

inline void timespec_subto(struct timespec * a, const struct timespec * b)
{
  a->tv_sec -= b->tv_sec;
  a->tv_nsec -= b->tv_nsec;
  timespec_fix(a);
}

#endif
