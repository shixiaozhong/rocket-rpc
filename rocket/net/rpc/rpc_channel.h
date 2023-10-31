#ifndef ROCKET_NET_RPC_RPC_CHANNEL_H
#define ROCKET_NET_RPC_RPC_CHANNEL_H

#include <google/protobuf/service.h>

#include <memory>

#include "rocket/net/tcp/net_addr.h"
#include "rocket/net/tcp/tcp_client.h"

namespace rocket {

#define NEWMESSAGE(type, var_name) \
  std::shared_ptr<type> var_name = std::make_shared<type>();

#define NEWCONTROLLER(var_name)                     \
  std::shared_ptr<rocket::RpcController> var_name = \
      std::make_shared<rocket::RpcController>();

#define NEWRPCCHANNEL(addr, var_name)            \
  std::shared_ptr<rocket::RpcChannel> var_name = \
      std::make_shared<rocket::RpcChannel>(      \
          std::make_shared<rocket::IPNetAddr>(addr));

#define CALLRPC(addr, method_name, controller, request, response, closure) \
  \ 
 {                                                                         \
    NEWRPCCHANNEL(addr, channel)                                           \
    channel->init(controller, request, response, closure);                 \
    Order_Stub(channel.get())                                              \
        .method_name(controller.get(), request.get(), response.get(),      \
                     closure.get());                                       \
  }

class RpcChannel : public google::protobuf::RpcChannel,
                   public std::enable_shared_from_this<RpcChannel> {
 public:
  using s_ptr = std::shared_ptr<RpcChannel>;
  using controller_s_ptr = std::shared_ptr<google::protobuf::RpcController>;
  using message_s_ptr = std::shared_ptr<google::protobuf::Message>;
  using closure_s_ptr = std::shared_ptr<google::protobuf::Closure>;

 public:
  RpcChannel(NetAddr::s_ptr peer_addr);

  ~RpcChannel();

  void init(controller_s_ptr controller, message_s_ptr req, message_s_ptr rsp,
            closure_s_ptr done);

  void CallMethod(const google::protobuf::MethodDescriptor* method,
                  google::protobuf::RpcController* controller,
                  const google::protobuf::Message* request,
                  google::protobuf::Message* response,
                  google::protobuf::Closure* done);

  google::protobuf::RpcController* getController() const;
  google::protobuf::Message* getRequest() const;
  google::protobuf::Message* getResponse() const;
  google::protobuf::Closure* getClosure() const;

  TcpClient::s_ptr getTcpClient() const;

  TimerEvent::s_ptr getTimerEvent() const;

 private:
  NetAddr::s_ptr m_local_addr{nullptr};
  NetAddr::s_ptr m_peer_addr{nullptr};

  controller_s_ptr m_controller{nullptr};
  message_s_ptr m_request{nullptr};
  message_s_ptr m_response{nullptr};
  closure_s_ptr m_closure{nullptr};

  bool m_is_init{false};

  TcpClient::s_ptr m_client{nullptr};

  TimerEvent::s_ptr m_timer_event{nullptr};
};

}  // namespace rocket

#endif