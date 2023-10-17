#include "rocket/net/io_thread_group.h"

namespace rocket {
IOThreadGroup::IOThreadGroup(int size) : m_size(size) {
  m_io_thread_group.resize(m_size);
  for (int i = 0; i < m_size; i++) {
    m_io_thread_group[i] = new IOThread();
  }
}

IOThreadGroup::~IOThreadGroup() {}

void IOThreadGroup::start() {
  for (auto e : m_io_thread_group) {
    e->start();
  }
}

void IOThreadGroup::join() {
  for (auto e : m_io_thread_group) {
    e->join();
  }
}

IOThread *IOThreadGroup::getIOThread() {
  if (m_index == m_io_thread_group.size()) {
    m_index = 0;
  }
  return m_io_thread_group[m_index++];
}
} // namespace rocket