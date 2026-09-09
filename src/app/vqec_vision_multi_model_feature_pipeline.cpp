#include "vqec_vision_multi_model_feature_pipeline.hpp"

namespace vqec::vision::ai {

multi_model_feature_pipeline::multi_model_feature_pipeline(
    multi_model_result_router& _result_router) noexcept
    : result_router_(_result_router) {}

status multi_model_feature_pipeline::vqec_vision_ai_appl_mmfpl_configure(
    const std::array<feature_fanout*, deployment_limits::g_max_models_per_source>&
        _feature_fanouts,
    std::uint16_t _model_count) {
    if (is_configured_) {
        return {status_code::invalid_state, "multi-model feature pipeline is already configured"};
    }
    if (_model_count == 0 || _model_count > _feature_fanouts.size() ||
        _model_count != result_router_.vqec_vision_ai_appl_mmrrt_get_stage_count()) {
        return {status_code::invalid_argument,
            "feature pipeline model count differs from result router"};
    }
    for (std::uint16_t slot = 0; slot < _model_count; ++slot) {
        if (_feature_fanouts[slot] == nullptr) {
            continue;
        }
        if (_feature_fanouts[slot]->vqec_vision_ai_appl_ftfan_get_stage_count() == 0) {
            return {status_code::invalid_state, "feature fan-out is not configured"};
        }
        for (std::uint16_t prior = 0; prior < slot; ++prior) {
            if (_feature_fanouts[prior] == _feature_fanouts[slot]) {
                return {status_code::invalid_argument,
                    "stateful feature fan-out is bound to more than one model"};
            }
        }
    }
    feature_fanouts_ = _feature_fanouts;
    model_count_ = _model_count;
    is_configured_ = true;
    return {};
}

status multi_model_feature_pipeline::vqec_vision_ai_appl_mmfpl_process_result(
    const tensor_result& _result, const multi_model_pump_report& _pump_report,
    std::uint64_t _now_monotonic_ns,
    std::array<observation_batch, deployment_limits::g_max_models_per_source>&
        _tracked_by_model,
    std::array<feature_event_batch, feature_fanout_limits::g_max_feature_stages>&
        _events,
    multi_model_feature_pipeline_report& _report) {
    _report = {};
    if (!is_configured_) {
        return {status_code::invalid_state, "multi-model feature pipeline is not configured"};
    }
    const auto routed = result_router_.vqec_vision_ai_appl_mmrrt_route_result(
        _result, _pump_report, _now_monotonic_ns, _tracked_by_model, _report.result_);
    if (routed.code_ != status_code::ok) {
        return routed;
    }
    const auto slot = _report.result_.model_slot_;
    if (slot >= model_count_) {
        return {status_code::invalid_state, "result router returned an unbound model slot"};
    }
    auto* fanout = feature_fanouts_[slot];
    if (fanout == nullptr) {
        return {};
    }
    _report.has_feature_fanout_ = true;
    return fanout->vqec_vision_ai_appl_ftfan_process(
        _tracked_by_model[slot], _now_monotonic_ns, _report.result_.is_source_gap_,
        _events, _report.features_);
}

}  // namespace vqec::vision::ai
