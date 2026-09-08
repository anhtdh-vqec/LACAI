#include "vqec_vision_camera_graph_pump.hpp"

#include <utility>

namespace vqec::vision::ai {

camera_graph_pump::camera_graph_pump(source_lifecycle& _source, plugin_graph& _graph,
    std::shared_ptr<graph_retention> _retention, std::uint64_t _cycle_id,
    std::uint64_t _job_timeout_ns)
    : source_(_source), graph_(_graph), retention_(std::move(_retention)),
      cycle_id_(_cycle_id), job_timeout_ns_(_job_timeout_ns) {}

void camera_graph_pump::vqec_vision_ai_appl_cgpmp_begin_stop() noexcept {
    is_stopping_ = true;
}

status camera_graph_pump::vqec_vision_ai_appl_cgpmp_pump_step(
    std::uint64_t _steady_now_ns, tensor_result& _result, camera_pump_report& _report) {
    _report = {};
    if (!retention_ || cycle_id_ == 0 || job_timeout_ns_ == 0 ||
        job_timeout_ns_ == UINT64_MAX || _steady_now_ns == UINT64_MAX) {
        return {status_code::invalid_argument, "invalid pump identity, retention or timing"};
    }
    if (is_armed_) {
        const auto ticket = graph_.vqec_vision_ai_qcom_plgr_get_pending_ticket();
        tensor_result candidate;
        const auto output = graph_.vqec_vision_ai_qcom_plgr_poll_result(_steady_now_ns, candidate);
        if (output.code_ == status_code::ok) {
            if (is_failed_) {
                return {status_code::invalid_state, "result discarded after pump failure"};
            }
            _result = std::move(candidate);
            _report.has_result_ = true;
            _report.ticket_ = ticket;
            return {};
        }
        if (output.code_ != status_code::pending) {
            is_failed_ = true;
            return output;
        }
        if (graph_.vqec_vision_ai_qcom_plgr_get_outstanding() != 0) {
            return {status_code::pending, "graph job remains outstanding; no camera receive"};
        }
    }
    if (is_failed_) {
        return {status_code::invalid_state, "pump failed; supervisor must reconcile this cycle"};
    }
    if (is_stopping_) {
        return {status_code::pending,
                "pump stopped receiving; supervisor drives graph/source drain"};
    }
    if (source_.vqec_vision_ai_camer_srclc_get_state() != camera_source_state::running ||
        graph_.vqec_vision_ai_qcom_plgr_get_state() != plugin_graph_state::playing) {
        return {status_code::invalid_state, "pump requires running camera and PLAYING graph"};
    }
    std::shared_ptr<const received_frame> frame;
    const auto received = source_.vqec_vision_ai_camer_srclc_receive(frame, 0);
    if (received.code_ == status_code::timeout) {
        return {status_code::pending, "no camera frame available"};
    }
    if (received.code_ != status_code::ok) {
        is_failed_ = true;
        return received;
    }
    const auto& descriptor = frame->vqec_vision_ai_camer_frsrc_get_descriptor();
    if (!is_armed_) {
        const auto armed = graph_.vqec_vision_ai_qcom_plgr_arm_submission(
            cycle_id_, descriptor.session_epoch_, job_timeout_ns_, retention_);
        if (armed.code_ != status_code::ok) {
            is_failed_ = true;
            return armed;  // This frame was never submitted; normal release may ACK it.
        }
        is_armed_ = true;
    }
    submission_ticket ticket;
    const auto submitted = graph_.vqec_vision_ai_qcom_plgr_submit_frame(
        descriptor, frame->vqec_vision_ai_camer_frsrc_get_fd(), frame, _steady_now_ns, ticket);
    if (ticket.token_.job_id_ != 0) {
        _report.has_submission_ = true;
        _report.ticket_ = ticket;
    }
    if (submitted.code_ != status_code::ok && submitted.code_ != status_code::pending) {
        is_failed_ = true;
    }
    return submitted;
}

}  // namespace vqec::vision::ai
