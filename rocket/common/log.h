#ifndef ROCKET_COMMON_LOG_H
#define ROCKET_COMMON_LOG_H

#include <semaphore.h>

#include <memory>
#include <queue>
#include <string>

#include "rocket/common/config.h"
#include "rocket/common/mutex.h"
#include "rocket/net/timer_event.h"

namespace rocket {

template <typename... Args>
std::string formatString(const char *str, Args &&... args) {
  int size = snprintf(nullptr, 0, str, args...);

  std::string result;
  if (size > 0) {
    result.resize(size);
    snprintf(&result[0], size + 1, str, args...);
  }

  return result;
}

#define DEBUGLOG(str, ...)                                                 \
  if (rocket::Logger::GetGlobalLogger()->getLogLevel() <= rocket::Debug) { \
    rocket::Logger::GetGlobalLogger()->pushLog(                            \
        rocket::LogEvent(rocket::LogLevel::Debug).toString() + "[" +       \
        std::string(__FILE__) + ":" + std::to_string(__LINE__) + "]\t" +   \
        rocket::formatString(str, ##__VA_ARGS__) + "\n");                  \
  }

#define INFOLOG(str, ...)                                                 \
  if (rocket::Logger::GetGlobalLogger()->getLogLevel() <= rocket::Info) { \
    rocket::Logger::GetGlobalLogger()->pushLog(                           \
        rocket::LogEvent(rocket::LogLevel::Info).toString() + "[" +       \
        std::string(__FILE__) + ":" + std::to_string(__LINE__) + "]\t" +  \
        rocket::formatString(str, ##__VA_ARGS__) + "\n");                 \
  }

#define ERRORLOG(str, ...)                                                 \
  if (rocket::Logger::GetGlobalLogger()->getLogLevel() <= rocket::Error) { \
    rocket::Logger::GetGlobalLogger()->pushLog(                            \
        rocket::LogEvent(rocket::LogLevel::Error).toString() + "[" +       \
        std::string(__FILE__) + ":" + std::to_string(__LINE__) + "]\t" +   \
        rocket::formatString(str, ##__VA_ARGS__) + "\n");                  \
  }

#define APPDEBUGLOG(str, ...)                                              \
  if (rocket::Logger::GetGlobalLogger()->getLogLevel() <= rocket::Debug) { \
    rocket::Logger::GetGlobalLogger()->pushAppLog(                         \
        rocket::LogEvent(rocket::LogLevel::Debug).toString() + "[" +       \
        std::string(__FILE__) + ":" + std::to_string(__LINE__) + "]\t" +   \
        rocket::formatString(str, ##__VA_ARGS__) + "\n");                  \
  }

#define APPINFOLOG(str, ...)                                              \
  if (rocket::Logger::GetGlobalLogger()->getLogLevel() <= rocket::Info) { \
    rocket::Logger::GetGlobalLogger()->pushAPPLog(                        \
        rocket::LogEvent(rocket::LogLevel::Info).toString() + "[" +       \
        std::string(__FILE__) + ":" + std::to_string(__LINE__) + "]\t" +  \
        rocket::formatString(str, ##__VA_ARGS__) + "\n");                 \
  }

#define APPERRORLOG(str, ...)                                              \
  if (rocket::Logger::GetGlobalLogger()->getLogLevel() <= rocket::Error) { \
    rocket::Logger::GetGlobalLogger()->pushAPPLog(                         \
        rocket::LogEvent(rocket::LogLevel::Error).toString() + "[" +       \
        std::string(__FILE__) + ":" + std::to_string(__LINE__) + "]\t" +   \
        rocket::formatString(str, ##__VA_ARGS__) + "\n");                  \
  }

class AsyncLogger {
 public:
  using s_ptr = std::shared_ptr<AsyncLogger>;

  AsyncLogger(const std::string &file_name, const std::string &file_path,
              int max_size);

  void stop();

  // 刷盘
  void flush();

  void pushLogBuffer(std::vector<std::string> &vec);

 public:
  static void *Loop(void *);

 private:
  std::queue<std::vector<std::string>> m_buffer;

  // 日志文件格式 file_path/file_name_yymmdd.1, file_path/file_name_yymmdd.2

  std::string m_file_name;  // 日志输出文件名称
  std::string m_file_path;  // 日志输出文件路径
  int m_max_file_size{0};   // 日志单个文件最大大小

  sem_t m_semphore;     // 信号量，用于通知线程打印数据到文件
  pthread_t m_pthread;  // 线程句柄
  pthread_cond_t m_condition_variable;  // 条件变量
  Mutex m_mutex;

  std::string m_date;             // 上次打印日志的文件日期
  FILE *m_file_handler{nullptr};  // 当前打开的日志文件句柄
  bool m_reopen_flag{false};      // 是否需要重新打开文件的标志
  int m_log_num{0};               // 日志文件序号

  bool m_stop_flag{false};
};

enum LogLevel { Unknown = 0, Debug = 1, Info = 2, Error = 3 };

std::string LogLevelToString(LogLevel level);

LogLevel StringToLogLevel(const std::string &log_level);

class Logger {
 public:
  typedef std::shared_ptr<Logger> s_ptr;

  Logger(LogLevel level);

  void pushLog(const std::string &msg);

  void pushAppLog(const std::string &msg);

  void log();

  LogLevel getLogLevel() const { return m_set_level; }

  void syncLoop();

  void init();

 public:
  static Logger *GetGlobalLogger();

  static void InitGlobalLogger();

 private:
  LogLevel m_set_level;
  std::vector<std::string> m_buffer;
  std::vector<std::string> m_app_buffer;

  Mutex m_mutex;

  Mutex m_app_mutex;

  // 日志文件格式 file_path/file_name_yymmdd.1, file_path/file_name_yymmdd.2
  std::string m_file_name;  // 日志输出文件名称
  std::string m_file_path;  // 日志输出文件路径
  int m_max_file_size{0};   // 日志单个文件最大大小

  AsyncLogger::s_ptr m_async_logger;

  AsyncLogger::s_ptr m_async_app_logger;

  TimerEvent::s_ptr m_timer_event;
};

class LogEvent {
 public:
  LogEvent(LogLevel level) : m_level(level) {}

  std::string getFileName() const { return m_file_name; }

  LogLevel getLogLevel() const { return m_level; }

  std::string toString();

 private:
  std::string m_file_name;  // 文件名
  int32_t m_file_line;      // 行号
  int32_t m_pid;            // 进程号
  int32_t m_thread_id;      // 线程号

  LogLevel m_level;  //日志级别
};

}  // namespace rocket

#endif