#ifndef ROCKET_NET_STRING_CODER_H
#define ROCKET_NET_STRING_CODER_H

#include <string>
#include <vector>

#include "rocket/net/coder/abstract_coder.h"

namespace rocket {

class StringProtocol : public AbstractProtocol {
 public:
  std::string info;
};

class StringCoder : public AbstractCoder {
 public:
  void encode(std::vector<AbstractProtocol::s_ptr> &messages,
              TcpBuffer::s_ptr out_buffer) {
    for (auto &e : messages) {
      auto msg = std::dynamic_pointer_cast<StringProtocol>(e);
      out_buffer->write2Buffer(msg->info.c_str(), msg->info.size());
    }
  }

  void decode(std::vector<AbstractProtocol::s_ptr> &out_messages,
              TcpBuffer::s_ptr buffer) {
    std::vector<char> re;
    buffer->readFromBuffer(re, buffer->readAble());
    std::string info(re.begin(), re.end());

    std::shared_ptr<StringProtocol> msg = std::make_shared<StringProtocol>();
    msg->info = info;
    msg->m_msg_id = "12345";
    out_messages.push_back(msg);
  }

  ~StringCoder(){};
};

}  // namespace rocket

#endif