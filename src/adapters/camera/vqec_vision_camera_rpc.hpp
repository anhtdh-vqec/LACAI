#ifndef VQEC_VISION_AI_CAMERA_CAMERA_RPC_HPP
#define VQEC_VISION_AI_CAMERA_CAMERA_RPC_HPP

#include <map>
#include <string>

#include "vqec/vision/ai/contracts/vqec_vision_status.hpp"

namespace vqec::vision::ai {

using camera_fields = std::map<std::string, std::string>;

class camera_rpc {
public:
    virtual ~camera_rpc() = default;
    // Transport success only; FW reply code is parsed separately. Output cleared on failure.
    // Serialized caller, bounded method timeout; exceptions must not cross external callbacks.
    [[nodiscard]] virtual status vqec_vision_ai_camer_cmrpc_call(
        const std::string& _method, const camera_fields& _request,
        int _timeout_ms, camera_fields& _response) = 0;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_CAMERA_CAMERA_RPC_HPP
