#ifndef VQEC_VISION_AI_APP_ENCODER_PREPARATION_HPP
#define VQEC_VISION_AI_APP_ENCODER_PREPARATION_HPP

#include "vqec/vision/ai/contracts/vqec_vision_encoder_window.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_encoder_backend.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_preview_pool.hpp"

namespace vqec::vision::ai {

// One nonblocking drain step: closes admission before backend call, never releases jobs.
// ok requires backend quiescence AND no outstanding ledger reservations.
// Continue polling/handling events and explicitly cancel never-submitted preparations.
[[nodiscard]] status vqec_vision_ai_appl_enprp_drain_backend(
    encoder_window& _window, encoder_backend& _backend);

// Calls backend once after ledger validation/one-shot admission. No automatic retry.
// Backend rejection is reconciled ONLY under encoder_backend's no-access/no-retention contract.
// Caller retains _input; exceptions stop admission, preserve reservations and propagate.
[[nodiscard]] status vqec_vision_ai_appl_enprp_submit_backend(
    encoder_window& _window, encoder_backend& _backend, const encoder_input& _input,
    std::uint64_t _expected_generation);

// Serialized pre-submission owner. The borrowed window must outlive this object.
// No hardware may retain borrow_data(); finish synchronous rendering before cancel/handoff.
// Explicit cancel or commit_input is required; destruction releases CPU storage only,
// never pretends that an abandoned ledger reservation has completed.
class encoder_preparation {
public:
    explicit encoder_preparation(encoder_window& _window) noexcept;
    encoder_preparation(const encoder_preparation&) = delete;
    encoder_preparation& operator=(const encoder_preparation&) = delete;
    encoder_preparation(encoder_preparation&&) = delete;
    encoder_preparation& operator=(encoder_preparation&&) = delete;
    [[nodiscard]] status vqec_vision_ai_appl_enprp_prepare(
        preview_surface_pool& _pool, const preview_frame_key& _frame,
        bool _has_demand, std::uint64_t _now_ns);
    // Captures generation at preparation, never at late handoff/retry.
    [[nodiscard]] status vqec_vision_ai_appl_enprp_prepare_backend(
        preview_surface_pool& _pool, const preview_frame_key& _frame,
        bool _has_demand, std::uint64_t _now_ns, std::uint64_t _dispatch_generation);
    [[nodiscard]] std::uint8_t* vqec_vision_ai_appl_enprp_borrow_data() noexcept;
    [[nodiscard]] status vqec_vision_ai_appl_enprp_cancel();
    // Requires empty destination. Commits ledger before no-throw seal/owner transfer.
    // Caller retains owner across backend submission through actual input completion.
    // This is NOT hardware submission; failed backend push needs explicit reconciliation.
    [[nodiscard]] status vqec_vision_ai_appl_enprp_commit_input(
        submission_ticket& _ticket,
        std::shared_ptr<const std::vector<std::uint8_t>>& _input);
    // Requires prepare_backend and destination without pixels. Failure preserves destination.
    [[nodiscard]] status vqec_vision_ai_appl_enprp_commit_backend(encoder_input& _input);

private:
    encoder_window& window_;
    submission_ticket ticket_{};
    writable_preview_surface writer_;
    preview_frame_key frame_;
    std::uint64_t dispatch_generation_{0};
};

// Serialized reserve/acquire/rollback. No hardware or rendering. Writer must be empty.
// Ticket unchanged before reservation; after reservation it identifies even a rolled-back attempt.
[[nodiscard]] status vqec_vision_ai_appl_enprp_prepare_input(
    encoder_window& _window, preview_surface_pool& _pool, const preview_frame_key& _frame,
    bool _has_demand, std::uint64_t _now_ns, submission_ticket& _ticket,
    writable_preview_surface& _writer);

// Only for unsubmitted work. Caller binds writer to token and ends all mutable borrows.
// Empty/sealed/moved-from writer rejects before ledger mutation.
// Failed cancellation preserves the writer; no automatic cancellation on destruction.
[[nodiscard]] status vqec_vision_ai_appl_enprp_cancel_input(
    encoder_window& _window, submission_token _token, writable_preview_surface& _writer);

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_APP_ENCODER_PREPARATION_HPP
