/** @file
 * @brief test io events
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

static int set_non_block(int fd)
{
  int opt = 1;
  return ioctl(fd, FIONBIO, &opt);
}

static int connect_host(int sockfd, const char * hostname, unsigned short port)
{
  struct sockaddr_in s4;
  struct hostent * host;

  host = gethostbyname(hostname);
  if (host == 0)
  {
    errno = EHOSTUNREACH;
    return -1;
  }

  if (host->h_addrtype != AF_INET)
  {
    errno = EPFNOSUPPORT;
    return -1;
  }

  s4.sin_family = AF_INET;
  s4.sin_port = htons(port);
  memcpy(&s4.sin_addr, host->h_addr_list[0], 4);
  return connect(sockfd, /*lint -e(740) */(const struct sockaddr *)&s4, sizeof(s4));
}

static void Test0_Callback(int /*fd*/, int event, void * user_data)
{
  EV_LOG(kInfo, "Test0_Callback");

  Event * ev = (Event *)user_data;

  if (event & (kEvErr|kEvCanceled))
  {
    if (event & kEvErr)
    {
      int err;
      socklen_t len = sizeof(err);
      EV_VERIFY(getsockopt(ev->fd, SOL_SOCKET, SO_ERROR, &err, &len) != -1);
      EV_LOG(kInfo, "Test0_Callback connect failed: %s", strerror(err));
    }
    if (event & kEvCanceled)
      EV_LOG(kInfo, "Test0_Callback kEvCanceled");

    safe_close(ev->fd);
    delete ev;
    return;
  }

  if (event & kEvOut)
  {
    EV_LOG(kInfo, "Test0_Callback connect OK");

    safe_close(ev->fd);
    delete ev;
    return;
  }
}

static void Test0()
{
  EV_LOG(kInfo, "Test 0: connect");

  ScopedPtr<Reactor> reactor(new Reactor);
  Event * ev[2];
  int res;
  int sockfd;

  sockfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  EV_VERIFY(sockfd != -1);
  EV_VERIFY(set_non_block(sockfd) == 0);
  res = connect_host(sockfd, "sdl-adgagadev", 80);// open
  EV_VERIFY(res == -1 && errno == EINPROGRESS);
  ev[0] = new Event;
  ev[0]->fd = sockfd;
  ev[0]->event = kEvOut|kEvET;
  ev[0]->callback = Test0_Callback;
  ev[0]->user_data = ev[0];

  sockfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  EV_VERIFY(sockfd != -1);
  EV_VERIFY(set_non_block(sockfd) == 0);
  res = connect_host(sockfd, "sdl-adgagadev", 8123);// not open
  EV_VERIFY(res == -1 && errno == EINPROGRESS);
  ev[1] = new Event;
  ev[1]->fd = sockfd;
  ev[1]->event = kEvOut|kEvET;
  ev[1]->callback = Test0_Callback;
  ev[1]->user_data = ev[1];

  EV_VERIFY(reactor->Init() == kEvOK);
  EV_VERIFY(reactor->Add(ev[0]) == kEvOK);
  EV_VERIFY(reactor->Add(ev[1]) == kEvOK);

  (void)reactor->Run();
  reactor.reset();

  EV_LOG(kInfo, "\n\n");
}

int main()
{
  Test0();
  return 0;
}
