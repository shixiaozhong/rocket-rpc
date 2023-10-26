#ifndef ROKCET_NET_ABSTRACT_PROTOCOL_H
#define ROKCET_NET_ABSTRACT_PROTOCOL_H

#include "rocket/net/tcp/tcp_buffer.h"
#include <memory>
#include <string>
#include <vector>

namespace rocket {

struct AbstractProtocol
    : public std::enable_shared_from_this<AbstractProtocol> {
public:
  using s_ptr = std::shared_ptr<AbstractProtocol>;

  virtual ~AbstractProtocol() {}

public:
  std::string m_req_id; // request id，唯一标识一个请求或者响应
};

} // namespace rocket

#endif