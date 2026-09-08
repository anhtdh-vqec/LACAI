#ifndef VQEC_VISION_AI_QUALCOMM_DMABUF_BRIDGE_HPP
#define VQEC_VISION_AI_QUALCOMM_DMABUF_BRIDGE_HPP

#include <cstdint>
#include <memory>

#include <gst/gst.h>

#include "vqec/vision/ai/contracts/vqec_vision_deployment_config.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_frame_descriptor.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_status.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_submission_window.hpp"

namespace vqec::vision::ai {

struct dmabuf_bridge_profile {
    std::uint32_t width_{0};
    std::uint32_t height_{0};
    // Required from validated per-source deployment configuration.
    std::uint64_t max_allocation_bytes_{0};
};

// Cold-path adapter composition after deployment validation. Works for any raw surface
// that conforms to the source profile; preserves _profile on failure.
[[nodiscard]] status vqec_vision_ai_qcom_dmbrg_make_profile(
    const source_deployment_config& _source, dmabuf_bridge_profile& _profile);

struct input_release_signal;

// Observer owns only a signal, never the camera frame. Poll on the serialized job executor.
// Underlying memory release is not a device fence: see the bridge ownership contract.
class read_completion {
public:
    read_completion() = default;
    read_completion(const read_completion&) = delete;
    read_completion& operator=(const read_completion&) = delete;
    [[nodiscard]] status vqec_vision_ai_qcom_dmbrg_poll_input(submission_window& _window);

private:
    friend status vqec_vision_ai_qcom_dmbrg_wrap_tracked_frame(
        const frame_descriptor& _descriptor, int _frame_fd, std::shared_ptr<const void> _owner,
        const dmabuf_bridge_profile& _profile, const submission_ticket& _ticket,
        GstBuffer*& _buffer, std::unique_ptr<read_completion>& _completion);
    std::shared_ptr<input_release_signal> signal_;
    submission_token token_;
    bool reported_{false};
};

// Both outputs must be empty. Reservation ticket must be from the receiving job window.
// Success sets pipeline PTS from the ticket and returns an independent completion observer.
[[nodiscard]] status vqec_vision_ai_qcom_dmbrg_wrap_tracked_frame(
    const frame_descriptor& _descriptor, int _frame_fd, std::shared_ptr<const void> _owner,
    const dmabuf_bridge_profile& _profile, const submission_ticket& _ticket,
    GstBuffer*& _buffer, std::unique_ptr<read_completion>& _completion);

// Private GStreamer boundary. Caller initializes GStreamer before calling.
// _frame_fd is borrowed; _owner must own that FD and keep the FW frame leased.
// Output must be null. Success returns one owned GstBuffer reference; unref it.
// Failure leaves output null and does not consume the caller's owner or FD.
// No pixel access, hardware submission or synchronization is performed here.
// Retain all GstMemory readers through device completion; this function cannot prove it.
// C++ allocation failures may throw; do not call across a C callback uncaught.
[[nodiscard]] status vqec_vision_ai_qcom_dmbrg_wrap_frame(
    const frame_descriptor& _descriptor, int _frame_fd, std::shared_ptr<const void> _owner,
    const dmabuf_bridge_profile& _profile, GstBuffer*& _buffer);

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_QUALCOMM_DMABUF_BRIDGE_HPP
