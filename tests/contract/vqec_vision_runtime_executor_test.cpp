// Device-free end-to-end contract: reference RAW source -> reference graph ->
// composition/session -> perception (decode + track) -> feature fan-out, driven by
// runtime_executor. It proves the orchestration seam only; it is not a board, model,
// accuracy or hardware-completion qualification.

#include <array>
#include <cassert>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "vqec_vision_feature_activation_manager.hpp"
#include "vqec_vision_feature_fanout.hpp"
#include "vqec_vision_feature_processor_registry.hpp"
#include "vqec_vision_reference_graph.hpp"
#include "vqec_vision_reference_sink.hpp"
#include "vqec_vision_reference_source.hpp"
#include "vqec_vision_runtime_composition_factory.hpp"
#include "vqec_vision_runtime_executor.hpp"

using namespace vqec::vision::ai;

namespace {

constexpr std::uint64_t g_mib = 1024ULL * 1024ULL;
constexpr std::uint32_t g_width = 640;
constexpr std::uint32_t g_height = 480;

class vqec_vision_ai_ctest_rtexe_decoder final : public model_decoder_port {
public:
    [[nodiscard]] status vqec_vision_ai_cntr_mddec_validate(
        const model_outputs& _outputs) const override {
        return _outputs.outputs_.size() == 1 && _outputs.outputs_[0].name_ == "boxes" ?
            status{} : status{status_code::unsupported, "fixture decoder requires boxes"};
    }
    [[nodiscard]] status vqec_vision_ai_cntr_mddec_decode(
        const tensor_result& _result, const preview_frame_key& _expected_frame,
        observation_batch& _observations) override {
        (void)_result;
        observation_batch candidate;
        candidate.frame_ = _expected_frame;
        candidate.geometry_ = {g_width, g_height};
        observation item;
        item.frame_ = _expected_frame;
        item.class_id_ = "person";
        item.box_ = {0.0F, 0.0F, 100.0F, 120.0F, 0xffffffffU, "person"};
        item.confidence_ = 0.9F;
        item.quality_ = observation_quality::high;
        candidate.observations_.push_back(std::move(item));
        _observations = std::move(candidate);
        return {};
    }
};

class vqec_vision_ai_ctest_rtexe_tracker final : public tracker_port {
public:
    [[nodiscard]] status vqec_vision_ai_ports_trker_validate_activation() const override {
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_trker_update_tracks(
        const observation_batch& _detections, std::uint64_t _now_monotonic_ns,
        bool _is_source_gap, observation_batch& _tracked) override {
        (void)_now_monotonic_ns;
        (void)_is_source_gap;
        observation_batch candidate = _detections;
        for (auto& item : candidate.observations_) {
            item.track_id_ = ++assigned_;
        }
        _tracked = std::move(candidate);
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_trker_reset_epoch(
        std::uint64_t _source_epoch) override {
        (void)_source_epoch;
        assigned_ = 0;
        return {};
    }

    std::uint64_t assigned_{0};
};

class vqec_vision_ai_ctest_rtexe_tracker_factory final : public tracker_factory_port {
public:
    [[nodiscard]] status vqec_vision_ai_track_trfac_validate_activation(
        const std::string& _source_id, const std::string& _model_id) const override {
        return !_source_id.empty() && _model_id == "detector" ?
            status{} : status{status_code::unsupported, "fixture tracker binding rejected"};
    }
    [[nodiscard]] status vqec_vision_ai_track_trfac_create_tracker(
        const std::string& _source_id, const std::string& _model_id,
        std::unique_ptr<tracker_port>& _tracker) override {
        (void)_source_id;
        (void)_model_id;
        _tracker = std::make_unique<vqec_vision_ai_ctest_rtexe_tracker>();
        return {};
    }
};

class vqec_vision_ai_ctest_rtexe_feature final : public feature_processor_port {
public:
    [[nodiscard]] status vqec_vision_ai_ports_ftpro_validate_activation(
        const feature_processor_config& _config) const override {
        config_ = _config;
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_ftpro_reset_epoch(
        std::uint64_t _source_epoch) override {
        (void)_source_epoch;
        next_event_id_ = 0;
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_ftpro_process_observations(
        const observation_batch& _tracked, std::uint64_t _now_monotonic_ns,
        bool _is_source_gap, feature_event_batch& _events) override {
        (void)_now_monotonic_ns;
        (void)_is_source_gap;
        feature_event_batch candidate;
        candidate.frame_ = _tracked.frame_;
        candidate.geometry_ = _tracked.geometry_;
        for (const auto& item : _tracked.observations_) {
            if (item.track_id_ == 0) {
                continue;
            }
            feature_event event;
            event.frame_ = _tracked.frame_;
            event.source_id_ = config_.source_id_;
            event.feature_id_ = config_.feature_id_;
            event.event_id_ = "evt_" + std::to_string(++next_event_id_);
            event.event_schema_id_ = "intrusion.event";
            event.event_schema_version_ = "1";
            event.kind_ = feature_event_kind::snapshot;
            event.occurred_at_ns_ = _tracked.frame_.source_pts_ns_;
            event.config_revision_ = config_.config_revision_;
            event.track_ids_.push_back(item.track_id_);
            candidate.events_.push_back(std::move(event));
        }
        _events = std::move(candidate);
        return {};
    }

    mutable feature_processor_config config_;
    std::uint64_t next_event_id_{0};
};

class vqec_vision_ai_ctest_rtexe_feature_factory final
    : public feature_processor_factory_port {
public:
    [[nodiscard]] status vqec_vision_ai_ports_ftfac_validate_configuration(
        const feature_catalog_entry& _feature, const feature_processor_config& _processor_config,
        const feature_configuration& _configuration) const override {
        (void)_feature;
        (void)_processor_config;
        (void)_configuration;
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_ftfac_create_processor(
        const feature_catalog_entry& _feature, const feature_processor_config& _processor_config,
        const feature_configuration& _configuration,
        std::unique_ptr<feature_processor_port>& _processor) override {
        (void)_feature;
        (void)_processor_config;
        (void)_configuration;
        _processor = std::make_unique<vqec_vision_ai_ctest_rtexe_feature>();
        return {};
    }
};

class vqec_vision_ai_ctest_rtexe_failing_feature final : public feature_processor_port {
public:
    [[nodiscard]] status vqec_vision_ai_ports_ftpro_validate_activation(
        const feature_processor_config& _config) const override {
        (void)_config;
        return {};
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
        return {status_code::io_error, "fixture failing feature"};
    }
};

class vqec_vision_ai_ctest_rtexe_failing_factory final
    : public feature_processor_factory_port {
public:
    [[nodiscard]] status vqec_vision_ai_ports_ftfac_validate_configuration(
        const feature_catalog_entry& _feature, const feature_processor_config& _processor_config,
        const feature_configuration& _configuration) const override {
        (void)_feature;
        (void)_processor_config;
        (void)_configuration;
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_ftfac_create_processor(
        const feature_catalog_entry& _feature, const feature_processor_config& _processor_config,
        const feature_configuration& _configuration,
        std::unique_ptr<feature_processor_port>& _processor) override {
        (void)_feature;
        (void)_processor_config;
        (void)_configuration;
        _processor = std::make_unique<vqec_vision_ai_ctest_rtexe_failing_feature>();
        return {};
    }
};

model_catalog_entry vqec_vision_ai_ctest_rtexe_make_model() {
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
    model.source_constraints_ = {g_width, g_height, 4096, 2160, 10, 1};
    model.resources_ = {32 * g_mib, 8 * g_mib, 2, 2, true};
    return model;
}

model_catalog vqec_vision_ai_ctest_rtexe_make_catalog() {
    model_catalog catalog;
    catalog.schema_version_ = model_catalog_limits::g_schema_version;
    catalog.revision_ = 7;
    catalog.catalog_id_ = "models_qcs6490_v1";
    catalog.models_.push_back(vqec_vision_ai_ctest_rtexe_make_model());
    return catalog;
}

deployment_config vqec_vision_ai_ctest_rtexe_make_deployment() {
    deployment_config deployment;
    deployment.schema_version_ = deployment_limits::g_schema_version;
    deployment.revision_ = 9;
    deployment.model_catalog_ref_ = "models_qcs6490_v1";
    deployment.max_total_resident_bytes_ = 512 * g_mib;
    deployment.max_model_resident_bytes_ = 64 * g_mib;
    source_deployment_config source;
    source.source_id_ = "source_0";
    source.raw_source_ref_ = "fw_raw_0";
    source.camera_id_ = 0;
    source.profile_ = {g_width, g_height, 25, 1};
    source.memory_.max_frame_allocation_bytes_ = 4 * g_mib;
    source.memory_.max_inflight_frames_ = 1;
    source.memory_.max_tensor_bytes_ = 8 * g_mib;
    source.memory_.max_temporal_bytes_ = g_mib;
    source.model_ids_.push_back("detector");
    deployment.sources_.push_back(std::move(source));
    return deployment;
}

feature_catalog vqec_vision_ai_ctest_rtexe_make_features() {
    feature_catalog features;
    features.schema_version_ = feature_catalog_limits::g_schema_version;
    features.revision_ = 3;
    features.catalog_id_ = "features_v1";
    features.model_catalog_ref_ = "models_qcs6490_v1";
    feature_catalog_entry feature;
    feature.feature_id_ = "intrusion";
    feature.feature_version_ = "1.0";
    feature.processor_contract_ = "intrusion.processor.v1";
    feature.configuration_schema_ = "intrusion.config.v1";
    feature.input_mode_ = feature_input_mode::single_model;
    feature.model_dependencies_ = {{"detector_role", "detector"}};
    feature.resources_ = {0, 4, 4, 4};
    features.features_.push_back(std::move(feature));
    feature_catalog_entry failing;
    failing.feature_id_ = "alert";
    failing.feature_version_ = "1.0";
    failing.processor_contract_ = "alert.processor.v1";
    failing.configuration_schema_ = "alert.config.v1";
    failing.input_mode_ = feature_input_mode::single_model;
    failing.model_dependencies_ = {{"detector_role", "detector"}};
    failing.resources_ = {0, 4, 4, 4};
    features.features_.push_back(std::move(failing));
    return features;
}

runtime_composition_activation vqec_vision_ai_ctest_rtexe_make_activation(
    const model_catalog_entry& _model, reference_raw_source& _source,
    reference_inference_graph& _graph) {
    runtime_composition_activation activation;
    activation.source_count_ = 1;
    activation.startup_timeout_ns_ = 30000000000ULL;
    activation.stop_timeout_ns_ = 10000000000ULL;
    activation.rpc_timeout_ms_ = 1000;
    activation.sources_[0].source_id_ = "source_0";
    activation.sources_[0].source_ = &_source;
    activation.sources_[0].model_count_ = 1;
    auto& model = activation.sources_[0].models_[0];
    model.model_id_ = _model.model_id_;
    model.graph_ = &_graph;
    model.paths_.model_id_ = _model.model_id_;
    model.paths_.target_id_ = _model.target_id_;
    model.paths_.artifact_ref_ = _model.artifact_ref_;
    model.paths_.model_path_ = "/opt/vqec/models/detector.bin";
    model.paths_.backend_path_ = "/usr/lib/libQnnHtp.so";
    model.paths_.system_path_ = "/usr/lib/libQnnSystem.so";
    model.resolved_output_manifest_ref_ = _model.output_manifest_ref_;
    model.outputs_.model_id_ = _model.model_id_;
    model.outputs_.model_version_ = _model.model_version_;
    model.outputs_.artifact_sha256_ = _model.artifact_sha256_;
    model.outputs_.decoder_contract_ = _model.decoder_contract_;
    model.outputs_.max_output_bytes_ = 16;
    model.outputs_.outputs_.push_back({"boxes", {1, 4}});
    model.tracker_contract_ = "bytetrack.v1";
    model.binding_.width_ = g_width;
    model.binding_.height_ = g_height;
    model.binding_.fps_numerator_ = 25;
    model.binding_.fps_denominator_ = 1;
    model.binding_.memory_kind_ = source_memory_kind::dmabuf;
    model.binding_.layout_ = source_memory_layout::linear_nv12;
    model.binding_.sync_mode_ = source_sync_mode::implicit_ready;
    model.binding_.color_profile_ = source_color_profile::bt709_limited;
    model.binding_.chroma_site_ = source_chroma_site::mpeg2;
    model.binding_.fw_memory_contract_ = "fw.dmabuf.v1";
    model.binding_.backend_memory_contract_ = "qcom.dmabuf.v1";
    model.binding_.preprocess_contract_ = _model.preprocess_contract_;
    model.cycle_id_ = 101;
    model.job_timeout_ns_ = 1000000000;
    return activation;
}

}  // namespace

int main() {
    const auto catalog = vqec_vision_ai_ctest_rtexe_make_catalog();
    const auto deployment = vqec_vision_ai_ctest_rtexe_make_deployment();
    const auto features = vqec_vision_ai_ctest_rtexe_make_features();

    reference_raw_source source({g_width, g_height, 25, 1});
    reference_inference_graph graph;
    auto activation = vqec_vision_ai_ctest_rtexe_make_activation(
        catalog.models_[0], source, graph);

    vqec_vision_ai_ctest_rtexe_decoder decoder;
    model_decoder_registry decoders;
    assert(decoders.vqec_vision_ai_detec_mdreg_register_decoder(
               "detector.decoder.v1", decoder).code_ == status_code::ok);
    vqec_vision_ai_ctest_rtexe_tracker_factory tracker_factory;
    tracker_registry trackers;
    assert(trackers.vqec_vision_ai_track_trreg_register_factory(
               "bytetrack.v1", tracker_factory).code_ == status_code::ok);

    // Feature activation is a caller prerequisite; the runtime only borrows the fan-out.
    vqec_vision_ai_ctest_rtexe_feature_factory feature_factory;
    vqec_vision_ai_ctest_rtexe_failing_factory failing_factory;
    feature_processor_registry feature_registry;
    assert(feature_registry.vqec_vision_ai_ftmgr_ftreg_register_factory(
               "intrusion.processor.v1", feature_factory).code_ == status_code::ok);
    assert(feature_registry.vqec_vision_ai_ftmgr_ftreg_register_factory(
               "alert.processor.v1", failing_factory).code_ == status_code::ok);
    feature_activation_manager feature_manager;
    assert(feature_manager.vqec_vision_ai_ftmgr_famgr_configure(
               features, catalog, deployment).code_ == status_code::ok);
    std::array<feature_activation_request,
        feature_activation_limits::g_max_associations> requests{};
    requests[0].source_id_ = "source_0";
    requests[0].feature_id_ = "intrusion";
    requests[0].desired_enabled_ = true;
    requests[0].entitlement_granted_ = true;
    requests[0].resource_admitted_ = true;
    requests[0].configuration_.schema_id_ = "intrusion.config.v1";
    requests[0].configuration_.revision_ = 1;
    requests[1].source_id_ = "source_0";
    requests[1].feature_id_ = "alert";
    requests[1].desired_enabled_ = true;
    requests[1].entitlement_granted_ = true;
    requests[1].resource_admitted_ = true;
    requests[1].configuration_.schema_id_ = "alert.config.v1";
    requests[1].configuration_.revision_ = 1;
    feature_activation_snapshot feature_snapshot;
    assert(feature_manager.vqec_vision_ai_ftmgr_famgr_reconcile(
               requests, 2, feature_registry, feature_snapshot).code_ == status_code::ok);
    assert(feature_snapshot.ready_count_ == 2);
    feature_fanout fanout;
    std::array<feature_stage*, feature_fanout_limits::g_max_feature_stages> stages{};
    stages[0] = feature_manager.vqec_vision_ai_ftmgr_famgr_get_stage(0);
    stages[1] = feature_manager.vqec_vision_ai_ftmgr_famgr_get_stage(1);
    assert(stages[0] != nullptr && stages[1] != nullptr);
    assert(fanout.vqec_vision_ai_appl_ftfan_configure(stages, 2).code_ == status_code::ok);
    runtime_feature_activation feature_wiring;
    feature_wiring.deployment_revision_ = deployment.revision_;
    feature_wiring.catalog_revision_ = catalog.revision_;
    feature_wiring.source_count_ = 1;
    feature_wiring.sources_[0].fanouts_[0] = &fanout;
    // B04: once stages are lent to a fan-out, reconcile must not invalidate the borrow.
    feature_manager.vqec_vision_ai_ftmgr_famgr_freeze();
    assert(feature_manager.vqec_vision_ai_ftmgr_famgr_is_frozen());
    feature_activation_snapshot frozen_snapshot;
    assert(feature_manager.vqec_vision_ai_ftmgr_famgr_reconcile(
               requests, 2, feature_registry, frozen_snapshot).code_ ==
           status_code::invalid_state);

    output_gate output_policy_gate;
    output_policy policy;
    policy.revision_ = 1;
    policy.not_before_ns_ = 0;
    policy.expires_ns_ = 1000000000000000000ULL;
    policy.rules_.push_back({"source_0", "intrusion", {}});
    policy.rules_.push_back({"source_0", "alert", {}});
    assert(output_policy_gate.vqec_vision_ai_core_otgat_apply_policy(
               policy, 0).code_ == status_code::ok);
    reference_event_sink event_sink;

    // B03: a feature descriptor pinned to other revisions must be rejected before any
    // owner is created.
    auto stale_wiring = feature_wiring;
    stale_wiring.catalog_revision_ += 1;
    std::unique_ptr<runtime_composition_bundle> rejected;
    assert(vqec_vision_ai_appl_rcfac_create_bundle(deployment, catalog, activation,
               decoders, trackers, rejected, &stale_wiring).code_ ==
           status_code::invalid_argument);
    assert(rejected == nullptr);

    std::unique_ptr<runtime_composition_bundle> bundle;
    assert(vqec_vision_ai_appl_rcfac_create_bundle(deployment, catalog, activation,
               decoders, trackers, bundle, &feature_wiring).code_ == status_code::ok);
    assert(bundle != nullptr && bundle->vqec_vision_ai_appl_rcfac_get_executor() != nullptr &&
           bundle->vqec_vision_ai_appl_rcfac_get_feature_pipeline(0) != nullptr);
    auto* executor = bundle->vqec_vision_ai_appl_rcfac_get_executor();
    executor->vqec_vision_ai_appl_rtexe_bind_event_delivery(output_policy_gate, event_sink);
    auto* composition = bundle->vqec_vision_ai_appl_rcfac_get_composition();
    assert(composition->vqec_vision_ai_cntr_acomp_activate().code_ == status_code::ok);
    // B04/A06: an active bundle must not be replaced underneath its running owners.
    {
        std::unique_ptr<runtime_composition_bundle> active_holder = std::move(bundle);
        assert(vqec_vision_ai_appl_rcfac_create_bundle(deployment, catalog, activation,
                   decoders, trackers, active_holder, &feature_wiring).code_ ==
               status_code::invalid_state);
        assert(active_holder != nullptr);
        bundle = std::move(active_holder);
    }

    bool routed = false;
    std::uint64_t tracked_count = 0;
    std::uint64_t event_count = 0;
    std::uint64_t now_ns = 1000000;
    for (unsigned step = 0; step < 400 && !routed; ++step, now_ns += 1000000) {
        runtime_executor_report report;
        const auto stepped = executor->vqec_vision_ai_appl_rtexe_step(now_ns, report);
        assert(stepped.code_ == status_code::ok || stepped.code_ == status_code::pending ||
               stepped.code_ == status_code::invalid_state);
        if (stepped.code_ != status_code::ok) {
            continue;
        }
        std::array<observation_batch, deployment_limits::g_max_models_per_source>
            tracked_by_model;
        std::array<feature_event_batch, feature_fanout_limits::g_max_feature_stages> events;
        runtime_executor_report taken;
        assert(executor->vqec_vision_ai_appl_rtexe_take_result(
                   tracked_by_model, events, taken).code_ == status_code::ok);
        if (!taken.has_tracked_) {
            continue;
        }
        assert(taken.source_index_ == 0 && taken.model_slot_ == 0);
        assert(tracked_by_model[0].observations_.size() == 1);
        assert(tracked_by_model[0].observations_[0].track_id_ == 1);
        tracked_count = tracked_by_model[0].observations_.size();
        // One healthy feature plus one failing feature must both be retained: the tracked
        // output and the successful batch survive, and the first error is recorded.
        assert(taken.first_error_code_ != status_code::ok);
        assert(taken.features_.processed_mask_ == 1U);
        assert(taken.features_.failed_mask_ == (1U << 1));
        if (taken.has_feature_fanout_) {
            event_count = events[0].events_.size();
            assert(events[1].events_.empty());
            feature_dispatch_report dispatch_report;
            assert(taken.captured_policy_revision_ == 1);
            assert(executor->vqec_vision_ai_appl_rtexe_dispatch_events(
                       events, taken.source_index_, taken.model_slot_,
                       taken.captured_policy_revision_, taken.features_.processed_mask_,
                       now_ns, dispatch_report).code_ == status_code::ok);
            assert(dispatch_report.attempted_ == 1 && dispatch_report.delivered_ == 1);
        }
        routed = true;
    }
    assert(routed);
    assert(tracked_count == 1);
    assert(event_count == 1);

    // B02: a result captured under policy revision 1 must not be relabelled by a later
    // regrant to revision 2; dispatch with the captured revision is denied, not delivered.
    bool stale_pending = false;
    for (unsigned step = 0; step < 400 && !stale_pending; ++step, now_ns += 1000000) {
        runtime_executor_report step_report;
        const auto progressed = executor->vqec_vision_ai_appl_rtexe_step(now_ns, step_report);
        assert(progressed.code_ == status_code::ok ||
               progressed.code_ == status_code::pending);
        stale_pending = executor->vqec_vision_ai_appl_rtexe_has_pending();
    }
    assert(stale_pending);
    std::array<observation_batch, deployment_limits::g_max_models_per_source> stale_tracked;
    std::array<feature_event_batch, feature_fanout_limits::g_max_feature_stages> stale_events;
    runtime_executor_report stale_taken;
    assert(executor->vqec_vision_ai_appl_rtexe_take_result(
               stale_tracked, stale_events, stale_taken).code_ == status_code::ok);
    assert(stale_taken.captured_policy_revision_ == 1);
    output_policy regranted = policy;
    regranted.revision_ = 2;
    assert(output_policy_gate.vqec_vision_ai_core_otgat_apply_policy(
               regranted, 1).code_ == status_code::ok);
    feature_dispatch_report stale_report;
    assert(executor->vqec_vision_ai_appl_rtexe_dispatch_events(
               stale_events, stale_taken.source_index_, stale_taken.model_slot_,
               stale_taken.captured_policy_revision_, stale_taken.features_.processed_mask_,
               now_ns, stale_report).code_ == status_code::unauthorized);
    assert(stale_report.delivered_ == 0 && stale_report.denied_ == 1);

    // A05: stop while a routed result is still pending. The drain must consume/discard it
    // so the composition can reach stopped instead of blocking on the pending slot.
    bool pending_at_stop = false;
    for (unsigned step = 0; step < 400 && !pending_at_stop; ++step, now_ns += 1000000) {
        runtime_executor_report step_report;
        const auto progressed = executor->vqec_vision_ai_appl_rtexe_step(now_ns, step_report);
        assert(progressed.code_ == status_code::ok ||
               progressed.code_ == status_code::pending);
        pending_at_stop = executor->vqec_vision_ai_appl_rtexe_has_pending();
    }
    assert(pending_at_stop);
    const auto stop_request =
        executor->vqec_vision_ai_appl_rtexe_request_stop(now_ns);
    assert(stop_request.code_ == status_code::ok ||
           stop_request.code_ == status_code::pending);
    now_ns += 1000000;
    bool stopped = false;
    for (unsigned step = 0; step < 400 && !stopped; ++step, now_ns += 1000000) {
        if (executor->vqec_vision_ai_appl_rtexe_has_pending()) {
            executor->vqec_vision_ai_appl_rtexe_discard_pending();
        }
        runtime_executor_report drain_report;
        const auto progressed =
            executor->vqec_vision_ai_appl_rtexe_step(now_ns, drain_report);
        assert(progressed.code_ == status_code::ok ||
               progressed.code_ == status_code::pending);
        stopped = executor->vqec_vision_ai_appl_rtexe_get_snapshot().state_ ==
            application_composition_state::stopped;
    }
    assert(stopped);
    return 0;
}
