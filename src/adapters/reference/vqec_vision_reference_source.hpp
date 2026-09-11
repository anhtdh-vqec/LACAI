#ifndef VQEC_VISION_AI_REFER_REFERENCE_SOURCE_HPP
#define VQEC_VISION_AI_REFER_REFERENCE_SOURCE_HPP

#include <cstdint>

#include "vqec/vision/ai/ports/vqec_vision_raw_source.hpp"

namespace vqec::vision::ai {

struct reference_source_config {
    std::uint32_t width_{0};
    std::uint32_t height_{0};
    std::uint32_t fps_numerator_{0};
    std::uint32_t fps_denominator_{0};
};

// Device-free development backend: generates deterministic synthetic NV12 frames that
// conform to the neutral RAW descriptor. It never touches a camera, DMA-BUF importer or
// FW transport, and its native handle must not be treated as a real hardware allocation.
// Serialized owner; one acquisition epoch per start.
class reference_raw_source final : public raw_source_port {
public:
    explicit reference_raw_source(reference_source_config _config) noexcept;
    reference_raw_source(const reference_raw_source& _other) = delete;
    reference_raw_source& operator=(const reference_raw_source& _other) = delete;
    ~reference_raw_source() noexcept override;

    [[nodiscard]] status vqec_vision_ai_ports_rawsr_start(int _timeout_ms) override;
    [[nodiscard]] status vqec_vision_ai_ports_rawsr_receive(
        raw_frame& _frame, int _timeout_ms) override;
    [[nodiscard]] status vqec_vision_ai_ports_rawsr_stop(int _timeout_ms) override;
    [[nodiscard]] raw_source_state
    vqec_vision_ai_ports_rawsr_get_state() const noexcept override;
    [[nodiscard]] raw_source_profile
    vqec_vision_ai_ports_rawsr_get_profile() const noexcept override;
    [[nodiscard]] unsigned
    vqec_vision_ai_ports_rawsr_get_outstanding() const noexcept override;

private:
    reference_source_config config_;
    raw_source_state state_{raw_source_state::idle};
    std::uint64_t next_buffer_id_{1};
    std::uint64_t epoch_{0};
    int synthetic_fd_{-1};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_REFER_REFERENCE_SOURCE_HPP
