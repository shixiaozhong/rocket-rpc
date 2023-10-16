#ifndef ROCKET_NET_FDEVENT_H
#define ROCKET_NET_FDEVENT_H
#include <functional>
#include <sys/epoll.h>

namespace rocket {
class FdEvent {
public:
  // 触发事件类型
  enum TriggerEvent {
    IN_EVENT = EPOLLIN,
    OUT_EVNET = EPOLLOUT,
  };

  FdEvent(int fd);

  FdEvent();

  ~FdEvent();

  // 处理触发事件的类型，返回对应的回调函数
  std::function<void()> handler(TriggerEvent event_type);

  // 设置epoll_event的类型和fdEvent对象对应的回调函数
  void listen(TriggerEvent event_type, std::function<void()> callback);

  // 返回fd
  int getFd() const { return m_fd; }

  // 获取文件描述符对应的epoll_event
  epoll_event getEpollEvent() const { return m_listen_event; }

protected:
  int m_fd{-1};                           // 文件描述符
  epoll_event m_listen_event;             // 监听事件
  std::function<void()> m_read_callback;  // 读回调
  std::function<void()> m_write_callback; // 写回调
};

} // namespace rocket

#endif