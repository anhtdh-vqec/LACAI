#ifndef VQEC_VISION_AI_QUALCOMM_FRAME_SUBMISSION_HPP
#define VQEC_VISION_AI_QUALCOMM_FRAME_SUBMISSION_HPP

#include <cstdint>
#include <memory>

#include <gst/app/gstappsrc.h>

#include "vqec_vision_dmabuf_bridge.hpp"

namespace vqec::vision::ai {

// Private adapter record. Serialized caller retains this until both completions.
// No graph/frame ownership: destruction is NOT cancellation or safe graph teardown.
class frame_submission {
public:
    frame_submission() = default;
    frame_submission(const frame_submission& _other) = delete;
    frame_submission& operator=(const frame_submission& _other) = delete;
    [[nodiscard]] status vqec_vision_ai_qcom_frsub_poll_input(submission_window& _window);
    // Caller has already released all output readers; this checks correlation only.
    [[nodiscard]] status vqec_vision_ai_qcom_frsub_complete_result(
        submission_window& _window, std::uint64_t _pipeline_pts_ns);
    [[nodiscard]] status vqec_vision_ai_qcom_frsub_reset();
    [[nodiscard]] bool vqec_vision_ai_qcom_frsub_has_submission() const noexcept;
    [[nodiscard]] submission_ticket vqec_vision_ai_qcom_frsub_get_ticket() const noexcept;

private:
    friend status vqec_vision_ai_qcom_frsub_push_frame(
        GstAppSrc* _source, submission_window& _window, const frame_descriptor& _descriptor,
        int _frame_fd, std::shared_ptr<const void> _owner, const dmabuf_bridge_profile& _profile,
        std::uint64_t _max_frame_bytes, std::uint64_t _steady_now_ns,
        frame_submission& _submission);
    submission_ticket ticket_;
    std::unique_ptr<read_completion> completion_;
    bool input_done_{false};
    bool result_done_{false};
};

// Borrowed source/window. Caller guarantees graph lifetime, binding/sync and sole producer.
// Requires an empty output record and block=false. No blocking wait, no implicit retry.
// Error AFTER commit still populates the record; always inspect has_submission().
// C++ allocation exceptions before commit cancel reservations; no exception crosses Gst C ABI.
[[nodiscard]] status vqec_vision_ai_qcom_frsub_push_frame(
    GstAppSrc* _source, submission_window& _window, const frame_descriptor& _descriptor,
    int _frame_fd, std::shared_ptr<const void> _owner, const dmabuf_bridge_profile& _profile,
    std::uint64_t _max_frame_bytes, std::uint64_t _steady_now_ns, frame_submission& _submission);

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_QUALCOMM_FRAME_SUBMISSION_HPP
