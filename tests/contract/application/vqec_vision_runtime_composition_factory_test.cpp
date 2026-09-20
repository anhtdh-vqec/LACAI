#include <cassert>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "vqec_vision_runtime_composition_factory.hpp"
#if defined(VQEC_VISION_AI_HAS_SERVICE_ACTIVATION_RECONCILER)
#include "vqec_vision_service_activation_reconciler.hpp"
#endif

using namespace vqec::vision::ai;

namespace {

constexpr std::uint64_t g_mib = 1024ULL * 1024ULL;

class vqec_vision_ai_ctest_rcfct_source final : public raw_source_port {
public:
    [[nodiscard]] status vqec_vision_ai_ports_rawsr_start(int _timeout_ms) override {
        (void)_timeout_ms;
        ++start_count_;
        state_ = raw_source_state::running;
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_rawsr_receive(
        raw_frame& _frame, int _timeout_ms) override {
        (void)_timeout_ms;
        if (emit_frames_) {
            frame_descriptor descriptor;
            descriptor.buffer_id_ = next_buffer_id_++;
            descriptor.session_epoch_ = 1;
            descriptor.width_ = 1920;
            descriptor.height_ = 1080;
            descriptor.offsets_ = {0, 1920U * 1080U};
            descriptor.strides_ = {1920, 1920};
            descriptor.view_size_bytes_ = 1920U * 1080U * 3U / 2U;
            descriptor.allocation_size_bytes_ = descriptor.view_size_bytes_;
            descriptor.pts_ns_ = descriptor.buffer_id_ * 40000000ULL;
            descriptor.dts_ns_ = UINT64_MAX;
            descriptor.duration_ns_ = 40000000ULL;
            _frame.descriptor_ = descriptor;
            _frame.native_handle_ = 7;
            _frame.owner_ = std::make_shared<unsigned>(descriptor.buffer_id_);
            return {};
        }
        return {status_code::pending, "fixture has no frame"};
    }
    [[nodiscard]] status vqec_vision_ai_ports_rawsr_stop(int _timeout_ms) override {
        (void)_timeout_ms;
        state_ = raw_source_state::stopped;
        return {};
    }
    [[nodiscard]] raw_source_state
    vqec_vision_ai_ports_rawsr_get_state() const noexcept override {
        return state_;
    }
    [[nodiscard]] raw_source_profile
    vqec_vision_ai_ports_rawsr_get_profile() const noexcept override {
        return {1920, 1080, 25, 1};
    }
    [[nodiscard]] unsigned
    vqec_vision_ai_ports_rawsr_get_outstanding() const noexcept override {
        return 0;
    }

    raw_source_state state_{raw_source_state::idle};
    unsigned start_count_{0};
    std::uint64_t next_buffer_id_{1};
    bool emit_frames_{false};
};

class vqec_vision_ai_ctest_rcfct_graph final : public inference_graph_port {
public:
    [[nodiscard]] status vqec_vision_ai_ports_infgr_validate_activation()
        const override {
        ++validation_count_;
        return activation_status_;
    }
    [[nodiscard]] status vqec_vision_ai_ports_infgr_configure(
        const inference_plan& _plan) override {
        (void)_plan;
        ++configure_count_;
        state_ = inference_graph_state::configured;
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_infgr_load() override {
        ++load_count_;
        state_ = inference_graph_state::ready;
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_infgr_poll_state() override {
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_infgr_bind_source(
        const source_binding& _binding) override {
        (void)_binding;
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_infgr_start(
        const std::vector<tensor_spec>& _outputs,
        std::uint64_t _max_output_bytes) override {
        (void)_outputs;
        (void)_max_output_bytes;
        ++graph_start_count_;
        state_ = inference_graph_state::running;
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_infgr_arm(
        std::uint64_t _cycle_id, std::uint64_t _source_epoch,
        std::uint64_t _job_timeout_ns, vqec::vision::ai::submission_sequence_policy) override {
        (void)_cycle_id;
        (void)_source_epoch;
        (void)_job_timeout_ns;
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_infgr_submit_frame(
        const raw_frame& _frame, std::uint64_t _steady_now_ns,
        submission_ticket& _ticket) override {
        (void)_frame;
        (void)_steady_now_ns;
        (void)_ticket;
        return {status_code::pending, "fixture does not submit"};
    }
    [[nodiscard]] status vqec_vision_ai_ports_infgr_poll_result(
        std::uint64_t _steady_now_ns, tensor_result& _result) override {
        (void)_steady_now_ns;
        (void)_result;
        return {status_code::pending, "fixture has no result"};
    }
    [[nodiscard]] status vqec_vision_ai_ports_infgr_request_drain() override {
        ++drain_count_;
        state_ = inference_graph_state::drained;
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_infgr_unload() override {
        ++unload_count_;
        state_ = inference_graph_state::configured;
        return {};
    }
    [[nodiscard]] inference_graph_state
    vqec_vision_ai_ports_infgr_get_state() const noexcept override {
        return state_;
    }
    [[nodiscard]] unsigned
    vqec_vision_ai_ports_infgr_get_outstanding() const noexcept override {
        return 0;
    }
    [[nodiscard]] submission_ticket
    vqec_vision_ai_ports_infgr_get_pending_ticket() const noexcept override {
        return {};
    }

    status activation_status_;
    inference_graph_state state_{inference_graph_state::empty};
    mutable unsigned validation_count_{0};
    unsigned configure_count_{0};
    unsigned load_count_{0};
    unsigned graph_start_count_{0};
    unsigned drain_count_{0};
    unsigned unload_count_{0};
};

#if defined(VQEC_VISION_AI_HAS_SERVICE_ACTIVATION_RECONCILER)
class vqec_vision_ai_ctest_rcfct_feature final : public feature_processor_port {
public:
    explicit vqec_vision_ai_ctest_rcfct_feature(feature_processor_config _config)
        : config_(std::move(_config)) {}

    [[nodiscard]] status vqec_vision_ai_ports_ftpro_validate_activation(
        const feature_processor_config& _config) const override {
        return _config.feature_id_ == config_.feature_id_ ? status{} :
            status{status_code::invalid_argument, "feature identity changed"};
    }
    [[nodiscard]] status vqec_vision_ai_ports_ftpro_reset_epoch(
        std::uint64_t _source_epoch) override {
        epoch_ = _source_epoch;
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_ftpro_process_observations(
        const observation_batch& _tracked, std::uint64_t _now_monotonic_ns,
        bool _is_source_gap, feature_event_batch& _events) override {
        (void)_now_monotonic_ns;
        (void)_is_source_gap;
        _events.frame_ = _tracked.frame_;
        _events.geometry_ = _tracked.geometry_;
        return _tracked.frame_.source_epoch_ == epoch_ ? status{} :
            status{status_code::invalid_state, "feature epoch changed"};
    }

private:
    feature_processor_config config_;
    std::uint64_t epoch_{0};
};

class vqec_vision_ai_ctest_rcfct_feature_factory final :
    public feature_processor_factory_port {
public:
    [[nodiscard]] status vqec_vision_ai_ports_ftfac_validate_configuration(
        const feature_catalog_entry& _feature,
        const feature_processor_config& _processor_config,
        const feature_configuration& _configuration) const override {
        return _feature.feature_id_ == _processor_config.feature_id_ &&
                _feature.configuration_schema_ == _configuration.schema_id_ ? status{} :
            status{status_code::invalid_argument, "feature configuration changed"};
    }
    [[nodiscard]] status vqec_vision_ai_ports_ftfac_create_processor(
        const feature_catalog_entry& _feature,
        const feature_processor_config& _processor_config,
        const feature_configuration& _configuration,
        std::unique_ptr<feature_processor_port>& _processor) override {
        (void)_feature;
        (void)_configuration;
        _processor = std::make_unique<vqec_vision_ai_ctest_rcfct_feature>(
            _processor_config);
        return {};
    }
};
#endif

class vqec_vision_ai_ctest_rcfct_decoder final : public model_decoder_port {
public:
    [[nodiscard]] status vqec_vision_ai_cntr_mddec_validate(
        const model_outputs& _outputs) const override {
        return _outputs.outputs_.size() == 1 &&
                _outputs.outputs_[0].name_ == "boxes" ?
            status{} : status{status_code::unsupported,
                "fixture decoder requires boxes"};
    }
    [[nodiscard]] status vqec_vision_ai_cntr_mddec_decode(
        const tensor_result& _result, const preview_frame_key& _expected_frame,
        observation_batch& _observations) override {
        (void)_result;
        _observations.frame_ = _expected_frame;
        return {};
    }
};

class vqec_vision_ai_ctest_rcfct_tracker final : public tracker_port {
public:
    [[nodiscard]] status vqec_vision_ai_ports_trker_validate_activation()
        const override {
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

class vqec_vision_ai_ctest_rcfct_tracker_factory final : public tracker_factory_port {
public:
    [[nodiscard]] status vqec_vision_ai_track_trfac_validate_activation(
        const std::string& _source_id, const std::string& _model_id) const override {
        return !_source_id.empty() &&
                (_model_id == "detector" || _model_id == "detector_b") ? status{} :
            status{status_code::unsupported, "fixture tracker binding rejected"};
    }
    [[nodiscard]] status vqec_vision_ai_track_trfac_create_tracker(
        const std::string& _source_id, const std::string& _model_id,
        std::unique_ptr<tracker_port>& _tracker) override {
        (void)_source_id;
        (void)_model_id;
        ++create_count_;
        _tracker = std::make_unique<vqec_vision_ai_ctest_rcfct_tracker>();
        return {};
    }

    unsigned create_count_{0};
};

model_catalog_entry vqec_vision_ai_ctest_rcfct_make_model() {
    model_catalog_entry model;
    model.model_id_ = "detector";
    model.model_version_ = "1.0";
    model.target_id_ = "qcs6490";
    model.artifact_ref_ = "detector.artifact";
    model.artifact_sha256_ = std::string(64, 'a');
    model.output_manifest_ref_ = "detector.outputs";
    model.decoder_contract_ = "detector.decoder.v1";
    model.preprocess_contract_ = "nv12.rgb.v1";
    model.graph_name_ = "detector.graph";
    model.tensor_width_ = 640;
    model.tensor_height_ = 640;
    model.placement_ = image_placement::centre;
    model.inference_fps_numerator_ = 10;
    model.inference_fps_denominator_ = 1;
    model.source_constraints_ = {640, 480, 4096, 2160, 10, 1};
    model.resources_ = {32 * g_mib, 8 * g_mib, 2, 2, true};
    return model;
}

model_catalog vqec_vision_ai_ctest_rcfct_make_catalog() {
    model_catalog catalog;
    catalog.schema_version_ = model_catalog_limits::g_schema_version;
    catalog.revision_ = 7;
    catalog.catalog_id_ = "models_qcs6490_v1";
    catalog.models_.push_back(vqec_vision_ai_ctest_rcfct_make_model());
    return catalog;
}

deployment_config vqec_vision_ai_ctest_rcfct_make_deployment() {
    deployment_config deployment;
    deployment.schema_version_ = deployment_limits::g_schema_version;
    deployment.revision_ = 9;
    deployment.model_catalog_ref_ = "models_qcs6490_v1";
    deployment.max_total_resident_bytes_ = 512 * g_mib;
    deployment.max_model_resident_bytes_ = 64 * g_mib;
    for (unsigned index = 0; index < 2; ++index) {
        source_deployment_config source;
        source.source_id_ = "source_" + std::to_string(index);
        source.raw_source_ref_ = "fw_raw_" + std::to_string(index);
        source.camera_id_ = index;
        source.profile_ = {1920, 1080, 25, 1};
        source.memory_.max_frame_allocation_bytes_ = 4 * g_mib;
        source.memory_.max_inflight_frames_ = 1;
        source.memory_.max_tensor_bytes_ = 8 * g_mib;
        source.memory_.max_temporal_bytes_ = g_mib;
        source.model_ids_.push_back("detector");
        deployment.sources_.push_back(std::move(source));
    }
    return deployment;
}

runtime_model_activation vqec_vision_ai_ctest_rcfct_make_model_activation(
    const model_catalog_entry& _model, inference_graph_port& _graph,
    std::uint64_t _cycle_id) {
    runtime_model_activation activation;
    activation.model_id_ = _model.model_id_;
    activation.graph_ = &_graph;
    activation.paths_.model_id_ = _model.model_id_;
    activation.paths_.target_id_ = _model.target_id_;
    activation.paths_.artifact_ref_ = _model.artifact_ref_;
    activation.paths_.model_path_ = "/opt/vqec/models/detector.bin";
    activation.paths_.backend_path_ = "/usr/lib/libQnnHtp.so";
    activation.paths_.system_path_ = "/usr/lib/libQnnSystem.so";
    activation.resolved_output_manifest_ref_ = _model.output_manifest_ref_;
    activation.outputs_.model_id_ = _model.model_id_;
    activation.outputs_.model_version_ = _model.model_version_;
    activation.outputs_.artifact_sha256_ = _model.artifact_sha256_;
    activation.outputs_.decoder_contract_ = _model.decoder_contract_;
    activation.outputs_.max_output_bytes_ = 16;
    activation.outputs_.outputs_.push_back({"boxes", {1, 4}, tensor_element_type::float32, {}});
    activation.tracker_contract_ = "bytetrack.v1";
    activation.binding_.width_ = 1920;
    activation.binding_.height_ = 1080;
    activation.binding_.fps_numerator_ = 25;
    activation.binding_.fps_denominator_ = 1;
    activation.binding_.memory_kind_ = source_memory_kind::dmabuf;
    activation.binding_.layout_ = source_memory_layout::linear_nv12;
    activation.binding_.sync_mode_ = source_sync_mode::implicit_ready;
    activation.binding_.color_profile_ = source_color_profile::bt709_limited;
    activation.binding_.chroma_site_ = source_chroma_site::mpeg2;
    activation.binding_.fw_memory_contract_ = "fw.dmabuf.v1";
    activation.binding_.backend_memory_contract_ = "qcom.dmabuf.v1";
    activation.binding_.preprocess_contract_ = _model.preprocess_contract_;
    activation.cycle_id_ = _cycle_id;
    activation.job_timeout_ns_ = 1000000000;
    return activation;
}

runtime_composition_activation vqec_vision_ai_ctest_rcfct_make_activation(
    const model_catalog_entry& _model,
    vqec_vision_ai_ctest_rcfct_source& _first_source,
    vqec_vision_ai_ctest_rcfct_source& _second_source,
    vqec_vision_ai_ctest_rcfct_graph& _first_graph,
    vqec_vision_ai_ctest_rcfct_graph& _second_graph) {
    runtime_composition_activation activation;
    activation.hardware_profile_.profile_id_ = "contract_fixture";
    activation.hardware_profile_.target_id_ = "qcs6490";
    activation.hardware_profile_.measurement_reference_ = "contract-test-fixture";
    activation.hardware_profile_.revision_ = 1;
    activation.hardware_profile_.max_total_resident_bytes_ = 4ULL * 1024 * 1024 * 1024;
    activation.hardware_profile_.max_frame_pool_bytes_ = 1024ULL * 1024 * 1024;
    activation.hardware_profile_.max_tensor_pool_bytes_ = 1024ULL * 1024 * 1024;
    activation.hardware_profile_.max_encoder_pool_bytes_ = 512ULL * 1024 * 1024;
    activation.hardware_profile_.max_cascade_roi_bytes_ = 512ULL * 1024 * 1024;
    activation.hardware_profile_.max_ddr_bandwidth_mbps_ = 12000;
    activation.hardware_profile_.max_fw_concurrency_slots_ = 16;
    activation.hardware_profile_.max_worker_concurrency_ = 64;
    activation.hardware_profile_.min_thermal_headroom_pct_ = 10;
    activation.source_count_ = 2;
    activation.startup_timeout_ns_ = 30000000000ULL;
    activation.stop_timeout_ns_ = 10000000000ULL;
    activation.rpc_timeout_ms_ = 1000;
    activation.sources_[0].source_id_ = "source_0";
    activation.sources_[0].source_ = &_first_source;
    activation.sources_[0].model_count_ = 1;
    activation.sources_[0].models_[0] =
        vqec_vision_ai_ctest_rcfct_make_model_activation(
            _model, _first_graph, 101);
    activation.sources_[1].source_id_ = "source_1";
    activation.sources_[1].source_ = &_second_source;
    activation.sources_[1].model_count_ = 1;
    activation.sources_[1].models_[0] =
        vqec_vision_ai_ctest_rcfct_make_model_activation(
            _model, _second_graph, 102);
    return activation;
}

#if defined(VQEC_VISION_AI_HAS_SERVICE_ACTIVATION_RECONCILER)
feature_catalog vqec_vision_ai_ctest_rcfct_make_features() {
    feature_catalog features;
    features.schema_version_ = feature_catalog_limits::g_schema_version;
    features.catalog_id_ = "activation_features";
    features.model_catalog_ref_ = "models_qcs6490_v1";
    features.revision_ = 5;
    for (unsigned index = 0; index < 2; ++index) {
        feature_catalog_entry feature;
        feature.feature_id_ = "fixture_feature_" + std::to_string(index + 1U);
        feature.feature_version_ = "1.0";
        feature.processor_contract_ = "fixture_processor";
        feature.configuration_schema_ = "fixture_configuration";
        feature.model_dependencies_.push_back({"root", "detector"});
        feature.resources_ = {4096, 4, 4, 4};
        features.features_.push_back(std::move(feature));
    }
    return features;
}

usecase_catalog vqec_vision_ai_ctest_rcfct_make_usecases() {
    usecase_catalog usecases;
    usecases.schema_version_ = usecase_activation_limits::g_schema_version;
    usecases.catalog_id_ = "activation_usecases";
    usecases.model_catalog_ref_ = "models_qcs6490_v1";
    usecases.revision_ = 6;
    usecases.usecases_.push_back(
        {"fixture_app_1", "1.0", {"detector"}, {"fixture_feature_1"}});
    usecases.usecases_.push_back(
        {"fixture_app_2", "1.0", {"detector"}, {"fixture_feature_2"}});
    return usecases;
}

app_runtime_association vqec_vision_ai_ctest_rcfct_make_app(
    const std::string& _source_id, unsigned _app_index,
    std::uint64_t _configuration_revision, const std::string& _model_id) {
    app_runtime_component component;
    component.component_id_ = _model_id;
    component.component_version_ = "1.0";
    component.type_ = app_component_type::model;
    component.target_id_ = "qcs6490";
    component.artifact_sha256_ = std::string(
        64, _model_id == "detector" ? 'a' : 'b');
    component.artifact_bytes_ = 1024;
    component.semantic_contract_sha256_ = std::string(64, 'b');
    component.model_role_ = app_model_role::primary;
    component.immutable_location_ = "/opt/lacai/fixture/" + _model_id;

    app_runtime_association association;
    association.app_id_ = "fixture_app_" + std::to_string(_app_index);
    association.source_id_ = _source_id;
    association.app_version_ = "1.0";
    association.release_sequence_ = 1;
    association.installed_ = true;
    association.entitled_ = true;
    association.desired_ = true;
    association.supported_ = true;
    association.compatible_ = true;
    association.admitted_ = true;
    association.configuration_revision_ = _configuration_revision;
    association.configuration_sha256_ = std::string(
        64, _configuration_revision == 1 ? 'c' : 'd');
    association.configuration_schema_id_ = "fixture_configuration";
    association.configuration_payload_ = {
        static_cast<std::uint8_t>(_configuration_revision)};
    association.output_scopes_ = {"fixture_attribute"};
    association.components_.push_back(std::move(component));
    association.entitlement_expires_utc_ns_ = UINT64_MAX - 1U;
    return association;
}

runtime_control_snapshot vqec_vision_ai_ctest_rcfct_make_runtime(
    std::uint64_t _revision) {
    runtime_control_snapshot runtime;
    runtime.schema_version_ = app_lifecycle_limits::g_schema_version;
    runtime.snapshot_revision_ = _revision;
    runtime.inventory_revision_ = 1;
    runtime.entitlement_revision_ = 1;
    runtime.desired_revision_ = _revision;
    for (unsigned source = 0; source < 2; ++source) {
        for (unsigned app = 1; app <= 2; ++app) {
            runtime.associations_.push_back(vqec_vision_ai_ctest_rcfct_make_app(
                "source_" + std::to_string(source), app, 1, "detector"));
        }
    }
    return runtime;
}

service_startup_resolution vqec_vision_ai_ctest_rcfct_make_startup(
    const deployment_config& _deployment, const model_catalog& _catalog,
    const feature_catalog& _features, const usecase_catalog& _usecases,
    const runtime_control_snapshot& _runtime) {
    service_startup_resolution startup;
    startup.catalog = _catalog;
    startup.base_deployment = _deployment;
    startup.deployment = _deployment;
    startup.features = _features;
    startup.runtime_control = _runtime;
    startup.has_runtime_control = true;
    startup.has_usecase_control = true;
    startup.usecase_control.catalog_ = _usecases;
    startup.usecase_control.control_revision_ = _runtime.snapshot_revision_;
    startup.usecase_control.entitlement_revision_ = _runtime.entitlement_revision_;
    startup.usecase_control.deployment_revision_ = _deployment.revision_;
    for (const auto& association : _runtime.associations_) {
        startup.usecase_control.requests_.push_back({association.source_id_,
            association.app_id_, association.desired_, association.installed_,
            association.entitled_, association.supported_, association.compatible_,
            association.admitted_});
    }
    const auto composed = vqec_vision_ai_core_ucact_compose_effective_deployment(
        startup.base_deployment, startup.catalog, startup.usecase_control.catalog_,
        startup.usecase_control.requests_, startup.usecase_activation,
        startup.active_deployment);
    assert(composed.code_ == status_code::ok);
    startup.usecase_activation.policy_revision_ = _runtime.snapshot_revision_;
    startup.usecase_activation.config_revision_ = _runtime.snapshot_revision_;
    assert(vqec_vision_ai_core_acdel_build_plan(
               startup.deployment, startup.catalog, startup.features,
               startup.usecase_control.catalog_, startup.runtime_control,
               startup.activation_plan).code_ == status_code::ok);
    return startup;
}
#endif

void vqec_vision_ai_ctest_rcfct_advance_to_running(
    application_composition& _composition,
    runtime_composition_bundle& _bundle, std::uint64_t& _now) {
    for (unsigned attempt = 0; attempt < 64; ++attempt) {
        const auto stepped = _composition.vqec_vision_ai_cntr_acomp_step(++_now);
        assert(stepped.code_ == status_code::ok ||
               stepped.code_ == status_code::pending);
        bool running = true;
        for (std::uint16_t source = 0;
             source < _bundle.vqec_vision_ai_appl_rcfac_get_source_count(); ++source) {
            const auto snapshot = _bundle.vqec_vision_ai_appl_rcfac_get_session(source)->
                vqec_vision_ai_appl_mmses_get_snapshot();
            running = running && snapshot.session_state_ ==
                    multi_model_session_state::running &&
                snapshot.delta_phase_ == multi_model_delta_phase::idle;
        }
        if (running) {
            return;
        }
    }
    assert(false && "composition did not reach running state");
}

void vqec_vision_ai_ctest_rcfct_advance_delta(
    application_composition& _composition,
    runtime_composition_bundle& _bundle, std::uint16_t _expected_mask,
    std::uint64_t& _now) {
    for (unsigned attempt = 0; attempt < 64; ++attempt) {
        const auto stepped = _composition.vqec_vision_ai_cntr_acomp_step(++_now);
        assert(stepped.code_ == status_code::ok ||
               stepped.code_ == status_code::pending);
        bool complete = true;
        for (std::uint16_t source = 0;
             source < _bundle.vqec_vision_ai_appl_rcfac_get_source_count(); ++source) {
            const auto snapshot = _bundle.vqec_vision_ai_appl_rcfac_get_session(source)->
                vqec_vision_ai_appl_mmses_get_snapshot();
            complete = complete && snapshot.active_model_mask_ == _expected_mask &&
                snapshot.desired_model_mask_ == _expected_mask &&
                snapshot.delta_phase_ == multi_model_delta_phase::idle;
        }
        if (complete) {
            return;
        }
    }
    assert(false && "model activation delta did not complete");
}

}  // namespace

int main() {
    const auto catalog = vqec_vision_ai_ctest_rcfct_make_catalog();
    const auto deployment = vqec_vision_ai_ctest_rcfct_make_deployment();
    vqec_vision_ai_ctest_rcfct_source first_source;
    vqec_vision_ai_ctest_rcfct_source second_source;
    vqec_vision_ai_ctest_rcfct_graph first_graph;
    vqec_vision_ai_ctest_rcfct_graph second_graph;
    auto activation = vqec_vision_ai_ctest_rcfct_make_activation(
        catalog.models_[0], first_source, second_source, first_graph, second_graph);

    vqec_vision_ai_ctest_rcfct_decoder decoder;
    model_decoder_registry decoders;
    assert(decoders.vqec_vision_ai_detec_mdreg_register_decoder(
               "detector.decoder.v1", decoder).code_ == status_code::ok);
    vqec_vision_ai_ctest_rcfct_tracker_factory tracker_factory;
    tracker_registry trackers;
    assert(trackers.vqec_vision_ai_track_trreg_register_factory(
               "bytetrack.v1", tracker_factory).code_ == status_code::ok);

    std::unique_ptr<runtime_composition_bundle> bundle;
    auto missing_profile = activation;
    missing_profile.hardware_profile_ = {};
    assert(vqec_vision_ai_appl_rcfac_create_bundle(deployment, catalog,
               missing_profile, decoders, trackers, bundle).code_ == status_code::unsupported);
    assert(bundle == nullptr && tracker_factory.create_count_ == 0);
    assert(vqec_vision_ai_appl_rcfac_create_bundle(deployment, catalog,
               activation, decoders, trackers, bundle).code_ == status_code::ok);
    assert(bundle != nullptr &&
           bundle->vqec_vision_ai_appl_rcfac_get_source_count() == 2 &&
           bundle->vqec_vision_ai_appl_rcfac_get_session(0) != nullptr &&
           bundle->vqec_vision_ai_appl_rcfac_get_session(1) != nullptr &&
           bundle->vqec_vision_ai_appl_rcfac_get_perception(0) != nullptr &&
           bundle->vqec_vision_ai_appl_rcfac_get_composition() != nullptr);
    const auto& admission = bundle->vqec_vision_ai_appl_rcfac_get_admission();
    assert(admission.deployment_revision_ == 9 &&
           admission.catalog_revision_ == 7 && admission.source_count_ == 2);
    const auto composition = bundle->vqec_vision_ai_appl_rcfac_get_composition()->
        vqec_vision_ai_cntr_acomp_get_snapshot();
    assert(composition.state_ == application_composition_state::validating &&
           composition.declared_sources_ == 2 && composition.admitted_sources_ == 0);
    assert(first_source.start_count_ == 0 && second_source.start_count_ == 0 &&
           first_graph.configure_count_ == 0 && second_graph.configure_count_ == 0 &&
           tracker_factory.create_count_ == 2);

    runtime_feature_activation empty_features;
    empty_features.deployment_revision_ = deployment.revision_;
    empty_features.catalog_revision_ = catalog.revision_;
    empty_features.source_count_ = 2;
    assert(bundle->vqec_vision_ai_appl_rcfac_rebind_features(
               &empty_features).code_ == status_code::ok);
    auto stale_features = empty_features;
    ++stale_features.catalog_revision_;
    assert(bundle->vqec_vision_ai_appl_rcfac_rebind_features(
               &stale_features).code_ == status_code::invalid_argument);
    assert(bundle->vqec_vision_ai_appl_rcfac_rebind_features(
               &empty_features).code_ == status_code::ok);

    auto* previous = bundle.get();
    const auto tracker_count = tracker_factory.create_count_;
    auto duplicate_source = activation;
    duplicate_source.sources_[1].source_ = &first_source;
    assert(vqec_vision_ai_appl_rcfac_create_bundle(deployment, catalog,
               duplicate_source, decoders, trackers, bundle).code_ ==
           status_code::invalid_argument);
    assert(bundle.get() == previous && tracker_factory.create_count_ == tracker_count);
    assert(first_source.start_count_ == 0 && second_source.start_count_ == 0 &&
           first_graph.configure_count_ == 0 && second_graph.configure_count_ == 0);

    auto wrong_preprocess = activation;
    wrong_preprocess.sources_[0].models_[0].binding_.preprocess_contract_ =
        "other.preprocess.v1";
    assert(vqec_vision_ai_appl_rcfac_create_bundle(deployment, catalog,
               wrong_preprocess, decoders, trackers, bundle).code_ ==
           status_code::invalid_argument);
    assert(bundle.get() == previous && tracker_factory.create_count_ == tracker_count);

    auto duplicate_cycle = activation;
    duplicate_cycle.sources_[1].models_[0].cycle_id_ = 101;
    assert(vqec_vision_ai_appl_rcfac_create_bundle(deployment, catalog,
               duplicate_cycle, decoders, trackers, bundle).code_ ==
           status_code::invalid_argument);
    assert(bundle.get() == previous && tracker_factory.create_count_ == tracker_count);

    auto duplicate_graph = activation;
    duplicate_graph.sources_[1].models_[0].graph_ = &first_graph;
    assert(vqec_vision_ai_appl_rcfac_create_bundle(deployment, catalog,
               duplicate_graph, decoders, trackers, bundle).code_ ==
           status_code::invalid_argument);
    assert(bundle.get() == previous && tracker_factory.create_count_ == tracker_count);

    auto wrong_manifest = activation;
    wrong_manifest.sources_[1].models_[0].outputs_.artifact_sha256_ =
        std::string(64, 'b');
    assert(vqec_vision_ai_appl_rcfac_create_bundle(deployment, catalog,
               wrong_manifest, decoders, trackers, bundle).code_ ==
           status_code::invalid_argument);
    assert(bundle.get() == previous && tracker_factory.create_count_ == tracker_count);

    second_source.state_ = raw_source_state::running;
    assert(vqec_vision_ai_appl_rcfac_create_bundle(deployment, catalog,
               activation, decoders, trackers, bundle).code_ ==
           status_code::invalid_state);
    assert(bundle.get() == previous && tracker_factory.create_count_ == tracker_count);
    second_source.state_ = raw_source_state::idle;

    second_graph.activation_status_ =
        {status_code::resource_exhausted, "fixture graph unavailable"};
    assert(vqec_vision_ai_appl_rcfac_create_bundle(deployment, catalog,
               activation, decoders, trackers, bundle).code_ ==
           status_code::resource_exhausted);
    assert(bundle.get() == previous && tracker_factory.create_count_ == tracker_count);

    second_graph.activation_status_ = {};
    auto* application = bundle->vqec_vision_ai_appl_rcfac_get_composition();
    assert(application->vqec_vision_ai_cntr_acomp_activate().code_ == status_code::ok);
    assert(application->vqec_vision_ai_cntr_acomp_step(1).code_ ==
           status_code::pending);
    assert(first_source.start_count_ == 0 && first_graph.configure_count_ == 0);
    assert(application->vqec_vision_ai_cntr_acomp_request_stop(1).code_ ==
           status_code::pending);
    bool reached_stopped = false;
    for (std::uint64_t now = 2; now <= 8 && !reached_stopped; ++now) {
        const auto stopped = application->vqec_vision_ai_cntr_acomp_step(now);
        assert(stopped.code_ == status_code::ok ||
               stopped.code_ == status_code::pending);
        reached_stopped = application->vqec_vision_ai_cntr_acomp_get_snapshot().state_ ==
            application_composition_state::stopped;
    }
    assert(reached_stopped);
    assert(first_source.start_count_ == 0 && second_source.start_count_ == 0 &&
           first_graph.configure_count_ == 0 && second_graph.configure_count_ == 0);

    // Cascade: a secondary catalog model that depends on detector makes detector a cascade
    // root. The source must declare a cascade budget and the session must own a store.
    {
        auto cascade_catalog = vqec_vision_ai_ctest_rcfct_make_catalog();
        model_catalog_entry embedding = vqec_vision_ai_ctest_rcfct_make_model();
        embedding.model_id_ = "embedding";
        embedding.artifact_ref_ = "embedding.artifact";
        embedding.output_manifest_ref_ = "embedding.outputs";
        embedding.decoder_contract_ = "embedding.decoder.v1";
        embedding.graph_name_ = "embedding.graph";
        embedding.tensor_width_ = 112;
        embedding.tensor_height_ = 112;
        embedding.role_ = model_role::secondary;
        embedding.depends_on_ = {{"detector", "1.0", "qcs6490"}};
        cascade_catalog.models_.push_back(embedding);

        auto cascade_deployment = vqec_vision_ai_ctest_rcfct_make_deployment();
        for (auto& source : cascade_deployment.sources_) {
            source.cascade_ = {2, 4, 4096};
            // Admission charges both the primary and dependency-activated secondary
            // tensor envelopes before runtime composition checks the cascade owner.
            source.memory_.max_tensor_bytes_ = 16 * g_mib;
        }
        {
            auto missing_budget = cascade_deployment;
            missing_budget.sources_[0].cascade_ = {};
            vqec_vision_ai_ctest_rcfct_source a;
            vqec_vision_ai_ctest_rcfct_source b;
            vqec_vision_ai_ctest_rcfct_graph c;
            vqec_vision_ai_ctest_rcfct_graph d;
            const auto activation = vqec_vision_ai_ctest_rcfct_make_activation(
                cascade_catalog.models_[0], a, b, c, d);
            std::unique_ptr<runtime_composition_bundle> rejected;
            assert(vqec_vision_ai_appl_rcfac_create_bundle(missing_budget, cascade_catalog,
                       activation, decoders, trackers, rejected).code_ ==
                   status_code::invalid_argument);
        }
        vqec_vision_ai_ctest_rcfct_source a;
        vqec_vision_ai_ctest_rcfct_source b;
        vqec_vision_ai_ctest_rcfct_graph c;
        vqec_vision_ai_ctest_rcfct_graph d;
        const auto activation = vqec_vision_ai_ctest_rcfct_make_activation(
            cascade_catalog.models_[0], a, b, c, d);
        std::unique_ptr<runtime_composition_bundle> cascade_bundle;
        assert(vqec_vision_ai_appl_rcfac_create_bundle(cascade_deployment, cascade_catalog,
                   activation, decoders, trackers, cascade_bundle).code_ == status_code::ok);
        runtime_executor_report cascade_report;
        assert(cascade_bundle->vqec_vision_ai_appl_rcfac_get_executor()
                   ->vqec_vision_ai_appl_rtexe_step(0, cascade_report).code_ ==
               status_code::invalid_state);
        auto* cascade_session = cascade_bundle->vqec_vision_ai_appl_rcfac_get_session(0);
        assert(cascade_session != nullptr);
        tensor_result cascade_result;
        source_session_progress cascade_progress;
        assert(cascade_session->vqec_vision_ai_appl_mmses_step(
                   0, cascade_result, cascade_progress).code_ == status_code::pending);
        assert(cascade_session->vqec_vision_ai_appl_mmses_has_cascade_store());
    }

    // Composition admits only the active subset while retaining a second neutral owner slot.
    // The inactive graph must not configure/load before an explicit delta request.
    {
        auto capacity_catalog = vqec_vision_ai_ctest_rcfct_make_catalog();
        auto detector_b = vqec_vision_ai_ctest_rcfct_make_model();
        detector_b.model_id_ = "detector_b";
        detector_b.artifact_ref_ = "detector_b.artifact";
        detector_b.artifact_sha256_ = std::string(64, 'b');
        detector_b.output_manifest_ref_ = "detector_b.outputs";
        detector_b.graph_name_ = "detector_b.graph";
        capacity_catalog.models_.push_back(detector_b);
        auto capacity_deployment = vqec_vision_ai_ctest_rcfct_make_deployment();
        for (auto& source : capacity_deployment.sources_) {
            source.model_ids_.push_back("detector_b");
            source.memory_.max_tensor_bytes_ = 16 * g_mib;
        }
        vqec_vision_ai_ctest_rcfct_source source_a;
        vqec_vision_ai_ctest_rcfct_source source_b;
        source_a.emit_frames_ = true;
        source_b.emit_frames_ = true;
        vqec_vision_ai_ctest_rcfct_graph graph_a0;
        vqec_vision_ai_ctest_rcfct_graph graph_a1;
        vqec_vision_ai_ctest_rcfct_graph graph_b0;
        vqec_vision_ai_ctest_rcfct_graph graph_b1;
        auto capacity_activation = vqec_vision_ai_ctest_rcfct_make_activation(
            capacity_catalog.models_[0], source_a, source_b, graph_a0, graph_b0);
        capacity_activation.sources_[0].model_count_ = 2;
        capacity_activation.sources_[0].models_[1] =
            vqec_vision_ai_ctest_rcfct_make_model_activation(
                capacity_catalog.models_[1], graph_a1, 103);
        capacity_activation.sources_[0].initial_active_model_mask_ = 1;
        capacity_activation.sources_[1].model_count_ = 2;
        capacity_activation.sources_[1].models_[1] =
            vqec_vision_ai_ctest_rcfct_make_model_activation(
                capacity_catalog.models_[1], graph_b1, 104);
        capacity_activation.sources_[1].initial_active_model_mask_ = 1;

        std::unique_ptr<runtime_composition_bundle> capacity_bundle;
        assert(vqec_vision_ai_appl_rcfac_create_bundle(
                   capacity_deployment, capacity_catalog, capacity_activation,
                   decoders, trackers, capacity_bundle).code_ == status_code::ok);
        assert(capacity_bundle->vqec_vision_ai_appl_rcfac_get_admission().
                   active_model_count_ == 1);
        auto* capacity_composition =
            capacity_bundle->vqec_vision_ai_appl_rcfac_get_composition();
        assert(capacity_composition->vqec_vision_ai_cntr_acomp_activate().code_ ==
            status_code::ok);
        std::uint64_t capacity_now = 0;
        vqec_vision_ai_ctest_rcfct_advance_to_running(
            *capacity_composition, *capacity_bundle, capacity_now);
        assert(graph_a0.graph_start_count_ == 1 && graph_b0.graph_start_count_ == 1 &&
               graph_a1.configure_count_ == 0 && graph_b1.configure_count_ == 0 &&
               graph_a1.load_count_ == 0 && graph_b1.load_count_ == 0);
        for (std::uint16_t source_slot = 0; source_slot < 2; ++source_slot) {
            assert(capacity_bundle->vqec_vision_ai_appl_rcfac_get_session(source_slot)->
                       vqec_vision_ai_appl_mmses_request_model_mask(
                           3, ++capacity_now).code_ == status_code::pending);
        }
        vqec_vision_ai_ctest_rcfct_advance_delta(
            *capacity_composition, *capacity_bundle, 3, capacity_now);
        assert(graph_a1.graph_start_count_ == 1 && graph_b1.graph_start_count_ == 1 &&
               source_a.start_count_ == 1 && source_b.start_count_ == 1);
        assert(capacity_composition->vqec_vision_ai_cntr_acomp_request_stop(
                   ++capacity_now).code_ == status_code::pending);
        for (unsigned attempt = 0; attempt < 64 &&
             capacity_composition->vqec_vision_ai_cntr_acomp_get_snapshot().state_ !=
                 application_composition_state::stopped; ++attempt) {
            const auto stopped =
                capacity_composition->vqec_vision_ai_cntr_acomp_step(++capacity_now);
            assert(stopped.code_ == status_code::ok ||
                   stopped.code_ == status_code::pending);
        }
        assert(capacity_composition->vqec_vision_ai_cntr_acomp_get_snapshot().state_ ==
            application_composition_state::stopped);
    }

#if defined(VQEC_VISION_AI_HAS_SERVICE_ACTIVATION_RECONCILER)
    // A complete App Manager snapshot is reconciled inside one running generation. Both
    // applications share the detector: removing the first consumer changes only its feature
    // owner; the graph/source remain running. A configuration delta then replaces only the
    // remaining feature stage. All-off remains an explicit generation boundary.
    {
        auto delta_catalog = vqec_vision_ai_ctest_rcfct_make_catalog();
        auto unique_model = vqec_vision_ai_ctest_rcfct_make_model();
        unique_model.model_id_ = "detector_b";
        unique_model.artifact_ref_ = "detector_b.artifact";
        unique_model.artifact_sha256_ = std::string(64, 'b');
        unique_model.output_manifest_ref_ = "detector_b.outputs";
        unique_model.graph_name_ = "detector_b.graph";
        delta_catalog.models_.push_back(unique_model);
        auto delta_deployment = vqec_vision_ai_ctest_rcfct_make_deployment();
        for (auto& source : delta_deployment.sources_) {
            source.model_ids_.push_back("detector_b");
            source.memory_.max_tensor_bytes_ = 16 * g_mib;
        }
        auto delta_features = vqec_vision_ai_ctest_rcfct_make_features();
        auto unique_feature = delta_features.features_.front();
        unique_feature.feature_id_ = "fixture_feature_3";
        unique_feature.model_dependencies_[0].model_id_ = "detector_b";
        delta_features.features_.push_back(std::move(unique_feature));
        auto delta_usecases = vqec_vision_ai_ctest_rcfct_make_usecases();
        delta_usecases.usecases_.push_back(
            {"fixture_app_3", "1.0", {"detector_b"}, {"fixture_feature_3"}});
        auto runtime = vqec_vision_ai_ctest_rcfct_make_runtime(1);
        for (unsigned source = 0; source < 2; ++source) {
            auto unique_app = vqec_vision_ai_ctest_rcfct_make_app(
                "source_" + std::to_string(source), 3, 1, "detector_b");
            unique_app.desired_ = false;
            runtime.associations_.push_back(std::move(unique_app));
        }
        auto startup = vqec_vision_ai_ctest_rcfct_make_startup(
            delta_deployment, delta_catalog, delta_features, delta_usecases,
            runtime);

        vqec_vision_ai_ctest_rcfct_source source_a;
        vqec_vision_ai_ctest_rcfct_source source_b;
        source_a.emit_frames_ = true;
        source_b.emit_frames_ = true;
        vqec_vision_ai_ctest_rcfct_graph graph_a;
        vqec_vision_ai_ctest_rcfct_graph graph_b;
        vqec_vision_ai_ctest_rcfct_graph unique_graph_a;
        vqec_vision_ai_ctest_rcfct_graph unique_graph_b;
        auto delta_activation = vqec_vision_ai_ctest_rcfct_make_activation(
            delta_catalog.models_[0], source_a, source_b, graph_a, graph_b);
        delta_activation.sources_[0].model_count_ = 2;
        delta_activation.sources_[0].models_[1] =
            vqec_vision_ai_ctest_rcfct_make_model_activation(
                delta_catalog.models_[1], unique_graph_a, 103);
        delta_activation.sources_[0].initial_active_model_mask_ = 1;
        delta_activation.sources_[1].model_count_ = 2;
        delta_activation.sources_[1].models_[1] =
            vqec_vision_ai_ctest_rcfct_make_model_activation(
                delta_catalog.models_[1], unique_graph_b, 104);
        delta_activation.sources_[1].initial_active_model_mask_ = 1;

        vqec_vision_ai_ctest_rcfct_feature_factory feature_factory;
        feature_processor_registry feature_registry;
        assert(feature_registry.vqec_vision_ai_ftmgr_ftreg_register_factory(
                   "fixture_processor", feature_factory).code_ == status_code::ok);
        parsed_arguments arguments;
        output_gate gate;
        auto feature_owner = std::make_unique<service_feature_activation>();
        assert(feature_owner->vqec_vision_ai_appl_svfac_configure(
                   startup, arguments, startup.deployment, startup.catalog,
                   startup.features, feature_registry, "fixture_attribute", gate,
                   delta_activation.source_count_).code_ == status_code::ok);
        auto* initial_wiring =
            feature_owner->vqec_vision_ai_appl_svfac_get_wiring();
        assert(initial_wiring != nullptr);
        auto* retained_feature_a = initial_wiring->sources_[0].fanouts_[0]->
            vqec_vision_ai_appl_ftfan_get_stage(1);
        auto* retained_feature_b = initial_wiring->sources_[1].fanouts_[0]->
            vqec_vision_ai_appl_ftfan_get_stage(1);

        std::unique_ptr<runtime_composition_bundle> delta_bundle;
        assert(vqec_vision_ai_appl_rcfac_create_bundle(
                   startup.deployment, startup.catalog, delta_activation, decoders,
                   trackers, delta_bundle, initial_wiring).code_ == status_code::ok);
        auto* delta_composition =
            delta_bundle->vqec_vision_ai_appl_rcfac_get_composition();
        assert(delta_composition->vqec_vision_ai_cntr_acomp_activate().code_ ==
            status_code::ok);
        std::uint64_t now = 0;
        vqec_vision_ai_ctest_rcfct_advance_to_running(
            *delta_composition, *delta_bundle, now);
        const auto first_start_count = source_a.start_count_;
        const auto second_start_count = source_b.start_count_;
        const auto first_graph_start_count = graph_a.graph_start_count_;
        const auto second_graph_start_count = graph_b.graph_start_count_;

        service_activation_reconciler reconciler;
        assert(reconciler.vqec_vision_ai_appl_svacr_configure(
                   startup, arguments, *delta_bundle, feature_owner,
                   feature_registry, "fixture_attribute", gate).code_ ==
            status_code::ok);
        auto acquired_runtime = runtime;
        acquired_runtime.snapshot_revision_ = 2;
        acquired_runtime.desired_revision_ = 2;
        for (auto& association : acquired_runtime.associations_) {
            if (association.app_id_ == "fixture_app_3") {
                association.desired_ = true;
            }
        }
        assert(reconciler.vqec_vision_ai_appl_svacr_apply_snapshot(
                   acquired_runtime, ++now).code_ == status_code::pending);
        vqec_vision_ai_ctest_rcfct_advance_delta(
            *delta_composition, *delta_bundle, 3, now);
        assert(reconciler.vqec_vision_ai_appl_svacr_apply_snapshot(
                   acquired_runtime, ++now).code_ == status_code::ok);
        assert(unique_graph_a.graph_start_count_ == 1 &&
               unique_graph_b.graph_start_count_ == 1 &&
               source_a.start_count_ == first_start_count &&
               source_b.start_count_ == second_start_count);

        auto retained_runtime = acquired_runtime;
        retained_runtime.snapshot_revision_ = 3;
        retained_runtime.desired_revision_ = 3;
        for (auto& association : retained_runtime.associations_) {
            if (association.app_id_ == "fixture_app_1") {
                association.desired_ = false;
            }
        }
        assert(reconciler.vqec_vision_ai_appl_svacr_apply_snapshot(
                   retained_runtime, ++now).code_ == status_code::ok);
        vqec_vision_ai_ctest_rcfct_advance_delta(
            *delta_composition, *delta_bundle, 3, now);
        const auto* retained_wiring =
            feature_owner->vqec_vision_ai_appl_svfac_get_wiring();
        assert(retained_wiring != nullptr &&
               retained_wiring->sources_[0].fanouts_[0]->
                   vqec_vision_ai_appl_ftfan_get_stage_count() == 1 &&
               retained_wiring->sources_[0].fanouts_[0]->
                   vqec_vision_ai_appl_ftfan_get_stage(0) == retained_feature_a &&
               retained_wiring->sources_[1].fanouts_[0]->
                   vqec_vision_ai_appl_ftfan_get_stage(0) == retained_feature_b);
        assert(source_a.start_count_ == first_start_count &&
               source_b.start_count_ == second_start_count &&
               graph_a.graph_start_count_ == first_graph_start_count &&
               graph_b.graph_start_count_ == second_graph_start_count &&
               graph_a.drain_count_ == 0 && graph_b.drain_count_ == 0);

        auto configured_runtime = retained_runtime;
        configured_runtime.snapshot_revision_ = 4;
        configured_runtime.inventory_revision_ = 2;
        for (auto& association : configured_runtime.associations_) {
            if (association.app_id_ == "fixture_app_2") {
                association.configuration_revision_ = 2;
                association.configuration_sha256_ = std::string(64, 'd');
                association.configuration_payload_ = {2};
            }
        }
        assert(reconciler.vqec_vision_ai_appl_svacr_apply_snapshot(
                   configured_runtime, ++now).code_ == status_code::ok);
        const auto* configured_wiring =
            feature_owner->vqec_vision_ai_appl_svfac_get_wiring();
        assert(configured_wiring->sources_[0].fanouts_[0]->
                   vqec_vision_ai_appl_ftfan_get_stage(0) != retained_feature_a &&
               configured_wiring->sources_[1].fanouts_[0]->
                   vqec_vision_ai_appl_ftfan_get_stage(0) != retained_feature_b &&
               source_a.start_count_ == first_start_count &&
               source_b.start_count_ == second_start_count &&
               graph_a.drain_count_ == 0 && graph_b.drain_count_ == 0);

        auto released_runtime = configured_runtime;
        released_runtime.snapshot_revision_ = 5;
        released_runtime.desired_revision_ = 5;
        for (auto& association : released_runtime.associations_) {
            if (association.app_id_ == "fixture_app_2") {
                association.desired_ = false;
            }
        }
        assert(reconciler.vqec_vision_ai_appl_svacr_apply_snapshot(
                   released_runtime, ++now).code_ == status_code::pending);
        vqec_vision_ai_ctest_rcfct_advance_delta(
            *delta_composition, *delta_bundle, 2, now);
        assert(reconciler.vqec_vision_ai_appl_svacr_apply_snapshot(
                   released_runtime, ++now).code_ == status_code::ok);
        assert(graph_a.drain_count_ == 1 && graph_b.drain_count_ == 1 &&
               graph_a.unload_count_ == 1 && graph_b.unload_count_ == 1 &&
               unique_graph_a.drain_count_ == 0 && unique_graph_b.drain_count_ == 0 &&
               unique_graph_a.graph_start_count_ == 1 &&
               unique_graph_b.graph_start_count_ == 1 &&
               source_a.start_count_ == first_start_count &&
               source_b.start_count_ == second_start_count);

        auto all_off = released_runtime;
        all_off.snapshot_revision_ = 6;
        all_off.desired_revision_ = 6;
        for (auto& association : all_off.associations_) {
            association.desired_ = false;
        }
        assert(reconciler.vqec_vision_ai_appl_svacr_apply_snapshot(
                   all_off, ++now).code_ == status_code::unsupported);
        assert(reconciler.vqec_vision_ai_appl_svacr_get_applied_revision() == 5 &&
               gate.vqec_vision_ai_core_otgat_get_revision() == 5);

        assert(delta_composition->vqec_vision_ai_cntr_acomp_request_stop(
                   ++now).code_ == status_code::pending);
        for (unsigned attempt = 0; attempt < 64 &&
             delta_composition->vqec_vision_ai_cntr_acomp_get_snapshot().state_ !=
                 application_composition_state::stopped; ++attempt) {
            const auto stopped =
                delta_composition->vqec_vision_ai_cntr_acomp_step(++now);
            assert(stopped.code_ == status_code::ok ||
                   stopped.code_ == status_code::pending);
        }
        assert(delta_composition->vqec_vision_ai_cntr_acomp_get_snapshot().state_ ==
            application_composition_state::stopped);
    }
#endif
    return 0;
}
