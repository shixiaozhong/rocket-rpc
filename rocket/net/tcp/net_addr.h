#ifndef ROCKET_NET_TCP_NET_ADDR_H
#define ROCKET_NET_TCP_NET_ADDR_H
#include <arpa/inet.h>
#include <memory>
#include <netinet/in.h>
#include <string>

namespace rocket {
class NetAddr {
public:
  using s_ptr = std::shared_ptr<NetAddr>;

  virtual sockaddr *getSockAddr() = 0;

  virtual socklen_t getSocklen() = 0;

  virtual int getFamily() = 0;

  virtual std::string toString() = 0;

  virtual bool checkValid() = 0;

private:
};

class IPNetAddr : public NetAddr {
public:
  IPNetAddr(const std::string &ip, uint16_t port);

  IPNetAddr(const std::string &addr);

  IPNetAddr(sockaddr_in addr);

  ~IPNetAddr();

  sockaddr *getSockAddr();
  socklen_t getSocklen();
  int getFamily();
  std::string toString();
  bool checkValid();

private:
  std::string m_ip;
  uint16_t m_port{0};
  sockaddr_in m_addr;
};

} // namespace rocket

#endif