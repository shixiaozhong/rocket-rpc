#include "rocket/net/tcp/tcp_connection.h"
#include "rocket/common/log.h"
#include "rocket/net/coder/abstract_protocol.h"
#include "rocket/net/fd_event_group.h"
#include <unistd.h>

namespace rocket {
TcpConnection::TcpConnection(
    EventLoop *event_loop, int fd, int buffer_size, NetAddr::s_ptr peer_addr,
    TcpConnectionType type /*TcpConnectionType::TcpConnectionByServer*/)
    : m_event_loop(event_loop), m_peer_addr(peer_addr),
      m_state(TcpState::NotConnected), m_fd(fd), m_connection_type(type) {

  // 创建输入输出的buffer
  m_in_buffer = std::make_shared<TcpBuffer>(buffer_size);
  m_out_buffer = std::make_shared<TcpBuffer>(buffer_size);

  // 获取到fd对应的fd_event
  m_fd_event = FdEventGroup::GetFdEventGroup()->getFdEvent(m_fd);

  // 设置读写为非阻塞
  m_fd_event->setNonBlocking();

  // 初始化编解码器
  m_coder = new TinyPBCoder();

  if (m_connection_type == TcpConnectionType::TcpConnectionByServer) {
    listenRead();
  }
}

TcpConnection::~TcpConnection() {
  DEBUGLOG("~TcpConnection::~TcpConnection()");
  if (m_coder) {
    delete m_coder;
    m_coder = nullptr;
  }
}

void TcpConnection::onRead() {
  // 1.从socket缓冲区，调用系统的read函数读取字节in_buffer里面
  if (m_state != TcpState::Connected) {
    INFOLOG(
        "onRead error, client has already disconnected, addr[%s], clientfd[%d]",
        m_peer_addr->toString().c_str(), m_fd);
    return;
  }

  bool is_read_all = false;
  bool is_close = false;
  while (!is_read_all) {
    if (m_in_buffer->writeAble() == 0) {
      // 扩容
      m_in_buffer->resizeBuffer(2 * m_in_buffer->m_buffer.size());
    }

    // 将所有的数据全部读到in_buffer中
    int read_count = m_in_buffer->writeAble();
    int write_index = m_in_buffer->writeIndex();

    int rt = ::read(m_fd, &(m_in_buffer->m_buffer[write_index]), read_count);
    DEBUGLOG("success read %d bytes from addr [%s], client fd[%d]", rt,
             m_peer_addr->toString().c_str(), m_fd);

    if (rt > 0) {
      m_in_buffer->moveWriteIndex(rt);
      if (rt == read_count) {
        continue;
      }
      if (rt < read_count) {
        is_read_all = true;
        break;
      }
    } else if (rt == 0) {
      // 对端关闭连接
      is_close = true;
      break;
    } else if (rt == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      is_read_all = true;
      break;
    }
  }
  if (is_close) {
    INFOLOG("peer closed, peer addr [%s], clientfd [%d]",
            m_peer_addr->toString().c_str(), m_fd);
    // 将连接清除
    clear();
    return;
  }
  if (!is_read_all) {
    ERRORLOG("not read all data");
  }

  // todo: 简单打印，进行rpc协议的解析
  excute();
}

void TcpConnection::onWrite() {
  // 将当前out_buffer中的所有数据全部发送给client
  if (m_state != TcpState::Connected) {
    INFOLOG("onWrite error, client has already disconnected, addr[%s], "
            "clientfd[%d]",
            m_peer_addr->toString().c_str(), m_fd);
    return;
  }

  if (m_connection_type == TcpConnectionType::TcpConnectionByClient) {
    // 将message encode得到字节流
    // 将数据写入到buffer中，然后全部发送
    std::vector<AbstractProtocol::s_ptr> messages;

    for (auto &e : m_write_dones) {
      messages.push_back(e.first);
    }

    m_coder->encode(messages, m_out_buffer);
  }

  bool is_write_all = false;
  while (true) {
    if (m_out_buffer->readAble() == 0) {
      DEBUGLOG("no data need to send to client [%s]",
               m_peer_addr->toString().c_str());
      is_write_all = true;
      break;
    }
    int write_size = m_out_buffer->readAble();
    int read_index = m_out_buffer->readIndex();
    int rt = ::write(m_fd, &(m_out_buffer->m_buffer[read_index]), write_size);

    std::string s;
    for (int i = read_index; i < write_size; i++) {
      s += m_out_buffer->m_buffer[i];
    }

    if (rt >= write_size) {
      DEBUGLOG("no data need to send to client [%s]",
               m_peer_addr->toString().c_str());
      is_write_all = true;
      break;
    }
    if (rt == -1 && errno == EAGAIN) {
      // 发送缓冲区已经满了，等下次fd可写的时候再发送
      ERRORLOG("write data error, errno==EAGAIN and rt == -1")
      break;
    }
  }

  // 如果已经读写完毕
  if (is_write_all) {
    // 取消监听fd_event的写事件
    m_fd_event->cancel(FdEvent::OUT_EVNET);
    m_event_loop->addEpollEvent(m_fd_event);
  }

  if (m_connection_type == TcpConnectionType::TcpConnectionByClient) {
    for (auto &e : m_write_dones) {
      e.second(e.first);
    }
    m_write_dones.clear();
  }
}

// 将rpc执行请求执行业务逻辑，获取rpc响应，再把rpc响应发送出去
void TcpConnection::excute() {
  if (m_connection_type == TcpConnectionType::TcpConnectionByServer) {
    std::vector<AbstractProtocol::s_ptr> result;
    std::vector<AbstractProtocol::s_ptr> responses;

    m_coder->decode(result, m_in_buffer);
    for (auto &e : result) {
      INFOLOG("success get request [%s] from client[%s]", e->m_req_id.c_str(),
              m_peer_addr->toString().c_str());
      // 1.针对每一个请求，调用rpc方法，获取响应message
      // 2. 将响应message放到发送缓冲区，监听可写事件回包

      auto message = std::make_shared<TinyPBProtocol>();
      message->m_pb_data = "hello. this is rocket rpc test data";
      message->m_req_id = e->m_req_id;

      responses.emplace_back(message);
    }
    m_coder->encode(responses, m_out_buffer);
    listenWrite();

  } else if (m_connection_type == TcpConnectionType::TcpConnectionByClient) {
    // 从buffer中decode得到message对象，执行其回调
    std::vector<AbstractProtocol::s_ptr> result;

    m_coder->decode(result, m_in_buffer);

    for (auto e : result) {
      std::string req_id = e->m_req_id;
      auto it = m_read_dones.find(req_id);
      if (it != m_read_dones.end()) {
        it->second(e);
      }
    }
  }
}

void TcpConnection::setState(const TcpState state) { m_state = state; }

TcpState TcpConnection::getState() const { return m_state; }

// 清除连接
void TcpConnection::clear() {
  // 处理一些关闭连接后的清理动作
  if (m_state == TcpState::Closed) {
    return;
  }

  // 将fd_event的读写事件都cancel掉
  m_fd_event->cancel(FdEvent::TriggerEvent::IN_EVENT);
  m_fd_event->cancel(FdEvent::TriggerEvent::OUT_EVNET);

  // 从eventloop中清除
  m_event_loop->deleteEpollEvent(m_fd_event);

  DEBUGLOG("TcpConnection clear success, fd [%d], peer_addr [%s]", m_fd,
           m_peer_addr->toString().c_str());
  // 将连接状态设置为Closed
  m_state = TcpState::Closed;
}

// 服务器主动关闭连接
void TcpConnection::shutdown() {
  if (m_state == TcpState::Closed || m_state == TcpState::NotConnected) {
    return;
  }

  // 设置为半关闭状态
  m_state = TcpState::HalfClosing;

  // 调用shutdown关闭读写，意味着服务器不会再对这个fd进行读写操作了
  // 发送FIN报文，触发四次挥手的第一个阶段
  // 当fd发生可读事件，但是可读的数据为0，即对端发生了FIN
  ::shutdown(m_fd, SHUT_RDWR);
}

void TcpConnection::setTcpConnectionType(const TcpConnectionType type) {
  m_connection_type = type;
}

void TcpConnection::listenRead() {
  // 为fd的读事件绑定onRead回调函数
  m_fd_event->listen(FdEvent::TriggerEvent::IN_EVENT,
                     std::bind(&TcpConnection::onRead, this));
  // fd_event添加到io线程的event_loop中
  m_event_loop->addEpollEvent(m_fd_event);
}

void TcpConnection::listenWrite() {
  m_fd_event->listen(FdEvent::TriggerEvent::OUT_EVNET,
                     std::bind(&TcpConnection::onWrite, this));
  m_event_loop->addEpollEvent(m_fd_event);
}

void TcpConnection::pushSendMessage(
    AbstractProtocol::s_ptr message,
    std::function<void(AbstractProtocol::s_ptr)> done) {
  m_write_dones.push_back(std::make_pair(message, done));
}

void TcpConnection::pushReadMessage(
    const std::string req_id,
    std::function<void(AbstractProtocol::s_ptr)> done) {
  m_read_dones.insert(std::make_pair(req_id, done));
}
} // namespace rocket
