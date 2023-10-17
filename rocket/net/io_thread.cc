#include "rocket/net/io_thread.h"
#include "rocket/common/log.h"
#include <cassert>

namespace rocket {

IOThread::IOThread() {
  // 初始化信号量
  int rt = sem_init(&m_init_semaphore, 0, 0);
  assert(rt == 0);
  rt = sem_init(&m_start_semaphore, 0, 0);
  assert(rt == 0);

  pthread_create(&m_thread, nullptr, Main, this);
  // wait, 知道新线程执行完Main函数的前置
  sem_wait(&m_init_semaphore);
  DEBUGLOG("IOThread %d create success", m_thread_id);
}

IOThread::~IOThread() {
  m_event_loop->stop();
  sem_destroy(&m_init_semaphore);
  sem_destroy(&m_start_semaphore);

  if (m_event_loop) {
    delete m_event_loop;
    m_event_loop = nullptr;
  }
}

void IOThread::start() {
  DEBUGLOG("Now invoke IOThread %d", m_thread_id);
  sem_post(&m_start_semaphore);
}

void IOThread::join() { pthread_join(m_thread, nullptr); }

void *IOThread::Main(void *arg) {
  IOThread *thread = static_cast<IOThread *>(arg);
  thread->m_event_loop = new EventLoop();
  thread->m_thread_id = getThreadId();

  // 前置执行完毕后，唤醒等待的线程
  sem_post(&thread->m_init_semaphore);

  // 让IO线程等待，直到我们主动启动loop循环
  DEBUGLOG("IOThread %d created, wait start event loop", thread->m_thread_id);
  sem_wait(&thread->m_start_semaphore);

  DEBUGLOG("IOThread %d start loop", thread->m_thread_id);
  thread->m_event_loop->loop();
  DEBUGLOG("IOThread %d stop loop", thread->m_thread_id);

  return nullptr;
}

} // namespace rocket
