#include "vqec_vision_frame_submission.hpp"

#include <utility>

namespace vqec::vision::ai {
namespace {

struct prepared_submission {
    submission_window& window_;
    submission_ticket ticket_;
    GstBuffer* buffer_{nullptr};
    bool is_reserved_{false};
    ~prepared_submission() noexcept {
        if (buffer_ != nullptr) {
            gst_buffer_unref(buffer_);
        }
        if (is_reserved_) {
            const auto cancelled =
                window_.vqec_vision_ai_core_subwn_cancel_reserved(ticket_.token_);
            (void)cancelled;  // Valid reservation, exclusively owned by this serialized operation.
        }
    }
};

}  // namespace

status vqec_vision_ai_qcom_frsub_push_frame(
    GstAppSrc* _source, submission_window& _window, const frame_descriptor& _descriptor,
    int _frame_fd, std::shared_ptr<const void> _owner, const dmabuf_bridge_profile& _profile,
    std::uint64_t _max_frame_bytes, std::uint64_t _steady_now_ns, frame_submission& _submission) {
    if (_source == nullptr || !GST_IS_APP_SRC(_source) || !_owner || _frame_fd < 0 ||
        _submission.completion_ || _max_frame_bytes == 0 ||
        _max_frame_bytes > 256ULL * 1024 * 1024 || _descriptor.view_size_bytes_ == 0 ||
        _descriptor.view_size_bytes_ > _max_frame_bytes) {
        return {status_code::invalid_argument,
                "invalid source, owner, output record or byte budget"};
    }
    gboolean is_blocking = FALSE;
    g_object_get(_source, "block", &is_blocking, nullptr);
    if (is_blocking) {
        return {status_code::invalid_argument, "bounded submission requires appsrc block=false"};
    }
    const auto health = _window.vqec_vision_ai_core_subwn_check_deadlines(_steady_now_ns);
    if (health.code_ != status_code::ok) {
        return health;
    }
    prepared_submission prepared{_window, {}};
    const auto reserved = _window.vqec_vision_ai_core_subwn_reserve(
        _descriptor.session_epoch_, _descriptor.buffer_id_, _descriptor.pts_ns_,
        _steady_now_ns, prepared.ticket_);
    if (reserved.code_ != status_code::ok) {
        return reserved;
    }
    prepared.is_reserved_ = true;
    std::unique_ptr<read_completion> completion;
    const auto wrapped = vqec_vision_ai_qcom_dmbrg_wrap_tracked_frame(
        _descriptor, _frame_fd, std::move(_owner), _profile, prepared.ticket_,
        prepared.buffer_, completion);
    if (wrapped.code_ != status_code::ok) {
        return wrapped;
    }
    const auto committed = _window.vqec_vision_ai_core_subwn_commit(prepared.ticket_.token_);
    if (committed.code_ != status_code::ok) {
        return committed;
    }
    prepared.is_reserved_ = false;
    _submission.ticket_ = prepared.ticket_;
    _submission.completion_ = std::move(completion);
    // Publish bookkeeping before handoff; downstream can release memory synchronously.
    const auto flow = gst_app_src_push_buffer(_source, std::exchange(prepared.buffer_, nullptr));
    if (flow != GST_FLOW_OK) {
        _window.vqec_vision_ai_core_subwn_mark_fault();
        return {status_code::io_error, "appsrc rejected push; committed job needs reconciliation"};
    }
    return {};
}

status frame_submission::vqec_vision_ai_qcom_frsub_poll_input(submission_window& _window) {
    if (!completion_) {
        return {status_code::invalid_state, "no committed frame submission"};
    }
    const auto result = completion_->vqec_vision_ai_qcom_dmbrg_poll_input(_window);
    if (result.code_ == status_code::ok) {
        input_done_ = true;
    }
    return result;
}

status frame_submission::vqec_vision_ai_qcom_frsub_complete_result(
    submission_window& _window, std::uint64_t _pipeline_pts_ns) {
    if (!completion_ || result_done_) {
        return {status_code::invalid_state, "no pending result for this submission"};
    }
    if (_pipeline_pts_ns != ticket_.pipeline_pts_ns_) {
        _window.vqec_vision_ai_core_subwn_mark_fault();
        return {status_code::protocol_error, "result PTS differs from committed ticket"};
    }
    const auto result = _window.vqec_vision_ai_core_subwn_complete_result(ticket_.token_);
    if (result.code_ == status_code::ok) {
        result_done_ = true;
    }
    return result;
}

status frame_submission::vqec_vision_ai_qcom_frsub_reset() {
    if (completion_ && (!input_done_ || !result_done_)) {
        return {status_code::pending, "submission still needs input and result completion"};
    }
    completion_.reset();
    ticket_ = {};
    input_done_ = false;
    result_done_ = false;
    return {};
}

bool frame_submission::vqec_vision_ai_qcom_frsub_has_submission() const noexcept {
    return completion_ != nullptr;
}

submission_ticket frame_submission::vqec_vision_ai_qcom_frsub_get_ticket() const noexcept {
    return ticket_;
}

}  // namespace vqec::vision::ai
