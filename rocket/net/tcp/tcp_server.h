#ifndef ROCKET_NET_TCP_TCP_SERVER_H
#define ROCKET_NET_TCP_TCP_SERVER_H
#include "rocket/net/io_thread_group.h"
#include "rocket/net/tcp/net_addr.h"
#include "rocket/net/tcp/tcp_accepter.h"
#include "rocket/net/tcp/tcp_connection.h"
#include <set>

namespace rocket {
class TcpServer {
public:
  TcpServer(NetAddr::s_ptr local_addr);

  ~TcpServer();

  // 启动TcpServer
  void start();

private:
  void init();
  void onAccept();

private:
  TcpAccepter::s_ptr m_accepter;             // TcpAccepter
  NetAddr::s_ptr m_local_addr;               // 本地监听的地址
  EventLoop *m_main_eventloop{nullptr};      // main reactor的eventloop
  IOThreadGroup *m_io_thread_group{nullptr}; // subReactor组
  FdEvent *m_listen_fd_event;                // server的listen event
  int m_client_counts;                      // 已经建立连接的用户数量
  std::set<TcpConnection::s_ptr> m_clients; // 所有client的connection
};

}; // namespace rocket

#endif