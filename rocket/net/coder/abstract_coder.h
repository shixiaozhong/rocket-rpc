#ifndef ROCKET_NET_ABSTRACT_CODER_H
#define ROCKET_NET_ABSTRACT_CODER_H

#include "rocket/net/coder/abstract_protocol.h"
#include "rocket/net/tcp/tcp_buffer.h"

#include <string>
#include <vector>

namespace rocket {
class AbstractCoder {
public:
  // 将message对象转换为字节流，写入到buffer
  virtual void encode(std::vector<AbstractProtocol::s_ptr> &messages,
                      TcpBuffer::s_ptr out_buffer) = 0;

  // 将buffer中的字节流转换为message对象
  virtual void decode(std::vector<AbstractProtocol::s_ptr> &out_messages,
                      TcpBuffer::s_ptr in_buffer) = 0;
  virtual ~AbstractCoder(){};

private:
};
} // namespace rocket

#endif