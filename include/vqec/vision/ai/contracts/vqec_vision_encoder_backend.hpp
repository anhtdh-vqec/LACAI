#ifndef VQEC_VISION_AI_CONTRACTS_ENCODER_BACKEND_HPP
#define VQEC_VISION_AI_CONTRACTS_ENCODER_BACKEND_HPP

#include <cstdint>
#include <memory>
#include <vector>

#include "vqec/vision/ai/contracts/vqec_vision_encoded_output.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_submission_window.hpp"

namespace vqec::vision::ai {

struct encoder_input {
    preview_frame_key frame_;
    preview_geometry geometry_;
    submission_ticket ticket_;
    std::uint64_t dispatch_generation_{0};
    std::shared_ptr<const std::vector<std::uint8_t>> pixels_;
};

enum class encoder_event_kind {
    input_complete,
    output_ready,
    output_dropped,
    fault
};

// Pure preflight; no retain, mutation, I/O or proof of committed/admitted job state.
// Expected ticket/generation/identity must come from independent runtime binding.
[[nodiscard]] status vqec_vision_ai_core_encct_validate_input(
    const encoder_input& _input, const preview_frame_key& _expected_frame,
    const preview_geometry& _expected_geometry, const submission_ticket& _expected_ticket,
    std::uint64_t _expected_generation);

struct encoder_event {
    encoder_event_kind kind_{encoder_event_kind::fault};
    submission_token token_;
    std::shared_ptr<const owned_h264_output> output_;
    status detail_;
};

// Structural event validation only; caller still correlates tokens and rejects duplicates.
// A valid fault never grants input/result completion.
[[nodiscard]] status vqec_vision_ai_core_encct_validate_event(const encoder_event& _event);

// Internal C++17 port. See encoder_backend.md for normative ownership/event rules.
// No thread safety, hardware implementation, configuration or cancellation is implied.
class encoder_backend {
public:
    virtual ~encoder_backend() = default;
    // ok = backend accepted responsibility and retains input as needed.
    // Non-ok = no device access/retention/future event for this attempt.
    [[nodiscard]] virtual status vqec_vision_ai_cntr_encbk_submit(
        const encoder_input& _input) = 0;
    // Nonblocking; pending/error preserves _event. Fault is not completion.
    [[nodiscard]] virtual status vqec_vision_ai_cntr_encbk_poll(encoder_event& _event) = 0;
    // Stops admission. Repeat/poll until ok; pending/error is not quiescence.
    // ok requires no readers, pending results or undelivered events.
    [[nodiscard]] virtual status vqec_vision_ai_cntr_encbk_begin_drain() = 0;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_CONTRACTS_ENCODER_BACKEND_HPP
