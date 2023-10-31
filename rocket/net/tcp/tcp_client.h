#ifndef ROCKET_NET_TCP_TCP_CLIENT_H
#define ROCKET_NET_TCP_TCP_CLIENT_H
#include "rocket/net/coder/abstract_protocol.h"
#include "rocket/net/fd_event.h"
#include "rocket/net/tcp/net_addr.h"
#include "rocket/net/tcp/tcp_connection.h"
#include "rocket/net/timer_event.h"

namespace rocket {
class TcpClient {
 public:
  using s_ptr = std::shared_ptr<TcpClient>;

  TcpClient(NetAddr::s_ptr peer_addr);
  ~TcpClient();

  // 异步的进行connect，connect完成，done会被执行
  void connect(std::function<void()> done);

  // 异步的发送Message，成功会调用done函数，函数的入参就是message
  void writeMessage(AbstractProtocol::s_ptr message,
                    std::function<void(AbstractProtocol::s_ptr)> done);

  // 异步的读取Message，成功会调用done函数，函数的入参就是message
  void readMessage(const std::string &msg_id,
                   std::function<void(AbstractProtocol::s_ptr)> done);

  void stop();

  bool isConnectSuccess();
  int getConnectErrCode() const;

  std::string getConnectErrInfo() const;

  NetAddr::s_ptr getPeerAddr() const;
  NetAddr::s_ptr getLocalAddr() const;

  void initLocalAddr();

  void addTimerEvent(TimerEvent::s_ptr timer_event);

 private:
  int m_fd{-1};
  FdEvent *m_fd_event{nullptr};
  NetAddr::s_ptr m_peer_addr;
  NetAddr::s_ptr m_local_addr;
  EventLoop *m_event_loop{nullptr};
  TcpConnection::s_ptr m_connection;

  int m_connect_error_code{0};
  std::string m_connect_error_info;
};
}  // namespace rocket

#endif