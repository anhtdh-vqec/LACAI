#include "vqec_vision_feature_event_dispatch.hpp"

#include <new>

namespace vqec::vision::ai {

status vqec_vision_ai_outpt_ftdsp_dispatch_event(
    const feature_event_batch& _batch, std::size_t _event_index,
    const feature_processor_config& _config, std::uint64_t _policy_revision,
    std::uint64_t _steady_now_ns, output_gate& _gate, feature_event_sink_port& _sink) {
    const auto valid = vqec_vision_ai_core_ftevt_validate_batch(
        _batch, _batch.frame_, _batch.geometry_, _config);
    if (valid.code_ != status_code::ok) {
        return valid;
    }
    if (_event_index >= _batch.events_.size() || _policy_revision == 0 ||
        _policy_revision == UINT64_MAX) {
        return {status_code::invalid_argument, "invalid feature event dispatch context"};
    }

    const auto& event = _batch.events_[_event_index];
    output_authorization authorization;
    try {
        authorization.policy_revision_ = _policy_revision;
        authorization.source_id_ = event.source_id_;
        authorization.feature_id_ = event.feature_id_;
        authorization.attributes_.reserve(event.fields_.size());
        for (const auto& field : event.fields_) {
            authorization.attributes_.push_back(field.schema_id_);
        }
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted, "feature authorization allocation failed"};
    }
    const auto authorized =
        _gate.vqec_vision_ai_core_otgat_authorize(authorization, _steady_now_ns);
    if (authorized.code_ != status_code::ok) {
        return authorized;
    }
    try {
        return _sink.vqec_vision_ai_ports_fesnk_deliver_event(event);
    } catch (...) {
        return {status_code::io_error, "feature event sink raised an exception"};
    }
}

}  // namespace vqec::vision::ai
