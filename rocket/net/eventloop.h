#ifndef ROCKET_NET_EVENTLOOP_h
#define ROCKET_NET_EVENTLOOP_H
#include <pthread.h>

#include <functional>
#include <mutex>
#include <queue>
#include <set>

#include "rocket/common/mutex.h"
#include "rocket/common/util.h"
#include "rocket/net/fd_event.h"
#include "rocket/net/timer.h"
#include "rocket/net/timer_event.h"
#include "rocket/net/wakeup_fd_event.h"

namespace rocket {
class EventLoop {
 public:
  EventLoop();
  ~EventLoop();

  // 核心loop循环
  void loop();

  // 唤醒wakeUpEvent
  void wakeup() { m_wakeup_fd_event->wakeup(); }

  // 停止
  void stop();

  // 添加epoll_event
  void addEpollEvent(FdEvent *event);

  // 删除epoll_event
  void deleteEpollEvent(FdEvent *event);

  // 判断是否是当前
  bool isInLoopThread() { return getThreadId() == m_thread_id; }

  // 添加任务
  void addTask(std::function<void()> cb, bool is_wake_up = false);

  // 添加定时任务
  void addTimerEvent(TimerEvent::s_ptr event);

  // 是否正在looping
  bool isLooping() const { return m_is_looping; }

 public:
  static EventLoop *GetCurrentEventLoop();

 private:
  void dealWakeup(){};

  // 初始化wakeUpEvent
  void initWakeUpFdEvent();

  void initTimer();

 private:
  pid_t m_thread_id{0};                       // 当前线程id
  int m_epoll_fd{0};                          // 标识epoll实例
  int m_wakeup_fd{0};                         // wakeUpEvent的fd
  WakeUpFdEvent *m_wakeup_fd_event{nullptr};  // wakeUpEvent对应的指针
  bool m_is_stop_flag{false};                 // loop循环停止的标志
  bool m_is_looping{false};                   // 是否正在loop中
  std::set<int> m_listen_fds;  // 储存Reactor模型监听的文件描述符列表
  std::queue<std::function<void()>> m_pending_tasks;  // 待决任务队列
  Mutex m_mutex;                                      // 互斥锁
  Timer *m_timer{nullptr};                            // 定时器
};

}  // namespace rocket

#endif