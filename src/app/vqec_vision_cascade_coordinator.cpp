#include "vqec_vision_cascade_coordinator.hpp"

#include <utility>

namespace vqec::vision::ai {

status cascade_coordinator::vqec_vision_ai_appl_cscrd_configure(
    const cascade_coordinator_config& _config) {
    if (is_configured_) {
        return {status_code::invalid_state, "cascade coordinator is already configured"};
    }
    if (_config.aligner_ == nullptr || _config.lease_ == nullptr ||
        _config.max_tasks_per_frame_ == 0 ||
        _config.max_tasks_per_frame_ > image_alignment_limits::g_max_points) {
        return {status_code::invalid_argument, "invalid cascade coordinator configuration"};
    }
    alignment_capabilities capabilities;
    const auto probed = _config.aligner_->vqec_vision_ai_ports_imaln_probe_capabilities(
        capabilities);
    if (probed.code_ != status_code::ok) {
        return probed;
    }
    const auto checked =
        _config.aligner_->vqec_vision_ai_ports_imaln_validate_template(
            _config.template_, capabilities);
    if (checked.code_ != status_code::ok) {
        return checked;
    }
    aligner_ = _config.aligner_;
    lease_ = _config.lease_;
    template_ = _config.template_;
    capabilities_ = capabilities;
    max_tasks_per_frame_ = _config.max_tasks_per_frame_;
    is_configured_ = true;
    return {};
}

bool cascade_coordinator::vqec_vision_ai_appl_cscrd_is_configured() const noexcept {
    return is_configured_;
}

status cascade_coordinator::vqec_vision_ai_appl_cscrd_process(
    const observation_batch& _tracked, std::vector<alignment_result>& _aligned,
    cascade_coordinator_report& _report) {
    _aligned.clear();
    _report = {};
    if (!is_configured_) {
        return {status_code::invalid_state, "cascade coordinator is not configured"};
    }
    const preview_frame_key& key = _tracked.frame_;
    if (key.source_epoch_ == 0 || key.frame_id_ == 0) {
        return {status_code::invalid_argument, "cascade batch has no source frame identity"};
    }
    std::size_t accepted = 0;
    for (const auto& observation : _tracked.observations_) {
        if (accepted >= max_tasks_per_frame_ ||
            observation.landmarks_.points_.empty() ||
            observation.landmarks_.schema_id_.empty()) {
            ++_report.skipped_;
            continue;
        }
        alignment_request request;
        request.frame_ = key;
        request.landmarks_ = observation.landmarks_;
        raw_frame frame;
        std::uint64_t ticket = 0;
        const auto acquired = lease_->vqec_vision_ai_ports_cflse_acquire(
            key, frame, ticket);
        if (acquired.code_ != status_code::ok) {
            ++_report.failed_;
            continue;
        }
        alignment_result result;
        const auto aligned = aligner_->vqec_vision_ai_ports_imaln_align(
            request, frame, template_, result, ticket);
        const bool align_ok = aligned.code_ == status_code::ok;
        if (align_ok) {
            _aligned.push_back(std::move(result));
            ++accepted;
        }
        // The ticket is completed once this dependent's read is done, even when alignment
        // failed after reading the frame; never leave a ticket outstanding.
        const bool complete_ok =
            lease_->vqec_vision_ai_ports_cflse_complete(ticket).code_ == status_code::ok;
        if (align_ok && complete_ok) {
            ++_report.accepted_;
        } else {
            ++_report.failed_;
            if (align_ok) {
                // The warp succeeded but the frame could not be released; do not publish a
                // result tied to a leaked ticket.
                _aligned.pop_back();
                --accepted;
            }
        }
    }
    // Close admission for this frame regardless of how many tasks were admitted, so the
    // pump-retained frame is released once the last ticket completes.
    const auto retired = lease_->vqec_vision_ai_ports_cflse_retire(key);
    if (retired.code_ != status_code::ok && retired.code_ != status_code::invalid_state) {
        ++_report.failed_;
    }
    return {};
}

}  // namespace vqec::vision::ai
