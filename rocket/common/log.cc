#include "rocket/common/log.h"

#include <sys/time.h>

#include <cassert>
#include <cstdio>
#include <sstream>

#include "rocket/common/config.h"
#include "rocket/common/runtime.h"
#include "rocket/common/util.h"
#include "rocket/net/eventloop.h"

namespace rocket {

static Logger *g_logger = NULL;

Logger::Logger(LogLevel level) : m_set_level(level) {
  m_async_logger = std::make_shared<AsyncLogger>(
      Config::GetGlobalConfig()->m_log_file_name + "_rpc",
      Config::GetGlobalConfig()->m_log_file_path,
      Config::GetGlobalConfig()->m_log_max_file_size);
  m_async_app_logger = std::make_shared<AsyncLogger>(
      Config::GetGlobalConfig()->m_log_file_name + "_app",
      Config::GetGlobalConfig()->m_log_file_path,
      Config::GetGlobalConfig()->m_log_max_file_size);
}

void Logger::syncLoop() {
  // 同步m_buffer到async_logger的buffer队尾
  std::vector<std::string> tmp_vec;
  ScopeMutex<Mutex> lock(m_mutex);
  tmp_vec.swap(m_buffer);
  lock.unlock();

  if (!tmp_vec.empty()) {
    m_async_logger->pushLogBuffer(tmp_vec);
  }
  tmp_vec.clear();

  // 同步m_app_buffer到async_logger的buffer队尾
  std::vector<std::string> tmp_vec2;
  ScopeMutex<Mutex> lock2(m_app_mutex);
  tmp_vec2.swap(m_app_buffer);
  lock2.unlock();

  if (!tmp_vec2.empty()) {
    m_async_app_logger->pushLogBuffer(tmp_vec2);
  }
  tmp_vec2.clear();
}

void Logger::init() {
  m_timer_event = std::make_shared<TimerEvent>(
      Config::GetGlobalConfig()->m_log_sync_inteval, true,
      std::bind(&Logger::syncLoop, this));

  EventLoop::GetCurrentEventLoop()->addTimerEvent(m_timer_event);
}

Logger *Logger::GetGlobalLogger() { return g_logger; }

void Logger::InitGlobalLogger() {
  LogLevel global_log_level =
      StringToLogLevel(Config::GetGlobalConfig()->m_log_level);
  printf("Init log level [%s]\n", LogLevelToString(global_log_level).c_str());
  g_logger = new Logger(global_log_level);

  g_logger->init();
}

std::string LogLevelToString(LogLevel level) {
  switch (level) {
    case Debug:
      return "DEBUG";

    case Info:
      return "INFO";

    case Error:
      return "ERROR";
    default:
      return "UNKNOWN";
  }
}

LogLevel StringToLogLevel(const std::string &log_level) {
  if (log_level == "DEBUG") {
    return Debug;
  } else if (log_level == "INFO") {
    return Info;
  } else if (log_level == "ERROR") {
    return Error;
  } else {
    return Unknown;
  }
}

std::string LogEvent::toString() {
  struct timeval now_time;

  gettimeofday(&now_time, nullptr);

  struct tm now_time_t;
  localtime_r(&(now_time.tv_sec), &now_time_t);

  char buf[128];
  strftime(&buf[0], 128, "%y-%m-%d %H:%M:%S", &now_time_t);
  std::string time_str(buf);
  int ms = now_time.tv_usec / 1000;
  time_str = time_str + "." + std::to_string(ms);

  m_pid = getPid();
  m_thread_id = getThreadId();

  std::stringstream ss;

  ss << "[" << LogLevelToString(m_level) << "]\t"
     << "[" << time_str << "]\t"
     << "[" << m_pid << ":" << m_thread_id << "]\t";

  // 获取当前线程处理请求的msg_id
  std::string msg_id = RunTime::GetRunTime()->m_msg_id;
  std::string method_name = RunTime::GetRunTime()->m_method_name;

  if (!msg_id.empty()) {
    ss << "[" << msg_id << "]";
  }

  if (!method_name.empty()) {
    ss << "[" << method_name << "]";
  }

  return ss.str();
}

void Logger::pushLog(const std::string &msg) {
  ScopeMutex<Mutex> lock(m_mutex);
  m_buffer.push_back(msg);
  lock.unlock();
}

void Logger::pushAppLog(const std::string &msg) {
  ScopeMutex<Mutex> lock(m_app_mutex);
  m_app_buffer.push_back(msg);
  lock.unlock();
}

void Logger::log() {}

AsyncLogger::AsyncLogger(const std::string &file_name,
                         const std::string &file_path, int max_size)
    : m_file_name(file_name),
      m_file_path(file_path),
      m_max_file_size(max_size) {
  // 初始化信号量,创建线程和初始化条件变量
  sem_init(&m_semphore, 0, 0);
  assert(pthread_create(&m_pthread, nullptr, &AsyncLogger::Loop, this) == 0);
  assert(pthread_cond_init(&m_condition_variable, nullptr) == 0);

  sem_wait(&m_semphore);
}

void *AsyncLogger::Loop(void *arg) {
  // 将buffer中的全部数据都打印到文件中，线程睡眠，直到有新的数据到来，重复这个过程

  AsyncLogger *logger = reinterpret_cast<AsyncLogger *>(arg);
  sem_post(&logger->m_semphore);

  while (1) {
    ScopeMutex<Mutex> lock(logger->m_mutex);
    while (logger->m_buffer.empty()) {
      pthread_cond_wait(&(logger->m_condition_variable),
                        logger->m_mutex.getMutex());
    }

    std::vector<std::string> tmp;
    tmp.swap(logger->m_buffer.front());
    logger->m_buffer.pop();

    lock.unlock();

    timeval now;
    gettimeofday(&now, nullptr);

    struct tm now_time;
    localtime_r(&now.tv_sec, &now_time);

    const char *format = "%Y%m%d";
    char date[32];
    strftime(date, sizeof(date), format, &now_time);

    if (std::string(date) != logger->m_date) {
      logger->m_log_num = 0;
      logger->m_reopen_flag = true;
      logger->m_date = std::string(date);
    }
    if (logger->m_file_handler == nullptr) {
      logger->m_reopen_flag = true;
    }
    std::stringstream ss;
    ss << logger->m_file_path << logger->m_file_name << "_" << std::string(date)
       << "_log.";
    std::string log_file_name = ss.str() + std::to_string(logger->m_log_num);

    if (logger->m_reopen_flag) {
      if (logger->m_file_handler) {
        fclose(logger->m_file_handler);
      }
      logger->m_file_handler = fopen(log_file_name.c_str(), "a");
      logger->m_reopen_flag = false;
    }

    // 判断文件大小是否超出最大值
    if (ftell(logger->m_file_handler) > logger->m_max_file_size) {
      fclose(logger->m_file_handler);
      log_file_name = ss.str() + std::to_string(++logger->m_log_num);

      logger->m_file_handler = fopen(log_file_name.c_str(), "a");
      logger->m_reopen_flag = false;
    }

    for (auto &e : tmp) {
      fwrite(e.c_str(), 1, e.size(), logger->m_file_handler);
    }

    // 刷新到磁盘
    fflush(logger->m_file_handler);

    if (logger->m_stop_flag) {
      return nullptr;
    }
  }
  return nullptr;
}

void AsyncLogger::stop() { m_stop_flag = true; }

void AsyncLogger::flush() {
  if (m_file_handler) {
    fflush(m_file_handler);
  }
}

void AsyncLogger::pushLogBuffer(std::vector<std::string> &vec) {
  ScopeMutex<Mutex> lock(m_mutex);
  m_buffer.push(vec);
  lock.unlock();

  // 此时唤醒异步日志线程
  pthread_cond_signal(&m_condition_variable);
}

}  // namespace rocket