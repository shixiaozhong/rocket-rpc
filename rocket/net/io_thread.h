#ifndef ROCKET_NET_IO_THREAD_H
#define ROCKET_NET_IO_THREAD_H
#include "rocket/net/eventloop.h"
#include <pthread.h>
#include <semaphore.h>

namespace rocket {
class IOThread {
public:
  IOThread();

  ~IOThread();

  EventLoop *getEventLoop() const { return m_event_loop; }

  void start();

  void join();

public:
  static void *Main(void *arg);

private:
  pid_t m_thread_id{-1};            // 线程id
  pthread_t m_thread{0};            // 线程句柄
  EventLoop *m_event_loop{nullptr}; // 当前io线程的eventloop
  sem_t m_init_semaphore;           // 用于保证初始化eventloop
  sem_t m_start_semaphore;          // 用于手动控制IO线程中的eventloop
};

} // namespace rocket

#endif