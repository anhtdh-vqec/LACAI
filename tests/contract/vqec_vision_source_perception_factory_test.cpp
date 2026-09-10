#include <array>
#include <cassert>
#include <cstdint>
#include <memory>
#include <string>

#include "vqec_vision_source_perception_factory.hpp"

using namespace vqec::vision::ai;

namespace {

constexpr std::uint64_t g_mib = 1024ULL * 1024ULL;

class vqec_vision_ai_ctest_spfct_decoder final : public model_decoder_port {
public:
    [[nodiscard]] status vqec_vision_ai_cntr_mddec_validate(
        const model_outputs& _outputs) const override {
        (void)_outputs;
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_cntr_mddec_decode(
        const tensor_result& _result, const preview_frame_key& _expected_frame,
        observation_batch& _observations) override {
        (void)_result;
        _observations.frame_ = _expected_frame;
        return {};
    }
};

class vqec_vision_ai_ctest_spfct_tracker final : public tracker_port {
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

class vqec_vision_ai_ctest_spfct_tracker_factory final : public tracker_factory_port {
public:
    [[nodiscard]] status vqec_vision_ai_track_trfac_validate_activation(
        const std::string& _source_id, const std::string& _model_id) const override {
        return _source_id == "source.front" && !_model_id.empty() ? status{} :
            status{status_code::unsupported, "unsupported fixture binding"};
    }
    [[nodiscard]] status vqec_vision_ai_track_trfac_create_tracker(
        const std::string& _source_id, const std::string& _model_id,
        std::unique_ptr<tracker_port>& _tracker) override {
        (void)_source_id;
        (void)_model_id;
        _tracker = std::make_unique<vqec_vision_ai_ctest_spfct_tracker>();
        return {};
    }
};

model_catalog_entry vqec_vision_ai_ctest_spfct_make_model(
    const std::string& _model_id, char _digest_character) {
    model_catalog_entry model;
    model.model_id_ = _model_id;
    model.model_version_ = "1.0";
    model.target_id_ = "qcs6490";
    model.artifact_ref_ = _model_id + ".artifact";
    model.artifact_sha256_ = std::string(64, _digest_character);
    model.output_manifest_ref_ = _model_id + ".outputs";
    model.decoder_contract_ = "detection.decoder.v1";
    model.preprocess_contract_ = "nv12.rgb.v1";
    model.graph_name_ = _model_id + ".graph";
    model.tensor_width_ = 640;
    model.tensor_height_ = 640;
    model.placement_ = image_placement::centre;
    model.inference_fps_numerator_ = 10;
    model.inference_fps_denominator_ = 1;
    model.source_constraints_ = {640, 480, 4096, 2160, 10, 1};
    model.resources_ = {32 * g_mib, 8 * g_mib, 2, 16, true};
    return model;
}

model_catalog vqec_vision_ai_ctest_spfct_make_models() {
    model_catalog models;
    models.schema_version_ = model_catalog_limits::g_schema_version;
    models.revision_ = 3;
    models.catalog_id_ = "models_qcs6490_v1";
    models.models_ = {
        vqec_vision_ai_ctest_spfct_make_model("person_detector", 'a'),
        vqec_vision_ai_ctest_spfct_make_model("pose_detector", 'b')};
    return models;
}

model_outputs vqec_vision_ai_ctest_spfct_make_outputs(
    const model_catalog_entry& _model) {
    model_outputs outputs;
    outputs.model_id_ = _model.model_id_;
    outputs.model_version_ = _model.model_version_;
    outputs.artifact_sha256_ = _model.artifact_sha256_;
    outputs.decoder_contract_ = _model.decoder_contract_;
    outputs.max_output_bytes_ = 16;
    outputs.outputs_.push_back({"boxes", {1, 4}});
    return outputs;
}

source_deployment_config vqec_vision_ai_ctest_spfct_make_source() {
    source_deployment_config source;
    source.source_id_ = "source.front";
    source.camera_id_ = 4;
    source.channel_id_ = 1;
    source.profile_ = {1920, 1080, 25, 1};
    source.model_ids_ = {"person_detector", "pose_detector"};
    return source;
}

}  // namespace

int main() {
    const auto models = vqec_vision_ai_ctest_spfct_make_models();
    const auto source = vqec_vision_ai_ctest_spfct_make_source();
    vqec_vision_ai_ctest_spfct_decoder decoder;
    model_decoder_registry decoders;
    assert(decoders.vqec_vision_ai_detec_mdreg_register_decoder(
               "detection.decoder.v1", decoder).code_ == status_code::ok);
    vqec_vision_ai_ctest_spfct_tracker_factory tracker_factory;
    tracker_registry trackers;
    assert(trackers.vqec_vision_ai_track_trreg_register_factory(
               "bytetrack.v1", tracker_factory).code_ == status_code::ok);

    std::array<perception_model_activation,
        deployment_limits::g_max_models_per_source> activations{};
    activations[0] = {"person_detector", "bytetrack.v1",
        models.models_[0].output_manifest_ref_,
        vqec_vision_ai_ctest_spfct_make_outputs(models.models_[0])};
    activations[1] = {"pose_detector", "bytetrack.v1",
        models.models_[1].output_manifest_ref_,
        vqec_vision_ai_ctest_spfct_make_outputs(models.models_[1])};
    std::unique_ptr<source_perception_bundle> bundle;
    assert(vqec_vision_ai_appl_spfac_create_source_bundle(models, source,
               activations, 2, decoders, trackers, bundle).code_ == status_code::ok);
    assert(bundle != nullptr &&
           bundle->vqec_vision_ai_appl_spfac_get_model_count() == 2U &&
           bundle->vqec_vision_ai_appl_spfac_get_stage_bundle(0) != nullptr &&
           bundle->vqec_vision_ai_appl_spfac_get_stage_bundle(1) != nullptr &&
           bundle->vqec_vision_ai_appl_spfac_get_result_router()->
               vqec_vision_ai_appl_mmrrt_get_stage_count() == 2U);

    auto* previous = bundle.get();
    auto wrong_order = activations;
    wrong_order[0].model_id_ = "pose_detector";
    assert(vqec_vision_ai_appl_spfac_create_source_bundle(models, source,
               wrong_order, 2, decoders, trackers, bundle).code_ ==
           status_code::invalid_argument);
    assert(bundle.get() == previous);
    auto wrong_manifest = activations;
    wrong_manifest[1].resolved_output_manifest_ref_ = "other.outputs";
    assert(vqec_vision_ai_appl_spfac_create_source_bundle(models, source,
               wrong_manifest, 2, decoders, trackers, bundle).code_ ==
           status_code::invalid_argument);
    assert(bundle.get() == previous);
    auto wrong_digest = activations;
    wrong_digest[1].outputs_.artifact_sha256_ = std::string(64, 'c');
    assert(vqec_vision_ai_appl_spfac_create_source_bundle(models, source,
               wrong_digest, 2, decoders, trackers, bundle).code_ ==
           status_code::invalid_argument);
    assert(bundle.get() == previous);
    auto missing_tracker = activations;
    missing_tracker[1].tracker_contract_ = "missing.v1";
    assert(vqec_vision_ai_appl_spfac_create_source_bundle(models, source,
               missing_tracker, 2, decoders, trackers, bundle).code_ ==
           status_code::unsupported);
    assert(bundle.get() == previous);
    return 0;
}
