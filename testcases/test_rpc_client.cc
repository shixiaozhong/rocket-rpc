#include <arpa/inet.h>
#include <assert.h>
#include <fcntl.h>
#include <google/protobuf/service.h>
#include <netinet/in.h>
#include <pthread.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <memory>
#include <string>

#include "order.pb.h"
#include "rocket/common/config.h"
#include "rocket/common/log.h"
#include "rocket/net/coder/abstract_protocol.h"
#include "rocket/net/coder/string_coder.h"
#include "rocket/net/coder/tinypb_protocol.h"
#include "rocket/net/rpc/rpc_channel.h"
#include "rocket/net/rpc/rpc_closure.h"
#include "rocket/net/rpc/rpc_controller.h"
#include "rocket/net/rpc/rpc_dispatcher.h"
#include "rocket/net/tcp/net_addr.h"
#include "rocket/net/tcp/tcp_client.h"
#include "rocket/net/tcp/tcp_server.h"

void test_tcp_client() {
  rocket::NetAddr::s_ptr addr =
      std::make_shared<rocket::IPNetAddr>("127.0.0.1", 12345);
  rocket::TcpClient client(addr);
  client.connect([addr, &client]() {
    DEBUGLOG("connect to [%s] successfully", addr->toString().c_str());
    auto message = std::make_shared<rocket::TinyPBProtocol>();
    message->m_msg_id = "99998888";
    message->m_pb_data = "test pb data";

    makeOrderRequest request;
    request.set_price(100);
    request.set_goods("apple");
    if (!request.SerializeToString(&(message->m_pb_data))) {
      ERRORLOG("serilize error");
      return;
    }

    message->m_method_name = "Order.makeOrder";

    client.writeMessage(message,
                        [request](rocket::AbstractProtocol::s_ptr msg_ptr) {
                          DEBUGLOG("send message successfully, request[%s]",
                                   request.ShortDebugString().c_str());
                        });

    client.readMessage("99998888", [](rocket::AbstractProtocol::s_ptr msg_ptr) {
      std::shared_ptr<rocket::TinyPBProtocol> message =
          std::dynamic_pointer_cast<rocket::TinyPBProtocol>(msg_ptr);
      DEBUGLOG("req_id [%s], get response [%s]", message->m_msg_id.c_str(),
               message->m_pb_data.c_str());
      makeOrderResponse response;
      if (!response.ParseFromString(message->m_pb_data)) {
        ERRORLOG("deserialize error");
        return;
      }
      DEBUGLOG("get response successfully, response[%s]",
               response.ShortDebugString().c_str());
    });
  });
}

void test_rpc_channel() {
  // rocket::IPNetAddr::s_ptr addr =
  //     std::make_shared<rocket::IPNetAddr>("127.0.0.1", 12345);
  // std::shared_ptr<rocket::RpcChannel> channel =
  //     std::make_shared<rocket::RpcChannel>(addr);

  NEWRPCCHANNEL("127.0.0.1:12345", channel);

  NEWMESSAGE(makeOrderRequest, request);
  NEWMESSAGE(makeOrderResponse, response);

  request->set_price(100);
  request->set_goods("apple");

  // std::shared_ptr<makeOrderResponse> response =
  //     std::make_shared<makeOrderResponse>();

  // std::shared_ptr<rocket::RpcController> controller =
  //     std::make_shared<rocket::RpcController>();

  NEWCONTROLLER(controller);
  controller->setMsgId("99998888");
  controller->setTimeout(10000);

  std::shared_ptr<rocket::RpcClosure> closure =
      std::make_shared<rocket::RpcClosure>([request, response, channel,
                                            controller]() mutable {
        if (controller->getErrorCode() == 0) {
          INFOLOG("call rpc success, request[%s], response[%s]",
                  request->ShortDebugString().c_str(),
                  response->ShortDebugString().c_str());
          // 成功之后，执行业务逻辑
          // ....
        } else {
          ERRORLOG(
              "call rpc failed, request [%s], error code [%d], error info [%s]",
              request->ShortDebugString().c_str(), controller->getErrorCode(),
              controller->getErrorInfo().c_str());
        }

        INFOLOG("now exit event loop");
        channel->getTcpClient()->stop();
        channel.reset();
      });

  // channel->init(controller, request, response, closure);
  // Order_Stub stub(channel.get());
  // stub.makeOrder(controller.get(), request.get(), response.get(),
  //                closure.get());

  CALLRPC("127.0.0.1:12345", makeOrder, controller, request, response, closure);
}

int main() {
  rocket::Config::SetGlobalConfig("../conf/rocket.xml");

  rocket::Logger::InitGlobalLogger();

  // test_tcp_client();
  test_rpc_channel();
  return 0;
}