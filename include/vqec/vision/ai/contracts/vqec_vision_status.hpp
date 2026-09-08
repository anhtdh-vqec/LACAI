#ifndef VQEC_VISION_AI_CONTRACTS_STATUS_HPP
#define VQEC_VISION_AI_CONTRACTS_STATUS_HPP

#include <string>

namespace vqec::vision::ai {

enum class status_code {
    ok,
    invalid_argument,
    unsupported,
    missing_plugin,
    incompatible_plugin,
    graph_link_failed,
    timeout,
    source_lost,
    protocol_error,
    resource_exhausted,
    unauthorized,
    io_error,
    invalid_state,
    pending
};

struct status {
    status_code code_{status_code::ok};
    std::string message_;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_CONTRACTS_STATUS_HPP
