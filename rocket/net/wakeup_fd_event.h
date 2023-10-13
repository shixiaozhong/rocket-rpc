#ifndef ROCKET_NET_WAKEUP_FDEVENT_H
#define ROCKET_EVENT_WAKE_UP_FD_H

#include "rocket/net/fd_event.h"

namespace rocket {
class WakeUpFdEvent : public FdEvent {
public:
  WakeUpFdEvent(int fd);
  ~WakeUpFdEvent();

  // 唤醒wakeUpEvent对象
  void wakeup();

private:
};

}; // namespace rocket
#endif