#include "rocket/net/tcp/net_addr.h"
#include "rocket/common/log.h"
#include <cstring>

namespace rocket {
IPNetAddr::IPNetAddr(const std::string &ip, uint16_t port)
    : m_ip(ip), m_port(port) {
  memset(&m_addr, 0, sizeof(m_addr));
  m_addr.sin_family = AF_INET;
  m_addr.sin_addr.s_addr = inet_addr(m_ip.c_str());
  m_addr.sin_port = htons(port);
}

IPNetAddr::IPNetAddr(const std::string &addr) {
  // 0.0.0.0:12345
  auto index = addr.find_first_of(":");
  if (index == std::string::npos) {
    ERRORLOG("invalid ipv4 addr %s", addr.c_str());
    return;
  }
  m_ip = addr.substr(0, index);
  m_port = std::atoi(addr.substr(index + 1, addr.size() - index - 1).c_str());

  memset(&m_addr, 0, sizeof(m_addr));
  m_addr.sin_family = AF_INET;
  m_addr.sin_addr.s_addr = inet_addr(m_ip.c_str());
  m_addr.sin_port = htons(m_port);
}

IPNetAddr::IPNetAddr(sockaddr_in addr) : m_addr(addr) {
  m_ip = inet_ntoa(m_addr.sin_addr);
  m_port = ntohs(m_addr.sin_port);
}

IPNetAddr::~IPNetAddr() {}

sockaddr *IPNetAddr::getSockAddr() {
  return reinterpret_cast<sockaddr *>(&m_addr);
}

socklen_t IPNetAddr::getSocklen() { return sizeof(m_addr); }

int IPNetAddr::getFamily() { return AF_INET; }

std::string IPNetAddr::toString() {
  std::string str;
  str = m_ip + ":" + std::to_string(m_port);
  return str;
}

bool IPNetAddr::checkValid() {
  if (m_ip.empty() || m_port == 0) {
    return false;
  }
  if (m_port < 0 || m_port > UINT16_MAX) {
    return false;
  }
  if (inet_addr(m_ip.c_str()) == INADDR_NONE) {
    return false;
  }
  return true;
}
} // namespace rocket