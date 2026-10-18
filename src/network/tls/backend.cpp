// backend.cpp —— TLS 后端接口的虚表锚点（基类也要有 cpp 放虚表）

#include "termchart/network/tls/backend.hpp"

namespace termchart::network::tls {

TlsBackend::~TlsBackend() = default;

}  // namespace termchart::network::tls
