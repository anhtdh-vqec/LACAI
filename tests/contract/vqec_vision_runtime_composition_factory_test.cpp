#include <cassert>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "vqec_vision_runtime_composition_factory.hpp"

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
        (void)_frame;
        (void)_timeout_ms;
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
        state_ = inference_graph_state::running;
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_infgr_arm(
        std::uint64_t _cycle_id, std::uint64_t _source_epoch,
        std::uint64_t _job_timeout_ns) override {
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
        state_ = inference_graph_state::drained;
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_infgr_unload() override {
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
};

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
        return !_source_id.empty() && _model_id == "detector" ? status{} :
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
    return 0;
}
