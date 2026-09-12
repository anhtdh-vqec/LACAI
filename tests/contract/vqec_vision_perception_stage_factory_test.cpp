#include <cassert>
#include <cstdint>
#include <memory>
#include <string>

#include "vqec_vision_perception_stage_factory.hpp"

using namespace vqec::vision::ai;

namespace {

constexpr std::uint64_t g_mib = 1024ULL * 1024ULL;

class vqec_vision_ai_ctest_psfct_decoder final : public model_decoder_port {
public:
    mutable unsigned validation_count_{0};
    bool reject_outputs_{false};

    [[nodiscard]] status vqec_vision_ai_cntr_mddec_validate(
        const model_outputs& _outputs) const override {
        ++validation_count_;
        return reject_outputs_ ?
            status{status_code::unsupported, "fixture decoder rejected output schema"} :
            (_outputs.outputs_.size() == 1 && _outputs.outputs_[0].name_ == "boxes" ?
                    status{} :
                    status{status_code::invalid_argument,
                        "fixture decoder requires boxes output"});
    }
    [[nodiscard]] status vqec_vision_ai_cntr_mddec_decode(
        const tensor_result& _result, const preview_frame_key& _expected_frame,
        observation_batch& _observations) override {
        (void)_result;
        _observations.frame_ = _expected_frame;
        return {};
    }
};

class vqec_vision_ai_ctest_psfct_tracker final : public tracker_port {
public:
    [[nodiscard]] status vqec_vision_ai_ports_trker_validate_activation() const override {
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_trker_update_tracks(
        const observation_batch& _detections, std::uint64_t _now_monotonic_ns,
        bool _is_source_gap, observation_batch& _tracked) override {
        (void)_now_monotonic_ns;
        (void)_is_source_gap;
        _tracked = _detections;
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_trker_reset_epoch(
        std::uint64_t _source_epoch) override {
        (void)_source_epoch;
        return {};
    }
};

class vqec_vision_ai_ctest_psfct_tracker_factory final : public tracker_factory_port {
public:
    unsigned create_count_{0};

    [[nodiscard]] status vqec_vision_ai_track_trfac_validate_activation(
        const std::string& _source_id, const std::string& _model_id) const override {
        return _source_id == "source.front" && _model_id == "person_detector" ? status{} :
            status{status_code::unsupported, "unsupported fixture binding"};
    }
    [[nodiscard]] status vqec_vision_ai_track_trfac_create_tracker(
        const std::string& _source_id, const std::string& _model_id,
        std::unique_ptr<tracker_port>& _tracker) override {
        (void)_source_id;
        (void)_model_id;
        ++create_count_;
        _tracker = std::make_unique<vqec_vision_ai_ctest_psfct_tracker>();
        return {};
    }
};

model_catalog_entry vqec_vision_ai_ctest_psfct_make_model() {
    model_catalog_entry model;
    model.model_id_ = "person_detector";
    model.model_version_ = "1.0";
    model.target_id_ = "qcs6490";
    model.artifact_ref_ = "person_detector.artifact";
    model.artifact_sha256_ = std::string(64, 'a');
    model.output_manifest_ref_ = "person_detector.outputs";
    model.decoder_contract_ = "person_detector.decoder.v1";
    model.preprocess_contract_ = "nv12.rgb.v1";
    model.graph_name_ = "person_detector.graph";
    model.tensor_width_ = 640;
    model.tensor_height_ = 640;
    model.placement_ = image_placement::centre;
    model.inference_fps_numerator_ = 10;
    model.inference_fps_denominator_ = 1;
    model.source_constraints_ = {640, 480, 4096, 2160, 10, 1};
    model.resources_ = {32 * g_mib, 8 * g_mib, 2, 16, true};
    return model;
}

model_outputs vqec_vision_ai_ctest_psfct_make_outputs(
    const model_catalog_entry& _model) {
    model_outputs outputs;
    outputs.model_id_ = _model.model_id_;
    outputs.model_version_ = _model.model_version_;
    outputs.artifact_sha256_ = _model.artifact_sha256_;
    outputs.decoder_contract_ = _model.decoder_contract_;
    outputs.max_output_bytes_ = 16;
    outputs.outputs_.push_back({"boxes", {1, 4}, tensor_element_type::float32, {}});
    return outputs;
}

source_deployment_config vqec_vision_ai_ctest_psfct_make_source() {
    source_deployment_config source;
    source.source_id_ = "source.front";
    source.camera_id_ = 4;
    source.channel_id_ = 1;
    source.profile_ = {1920, 1080, 25, 1};
    source.model_ids_ = {"person_detector"};
    return source;
}

}  // namespace

int main() {
    vqec_vision_ai_ctest_psfct_decoder decoder;
    model_decoder_registry decoders;
    assert(decoders.vqec_vision_ai_detec_mdreg_register_decoder(
               "person_detector.decoder.v1", decoder).code_ == status_code::ok);
    vqec_vision_ai_ctest_psfct_tracker_factory tracker_factory;
    tracker_registry trackers;
    assert(trackers.vqec_vision_ai_track_trreg_register_factory(
               "bytetrack.v1", tracker_factory).code_ == status_code::ok);

    const auto model = vqec_vision_ai_ctest_psfct_make_model();
    const auto outputs = vqec_vision_ai_ctest_psfct_make_outputs(model);
    const auto source = vqec_vision_ai_ctest_psfct_make_source();
    std::unique_ptr<perception_stage_bundle> bundle;
    assert(vqec_vision_ai_appl_prfac_create_bundle(
               model, source, model.output_manifest_ref_, outputs,
               "bytetrack.v1", decoders, trackers, bundle).code_ ==
           status_code::ok);
    assert(bundle != nullptr && bundle->vqec_vision_ai_appl_prfac_get_result_stage() !=
           nullptr);
    const perception_result_config expected{4, 1, {1920, 1080}};
    assert(bundle->vqec_vision_ai_appl_prfac_get_result_stage()->
               vqec_vision_ai_appl_prstg_validate_config(expected).code_ == status_code::ok);

    auto* previous = bundle.get();
    assert(vqec_vision_ai_appl_prfac_create_bundle(
               model, source, model.output_manifest_ref_, outputs,
               "missing.v1", decoders, trackers, bundle).code_ ==
           status_code::unsupported);
    assert(bundle.get() == previous);
    auto unassigned = source;
    unassigned.model_ids_.clear();
    assert(vqec_vision_ai_appl_prfac_create_bundle(
               model, unassigned, model.output_manifest_ref_, outputs,
               "bytetrack.v1", decoders, trackers, bundle).code_ ==
           status_code::invalid_argument);
    assert(bundle.get() == previous);
    const auto tracker_count = tracker_factory.create_count_;
    const auto validation_count = decoder.validation_count_;
    assert(vqec_vision_ai_appl_prfac_create_bundle(
               model, source, "other.outputs", outputs,
               "bytetrack.v1", decoders, trackers, bundle).code_ ==
           status_code::invalid_argument);
    assert(bundle.get() == previous &&
           decoder.validation_count_ == validation_count &&
           tracker_factory.create_count_ == tracker_count);
    decoder.reject_outputs_ = true;
    assert(vqec_vision_ai_appl_prfac_create_bundle(
               model, source, model.output_manifest_ref_, outputs,
               "bytetrack.v1", decoders, trackers, bundle).code_ ==
           status_code::unsupported);
    assert(bundle.get() == previous &&
           tracker_factory.create_count_ == tracker_count);
    assert(decoder.validation_count_ == validation_count + 1U);
    return 0;
}
