#include "rocket/net/rpc/rpc_controller.h"

namespace rocket {

void RpcController::Reset() {
  m_error_code = 0;
  m_error_info = "";
  m_msg_id = "";
  m_is_failed = false;
  m_is_canceled = false;
  m_local_addr = nullptr;
  m_peer_addr = nullptr;
  m_timeout = 1000;  // ms
}

bool RpcController::Failed() const { return m_is_failed; }

std::string RpcController::ErrorText() const { return m_error_info; }

void RpcController::StartCancel() { m_is_canceled = true; }

void RpcController::SetFailed(const std::string& reason) {
  m_error_info = reason;
}

bool RpcController::IsCanceled() const { return m_is_canceled; }

void RpcController::NotifyOnCancel(google::protobuf::Closure* callback) {}

// error code and error info
void RpcController::setErrorCode(const int32_t err_code,
                                 const std::string err_info) {
  m_error_code = err_code;
  m_error_info = err_info;
}

int32_t RpcController::getErrorCode() const { return m_error_code; }
std::string RpcController::getErrorInfo() const { return m_error_info; }

void RpcController::setError(int32_t error_code, const std::string error_info) {
  m_error_code = error_code;
  m_error_info = error_info;
  m_is_failed = true;
}

// reqId
void RpcController::setMsgId(const std::string& msg_id) { m_msg_id = msg_id; }
std::string RpcController::getMsgId() const { return m_msg_id; }

// timeout
void RpcController::setTimeout(const int32_t timeout) { m_timeout = timeout; }
int32_t RpcController::getTimeout() const { return m_timeout; }

// server address
void RpcController::setLocalAddr(NetAddr::s_ptr addr) { m_local_addr = addr; }
NetAddr::s_ptr RpcController::getLocalAddr() const { return m_local_addr; }

// client address
void RpcController::setPeerAddr(NetAddr::s_ptr addr) { m_peer_addr = addr; }
NetAddr::s_ptr RpcController::getPeerAddr() const { return m_peer_addr; }

}  // namespace rocket