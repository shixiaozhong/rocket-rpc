#include "rocket/net/rpc/rpc_channel.h"

#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>
#include <google/protobuf/service.h>

#include "rocket/common/err_code.h"
#include "rocket/common/log.h"
#include "rocket/common/msg_util.h"
#include "rocket/net/coder/tinypb_protocol.h"
#include "rocket/net/rpc/rpc_controller.h"
#include "rocket/net/tcp/tcp_client.h"
#include "rocket/net/timer_event.h"

namespace rocket {

RpcChannel::RpcChannel(NetAddr::s_ptr peer_addr) : m_peer_addr(peer_addr) {
  m_client = std::make_shared<TcpClient>(m_peer_addr);
}

RpcChannel::~RpcChannel() { INFOLOG("~RpcChannel()"); }

void RpcChannel::init(controller_s_ptr controller, message_s_ptr req,
                      message_s_ptr rsp, closure_s_ptr done) {
  if (m_is_init) {
    return;
  }
  m_controller = controller;
  m_request = req;
  m_response = rsp;
  m_closure = done;

  m_is_init = true;
}

void RpcChannel::CallMethod(const google::protobuf::MethodDescriptor* method,
                            google::protobuf::RpcController* controller,
                            const google::protobuf::Message* request,
                            google::protobuf::Message* response,
                            google::protobuf::Closure* done) {
  auto req_protocol = std::make_shared<rocket::TinyPBProtocol>();
  RpcController* my_controller = dynamic_cast<RpcController*>(controller);
  if (my_controller == nullptr) {
    ERRORLOG("failed call method, RpcController convert error");
    return;
  }

  if (my_controller->getMsgId().empty()) {
    req_protocol->m_msg_id = MsgUtil::GetMsgID();
  } else {
    req_protocol->m_msg_id = my_controller->getMsgId();
  }
  req_protocol->m_method_name = method->full_name();
  INFOLOG("%s | call method name [%s]", req_protocol->m_msg_id.c_str(),
          req_protocol->m_method_name.c_str());

  if (!m_is_init) {
    std::string err_info{"Rpc Channel not init"};
    my_controller->setErrorCode(ERROR_RPC_CHANNEL_INIT, err_info);
    ERRORLOG("%s | %s, origin request [%s]", req_protocol->m_msg_id.c_str(),
             err_info.c_str(), request->ShortDebugString().c_str());
    return;
  }

  // 序列化request
  if (!request->SerializeToString(&req_protocol->m_pb_data)) {
    std::string err_info{"failed to serialize request"};
    my_controller->setErrorCode(ERROR_FAILED_SERIALIZE, err_info);

    ERRORLOG("%s | %s, origin request [%s]", req_protocol->m_msg_id.c_str(),
             err_info.c_str(), request->ShortDebugString().c_str());
    return;
  }

  // 获取到当前对象的shared_ptr;
  s_ptr channel = shared_from_this();

  // 添加定时任务
  m_timer_event = std::make_shared<TimerEvent>(
      my_controller->getTimeout(), false, [my_controller, channel]() mutable {
        my_controller->StartCancel();
        my_controller->setError(
            ERROR_RPC_CALL_TIMEOUT,
            "rpc call timeout " + std::to_string(my_controller->getTimeout()));
        if (channel->getClosure()) {
          channel->getClosure()->Run();
        }
        channel.reset();
      });
  m_client->addTimerEvent(m_timer_event);

  m_client->connect([req_protocol, channel]() mutable {
    RpcController* my_controller =
        dynamic_cast<RpcController*>(channel->getController());

    if (channel->getTcpClient()->getConnectErrCode() != 0) {
      my_controller->setError(channel->getTcpClient()->getConnectErrCode(),
                              channel->getTcpClient()->getConnectErrInfo());
      ERRORLOG(
          "%s | connect error, error code [%d], error info [%s], peer adderss "
          "[%s]",
          req_protocol->m_msg_id.c_str(), my_controller->getErrorCode(),
          my_controller->getErrorInfo().c_str(),
          channel->getTcpClient()->getPeerAddr()->toString().c_str());
      return;
    }

    channel->getTcpClient()->writeMessage(req_protocol, [req_protocol, channel,
                                                         my_controller](
                                                            AbstractProtocol::
                                                                s_ptr) mutable {
      INFOLOG(
          "%s | send request success, call method name [%s], peer address "
          "[%s], local address [%s]",
          req_protocol->m_msg_id.c_str(), req_protocol->m_method_name.c_str(),
          channel->getTcpClient()->getPeerAddr()->toString().c_str(),
          channel->getTcpClient()->getLocalAddr()->toString().c_str());

      channel->getTcpClient()->readMessage(
          req_protocol->m_msg_id,
          [channel](AbstractProtocol::s_ptr msg) mutable {
            auto rsp_protocol =
                std::dynamic_pointer_cast<rocket::TinyPBProtocol>(msg);
            INFOLOG(
                "%s | success get rpc response, call method name [%s], "
                "peer addr [%s], local addr [%s]",
                rsp_protocol->m_msg_id.c_str(),
                rsp_protocol->m_method_name.c_str(),
                channel->getTcpClient()->getPeerAddr()->toString().c_str(),
                channel->getTcpClient()->getLocalAddr()->toString().c_str());

            // 当成功读取到回包后， 取消定时任务
            channel->getTimerEvent()->setCancel(true);

            RpcController* my_controller =
                dynamic_cast<RpcController*>(channel->getController());

            if (!(channel->getResponse()->ParseFromString(
                    rsp_protocol->m_pb_data))) {
              ERRORLOG("%s | serialize error", rsp_protocol->m_msg_id.c_str());
              my_controller->setErrorCode(ERROR_FAILED_SERIALIZE,
                                          "serialize error");
              return;
            }

            if (rsp_protocol->m_err_code != 0) {
              ERRORLOG("%s | call rpc failed, error code[%d]. error info [%s]",
                       rsp_protocol->m_msg_id.c_str(), rsp_protocol->m_err_code,
                       rsp_protocol->m_err_info.c_str());
              my_controller->setErrorCode(rsp_protocol->m_err_code,
                                          rsp_protocol->m_err_info);
              return;
            }

            INFOLOG(
                "%s | call rpc success, call method name [%s], peer "
                "addr [%s], local addr [%s]",
                rsp_protocol->m_msg_id.c_str(),
                rsp_protocol->m_method_name.c_str(),
                channel->getTcpClient()->getPeerAddr()->toString().c_str(),
                channel->getTcpClient()->getLocalAddr()->toString().c_str());

            if (!my_controller->IsCanceled() && channel->getClosure()) {
              channel->getClosure()->Run();
            }
            channel.reset();
          });
    });
  });
}

google::protobuf::RpcController* RpcChannel::getController() const {
  return m_controller.get();
}
google::protobuf::Message* RpcChannel::getRequest() const {
  return m_request.get();
}
google::protobuf::Message* RpcChannel::getResponse() const {
  return m_response.get();
}
google::protobuf::Closure* RpcChannel::getClosure() const {
  return m_closure.get();
}

TcpClient::s_ptr RpcChannel::getTcpClient() const { return m_client; }

TimerEvent::s_ptr RpcChannel::getTimerEvent() const { return m_timer_event; }

}  // namespace rocket