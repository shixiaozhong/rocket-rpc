#include "rocket/common/config.h"
#include "rocket/common/log.h"
#include "rocket/net/eventloop.h"
#include "rocket/net/fd_event.h"
#include "rocket/net/timer_event.h"
#include <arpa/inet.h>
#include <cstring>
#include <memory>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>

int main() {

  rocket::Config::SetGlobalConfig("../conf/rocket.xml");

  rocket::Logger::InitGlobalLogger();

  rocket::EventLoop *event_loop = new rocket::EventLoop();

  int listenfd = socket(AF_INET, SOCK_STREAM, 0);
  if (listenfd == -1) {
    ERRORLOG("listenfd = -1");
    exit(0);
  }
  sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_port = htons(12345);
  addr.sin_family = AF_INET;
  inet_aton("127.0.0.1", &addr.sin_addr);

  int rt = bind(listenfd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr));
  if (rt != 0) {
    ERRORLOG("bind error");
    exit(1);
  }

  rt = listen(listenfd, 100);
  if (rt != 0) {
    ERRORLOG("listen error");
    exit(1);
  }

  rocket::FdEvent event(listenfd);
  event.listen(rocket::FdEvent::IN_EVENT, [listenfd]() {
    sockaddr_in peer_addr;
    socklen_t addr_len = sizeof(peer_addr);
    memset(&peer_addr, 0, sizeof(peer_addr));
    int clientfd =
        accept(listenfd, reinterpret_cast<sockaddr *>(&peer_addr), &addr_len);
    DEBUGLOG("success get client fd[%d], peer_addr: [%s, %d]", clientfd,
             inet_ntoa(peer_addr.sin_addr), ntohs(peer_addr.sin_port));
  });

  event_loop->addEpollEvent(&event);

  int i = 0;
  auto timer_event = std::make_shared<rocket::TimerEvent>(
      1000, true, [&i]() { DEBUGLOG("trigger timer event, count=[%d]", i++); });

  event_loop->addTimerEvent(timer_event);

  event_loop->loop();

  return 0;
}