/** @file
 * @brief http get test
 * @author zhangyafeikimi@gmail.com
 * @date
 * @version
 *
 */
#include "ev.h"
#include "log.h"
#include "scoped_ptr.h"
#include "header.h"
#include <string>

using namespace libev;

static int set_non_block(int fd)
{
  int opt = 1;
  return ioctl(fd, FIONBIO, &opt);
}

struct Connection
{
  Reactor * reactor;
  int fd;
  std::string host_name;

  Event ev_in;
  Event ev_out;

  std::string send_buf;
  int send_cursor;
  std::string recv_buf;
  int recv_cursor;
};

static void OutCallback(int fd, int event, void * user_data)
{
  Connection * conn = static_cast<Connection *>(user_data);
  int res;

  if (event & kEvErr)
  {
    socklen_t len = sizeof(res);
    EV_VERIFY(getsockopt(conn->fd, SOL_SOCKET, SO_ERROR, &res, &len) != -1);
    EV_LOG(kInfo, "failed: %s", strerror(res));
    goto err;
  }

  if (event & kEvOut)
  {
    if (conn->send_buf.empty())
    {
      EV_LOG(kInfo, "connect OK");

      conn->send_buf =
        "GET / HTTP/1.1\r\n"
        "Host: ";
      conn->send_buf += conn->host_name;
      conn->send_buf +=
        "\r\n"
        "Connection: closed\r\n"
        "User-Agent: Mozilla/5.0 (Windows NT 6.1) AppleWebKit/535.7 (KHTML, like Gecko) Chrome/16.0.912.75 Safari/535.7 360EE\r\n"
        "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n"
        "Accept-Language: zh-CN,zh;q=0.8\r\n"
        "Accept-Charset: GBK;q=0.7,*;q=0.3\r\n"
        "\r\n";
      conn->send_cursor = 0;
    }

    for (;;)
    {
      if (conn->send_buf.size() == static_cast<size_t>(conn->send_cursor))
      {
        EV_LOG(kInfo, "send GET OK");

        conn->recv_buf.resize(1024);
        conn->recv_cursor = 0;
        EV_VERIFY(conn->reactor->Add(&conn->ev_in) == kEvOK);

        EV_VERIFY(conn->ev_out.Del() == kEvOK);
        return;
      }
      else
      {
resend:
        res = send(conn->fd,
            &conn->send_buf[conn->send_cursor],
            conn->send_buf.size() - conn->send_cursor,
            0);

        if (res == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
          goto resend;
        }
        else if (res == -1)
        {
          EV_LOG(kInfo, "send failed: %s", strerror(errno));
          goto err;
        }
        else if (res == 0)
        {
          EV_LOG(kInfo, "send EOF");
          goto err;
        }
        else
        {
          conn->send_cursor += res;
          EV_LOG(kInfo, "send %d bytes", conn->send_cursor);
        }
      }
    }
  }

err:
  EV_VERIFY(conn->ev_out.Del() == kEvOK);
  safe_close(conn->fd);
  delete conn;
}

static void InCallback(int fd, int event, void * user_data)
{
  Connection * conn = static_cast<Connection *>(user_data);
  int res;

  if (event & kEvErr)
  {
    socklen_t len = sizeof(res);
    EV_VERIFY(getsockopt(conn->fd, SOL_SOCKET, SO_ERROR, &res, &len) != -1);
    EV_LOG(kInfo, "failed: %s", strerror(res));
    goto err;
  }

  if (event & kEvIn)
  {
    for (;;)
    {
      if (conn->recv_buf.size() == static_cast<size_t>(conn->recv_cursor))
        conn->recv_buf.resize(conn->recv_buf.size() * 2);

      res = recv(conn->fd,
          &conn->recv_buf[conn->recv_cursor],
          conn->recv_buf.size() - conn->recv_cursor,
          0);

      if (res == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
      {
        EV_LOG(kInfo, "too fast to recv, wait");
        return;
      }
      else if (res == -1)
      {
        EV_LOG(kInfo, "recv failed: %s", strerror(errno));
        goto err;
      }
      else if (res == 0)
      {
        EV_LOG(kInfo, "recv EOF");
        goto err;
      }
      else
      {
        conn->recv_cursor += res;
        EV_LOG(kInfo, "recv %d bytes", conn->recv_cursor);

        if (conn->recv_cursor >= 4
            && strncmp(&conn->recv_buf[conn->recv_cursor-4], "\r\n\r\n", 4) == 0)
        {
          EV_LOG(kInfo, "recv OK", conn->recv_cursor);
          goto err;
        }
      }
    }
  }

err:
  EV_VERIFY(conn->ev_in.Del() == kEvOK);
  safe_close(conn->fd);
  delete conn;
}

void HttpGet(const char * url)
{
  EV_LOG(kInfo, "HTTP get %s", url);

  ScopedPtr<Reactor> reactor(new Reactor);
  Connection * conn = new Connection;

  int res;
  int sockfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  EV_VERIFY(sockfd != -1);
  EV_VERIFY(set_non_block(sockfd) == 0);

  struct sockaddr_in s4;
  struct hostent * host = gethostbyname(url);
  EV_VERIFY(host);
  EV_VERIFY(host->h_addrtype == AF_INET);

  EV_LOG(kInfo, "host name: %s", host->h_name);

  s4.sin_family = AF_INET;
  s4.sin_port = htons(80);
  memcpy(&s4.sin_addr, host->h_addr_list[0], 4);
  res = connect(sockfd, (const struct sockaddr *)&s4, sizeof(s4));
  EV_VERIFY(res == -1 && errno == EINPROGRESS);

  conn->reactor = reactor.get();
  conn->fd = sockfd;
  conn->host_name = url;

  conn->ev_in.fd = sockfd;
  conn->ev_in.event = kEvIn|kEvET|kEvPersist;
  conn->ev_in.callback = InCallback;
  conn->ev_in.user_data = conn;

  conn->ev_out.fd = sockfd;
  conn->ev_out.event = kEvOut|kEvET|kEvPersist;
  conn->ev_out.callback = OutCallback;
  conn->ev_out.user_data = conn;

  EV_VERIFY(reactor->Init() == kEvOK);
  EV_VERIFY(reactor->Add(&conn->ev_out) == kEvOK);

  reactor->Run();
  reactor.reset();

  EV_LOG(kInfo, "\n\n");
}


int main()
{
  HttpGet("www.qq.com");
  HttpGet("www.douban.com");
  HttpGet("www.baidu.com");
  return 0;
}
