#include "rocket/net/coder/tinypb_protocol.h"

namespace rocket {

// 静态成员类外初始化
char TinyPBProtocol::PB_START = 0x02;
char TinyPBProtocol::PB_END = 0x03;

} // namespace rocket