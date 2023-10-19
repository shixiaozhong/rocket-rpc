#include "rocket/net/tcp/tcp_accepter.h"
#include "rocket/common/log.h"
#include <cstring>
#include <sys/socket.h>
#include <sys/types.h>

namespace rocket {
TcpAccepter::TcpAccepter(IPNetAddr::s_ptr local_addr)
    : m_local_addr(local_addr) {

  // 判断地址是否有效
  if (!local_addr->checkValid()) {
    ERRORLOG("invalid local addr %s", local_addr->toString().c_str());
    exit(1);
  }

  m_family = m_local_addr->getFamily(); // 初始化family

  m_listenfd = socket(m_family, SOCK_STREAM, 0); // 创建listenfd
  if (m_listenfd < 0) {
    ERRORLOG("invalid listenfd %d", m_listenfd);
    exit(0);
  }

  int val = 1;
  if (setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) !=
      0) {
    ERRORLOG("setsockopt SO_REUSEADDR error ,errno=%d", errno, strerror(errno));
  }

  socklen_t len = m_local_addr->getSocklen();
  // bind服务器端地址
  if (bind(m_listenfd, m_local_addr->getSockAddr(), len) != 0) {
    ERRORLOG("bind error, errno=%d, error=%d", errno, strerror(errno));
    exit(0);
  }

  // 开启监听
  if (listen(m_listenfd, 1000) != 0) {
    ERRORLOG("listen error, errno=%d, error=%s", errno, strerror(errno));
    exit(0);
  }
}

TcpAccepter::~TcpAccepter() {}

std::pair<int, NetAddr::s_ptr> TcpAccepter::accept() {
  if (m_family == AF_INET) {
    sockaddr_in client_addr;
    memset(&client_addr, 0, sizeof(client_addr));
    socklen_t client_addr_len = sizeof(client_addr);

    // 接收client连接
    int client_fd =
        ::accept(m_listenfd, reinterpret_cast<sockaddr *>(&client_addr),
                 &client_addr_len);
    if (client_fd == -1) {
      ERRORLOG("accept error, errno=%d, error=%s", errno, strerror(errno));
    }
    auto peer_addr = std::make_shared<IPNetAddr>(client_addr); // shared_ptr
    INFOLOG("A client have accepted success, peer addr [%s]",
            peer_addr->toString().c_str());
    // 将client fd和addr都返回
    return std::make_pair(client_fd, peer_addr);
  } else {
    // 其他协议
    return std::make_pair(-1, nullptr);
  }
}

int TcpAccepter::getFdEvent() const { return m_listenfd; }
} // namespace rocket
