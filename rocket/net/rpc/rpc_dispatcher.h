#ifndef ROCKET_NET_RPC_RPC_DISPATCHER_H
#define ROCKET_NET_RPC_RPC_DISPATCHER_H

#include <google/protobuf/service.h>

#include <map>
#include <memory>
#include <string>

#include "rocket/net/coder/abstract_protocol.h"
#include "rocket/net/tcp/tcp_connection.h"

namespace rocket {
// 前置声明，头文件循环引用
class TcpConnection;

class RpcDispatcher {
 public:
  static RpcDispatcher* GetRpcDispatcherInstance();

  using service_s_ptr = std::shared_ptr<google::protobuf::Service>;

  void dispatch(AbstractProtocol::s_ptr request,
                AbstractProtocol::s_ptr response, TcpConnection* connection);

  void registerService(RpcDispatcher::service_s_ptr service);

 private:
  bool parseServiceFullName(const std::string& full_name,
                            std::string& service_name,
                            std::string& method_name);

  void setTinyPBError(std::shared_ptr<TinyPBProtocol> msg, int32_t err_code,
                      const std::string error_info);

 private:
  std::map<std::string, std::shared_ptr<google::protobuf::Service>>
      m_service_map;
};

}  // namespace rocket
#endif