#include <arpa/inet.h>
#include <assert.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <functional>
#include <memory>
#include <string>

#include "order.pb.h"
#include "rocket/common/config.h"
#include "rocket/common/log.h"
#include "rocket/net/coder/string_coder.h"
#include "rocket/net/coder/tinypb_protocol.h"
#include "rocket/net/tcp/tcp_client.h"

void test_tcp_client() {
  rocket::NetAddr::s_ptr addr =
      std::make_shared<rocket::IPNetAddr>("127.0.0.1", 12345);
  rocket::TcpClient client(addr);
  client.connect([addr, &client]() {
    DEBUGLOG("connect to [%s] successfully", addr->toString().c_str());
    auto message = std::make_shared<rocket::TinyPBProtocol>();
    message->m_req_id = "99998888";
    message->m_pb_data = "test pb data";

    makeOrderRquest request;
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
      DEBUGLOG("req_id [%s], get response [%s]", message->m_req_id.c_str(),
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

int main() {
  rocket::Config::SetGlobalConfig("../conf/rocket.xml");

  rocket::Logger::InitGlobalLogger();

  test_tcp_client();
  return 0;
}