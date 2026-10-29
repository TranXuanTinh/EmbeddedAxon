#include <cstddef>
#include <cstdint>
#include <string_view>

#include "aegis/ipc/command_protocol.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, std::size_t size) {
  const std::string_view input(reinterpret_cast<const char*>(data), size);
  auto res = aegis::ipc::CommandProtocol::parse_line(input);
  if (res.ok()) {
    const auto formatted = aegis::ipc::CommandProtocol::format_response(
        aegis::ipc::Response::OkJson("{\"fuzz\":true}"));
    (void)formatted;
  } else {
    const auto formatted = aegis::ipc::CommandProtocol::format_response(
        aegis::ipc::Response::Error(res.code(), res.message()));
    (void)formatted;
  }
  return 0;
}
