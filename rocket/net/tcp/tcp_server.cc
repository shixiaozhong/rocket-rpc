#include "rocket/net/tcp/tcp_server.h"

#include <string>

#include "rocket/common/log.h"
#include "rocket/net/tcp/tcp_connection.h"

namespace rocket {
TcpServer::TcpServer(NetAddr::s_ptr local_addr) : m_local_addr(local_addr) {
  init();
  INFOLOG("rocket TcpServer listen success on [%s]",
          m_local_addr->toString().c_str());
}

TcpServer::~TcpServer() {}

// 进行server的初始化
void TcpServer::init() {
  // 创建一个accepter
  m_accepter = std::make_shared<TcpAccepter>(m_local_addr);

  // 获取到主线程的eventloop
  m_main_eventloop = EventLoop::GetCurrentEventLoop();

  // 创建io线程组
  m_io_thread_group = new IOThreadGroup(2);

  // 获取到listenfd event
  m_listen_fd_event = new FdEvent(m_accepter->getFdEvent());

  // 给listen_fd_event的输入事件绑定回调函数
  m_listen_fd_event->listen(FdEvent::IN_EVENT,
                            std::bind(&TcpServer::onAccept, this));
  // 将该listen_fd_event添加到主线程的eventloop中
  m_main_eventloop->addEpollEvent(m_listen_fd_event);
}

void TcpServer::onAccept() {
  // listenfd上有输入事件，代表有新连接到来，进行accept
  auto result = m_accepter->accept();

  // 获取clentfd和client的地址信息
  int client_fd = result.first;
  NetAddr::s_ptr peer_addr = result.second;

  // client数量++
  m_client_counts++;

  // 把clientfd添加到任意一个io线程中
  IOThread *io_thread = m_io_thread_group->getIOThread();

  // 为client建立新连接
  TcpConnection::s_ptr connection = std::make_shared<TcpConnection>(
      io_thread->getEventLoop(), client_fd, 128, m_local_addr, peer_addr);

  // 设置建立的连接为Connected
  connection->setState(TcpState::Connected);

  // 将该client所建立的连接添加到m_clients中
  m_clients.insert(connection);

  INFOLOG("TcpServer success get client, fd=%d", client_fd);
}

void TcpServer::start() {
  // 线程组启动
  m_io_thread_group->start();

  // 主线程的loop开始
  m_main_eventloop->loop();
}

}  // namespace rocket