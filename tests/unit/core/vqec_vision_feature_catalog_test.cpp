#include <cassert>
#include <cstdint>
#include <string>

#include "vqec/vision/ai/contracts/vqec_vision_feature_catalog.hpp"

using namespace vqec::vision::ai;

namespace {

constexpr std::uint64_t g_mib = 1024ULL * 1024ULL;

model_catalog_entry vqec_vision_ai_unit_fctst_make_model(
    const std::string& _model_id, char _digest_character) {
    model_catalog_entry model;
    model.model_id_ = _model_id;
    model.model_version_ = "1.0";
    model.target_id_ = "qcs6490";
    model.artifact_ref_ = _model_id + ".artifact";
    model.artifact_sha256_ = std::string(64, _digest_character);
    model.output_manifest_ref_ = _model_id + ".outputs";
    model.decoder_contract_ = _model_id + ".decoder.v1";
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

feature_catalog_entry vqec_vision_ai_unit_fctst_make_feature() {
    feature_catalog_entry feature;
    feature.feature_id_ = "counting";
    feature.feature_version_ = "1.0";
    feature.processor_contract_ = "counting.processor.v1";
    feature.configuration_schema_ = "counting.configuration.v1";
    feature.model_dependencies_.push_back({"person_tracks", "person_detector"});
    feature.resources_ = {g_mib, 4, 8, 8};
    return feature;
}

}  // namespace

int main() {
    model_catalog models;
    models.schema_version_ = model_catalog_limits::g_schema_version;
    models.revision_ = 3;
    models.catalog_id_ = "models_qcs6490_v1";
    models.models_.push_back(
        vqec_vision_ai_unit_fctst_make_model("person_detector", 'a'));

    feature_catalog features;
    features.schema_version_ = feature_catalog_limits::g_schema_version;
    features.revision_ = 5;
    features.catalog_id_ = "features_qcs6490_v1";
    features.model_catalog_ref_ = models.catalog_id_;
    features.features_.push_back(vqec_vision_ai_unit_fctst_make_feature());
    assert(vqec_vision_ai_core_ftcat_validate_catalog(features).code_ == status_code::ok);
    assert(vqec_vision_ai_core_ftcat_validate_model_dependencies(features, models).code_ ==
           status_code::ok);

    feature_processor_config processor_config{"preserved", "preserved", 9, 1, 1, 1};
    assert(vqec_vision_ai_core_ftcat_compose_processor_config(
               features.features_[0], "source.front", 7, processor_config).code_ ==
           status_code::ok);
    assert(processor_config.source_id_ == "source.front" &&
           processor_config.feature_id_ == "counting" &&
           processor_config.config_revision_ == 7 &&
           processor_config.max_events_per_update_ == 4);

    auto invalid_feature = features.features_[0];
    invalid_feature.model_dependencies_.push_back({"pose", "pose_model"});
    assert(vqec_vision_ai_core_ftcat_compose_processor_config(
               invalid_feature, "source.front", 8, processor_config).code_ ==
           status_code::invalid_argument);
    assert(processor_config.source_id_ == "source.front" &&
           processor_config.config_revision_ == 7);

    auto unknown_model = features;
    unknown_model.features_[0].model_dependencies_[0].model_id_ = "missing_model";
    assert(vqec_vision_ai_core_ftcat_validate_model_dependencies(
               unknown_model, models).code_ == status_code::invalid_argument);

    auto wrong_catalog = features;
    wrong_catalog.model_catalog_ref_ = "other_catalog";
    assert(vqec_vision_ai_core_ftcat_validate_model_dependencies(
               wrong_catalog, models).code_ == status_code::invalid_argument);

    auto duplicate_attribute = features;
    duplicate_attribute.features_[0].attribute_dependencies_ = {
        {"person.shirt.color", "1", 1000}, {"person.shirt.color", "2", 1000}};
    assert(vqec_vision_ai_core_ftcat_validate_catalog(duplicate_attribute).code_ ==
           status_code::invalid_argument);

    auto temporal = features;
    temporal.features_[0].input_mode_ = feature_input_mode::temporal_join;
    temporal.features_[0].model_dependencies_.push_back({"pose", "pose_model"});
    models.models_.push_back(vqec_vision_ai_unit_fctst_make_model("pose_model", 'b'));
    assert(vqec_vision_ai_core_ftcat_validate_model_dependencies(temporal, models).code_ ==
           status_code::ok);

    temporal.features_[0].resources_.max_temporal_bytes_per_source_ =
        deployment_limits::g_max_temporal_bytes_per_source + 1;
    assert(vqec_vision_ai_core_ftcat_validate_catalog(temporal).code_ ==
           status_code::invalid_argument);
    return 0;
}
