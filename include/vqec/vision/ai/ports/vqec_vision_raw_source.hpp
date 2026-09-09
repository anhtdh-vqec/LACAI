#ifndef VQEC_VISION_AI_PORTS_RAW_SOURCE_HPP
#define VQEC_VISION_AI_PORTS_RAW_SOURCE_HPP

#include <cstdint>
#include <memory>

#include "vqec/vision/ai/contracts/vqec_vision_frame_descriptor.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_status.hpp"

namespace vqec::vision::ai {

enum class raw_source_state {
    idle,
    starting,
    running,
    draining,
    stopped
};

struct raw_source_profile {
    std::uint32_t width_{0};
    std::uint32_t height_{0};
    std::uint32_t fps_numerator_{0};
    std::uint32_t fps_denominator_{0};
};

struct raw_frame {
    frame_descriptor descriptor_;
    // Adapter-native handle interpreted according to the admitted source binding.
    // Current Linux adapters carry a DMA-BUF FD; -1 is always invalid.
    std::int64_t native_handle_{-1};
    // Retain through the last real hardware read. Resetting is not device cancellation.
    std::shared_ptr<const void> owner_;
};

// Serialized owner for one admitted FW RAW source and one acquisition epoch.
// Implementations must not hide retry loops, unbounded queues or destructor RPC.
class raw_source_port {
public:
    virtual ~raw_source_port() = default;
    [[nodiscard]] virtual status vqec_vision_ai_ports_rawsr_start(int _timeout_ms) = 0;
    [[nodiscard]] virtual status vqec_vision_ai_ports_rawsr_receive(
        raw_frame& _frame, int _timeout_ms) = 0;
    [[nodiscard]] virtual status vqec_vision_ai_ports_rawsr_stop(int _timeout_ms) = 0;
    [[nodiscard]] virtual raw_source_state
    vqec_vision_ai_ports_rawsr_get_state() const noexcept = 0;
    [[nodiscard]] virtual raw_source_profile
    vqec_vision_ai_ports_rawsr_get_profile() const noexcept = 0;
    [[nodiscard]] virtual unsigned
    vqec_vision_ai_ports_rawsr_get_outstanding() const noexcept = 0;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_PORTS_RAW_SOURCE_HPP
