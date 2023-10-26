#ifndef ROCKET_NET_TCP_TCP_CLIENT_H
#define ROCKET_NET_TCP_TCP_CLIENT_H
#include "rocket/net/coder/abstract_protocol.h"
#include "rocket/net/fd_event.h"
#include "rocket/net/tcp/net_addr.h"
#include "rocket/net/tcp/tcp_connection.h"

namespace rocket {
class TcpClient {
public:
  TcpClient(NetAddr::s_ptr peer_addr);
  ~TcpClient();

  // 异步的进行connect
  void connect(std::function<void()> done);

  // 异步的发送Message，成功会调用done函数，函数的入参就是message
  void writeMessage(AbstractProtocol::s_ptr message,
                    std::function<void(AbstractProtocol::s_ptr)> done);

  // 异步的读取Message，成功会调用done函数，函数的入参就是message
  void readMessage(const std::string &req_id,
                   std::function<void(AbstractProtocol::s_ptr)> done);

private:
  int m_fd{-1};
  FdEvent *m_fd_event{nullptr};
  NetAddr::s_ptr m_peer_addr;
  EventLoop *m_event_loop{nullptr};
  TcpConnection::s_ptr m_connection;
};
} // namespace rocket

#endif