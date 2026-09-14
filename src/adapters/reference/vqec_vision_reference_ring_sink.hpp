#ifndef VQEC_VISION_AI_REFER_REFERENCE_RING_SINK_HPP
#define VQEC_VISION_AI_REFER_REFERENCE_RING_SINK_HPP

#include <cstddef>
#include <cstdint>

#include "vqec/vision/ai/contracts/vqec_vision_encoded_sink.hpp"

namespace vqec::vision::ai {

struct reference_ring_sink_config {
    std::size_t capacity_{4};
    std::uint64_t mapping_generation_{1};
    std::size_t max_access_unit_bytes_{1024U * 1024U};
};

// Device-free encoded sink with bounded ring semantics: fixed capacity, explicit
// backpressure when full, mapping-generation rejection and a consumer drain hook. It stores
// only metadata, never pixels, and proves the sink contract, not FW ring compatibility.
class reference_ring_sink final : public encoded_sink {
public:
    reference_ring_sink() = default;
    explicit reference_ring_sink(reference_ring_sink_config _config);

    [[nodiscard]] status vqec_vision_ai_cntr_encsk_query_demand(
        encoded_sink_demand& _demand) override;
    [[nodiscard]] status vqec_vision_ai_cntr_encsk_write(
        const h264_access_unit_view& _unit, std::uint64_t _expected_generation) override;

    void vqec_vision_ai_refer_rring_set_active_consumers(unsigned _consumers) noexcept;
    // Test hook: simulate one consumer reading a slot, freeing ring capacity.
    void vqec_vision_ai_refer_rring_consume_one() noexcept;
    [[nodiscard]] std::size_t vqec_vision_ai_refer_rring_get_depth() const noexcept;
    [[nodiscard]] std::size_t vqec_vision_ai_refer_rring_get_capacity() const noexcept;
    [[nodiscard]] std::uint64_t vqec_vision_ai_refer_rring_get_written() const noexcept;
    [[nodiscard]] std::uint64_t vqec_vision_ai_refer_rring_get_rejected() const noexcept;
    [[nodiscard]] std::size_t vqec_vision_ai_refer_rring_get_last_bytes() const noexcept;

private:
    reference_ring_sink_config config_;
    std::size_t depth_{0};
    unsigned consumers_{0};
    std::uint64_t written_{0};
    std::uint64_t rejected_{0};
    std::size_t last_bytes_{0};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_REFER_REFERENCE_RING_SINK_HPP
