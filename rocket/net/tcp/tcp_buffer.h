#ifndef ROCKET_NET_TCP_TCP_BUFFER_H
#define ROCKET_NET_TCP_TCP_BUFFER_H

#include <memory>
#include <vector>

namespace rocket {
class TcpBuffer {
public:
  using s_ptr = std::shared_ptr<TcpBuffer>;
  TcpBuffer(int size);

  ~TcpBuffer();

  // 返回可读字节数
  int readAble();

  // 返回可写字节数
  int writeAble();

  int readIndex();

  int writeIndex();

  void write2Buffer(const char *str, int size);

  void readFromBuffer(std::vector<char> &ret, int size);

  void resizeBuffer(int new_size);

  void adjustBuffer();

  void moveReadIndex(int offset);

  void moveWriteIndex(int offset);

private:
  int m_read_index{0};
  int m_write_index{0};
  int m_size{0};

public:
  std::vector<char> m_buffer;
};

} // namespace rocket

#endif