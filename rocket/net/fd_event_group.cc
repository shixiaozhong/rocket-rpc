#include "rocket/net/fd_event_group.h"

namespace rocket {

static FdEventGroup *g_fd_event_group = nullptr;

FdEventGroup::FdEventGroup(int size) : m_size(size) {
  for (int i = 0; i < m_size; i++) {
    m_fd_group.push_back(new FdEvent(i));
  }
}

FdEventGroup::~FdEventGroup() {
  for (int i = 0; i < m_size; i++) {
    if (m_fd_group[i] != nullptr) {
      delete m_fd_group[i];
      m_fd_group[i] = nullptr;
    }
  }
}

FdEventGroup *FdEventGroup::GetFdEventGroup() {
  if (g_fd_event_group) {
    return g_fd_event_group;
  }
  g_fd_event_group = new FdEventGroup(128);
  return g_fd_event_group;
}

FdEvent *FdEventGroup::getFdEvent(int fd) {
  ScopeMutex<Mutex> lock(m_mutex);
  if (fd < static_cast<int>(m_fd_group.size())) {
    return m_fd_group[fd];
  }
  int new_size = static_cast<int>(fd * 1.5);
  for (int i = m_fd_group.size(); i < new_size; i++) {
    m_fd_group.push_back(new FdEvent(i));
  }
  return m_fd_group[fd];
}

} // namespace rocket