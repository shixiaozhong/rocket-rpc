#ifndef ROCKET_NET_TCP_TCP_ACCEPTER_H
#define ROCKET_NET_TCP_TCP_ACCEPTER_H

#include "rocket/net/tcp/net_addr.h"
#include <memory>

namespace rocket {
class TcpAccepter {
public:
  using s_ptr = std::shared_ptr<TcpAccepter>;

  TcpAccepter(NetAddr::s_ptr local_addr);

  ~TcpAccepter();

  std::pair<int, NetAddr::s_ptr> accept();

  int getFdEvent() const;

private:
  IPNetAddr::s_ptr m_local_addr; // 服务端监听的地址：(ip:port)
  int m_family{-1};
  int m_listenfd = {-1}; // listenfd
};

} // namespace rocket

#endif