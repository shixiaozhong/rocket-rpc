#include "rocket/common/util.h"
#include <arpa/inet.h>
#include <cstring>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

namespace rocket {

static int g_pid = 0;

static thread_local int g_thread_id = 0;

pid_t getPid() {
  if (g_pid != 0) {
    return g_pid;
  }
  return getpid();
}

pid_t getThreadId() {
  if (g_thread_id != 0) {
    return g_thread_id;
  }
  return syscall(SYS_gettid);
}

int64_t getNowMs() {
  timeval now_time;
  gettimeofday(&now_time, nullptr);
  return now_time.tv_sec * 1000 + now_time.tv_usec / 1000;
}

int32_t getInt32FromNetByte(const char *buf) {
  int32_t result;
  memcpy(&result, buf, sizeof(result));
  return ntohl(result);
}

} // namespace rocket