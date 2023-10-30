#include "rocket/net/fd_event.h"

#include <fcntl.h>

#include <cstring>

namespace rocket {

FdEvent::FdEvent(int fd) : m_fd(fd) {
  memset(&m_listen_event, 0, sizeof(m_listen_event));
}

FdEvent::FdEvent() { memset(&m_listen_event, 0, sizeof(m_listen_event)); }

FdEvent::~FdEvent() {}

std::function<void()> FdEvent::handler(TriggerEvent event_type) {
  // 返回对应的回调函数
  if (event_type == TriggerEvent::IN_EVENT) {
    return m_read_callback;
  } else if (event_type == TriggerEvent::OUT_EVNET) {
    return m_write_callback;
  } else if (event_type == TriggerEvent::ERROR_EVENT) {
    return m_error_callback;
  }
  return nullptr;
}

void FdEvent::listen(TriggerEvent event_type, std::function<void()> callback,
                     std::function<void()> error_callback /* = nullptr*/) {
  if (event_type == TriggerEvent::IN_EVENT) {
    // 设置epoll_event的类型和FdEvent对应的回调函数
    m_listen_event.events |= EPOLLIN;
    m_read_callback = callback;
  } else if (event_type == TriggerEvent::OUT_EVNET) {
    m_listen_event.events |= EPOLLOUT;
    m_write_callback = callback;
  }

  if (m_error_callback == nullptr) {
    m_error_callback = error_callback;
  } else {
    m_error_callback = nullptr;
  }
  // 设置fdEvent对象指针
  m_listen_event.data.ptr = this;
}

void FdEvent::setNonBlocking() {
  int flag = fcntl(m_fd, F_GETFL, 0);
  if (flag & O_NONBLOCK) {
    return;
  }
  fcntl(m_fd, F_SETFL, flag | O_NONBLOCK);
}

void FdEvent::cancel(TriggerEvent event_type) {
  if (event_type == TriggerEvent::IN_EVENT) {
    m_listen_event.events &= ~EPOLLIN;
  } else {
    m_listen_event.events &= ~EPOLLOUT;
  }
}

}  // namespace rocket