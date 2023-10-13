#include "rocket/net/wakeup_fd_event.h"
#include "rocket/common/log.h"
#include <unistd.h>

namespace rocket {
WakeUpFdEvent::WakeUpFdEvent(int fd) : FdEvent(fd) {}
WakeUpFdEvent::~WakeUpFdEvent() {}

void WakeUpFdEvent::wakeup() {
  char buf[8]{'a'};
  int rt = write(m_fd, buf, sizeof(buf)); // 往文件中写数据

  if (rt != sizeof(buf)) {
    ERRORLOG("write to wakeup fd less than 8 bytes, fd[%d]", m_fd);
  }
}

} // namespace rocket