#include "vqec_vision_perception_stage_factory.hpp"

#include <new>
#include <utility>

namespace vqec::vision::ai {

perception_stage_bundle::perception_stage_bundle(
    model_decoder_port& _decoder, std::unique_ptr<tracker_port> _tracker,
    preview_geometry _geometry)
    : tracker_(std::move(_tracker)),
      tracking_stage_(std::make_unique<tracking_stage>(*tracker_)),
      decoder_stage_(std::make_unique<model_decode_stage>(_decoder, _geometry)),
      result_stage_(std::make_unique<perception_result_stage>(
          *decoder_stage_, *tracking_stage_)) {}

perception_result_stage* perception_stage_bundle::
vqec_vision_ai_appl_prfac_get_result_stage() noexcept {
    return result_stage_.get();
}

const perception_result_stage* perception_stage_bundle::
vqec_vision_ai_appl_prfac_get_result_stage() const noexcept {
    return result_stage_.get();
}

status vqec_vision_ai_appl_prfac_create_bundle(
    const model_catalog_entry& _model, const source_deployment_config& _source,
    const std::string& _tracker_contract,
    const model_decoder_registry& _decoder_registry,
    const tracker_registry& _tracker_registry,
    std::unique_ptr<perception_stage_bundle>& _bundle) {
    if (_model.model_id_.empty() || _source.source_id_.empty() ||
        _model.decoder_contract_.empty() || _tracker_contract.empty()) {
        return {status_code::invalid_argument,
            "perception binding identities and contracts are required"};
    }
    bool model_assigned = false;
    for (const auto& model_id : _source.model_ids_) {
        if (model_id == _model.model_id_) {
            model_assigned = true;
            break;
        }
    }
    if (!model_assigned) {
        return {status_code::invalid_argument,
            "source does not assign the requested model"};
    }
    if (_source.profile_.width_ == 0 || _source.profile_.height_ == 0) {
        return {status_code::invalid_argument,
            "source profile geometry is required"};
    }

    model_decoder_port* decoder = nullptr;
    const auto resolved_decoder =
        _decoder_registry.vqec_vision_ai_detec_mdreg_resolve_decoder(
            _model.decoder_contract_, decoder);
    if (resolved_decoder.code_ != status_code::ok) {
        return resolved_decoder;
    }
    std::unique_ptr<tracker_port> tracker;
    const auto created_tracker =
        _tracker_registry.vqec_vision_ai_track_trreg_create_tracker(
            _tracker_contract, _source.source_id_, _model.model_id_, tracker);
    if (created_tracker.code_ != status_code::ok) {
        return created_tracker;
    }
    try {
        auto candidate = std::unique_ptr<perception_stage_bundle>(
            new perception_stage_bundle(*decoder, std::move(tracker),
                {_source.profile_.width_, _source.profile_.height_}));
        const auto configured = candidate->result_stage_->
            vqec_vision_ai_appl_prstg_configure({_source.camera_id_,
                _source.channel_id_,
                {_source.profile_.width_, _source.profile_.height_}});
        if (configured.code_ != status_code::ok) {
            return configured;
        }
        _bundle = std::move(candidate);
        return {};
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted,
            "perception stage bundle allocation failed"};
    } catch (...) {
        return {status_code::io_error,
            "perception stage bundle construction raised an exception"};
    }
}

}  // namespace vqec::vision::ai
