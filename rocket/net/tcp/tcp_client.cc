#include "rocket/net/tcp/tcp_client.h"

#include <cstring>

#include "rocket/common/err_code.h"
#include "rocket/common/log.h"
#include "rocket/net/fd_event_group.h"
#include "rocket/net/tcp/net_addr.h"

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
    m_connection->setState(TcpState::Connected);
    if (done) {
      done();
    }
  } else if (rt == -1) {
    if (errno == EINPROGRESS) {
      // epoll 监听可写事件，然后判断错误码
      m_fd_event->listen(
          FdEvent::TriggerEvent::OUT_EVNET,
          [this, done]() {
            // 利用重复连接来判断connect是否成功
            int rt = ::connect(m_fd, m_peer_addr->getSockAddr(),
                               m_peer_addr->getSocklen());
            if ((rt < 0 && errno == EISCONN) || rt == 0) {
              DEBUGLOG("connect [%s], success",
                       m_peer_addr->toString().c_str());
              initLocalAddr();
              m_connection->setState(TcpState::Connected);
            } else {
              if (errno == ECONNREFUSED) {
                m_connect_error_code = ERROR_PEER_CLOSED;
                m_connect_error_info = "connect refused, sys error = " +
                                       std::string(strerror(errno));
              } else {
                m_connect_error_code = ERROR_FAILED_CONNECT;
                m_connect_error_info = "connect unkown error, sys error = " +
                                       std::string(strerror(errno));
              }
              ERRORLOG("connect error, errno=%d, error=%s", errno,
                       strerror(errno));
              // 将之前的fd关闭，重新申请一个
              close(m_fd);
              m_fd = socket(m_peer_addr->getFamily(), SOCK_STREAM, 0);
            }

            // int error = 0;
            // socklen_t error_len = sizeof(error);
            // getsockopt(m_fd, SOL_SOCKET, SO_ERROR, &error, &error_len);

            // if (error == 0) {
            //   DEBUGLOG("connect [%s] sussess",
            //   m_peer_addr->toString().c_str());

            //   initLocalAddr();

            //   m_connection->setState(TcpState::Connected);

            // } else {
            //   // 连接失败的错误码
            //   m_connect_error_code = ERROR_FAILED_CONNECT;
            //   // 错误信息
            //   m_connect_error_info =
            //       "connect error, sys error = " +
            //       std::string(strerror(errno));

            //   ERRORLOG("connect errror, errno=%d, error=%s", errno,
            //            strerror(errno));
            // }

            // 连接完成后从epoll中删除fd_event
            m_event_loop->deleteEpollEvent(m_fd_event);
            DEBUGLOG("now begin to done");
            // 连接成功后再执行回调
            if (done) {
              done();
            }
          }  // ErrorCallback
             // [this, done]() {
             //   if (errno == ECONNREFUSED) {
             //     m_connect_error_code = ERROR_FAILED_CONNECT;
             //     m_connect_error_info = "connect refused, sys error = " +
             //                            std::string(strerror(errno));
             //   } else {
             //     m_connect_error_code = ERROR_FAILED_CONNECT;
          //     m_connect_error_info = "connect unknown error, sys error = " +
          //                            std::string(strerror(errno));
          //   }
          //   ERRORLOG("connect errror, errno=%d, error=%s", errno,
          //            strerror(errno));
          // }
      );
      m_event_loop->addEpollEvent(m_fd_event);

      if (!m_event_loop->isLooping()) {
        m_event_loop->loop();
      }
    } else {
      // 连接失败的错误码
      m_connect_error_code = ERROR_FAILED_CONNECT;
      // 错误信息
      m_connect_error_info =
          "connect error, sys error = " + std::string(strerror(errno));
      ERRORLOG("connect errror, errno=%d, error=%s", errno, strerror(errno));
      if (done) {
        done();
      }
    }
  }
}

void TcpClient::readMessage(const std::string &msg_id,
                            std::function<void(AbstractProtocol::s_ptr)> done) {
  // 1. 监听可读事件
  // 2. 从buffer里decode得到message对象，
  // 判断msg_id是否相等，相等则都成功，执行其回调
  m_connection->pushReadMessage(msg_id, done);
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

void TcpClient::stop() {
  if (m_event_loop->isLooping()) {
    m_event_loop->stop();
  }
}

void TcpClient::initLocalAddr() {
  sockaddr_in local_addr;
  socklen_t len = sizeof(local_addr);
  int ret = getsockname(m_fd, reinterpret_cast<sockaddr *>(&local_addr), &len);
  if (ret != 0) {
    ERRORLOG("initLocalAddr error, getsockname error, errno=%d", errno,
             strerror(errno));
    return;
  }
  m_local_addr = std::make_shared<IPNetAddr>(local_addr);
}

bool TcpClient::isConnectSuccess() { return m_connect_error_code != 0; }

int TcpClient::getConnectErrCode() const { return m_connect_error_code; }

std::string TcpClient::getConnectErrInfo() const {
  return m_connect_error_info;
}

NetAddr::s_ptr TcpClient::getPeerAddr() const { return m_peer_addr; }
NetAddr::s_ptr TcpClient::getLocalAddr() const { return m_local_addr; }

}  // namespace rocket