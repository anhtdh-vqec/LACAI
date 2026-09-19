#include "vqec_vision_single_image_inference.hpp"

#include <limits>
#include <utility>

#include "vqec/vision/ai/contracts/inference/vqec_vision_tensor_contract.hpp"

namespace vqec::vision::ai {

status single_image_inference::vqec_vision_ai_appl_siinf_configure(
    const single_image_inference_config& _config) {
    if (is_configured_ || _config.processor_ == nullptr || _config.graph_ == nullptr ||
        _config.decoder_ == nullptr || _config.plan_ == nullptr ||
        _config.geometry_.width_ == 0 || _config.geometry_.height_ == 0 ||
        _config.cycle_id_ == 0 || _config.job_timeout_ns_ == 0) {
        return {status_code::invalid_argument, "invalid single-image inference config"};
    }
    config_ = _config;
    is_configured_ = true;
    return {};
}

status single_image_inference::vqec_vision_ai_appl_siinf_prepare_input() {
    if (config_.graph_->vqec_vision_ai_ports_infgr_get_state() !=
        inference_graph_state::running) {
        return {status_code::invalid_state,
            "single-image inference graph is not running"};
    }
    if (is_input_prepared_) return {};
    std::vector<tensor_spec> inputs;
    const auto resolved = config_.graph_->vqec_vision_ai_ports_infgr_get_input_specs(inputs);
    if (resolved.code_ != status_code::ok) return resolved;
    if (inputs.size() != 1 || vqec_vision_ai_core_tnctr_shape_bytes(inputs[0]) == 0) {
        return {status_code::unsupported, "single-image inference requires one fixed input"};
    }
    input_spec_ = inputs[0];
    input_.resize(1);
    input_[0].spec_ = input_spec_;
    input_[0].bytes_.resize(
        static_cast<std::size_t>(vqec_vision_ai_core_tnctr_shape_bytes(input_spec_)));
    is_input_prepared_ = true;
    return {};
}

status single_image_inference::vqec_vision_ai_appl_siinf_run(
    const raw_frame& _frame, std::uint64_t _steady_now_ns,
    observation_batch& _observations) {
    if (!is_configured_) {
        return {status_code::invalid_state, "single-image inference is not configured"};
    }
    if (!_frame.owner_ || _frame.descriptor_.buffer_id_ == 0 ||
        _frame.descriptor_.session_epoch_ == 0 ||
        _frame.descriptor_.pts_ns_ == std::numeric_limits<std::uint64_t>::max() ||
        _steady_now_ns == std::numeric_limits<std::uint64_t>::max() ||
        _frame.descriptor_.width_ != config_.geometry_.width_ ||
        _frame.descriptor_.height_ != config_.geometry_.height_) {
        return {status_code::invalid_argument, "invalid single-image source frame"};
    }
    const auto prepared = vqec_vision_ai_appl_siinf_prepare_input();
    if (prepared.code_ != status_code::ok) return prepared;
    if (config_.graph_->vqec_vision_ai_ports_infgr_get_outstanding() != 0) {
        return {status_code::resource_exhausted, "single-image graph still has a job"};
    }
    const auto preprocessed = config_.processor_->vqec_vision_ai_ports_imgpr_preprocess(
        _frame, *config_.plan_, input_spec_, input_);
    if (preprocessed.code_ != status_code::ok) return preprocessed;
    if (armed_source_epoch_ != _frame.descriptor_.session_epoch_) {
        const auto armed = config_.graph_->vqec_vision_ai_ports_infgr_arm(
            config_.cycle_id_, _frame.descriptor_.session_epoch_, config_.job_timeout_ns_,
            submission_sequence_policy::unique_source_frames);
        if (armed.code_ != status_code::ok) return armed;
        armed_source_epoch_ = _frame.descriptor_.session_epoch_;
    }
    submission_ticket ticket;
    const auto submitted = config_.graph_->vqec_vision_ai_ports_infgr_submit_tensors(
        _frame.descriptor_.session_epoch_, _frame.descriptor_.buffer_id_,
        _frame.descriptor_.pts_ns_, input_, _steady_now_ns, ticket);
    if (submitted.code_ != status_code::ok) return submitted;
    tensor_result result;
    const auto polled = config_.graph_->vqec_vision_ai_ports_infgr_poll_result(
        _steady_now_ns, result);
    if (polled.code_ != status_code::ok) return polled;
    const preview_frame_key key{config_.camera_id_, config_.channel_id_,
        _frame.descriptor_.session_epoch_, _frame.descriptor_.buffer_id_,
        _frame.descriptor_.pts_ns_};
    scratch_ = {};
    status decoded;
    try {
        decoded = config_.decoder_->vqec_vision_ai_cntr_mddec_decode(
            result, key, scratch_);
    } catch (...) {
        return {status_code::io_error, "single-image decoder raised an exception"};
    }
    if (decoded.code_ != status_code::ok) return decoded;
    const auto valid = vqec_vision_ai_core_obval_validate_detections(
        scratch_, key, config_.geometry_);
    if (valid.code_ != status_code::ok) return valid;
    std::swap(_observations, scratch_);
    return {};
}

}  // namespace vqec::vision::ai
