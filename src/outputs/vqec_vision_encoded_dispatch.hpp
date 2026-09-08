#ifndef VQEC_VISION_AI_OUTPUTS_ENCODED_DISPATCH_HPP
#define VQEC_VISION_AI_OUTPUTS_ENCODED_DISPATCH_HPP

#include "vqec/vision/ai/contracts/vqec_vision_encoded_output.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_encoded_sink.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_output_gate.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_encoder_window.hpp"

namespace vqec::vision::ai {

struct encoded_dispatch_context {
    preview_frame_key expected_frame_;
    preview_geometry expected_geometry_;
    std::string expected_source_id_;
    std::uint64_t mapping_generation_{0};
    std::uint64_t created_monotonic_ns_{0};
    std::uint64_t max_age_ns_{0};
    // Trusted complete set of scopes rendered into pixels, never untrusted request claims.
    std::vector<output_authorization> rendered_scopes_;
};

// Synchronous, serialized/non-reentrant with policy/source changes. Retains no input.
// Pending=no viewers/no write; failures do not trigger retry or release encoder input.
[[nodiscard]] status vqec_vision_ai_outpt_encdp_dispatch(
    const owned_h264_output& _output, const encoded_dispatch_context& _context,
    std::uint64_t _now_ns, output_gate& _gate, encoded_sink& _sink);

// Terminal one-event handling; no retry/queue. Return value reports event/ledger status.
// For output_ready, _delivery reports dispatch outcome even when output is discarded.
// Non-output/invalid events preserve _delivery. Do not replay a successfully handled event.
// Context must be retained from that job, never obtained by retagging with a current binding.
[[nodiscard]] status vqec_vision_ai_outpt_encdp_handle_event(
    const encoder_event& _event, encoder_window& _window,
    const encoded_dispatch_context& _context, std::uint64_t _now_ns,
    output_gate& _gate, encoded_sink& _sink, status& _delivery);

// Polls at most one event, never blocks/retries internally. Destination must be reset {}.
// Pending/error preserves destination. Successful transfer does not apply completion.
// Keep the returned event until handle_event or explicit recovery; do not poll over it.
[[nodiscard]] status vqec_vision_ai_outpt_encdp_poll_event(
    encoder_backend& _backend, encoder_window& _window, encoder_event& _event);

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_OUTPUTS_ENCODED_DISPATCH_HPP
