#include "rocket/net/eventloop.h"
#include "rocket/common/log.h"
#include "rocket/net/timer.h"
#include <chrono>
#include <cstring>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>

/*
epoll_ctl 函数是 Linux 中用于控制 epoll
实例的操作函数之一，它用于向epoll实例中添加、修改或删除感兴趣的文件描述符。

EPOLL_CTL_ADD：将文件描述符 fd添加到 epoll 实例中。
EPOLL_CTL_MOD：修改文件描述符 fd 在 epoll实例中的事件状态。
EPOLL_CTL_DEL：从 epoll 实例中删除文件描述符 fd。

*/

#define ADD_TO_EPOLL()                                                         \
  auto it = m_listen_fds.find(event->getFd());                                 \
  int op = EPOLL_CTL_ADD;                                                      \
  if (it != m_listen_fds.end()) {                                              \
    op = EPOLL_CTL_MOD;                                                        \
  }                                                                            \
  /*获取FdEvent对应的epoll_event*/                                        \
  epoll_event tmp = event->getEpollEvent();                                    \
  INFOLOG("epoll_event.events = %d", (int)tmp.events);                         \
  /*epoll中加入或者修改event对应的epoll_event*/                      \
  int rt = epoll_ctl(m_epoll_fd, op, event->getFd(), &tmp);                    \
  if (rt == -1) {                                                              \
    ERRORLOG("failed epoll_ctl when add fd, errno=%d, errno=%s", errno,        \
             std::strerror(errno));                                            \
  }                                                                            \
  m_listen_fds.insert(event->getFd());                                         \
  DEBUGLOG("add event success, fd[%d]", event->getFd());

#define DEL_TO_EPOLL()                                                         \
  auto it = m_listen_fds.find(event->getFd());                                 \
  if (it == m_listen_fds.end()) {                                              \
    return;                                                                    \
  }                                                                            \
  int op = EPOLL_CTL_DEL;                                                      \
  epoll_event tmp = event->getEpollEvent();                                    \
  /*从epoll中删除FdEvent对应的epoll_event*/                             \
  int rt = epoll_ctl(m_epoll_fd, op, event->getFd(), &tmp);                    \
  if (rt == -1) {                                                              \
    ERRORLOG("failed epoll_ctl when add fd errno=%d, error=%s", errno,         \
             std::strerror(errno));                                            \
  }                                                                            \
  m_listen_fds.erase(event->getFd());                                          \
  DEBUGLOG("delete event success, fd[%d]", event->getFd());

namespace rocket {
static thread_local EventLoop *t_current_eventloop =
    nullptr;                        // 获取当前线程的eventloop指针
static int g_epoll_timeout = 10000; // epoll_wait的延迟时间
static int g_epoll_max_events = 10; // epoll_wait等待的事件数量

EventLoop::EventLoop() {
  // 判断当前线程是否已经创建过了eventloop
  if (t_current_eventloop != nullptr) {
    ERRORLOG("failed to create event loop ,this thread has created event loop");
    exit(0);
  }

  m_epoll_fd = epoll_create(1); // 创建epoll实例，返回一个文件描述符
  m_thread_id = getThreadId(); // 获取当前线程id
  if (m_epoll_fd == -1) {
    ERRORLOG(
        "failed to create event loop , epoll_create error , error info[%d]",
        errno);
    exit(0);
  }

  // 初始化wakeUpEvent
  initWakeUpFdEvent();

  // 初始化Timer
  initTimer();
  INFOLOG("success create event loop in thread %d", m_thread_id);

  t_current_eventloop = this;
}

EventLoop::~EventLoop() {
  close(m_epoll_fd); // 关闭文件描述符
  if (m_wakeup_fd_event) {
    // 如果指针没有释放，将其释放
    delete m_wakeup_fd_event;
    m_wakeup_fd_event = nullptr;
  }

  if (m_timer) {
    delete m_timer;
    m_timer = nullptr;
  }
}

void EventLoop::initTimer() {
  m_timer = new Timer();
  addEpollEvent(m_timer);
}

void EventLoop::initWakeUpFdEvent() {
  m_wakeup_fd = eventfd(
      0, EFD_NONBLOCK); // 为wakeUpEvent创建一个fd，并且设置为非阻塞模式。
  // 非阻塞模式下，对读取和写入操作会立即返回，不会阻塞进程，无法立即完成会将errno设置为EAGAIN
  // 或 EWOULDBLOCK

  if (m_wakeup_fd < 0) {
    ERRORLOG("failed to create wakeUp event fd, eventfd create error, error "
             "info[%d]",
             errno);
    exit(0);
  }

  INFOLOG("wakeup fd = %d", m_wakeup_fd);

  m_wakeup_fd_event = new WakeUpFdEvent(m_wakeup_fd); // new一个wakeUpEvent对象

  // 设置为epoll_event为读事件和设置对应回调函数
  m_wakeup_fd_event->listen(FdEvent::IN_EVENT, [this]() {
    char buf[8];
    while (read(m_wakeup_fd, buf, sizeof(buf)) != -1 && errno != EAGAIN &&
           errno != EWOULDBLOCK) {
    }
    DEBUGLOG("read full butes from wakeup fd [%d]", m_wakeup_fd);
  });

  // 添加到epoll_event中
  addEpollEvent(m_wakeup_fd_event);
}

void EventLoop::loop() {
  m_is_looping = true;

  while (!m_is_stop_flag) {
    ScopeMutex<Mutex> lock(m_mutex);
    std::queue<std::function<void()>> tmp_tasks;
    m_pending_tasks.swap(
        tmp_tasks); // 将pending_tasks中数据交换到tmp中去做处理，顺便清空pending_tasks
    lock.unlock();

    // 将任务队列中的任务全部处理掉
    while (!tmp_tasks.empty()) {
      auto cb = tmp_tasks.front();
      tmp_tasks.pop();
      if (cb) {
        cb();
      }
    }

    /*epoll_wait 函数是 Linux
        中用于等待事件的函数，它会阻塞当前线程，直到有事件发生或超时。
        epfd：epoll 实例的文件描述符，由 epoll_create 或 epoll_create1
       函数返回。
    */
    /*
    events：指向 struct epoll_event 结构的指针，用于存储发生的事件。
    maxevents：events 数组的大小，表示最多可以存储多少个事件。
    timeout：超时时间（以毫秒为单位），指定等待事件的最长时间。传递以下值之一：
      -1：永久等待，直到有事件发生。
      0：非阻塞模式，立即返回，如果没有事件发生则返回 0。
      大于 0：等待指定的毫秒数后返回，如果没有事件发生则返回 0。
    */

    /*返回值：
      成功时，返回发生的事件数量。
      失败时，返回-1，并设置 errno 来指示错误的原因。
    */
    int timeout = g_epoll_timeout;
    epoll_event result_events[g_epoll_max_events]; // 用于存储发生的事件

    DEBUGLOG("now begin to epoll_wait");
    int rt = epoll_wait(m_epoll_fd, result_events, g_epoll_max_events, timeout);
    DEBUGLOG("now end epoll_wait, rt=%d", rt);

    if (rt < 0) {
      ERRORLOG("epoll_wait error ,errno=", errno);
    } else {
      // 处理所有已经发生的事件
      for (int i = 0; i < rt; i++) {
        epoll_event trigger_event = result_events[i];
        FdEvent *fd_event = static_cast<FdEvent *>(
            trigger_event.data.ptr); // 获取到epoll_event对应的FdEvent
        if (fd_event == nullptr) {
          continue;
        }
        // 将对应的event的回调函数添加到任务队列中
        if (trigger_event.events & EPOLLIN) {
          DEBUGLOG("fd %d trigger EPOLLIN event", fd_event->getFd());
          addTask(fd_event->handler(FdEvent::IN_EVENT));
        }
        if (trigger_event.events & EPOLLOUT) {
          DEBUGLOG("fd %d trigger EPOLLOUT event", fd_event->getFd());
          addTask(fd_event->handler(FdEvent::OUT_EVNET));
        }
      }
    }
  }
}

void EventLoop::addEpollEvent(FdEvent *event) {
  // 判断是否为当前线程，如果不是只需要添加epoll中，不需要添加到任务队列中
  if (isInLoopThread()) {
    ADD_TO_EPOLL()
  } else {
    auto cb = [this, event]() { ADD_TO_EPOLL(); };
    addTask(cb, true);
  }
}

void EventLoop::deleteEpollEvent(FdEvent *event) {
  // 判断是否为当前线程，如果不是只需要从epoll中删除，不需要添加到任务队列中
  if (isInLoopThread()) {
    DEL_TO_EPOLL()
  } else {
    auto cb = [this, event]() { DEL_TO_EPOLL() };
    addTask(cb, true);
  }
}

void EventLoop::addTask(std::function<void()> cb, bool is_wake_up) {
  ScopeMutex<Mutex> lock(m_mutex);
  m_pending_tasks.push(cb);
  lock.unlock();
  // 判断是否需要wake_up
  if (is_wake_up) {
    wakeup();
  }
}

void EventLoop::addTimerEvent(TimerEvent::s_ptr event) {
  m_timer->addTimerEvent(event);
}

EventLoop *EventLoop::GetCurrentEventLoop() {
  if (t_current_eventloop) {
    return t_current_eventloop;
  }
  t_current_eventloop = new EventLoop();
  return t_current_eventloop;
}
} // namespace rocket