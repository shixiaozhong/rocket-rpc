#ifndef ROCKET_NET_RPC_RPC_CONTROLLER_H
#define ROCKET_NET_RPC_RPC_CONTROLLER_H

#include <google/protobuf/service.h>
#include <google/protobuf/stubs/callback.h>

#include "rocket/net/tcp/net_addr.h"

namespace rocket {
class RpcController : public google::protobuf::RpcController {
 public:
  RpcController(){};
  ~RpcController(){};

  void Reset();

  bool Failed() const;

  std::string ErrorText() const;

  void StartCancel();

  void SetFailed(const std::string& reason);

  bool IsCanceled() const;

  void NotifyOnCancel(google::protobuf::Closure* callback);

  // error code and error info
  void setErrorCode(const int32_t err_code, const std::string err_info);
  int32_t getErrorCode() const;
  std::string getErrorInfo() const;

  // reqId
  void setMsgId(const std::string& msg_id);
  std::string getMsgId() const;

  void setError(int32_t error_code, const std::string error_info);

  // timeout
  void setTimeout(const int32_t timeout);
  int32_t getTimeout() const;

  // server address
  void setLocalAddr(NetAddr::s_ptr addr);
  NetAddr::s_ptr getLocalAddr() const;

  // client address
  void setPeerAddr(NetAddr::s_ptr addr);
  NetAddr::s_ptr getPeerAddr() const;

 private:
  int m_error_code{0};

  std::string m_error_info;
  std::string m_msg_id;

  bool m_is_failed{false};
  bool m_is_canceled{false};
  NetAddr::s_ptr m_local_addr;
  NetAddr::s_ptr m_peer_addr;

  int m_timeout{1000};  // ms
};

}  // namespace rocket

#endif