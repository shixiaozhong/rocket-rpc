#include "rocket/net/timer.h"
#include "rocket/common/log.h"
#include "rocket/common/util.h"
#include <cstring>
#include <sys/timerfd.h>
#include <vector>

namespace rocket {

Timer::Timer() : FdEvent() {
  /*timerfd_create 函数是 Linux
   * 系统提供的一个系统调用，用于创建一个定时器文件描述符（timer file
   * descriptor）。它提供了一种方便的方式来创建定时器，使得应用程序可以通过文件描述符的方式对定时器进行操作。
   int timerfd_create(int clockid, int flags);
  */
  /*
  clockid：指定定时器使用的时钟类型，可以是下列值之一：
    CLOCK_REALTIME：系统实时时间时钟。
    CLOCK_MONOTONIC：系统连续时间时钟，不受时间调整的影响。
    CLOCK_BOOTTIME：系统开机时间时钟，包括系统休眠时间。
    CLOCK_REALTIME_ALARM：系统实时闹钟时钟。
    CLOCK_BOOTTIME_ALARM：系统开机闹钟时钟。

  flags：可以是以下标志的按位或（OR）组合：
    TFD_NONBLOCK：将文件描述符设置为非阻塞模式。
    TFD_CLOEXEC：在 exec 调用时关闭文件描述符。
  */

  m_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
  DEBUGLOG("timer fd=%d", m_fd);

  // 把fd可读事件放到eventloop上进行监听
  listen(FdEvent::IN_EVENT, std::bind(&Timer::onTimer, this));
}

Timer::~Timer() {}

void Timer::onTimer() {
  // 处理缓冲区数据，防止下一次继续触发可读事件
  char buf[8];
  while (true) {
    if (read(m_fd, buf, 8) == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      break;
    }
  }

  // 执行定时任务
  int64_t now = getNowMs();
  std::vector<TimerEvent::s_ptr> tmps;
  std::vector<std::pair<int64_t, std::function<void()>>> tasks;

  ScopeMutext<Mutex> lock(m_mutex);
  auto it = m_pending_events.begin();
  for (it = m_pending_events.begin(); it != m_pending_events.end(); it++) {
    if ((*it).first <= now) {
      if (!(*it).second->isCancled()) {
        tmps.emplace_back((*it).second);
        tasks.emplace_back(std::make_pair((*it).second->getArriveTime(),
                                          (*it).second->getCallBack()));
      }
    } else {
      break;
    }
  }

  m_pending_events.erase(m_pending_events.begin(), it);
  lock.unlock();

  // 需要把重复的event再次添加进去
  for (auto &e : tmps) {
    if (e->isRepeated()) {
      e->resetArriveTime();
      addTimerEvent(e);
    }
  }

  resetArriveTime();

  // 执行回调函数
  for (auto &task : tasks) {
    if (task.second) {
      task.second();
    }
  }
}

void Timer::addTimerEvent(TimerEvent::s_ptr event) {
  bool is_reset_timerfd = false;
  ScopeMutext<Mutex> lock(m_mutex);
  if (m_pending_events.empty()) {
    is_reset_timerfd = true;
  } else {
    auto it = m_pending_events.begin();
    if ((*it).second->getArriveTime() > event->getArriveTime()) {
      is_reset_timerfd = true;
    }
  }
  m_pending_events.emplace(event->getArriveTime(), event);
  lock.unlock();

  if (is_reset_timerfd) {
    resetArriveTime();
  }
}

void Timer::deleteTimerEvent(TimerEvent::s_ptr event) {
  event->setCancel(true);

  ScopeMutext<Mutex> lock(m_mutex);
  auto begin = m_pending_events.lower_bound(event->getArriveTime());
  auto end = m_pending_events.upper_bound(event->getArriveTime());

  auto it = begin;
  for (it = begin; it != end; it++) {
    if (it->second == event) {
      break;
    }
  }
  if (it != end) {
    m_pending_events.erase(it);
  }
  lock.unlock();
  DEBUGLOG("success delete TimerEvent at arrive time %lld",
           event->getArriveTime());
}

void Timer::resetArriveTime() {
  ScopeMutext<Mutex> lock(m_mutex);
  auto tmp = m_pending_events;
  lock.unlock();
  if (tmp.size() == 0) {
    return;
  }

  auto now = getNowMs();
  auto it = tmp.begin();
  int64_t interval = 0;
  if (it->second->getArriveTime() > now) {
    interval = it->second->getArriveTime() - now;
  } else {
    interval = 100;
  }

  timespec ts;
  memset(&ts, 0, sizeof(ts));
  ts.tv_sec = interval / 1000;
  ts.tv_nsec = (interval % 1000) * 1000000;

  itimerspec value;
  memset(&value, 0, sizeof(value));
  value.it_value = ts;

  int rt = timerfd_settime(m_fd, 0, &value, nullptr);
  if (rt != 0) {
    ERRORLOG("timerfd_settime error, errno=%d, error=%s", errno,
             strerror(errno));
  }
  DEBUGLOG("timer reset to %lld", now + interval);
}

} // namespace rocket