#ifndef ROCKET_NET_TIME_EVENT_H
#define ROCKET_NET_TIME_EVENT_H
#include <functional>
#include <memory>

namespace rocket {
class TimerEvent {
public:
  using s_ptr = std::shared_ptr<TimerEvent>;

  TimerEvent(int64_t interval, bool is_repeated, std::function<void()> cb);

  int64_t getArriveTime() const { return m_arrived_time; }

  void setCancel(bool value) { m_is_canceled = value; }

  bool isCancled() const { return m_is_canceled; }

  bool isRepeated() const { return m_is_repeated; }

  std::function<void()> getCallBack() const { return m_task; }

  void resetArriveTime();

private:
  int64_t m_arrived_time; // ms,到达时间
  int64_t m_interval;     // ms,间隔
  bool m_is_repeated{false};
  bool m_is_canceled{false};
  std::function<void()> m_task;
};

}; // namespace rocket

#endif