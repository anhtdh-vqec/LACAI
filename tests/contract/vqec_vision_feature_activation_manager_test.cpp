#include <array>
#include <cassert>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "vqec_vision_feature_activation_manager.hpp"

using namespace vqec::vision::ai;

namespace {

constexpr std::uint64_t g_mib = 1024ULL * 1024ULL;

model_catalog vqec_vision_ai_ctest_famct_make_models() {
    model_catalog models;
    models.schema_version_ = model_catalog_limits::g_schema_version;
    models.revision_ = 3;
    models.catalog_id_ = "models_qcs6490_v1";
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
    models.models_.push_back(std::move(model));
    return models;
}

deployment_config vqec_vision_ai_ctest_famct_make_deployment() {
    deployment_config deployment;
    deployment.schema_version_ = deployment_limits::g_schema_version;
    deployment.revision_ = 9;
    deployment.model_catalog_ref_ = "models_qcs6490_v1";
    deployment.max_total_resident_bytes_ = 1024 * g_mib;
    deployment.max_model_resident_bytes_ = 128 * g_mib;
    source_deployment_config source;
    source.source_id_ = "source.front";
    source.raw_source_ref_ = "fw_raw_front";
    source.camera_id_ = 4;
    source.channel_id_ = 1;
    source.profile_ = {1920, 1080, 25, 1};
    source.memory_ = {4 * g_mib, 1, 0, 16 * g_mib, 4 * g_mib};
    source.model_ids_ = {"person_detector"};
    deployment.sources_.push_back(std::move(source));
    return deployment;
}

feature_catalog vqec_vision_ai_ctest_famct_make_features() {
    feature_catalog features;
    features.schema_version_ = feature_catalog_limits::g_schema_version;
    features.revision_ = 5;
    features.catalog_id_ = "features_qcs6490_v1";
    features.model_catalog_ref_ = "models_qcs6490_v1";
    feature_catalog_entry feature;
    feature.feature_id_ = "counting";
    feature.feature_version_ = "1.0";
    feature.processor_contract_ = "counting.processor.v1";
    feature.configuration_schema_ = "counting.configuration.v1";
    feature.model_dependencies_.push_back({"person_tracks", "person_detector"});
    feature.resources_ = {g_mib, 4, 8, 8};
    features.features_.push_back(std::move(feature));
    return features;
}

class vqec_vision_ai_ctest_famct_processor final : public feature_processor_port {
public:
    explicit vqec_vision_ai_ctest_famct_processor(feature_processor_config _config)
        : config_(std::move(_config)) {}

    [[nodiscard]] status vqec_vision_ai_ports_ftpro_validate_activation(
        const feature_processor_config& _config) const override {
        return _config.config_revision_ == config_.config_revision_ ? status{} :
            status{status_code::invalid_argument, "revision mismatch"};
    }
    [[nodiscard]] status vqec_vision_ai_ports_ftpro_reset_epoch(
        std::uint64_t _source_epoch) override {
        (void)_source_epoch;
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_ftpro_process_observations(
        const observation_batch& _tracked, std::uint64_t _now_monotonic_ns,
        bool _is_source_gap, feature_event_batch& _events) override {
        (void)_tracked;
        (void)_now_monotonic_ns;
        (void)_is_source_gap;
        (void)_events;
        return {};
    }

private:
    feature_processor_config config_;
};

class vqec_vision_ai_ctest_famct_factory final : public feature_processor_factory_port {
public:
    [[nodiscard]] status vqec_vision_ai_ports_ftfac_validate_configuration(
        const feature_catalog_entry& _feature,
        const feature_processor_config& _processor_config,
        const feature_configuration& _configuration) const override {
        if (_feature.processor_contract_ != "counting.processor.v1" ||
            _processor_config.feature_id_ != "counting" ||
            _configuration.payload_ != std::vector<std::uint8_t>{1, 2, 3}) {
            return {status_code::invalid_argument, "fixture configuration rejected"};
        }
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_ftfac_create_processor(
        const feature_catalog_entry& _feature,
        const feature_processor_config& _processor_config,
        const feature_configuration& _configuration,
        std::unique_ptr<feature_processor_port>& _processor) override {
        (void)_feature;
        (void)_configuration;
        _processor = std::make_unique<vqec_vision_ai_ctest_famct_processor>(
            _processor_config);
        return {};
    }
};

feature_activation_request vqec_vision_ai_ctest_famct_make_request(
    bool _desired, bool _entitled, bool _resource) {
    feature_activation_request request;
    request.source_id_ = "source.front";
    request.feature_id_ = "counting";
    request.desired_enabled_ = _desired;
    request.entitlement_granted_ = _entitled;
    request.resource_admitted_ = _resource;
    request.configuration_ = {"counting.configuration.v1", 7, {1, 2, 3}};
    return request;
}

}  // namespace

int main() {
    const auto models = vqec_vision_ai_ctest_famct_make_models();
    const auto deployment = vqec_vision_ai_ctest_famct_make_deployment();
    const auto features = vqec_vision_ai_ctest_famct_make_features();
    feature_activation_manager manager;
    assert(manager.vqec_vision_ai_ftmgr_famgr_configure(
               features, models, deployment).code_ == status_code::ok);
    assert(manager.vqec_vision_ai_ftmgr_famgr_configure(
               features, models, deployment).code_ == status_code::invalid_state);

    feature_processor_registry empty_registry;
    std::array<feature_activation_request,
        feature_activation_limits::g_max_associations> requests{};
    auto request = vqec_vision_ai_ctest_famct_make_request(true, true, true);
    requests[0] = request;
    feature_activation_snapshot snapshot;
    assert(manager.vqec_vision_ai_ftmgr_famgr_reconcile(
               requests, 1, empty_registry, snapshot).code_ == status_code::ok);
    assert(snapshot.unsupported_count_ == 1U &&
           manager.vqec_vision_ai_ftmgr_famgr_get_record(0)->state_ ==
               feature_effective_state::unsupported);

    feature_processor_registry registry;
    vqec_vision_ai_ctest_famct_factory factory;
    assert(registry.vqec_vision_ai_ftmgr_ftreg_register_factory(
               "counting.processor.v1", factory).code_ == status_code::ok);
    requests[0] = vqec_vision_ai_ctest_famct_make_request(true, false, true);
    assert(manager.vqec_vision_ai_ftmgr_famgr_reconcile(
               requests, 1, registry, snapshot).code_ == status_code::ok);
    assert(snapshot.denied_count_ == 1U);
    requests[0] = vqec_vision_ai_ctest_famct_make_request(true, true, false);
    assert(manager.vqec_vision_ai_ftmgr_famgr_reconcile(
               requests, 1, registry, snapshot).code_ == status_code::ok);
    assert(snapshot.resource_limited_count_ == 1U);
    requests[0] = vqec_vision_ai_ctest_famct_make_request(false, true, true);
    assert(manager.vqec_vision_ai_ftmgr_famgr_reconcile(
               requests, 1, registry, snapshot).code_ == status_code::ok);
    assert(manager.vqec_vision_ai_ftmgr_famgr_get_record(0)->state_ ==
           feature_effective_state::disabled);

    requests[0] = vqec_vision_ai_ctest_famct_make_request(true, true, true);
    assert(manager.vqec_vision_ai_ftmgr_famgr_reconcile(
               requests, 1, registry, snapshot).code_ == status_code::ok);
    const auto* ready = manager.vqec_vision_ai_ftmgr_famgr_get_record(0);
    assert(snapshot.ready_count_ == 1U && ready != nullptr &&
           ready->processor_ != nullptr && ready->stage_ != nullptr &&
           manager.vqec_vision_ai_ftmgr_famgr_get_feature(0) != nullptr &&
           manager.vqec_vision_ai_ftmgr_famgr_get_feature(0)->feature_id_ == "counting" &&
           manager.vqec_vision_ai_ftmgr_famgr_get_stage(0)->
               vqec_vision_ai_ftmgr_ftstg_is_active());

    auto malformed = requests;
    malformed[0].source_id_.clear();
    assert(manager.vqec_vision_ai_ftmgr_famgr_reconcile(
               malformed, 1, registry, snapshot).code_ == status_code::invalid_argument);
    assert(manager.vqec_vision_ai_ftmgr_famgr_get_record(0) == ready);

    auto duplicate = requests;
    duplicate[1] = requests[0];
    assert(manager.vqec_vision_ai_ftmgr_famgr_reconcile(
               duplicate, 2, registry, snapshot).code_ == status_code::invalid_argument);
    assert(manager.vqec_vision_ai_ftmgr_famgr_get_count() == 1U);
    return 0;
}
