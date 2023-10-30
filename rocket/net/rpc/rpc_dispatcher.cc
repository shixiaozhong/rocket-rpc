#include "rocket/net/rpc/rpc_dispatcher.h"

#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>

#include "rocket/common/err_code.h"
#include "rocket/common/log.h"
#include "rocket/net/coder/tinypb_protocol.h"
#include "rocket/net/rpc/rpc_controller.h"
#include "rocket/net/tcp/net_addr.h"

namespace rocket {

static RpcDispatcher* g_rpc_dispatcher = nullptr;

RpcDispatcher* RpcDispatcher::GetRpcDispatcherInstance() {
  if (g_rpc_dispatcher != nullptr) {
    return g_rpc_dispatcher;
  }
  g_rpc_dispatcher = new RpcDispatcher();
  return g_rpc_dispatcher;
}

void RpcDispatcher::dispatch(AbstractProtocol::s_ptr request,
                             AbstractProtocol::s_ptr response,
                             TcpConnection* connection) {
  std::shared_ptr<TinyPBProtocol> req_protocol =
      std::dynamic_pointer_cast<TinyPBProtocol>(request);
  std::shared_ptr<TinyPBProtocol> rsp_protocol =
      std::dynamic_pointer_cast<TinyPBProtocol>(response);

  std::string method_full_name = req_protocol->m_method_name;
  std::string service_name{'\0'};
  std::string method_name{'\0'};

  rsp_protocol->m_msg_id = req_protocol->m_msg_id;
  rsp_protocol->m_method_name = req_protocol->m_method_name;

  if (!parseServiceFullName(method_full_name, service_name, method_name)) {
    // 错误处理
    ERRORLOG("msg_id %s | pasre service name error, service name: %s",
             rsp_protocol->m_msg_id.c_str(), service_name.c_str());
    setTinyPBError(rsp_protocol, ERROR_PARSE_SERVICE_NAME,
                   "parse_service_name error");
  }
  auto it = m_service_map.find(service_name);
  if (it == m_service_map.end()) {
    // 错误处理
    ERRORLOG("msg_id %s | service name not found: %s",
             rsp_protocol->m_msg_id.c_str(), service_name.c_str());
    setTinyPBError(rsp_protocol, ERROR_SERVICE_NOT_FOUND,
                   "service not found error");
  }

  service_s_ptr service = it->second;

  const google::protobuf::MethodDescriptor* method =
      service->GetDescriptor()->FindMethodByName(method_name);
  if (method == nullptr) {
    // 错误处理
    ERRORLOG("msg_id %s | method %s not found in service [%s]",
             rsp_protocol->m_msg_id, method_name.c_str(), service_name.c_str());
    setTinyPBError(rsp_protocol, ERROR_METHOD_NOT_FOUND,
                   "method not found error");
  }

  google::protobuf::Message* req_msg =
      service->GetRequestPrototype(method).New();

  // 反序列化， 将pb_data反序列化为req_msg
  if (!req_msg->ParseFromString(req_protocol->m_pb_data)) {
    // 错误处理
    ERRORLOG("msg_id %s | deserialized error", rsp_protocol->m_msg_id.c_str(),
             method_name.c_str(), service_name.c_str());
    setTinyPBError(rsp_protocol, ERROR_FAILED_DESERIALIZE, "deserialize error");
  }

  INFOLOG("msg_id %s, get rpc request [%s]", req_protocol->m_msg_id.c_str(),
          req_msg->ShortDebugString().c_str());

  google::protobuf::Message* rsp_msg =
      service->GetResponsePrototype(method).New();

  RpcController rpcController;
  rpcController.setLocalAddr(connection->getLocalAddr());
  rpcController.setPeerAddr(connection->getPeerAddr());
  rpcController.setMsgId(req_protocol->m_msg_id);

  service->CallMethod(method, &rpcController, req_msg, rsp_msg, nullptr);

  if (rsp_msg->SerializeToString(&rsp_protocol->m_pb_data)) {
    ERRORLOG("msg_id %s | serialize error, origin message [%s]",
             rsp_protocol->m_msg_id.c_str(),
             rsp_msg->ShortDebugString().c_str());
    setTinyPBError(rsp_protocol, ERROR_FAILED_SERIALIZE, "serialize error");
  }

  // 将错误码设置为0
  rsp_protocol->m_err_code = 0;

  INFOLOG("msg_id %s | dispath successfully, request [%s], response [%s]",
          rsp_protocol->m_msg_id.c_str(), req_msg->ShortDebugString().c_str(),
          rsp_msg->ShortDebugString().c_str());
}

bool RpcDispatcher::parseServiceFullName(const std::string& full_name,
                                         std::string& service_name,
                                         std::string& method_name) {
  if (full_name.empty()) {
    ERRORLOG("full name empty");
    return false;
  }
  size_t index = full_name.find_first_of(".");
  if (index == std::string::npos) {
    ERRORLOG("not find . in full name [%s]", full_name.c_str());
    return false;
  }
  service_name = full_name.substr(0, index);
  method_name = full_name.substr(index + 1, full_name.size() - 1 - index);
  INFOLOG("parse service_name [%s] and method_name [%s] from full name [%s]",
          service_name.c_str(), method_name.c_str(), full_name.c_str());
  return true;
}

void RpcDispatcher::registerService(RpcDispatcher::service_s_ptr service) {
  std::string service_name = service->GetDescriptor()->full_name();
  m_service_map[service_name] = service;
}

void RpcDispatcher::setTinyPBError(std::shared_ptr<TinyPBProtocol> msg,
                                   int32_t err_code,
                                   const std::string err_info) {
  msg->m_err_code = err_code;
  msg->m_err_info = err_info;
  msg->m_err_info_len = err_info.size();
}

}  // namespace rocket