#ifndef ROCKET_NET_CODER_TINYPB_CODER_H
#define ROKET_NET_CODER_TINYPB_CODER_H

#include "rocket/net/coder/abstract_coder.h"
#include "rocket/net/coder/tinypb_protocol.h"

namespace rocket {
class TinyPBCoder : public AbstractCoder {
public:
  // 将message对象转换为字节流，写入到buffer
  void encode(std::vector<AbstractProtocol::s_ptr> &messages,
              TcpBuffer::s_ptr out_buffer);

  // 将buffer中的字节流转换为message对象
  void decode(std::vector<AbstractProtocol::s_ptr> &out_messages,
              TcpBuffer::s_ptr buffer);
  TinyPBCoder() {}
  ~TinyPBCoder() {}

private:
  const char *ecncodeTinyPb(std::shared_ptr<TinyPBProtocol> message, int &len);
};

} // namespace rocket
#endif