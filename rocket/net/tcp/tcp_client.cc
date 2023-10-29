#include "rocket/net/tcp/tcp_client.h"

#include <cstring>

#include "rocket/common/log.h"
#include "rocket/net/fd_event_group.h"

namespace rocket {
TcpClient::TcpClient(NetAddr::s_ptr peer_addr) : m_peer_addr(peer_addr) {
  // 获取当前线程的eventloop
  m_event_loop = EventLoop::GetCurrentEventLoop();
  // 创建客户端的fd
  m_fd = socket(m_peer_addr->getFamily(), SOCK_STREAM, 0);

  if (m_fd < 0) {
    ERRORLOG("TcpClient::TcpClient() error, failed to create fd");
    return;
  }
  m_fd_event = FdEventGroup::GetFdEventGroup()->getFdEvent(m_fd);
  m_fd_event->setNonBlocking();
  m_connection = std::make_shared<TcpConnection>(
      m_event_loop, m_fd, 128, nullptr, m_peer_addr,
      TcpConnectionType::TcpConnectionByClient);
  m_connection->setTcpConnectionType(TcpConnectionType::TcpConnectionByClient);
}

TcpClient::~TcpClient() {
  if (m_fd > 0) {
    close(m_fd);
  }
}

void TcpClient::connect(std::function<void()> done) {
  int rt =
      ::connect(m_fd, m_peer_addr->getSockAddr(), m_peer_addr->getSocklen());
  if (rt == 0) {
    DEBUGLOG("connect [%s] sussess", m_peer_addr->toString().c_str());
    if (done) {
      done();
    }
  } else if (rt == -1) {
    if (errno == EINPROGRESS) {
      // epoll 监听可写事件，然后判断错误码
      m_fd_event->listen(FdEvent::TriggerEvent::OUT_EVNET, [this, done]() {
        int error = 0;
        socklen_t error_len = sizeof(error);
        getsockopt(m_fd, SOL_SOCKET, SO_ERROR, &error, &error_len);

        bool is_connect_succes = false;

        if (error == 0) {
          DEBUGLOG("connect [%s] sussess", m_peer_addr->toString().c_str());

          is_connect_succes = true;
          m_connection->setState(TcpState::Connected);

        } else {
          ERRORLOG("connect errror, errno=%d, error=%s", errno,
                   strerror(errno));
        }
        // 连接完后需要去掉可写事件的监听，不然会一直触发
        m_fd_event->cancel(FdEvent::TriggerEvent::OUT_EVNET);
        m_event_loop->addEpollEvent(m_fd_event);

        // 连接成功后再执行回调
        if (is_connect_succes == true && done) {
          done();
        }
      });
      m_event_loop->addEpollEvent(m_fd_event);

      if (!m_event_loop->isLooping()) {
        m_event_loop->loop();
      }
    } else {
      ERRORLOG("connect errror, errno=%d, error=%s", errno, strerror(errno));
    }
  }
}

void TcpClient::readMessage(const std::string &req_id,
                            std::function<void(AbstractProtocol::s_ptr)> done) {
  // 1. 监听可读事件
  // 2. 从buffer里decode得到message对象，
  // 判断req_id是否相等，相等则都成功，执行其回调
  m_connection->pushReadMessage(req_id, done);
  m_connection->listenRead();
}

void TcpClient::writeMessage(
    AbstractProtocol::s_ptr message,
    std::function<void(AbstractProtocol::s_ptr)> done) {
  // 1.把message对象写入 到Connection的buffer中，done也写入
  // 2.启动connection的可写事件
  m_connection->pushSendMessage(message, done);
  m_connection->listenWrite();
}

}  // namespace rocket