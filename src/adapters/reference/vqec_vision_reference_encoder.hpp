#ifndef VQEC_VISION_AI_REFER_REFERENCE_ENCODER_HPP
#define VQEC_VISION_AI_REFER_REFERENCE_ENCODER_HPP

#include <cstddef>
#include <cstdint>
#include <deque>

#include "vqec/vision/ai/contracts/media/vqec_vision_encoder_backend.hpp"

namespace vqec::vision::ai {

struct reference_encoder_config {
    std::size_t max_pending_{8};
};

// Device-free encoder backend: accepts at most max_pending inputs, then emits one synthetic
// access unit per submitted input when polled. It owns no pixels and proves only the encoder
// lifecycle and ownership contract, never hardware encoding or output correctness.
class reference_encoder final : public encoder_backend {
public:
    reference_encoder() = default;
    explicit reference_encoder(reference_encoder_config _config);

    [[nodiscard]] status vqec_vision_ai_cntr_encbk_submit(
        const encoder_input& _input) override;
    [[nodiscard]] status vqec_vision_ai_cntr_encbk_poll(encoder_event& _event) override;
    [[nodiscard]] status vqec_vision_ai_cntr_encbk_begin_drain() override;

    [[nodiscard]] std::size_t vqec_vision_ai_refer_renc_get_pending() const noexcept;
    [[nodiscard]] std::uint64_t vqec_vision_ai_refer_renc_get_submitted() const noexcept;
    [[nodiscard]] std::uint64_t vqec_vision_ai_refer_renc_get_delivered() const noexcept;

private:
    struct pending_input {
        submission_ticket ticket_;
        preview_frame_key frame_;
        preview_geometry geometry_;
        std::uint64_t dispatch_generation_{0};
    };

    reference_encoder_config config_;
    std::deque<pending_input> pending_;
    std::uint64_t submitted_{0};
    std::uint64_t delivered_{0};
    bool is_draining_{false};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_REFER_REFERENCE_ENCODER_HPP
