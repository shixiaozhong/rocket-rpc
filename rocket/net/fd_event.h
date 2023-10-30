#ifndef ROCKET_NET_FDEVENT_H
#define ROCKET_NET_FDEVENT_H
#include <sys/epoll.h>

#include <functional>

namespace rocket {
class FdEvent {
 public:
  // 触发事件类型
  enum TriggerEvent {
    IN_EVENT = EPOLLIN,
    OUT_EVNET = EPOLLOUT,
    ERROR_EVENT = EPOLLERR,
  };

  FdEvent(int fd);

  FdEvent();

  ~FdEvent();

  void setNonBlocking();

  // 处理触发事件的类型，返回对应的回调函数
  std::function<void()> handler(TriggerEvent event_type);

  // 设置epoll_event的类型和fdEvent对象对应的回调函数
  void listen(TriggerEvent event_type, std::function<void()> callback,
              std::function<void()> error_callback = nullptr);

  // 返回fd
  int getFd() const { return m_fd; }

  // 获取文件描述符对应的epoll_event
  epoll_event getEpollEvent() const { return m_listen_event; }

  void cancel(TriggerEvent event_type);

 protected:
  int m_fd{-1};                                     // 文件描述符
  epoll_event m_listen_event;                       // 监听事件
  std::function<void()> m_read_callback{nullptr};   // 读回调
  std::function<void()> m_write_callback{nullptr};  // 写回调
  std::function<void()> m_error_callback{nullptr};  // 错误回调
};

}  // namespace rocket

#endif