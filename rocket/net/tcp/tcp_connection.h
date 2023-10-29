#ifndef ROCKET_NET_TCP_TCP_CONNECTION_H
#define ROCKET_NET_TCP_TCP_CONNECTION_H

#include <map>
#include <memory>
#include <queue>
#include <string>
#include <vector>

#include "rocket/net/coder/abstract_coder.h"
#include "rocket/net/coder/abstract_protocol.h"
#include "rocket/net/coder/tinypb_coder.h"
#include "rocket/net/io_thread.h"
#include "rocket/net/rpc/rpc_dispatcher.h"
#include "rocket/net/tcp/net_addr.h"
#include "rocket/net/tcp/tcp_buffer.h"

namespace rocket {
enum TcpState {
  NotConnected = 1,
  Connected = 2,
  HalfClosing = 3,
  Closed = 4,
};

enum TcpConnectionType {
  TcpConnectionByServer = 1,  // 作为服务端使用，代表跟对端客户端连接
  TcpConnectionByClient = 2,  // 作为客户端使用，代表跟对端服务端连接
};

class RpcDispatcher;

class TcpConnection {
 public:
  using s_ptr = std::shared_ptr<TcpConnection>;

  TcpConnection(
      EventLoop *event_loop, int fd, int buffer_size, NetAddr::s_ptr local_addr,
      NetAddr::s_ptr peer_addr,
      TcpConnectionType type = TcpConnectionType::TcpConnectionByServer);

  ~TcpConnection();

  void onRead();

  void excute();

  void onWrite();

  void setState(const TcpState state);

  TcpState getState() const;

  void clear();

  // 服务器主动关闭连接
  void shutdown();

  // 设置TcpConnection的类型
  void setTcpConnectionType(const TcpConnectionType type);

  // 监听可写事件
  void listenWrite();

  // 监听可读事件
  void listenRead();

  void pushSendMessage(AbstractProtocol::s_ptr message,
                       std::function<void(AbstractProtocol::s_ptr)> done);

  void pushReadMessage(const std::string req_id,
                       std::function<void(AbstractProtocol::s_ptr)> done);

  NetAddr::s_ptr getLocalAddr() const { return m_local_addr; };
  NetAddr::s_ptr getPeerAddr() const { return m_peer_addr; };

 private:
  EventLoop *m_event_loop{nullptr};  // 对应的event_loop

  NetAddr::s_ptr m_local_addr;    // 连接中的server地址信息
  NetAddr::s_ptr m_peer_addr;     // 连接中的client地址信息
  TcpBuffer::s_ptr m_in_buffer;   // 接收缓冲区
  TcpBuffer::s_ptr m_out_buffer;  // 发送缓冲区

  FdEvent *m_fd_event{nullptr};  // 连接中的fd_event
  TcpState m_state;              // 连接的状态
  int m_fd{-1};                  // 连接的fd，即client_fd

  TcpConnectionType m_connection_type;  // 标识tcpConnection的类型
  AbstractCoder *m_coder{nullptr};      // 编解码器

  std::vector<std::pair<AbstractProtocol::s_ptr,
                        std::function<void(AbstractProtocol::s_ptr)>>>
      m_write_dones;

  // key is req_id
  std::map<std::string, std::function<void(AbstractProtocol::s_ptr)>>
      m_read_dones;
};

}  // namespace rocket

#endif