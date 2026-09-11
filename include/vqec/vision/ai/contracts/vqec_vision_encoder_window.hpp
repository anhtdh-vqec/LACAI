#ifndef VQEC_VISION_AI_CONTRACTS_ENCODER_WINDOW_HPP
#define VQEC_VISION_AI_CONTRACTS_ENCODER_WINDOW_HPP

#include <array>
#include <cstdint>

#include "vqec/vision/ai/contracts/vqec_vision_preview_contract.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_encoder_backend.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_submission_window.hpp"

namespace vqec::vision::ai {

struct encoder_window_config {
    submission_config submission_;
    std::uint32_t camera_id_{0};
    std::uint32_t channel_id_{0};
    preview_geometry geometry_;
    std::uint64_t max_input_bytes_{0};
};

// Serialized, no image ownership/I/O. Supervisor retains this ledger until drained.
// Actual encoder jobs retain sealed input owners independently of result completion.
class encoder_window {
public:
    [[nodiscard]] status vqec_vision_ai_core_encwn_configure(const encoder_window_config& _config);
    // Reserve BEFORE allocation; failure leaves _ticket unchanged. Demand is not authorization.
    [[nodiscard]] status vqec_vision_ai_core_encwn_reserve(
        const preview_frame_key& _frame, bool _has_demand, std::uint64_t _now_ns,
        submission_ticket& _ticket);
    [[nodiscard]] status vqec_vision_ai_core_encwn_commit(submission_token _token);
    // Mark once immediately BEFORE backend submit. Never retry the same token, even on error.
    // Failure means do not call backend and do not synthesize rejection completions.
    [[nodiscard]] status vqec_vision_ai_core_encwn_begin_submission(submission_token _token);
    // Preferred backend entry: validates against stored reservation before marking attempted.
    // _expected_generation comes from the independently retained output binding.
    [[nodiscard]] status vqec_vision_ai_core_encwn_begin_input(
        const encoder_input& _input, std::uint64_t _expected_generation);
    [[nodiscard]] status vqec_vision_ai_core_encwn_cancel_reserved(submission_token _token);
    [[nodiscard]] status vqec_vision_ai_core_encwn_complete_input(submission_token _token);
    [[nodiscard]] status vqec_vision_ai_core_encwn_complete_result(
        submission_token _token, const h264_access_unit_view& _unit);
    // Requires backend proof of terminal no-output; never a timeout cancellation.
    [[nodiscard]] status vqec_vision_ai_core_encwn_complete_dropped_result(submission_token _token);
    // Call after handling/discarding output_ready payload; this method does not deliver it.
    // Invalid events/faults stop admission without releasing outstanding reservations.
    [[nodiscard]] status vqec_vision_ai_core_encwn_apply_event(const encoder_event& _event);
    // Run BEFORE output delivery; no mutation. Serialize through handling and apply_event.
    [[nodiscard]] status vqec_vision_ai_core_encwn_validate_event(const encoder_event& _event) const;
    [[nodiscard]] status vqec_vision_ai_core_encwn_check_deadlines(std::uint64_t _now_ns);
    void vqec_vision_ai_core_encwn_begin_drain() noexcept;
    // Quarantine on a malformed/failed backend event: stops admission and reports faulted
    // without releasing outstanding accounting. Distinct from a graceful drain.
    void vqec_vision_ai_core_encwn_mark_fault() noexcept;
    [[nodiscard]] unsigned vqec_vision_ai_core_encwn_outstanding() const noexcept;
    [[nodiscard]] std::uint64_t vqec_vision_ai_core_encwn_reserved_bytes() const noexcept;
    [[nodiscard]] preview_geometry vqec_vision_ai_core_encwn_geometry() const noexcept;

private:
    struct entry {
        submission_token token_;
        preview_frame_key frame_;
        bool input_done_{false};
        bool result_done_{false};
        bool committed_{false};
        bool submission_attempted_{false};
        std::uint64_t pipeline_pts_ns_{0};
    };
    [[nodiscard]] entry* vqec_vision_ai_core_encwn_find(submission_token _token) noexcept;
    [[nodiscard]] status vqec_vision_ai_core_encwn_complete(submission_token _token, bool _input);
    [[nodiscard]] bool vqec_vision_ai_core_encwn_accept_clock(std::uint64_t _now_ns) noexcept;
    submission_window ledger_;
    encoder_window_config config_;
    std::array<entry, submission_limits::g_max_jobs> entries_{};
    std::uint64_t surface_bytes_{0};
    std::uint64_t reserved_bytes_{0};
    std::uint64_t last_now_ns_{0};
    bool configured_{false};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_CONTRACTS_ENCODER_WINDOW_HPP
