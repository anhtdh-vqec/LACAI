// LACAI service composition root. This is the required externally named executable
// `vqec_ai_vision_applications`. It loads validated deployment/model/feature metadata,
// selects a platform owner and constructs the neutral runtime bundle, then runs the
// serialized executor loop.
//
// `--mode harness` (default) runs the device-free fake platform for development.
// Production selects an explicit fake, reference or Qualcomm owner and never falls back.
// The Qualcomm owner resolves the model package, executes QNN and optionally produces the
// configured overlay/H.264 ring. See docs/architecture/runtime_executor.md.

#include <array>
#include <chrono>
#include <thread>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <limits>
#include <memory>
#include <new>
#include <string>
#include <utility>
#include <vector>
#include <unistd.h>

#include "vqec_vision_service_runtime.hpp"

#include "vqec_vision_service_options.hpp"
#include "vqec_vision_service_fixture.hpp"
#include "vqec_vision_service_startup.hpp"
#include "vqec_vision_service_feature_registry.hpp"
#include "vqec_vision_deployment_config.hpp"
#include "vqec_vision_cascade_coordinator.hpp"
#include "vqec_vision_cascade_execution_worker.hpp"
#include "vqec_vision_cascade_graph_session.hpp"
#include "vqec_vision_service_cascade_runtime.hpp"
#include "vqec_vision_feature_activation_manager.hpp"
#include "vqec_vision_feature_catalog.hpp"
#include "vqec_vision_usecase_config.hpp"
#include "vqec_vision_usecase_control_manager.hpp"
#include "vqec_vision_feature_fanout.hpp"
#include "vqec_vision_feature_processor_registry.hpp"
#include "vqec_vision_model_catalog.hpp"
#include "vqec_vision_model_package_registry.hpp"
#include "vqec_vision_reference_graph.hpp"
#include "vqec_vision_reference_sink.hpp"
#include "vqec_vision_reference_source.hpp"
#include "vqec_vision_fake_platform.hpp"
#include "vqec_vision_production_platform.hpp"
#include "vqec_vision_reference_platform.hpp"
#include "vqec_vision_runtime_composition_factory.hpp"
#include "vqec_vision_hardware_admission_profile.hpp"
#include "vqec_vision_overlay_preparation.hpp"
#include "vqec_vision_exact_embedding_index.hpp"
#include "vqec_vision_recognition_session.hpp"
#include "vqec_vision_face_enrollment_controller.hpp"
#include "vqec_vision_face_enrollment_image_pipeline.hpp"
#include "vqec_vision_single_image_inference.hpp"
#include "vqec_vision_image_path_authorizer.hpp"
#include "vqec_vision_face_enrollment_image_source.hpp"
#include "vqec_vision_encrypted_face_gallery_store.hpp"
#if defined(VQEC_VISION_AI_HAS_ZVEC)
#include "vqec_vision_zvec_embedding_index.hpp"
#endif
#if defined(VQEC_VISION_AI_HAS_FACE_ENROLLMENT_DBUS)
#include "vqec_vision_face_enrollment_dbus.hpp"
#endif
#if defined(VQEC_VISION_AI_HAS_USECASE_CONTROL_DBUS)
#include "vqec_vision_usecase_control_dbus.hpp"
#endif
#if defined(VQEC_VISION_AI_HAS_APP_MANAGER_DBUS)
#include "vqec_vision_app_manager_dbus.hpp"
#endif
#include "vqec_vision_event_delivery_seam.hpp"
#include "vqec_vision_metadata_runtime.hpp"

using namespace vqec::vision::ai;

namespace {

volatile std::sig_atomic_t g_stop_requested = 0;
constexpr std::uint64_t g_mib = 1024ULL * 1024ULL;
constexpr std::uint64_t g_step_interval_ns =
    service_options_limits::g_default_runtime_step_interval_ns;
// Internal generation outcomes; recovery-required must never enter candidate rollback.
constexpr int g_reconcile_generation_exit_code = 4;
constexpr int g_recovery_required_exit_code = 5;
constexpr std::size_t g_max_active_track_labels = 256;
constexpr std::uint64_t g_routed_log_interval_ns = 1000000000ULL;
constexpr std::uint64_t g_nanoseconds_per_second = 1000000000ULL;
constexpr std::uint64_t g_stop_drain_final_observation_steps = 1U;

void vqec_vision_ai_appl_svcmn_on_signal(int) {
    g_stop_requested = 1;
}

// Real monotonic clock for the executor loop. Never UTC; never a fabricated counter.
std::uint64_t vqec_vision_ai_appl_svcmn_monotonic_ns() noexcept {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

hardware_admission_profile vqec_vision_ai_appl_svcmn_make_fixture_hardware_profile() {
    hardware_admission_profile profile;
    profile.profile_id_ = "device_free_test_fixture";
    profile.target_id_ = "qcs6490_qlinux_1_8";
    profile.measurement_reference_ = "fixture-only-not-a-board-measurement";
    profile.revision_ = 1;
    profile.max_total_resident_bytes_ = 4096ULL * g_mib;
    profile.max_frame_pool_bytes_ = 1024ULL * g_mib;
    profile.max_tensor_pool_bytes_ = 1024ULL * g_mib;
    profile.max_encoder_pool_bytes_ = 512ULL * g_mib;
    profile.max_cascade_roi_bytes_ = 512ULL * g_mib;
    profile.max_ddr_bandwidth_mbps_ = 12000;
    profile.max_fw_concurrency_slots_ = 16;
    profile.max_worker_concurrency_ = 64;
    profile.min_thermal_headroom_pct_ = 10;
    return profile;
}

status vqec_vision_ai_appl_svcmn_resolve_hardware_profile(
    const parsed_arguments& _args, bool _use_production_platform,
    hardware_admission_profile& _profile) {
    if (_args.hardware_profile_path.empty()) {
        if (_use_production_platform) {
            return {status_code::unsupported,
                "Qualcomm production requires --hardware-profile"};
        }
        _profile = vqec_vision_ai_appl_svcmn_make_fixture_hardware_profile();
        return {};
    }
    std::ifstream stream(_args.hardware_profile_path, std::ios::binary);
    if (!stream) {
        return {status_code::io_error, "cannot open hardware admission profile"};
    }
    return vqec_vision_ai_admis_hwprf_load(stream, _profile);
}

std::string vqec_vision_ai_appl_svcmn_dev_model_path(
    const std::string& _model_root, const std::string& _model_id) {
    return _model_root + _model_id + ".bin";
}

// Builds the parsed output metadata the runtime validates against the catalog identity.
// Device-free harness only; a real deployment reads the model package output manifest.
model_outputs vqec_vision_ai_appl_svcmn_synthetic_outputs(const model_catalog_entry& _model) {
    model_outputs outputs;
    outputs.model_id_ = _model.model_id_;
    outputs.model_version_ = _model.model_version_;
    outputs.artifact_sha256_ = _model.artifact_sha256_;
    outputs.decoder_contract_ = _model.decoder_contract_;
    outputs.max_output_bytes_ = service_harness::g_output_bytes;
    outputs.outputs_.push_back({service_harness::g_box_tensor_name,
        {1, service_harness::g_box_elements}, tensor_element_type::float32, {}});
    return outputs;
}

std::uint16_t vqec_vision_ai_appl_svcmn_model_slot(
    const source_deployment_config& _source, const std::string& _model_id) {
    for (std::uint16_t slot = 0; slot < _source.model_ids_.size(); ++slot) {
        if (_source.model_ids_[slot] == _model_id) {
            return slot;
        }
    }
    return g_invalid_model_slot;
}

const model_catalog_entry* vqec_vision_ai_appl_svcmn_find_model(
    const model_catalog& _catalog, const std::string& _model_id) {
    for (const auto& model : _catalog.models_) {
        if (model.model_id_ == _model_id) {
            return &model;
        }
    }
    return nullptr;
}

using model_observation_cache =
    std::array<observation_batch, deployment_limits::g_max_models_per_source>;

void vqec_vision_ai_appl_svcmn_merge_observations(
    const model_observation_cache& _models, std::uint16_t _model_count,
    std::uint64_t _now_ns, std::uint64_t _max_age_ns,
    observation_batch& _merged) {
    (void)_now_ns;
    (void)_max_age_ns;
    observation_batch merged;
    for (std::uint16_t slot = 0;
         slot < _model_count && slot < deployment_limits::g_max_models_per_source;
         ++slot) {
        const auto& batch = _models[slot];
        if (batch.frame_.source_epoch_ == 0) {
            continue;
        }
        if (merged.frame_.source_epoch_ == 0) {
            merged.frame_ = batch.frame_;
            merged.geometry_ = batch.geometry_;
        } else if (merged.frame_.source_epoch_ != batch.frame_.source_epoch_) {
            continue;
        }
        for (const auto& item : batch.observations_) {
            if (merged.observations_.size() >= observation_limits::g_max_observations) {
                break;
            }
            auto merged_item = item;
            merged_item.frame_ = merged.frame_;
            if (merged_item.track_id_ != 0) {
                merged_item.track_id_ =
                    (static_cast<std::uint64_t>(slot + 1) << 32) |
                    (merged_item.track_id_ & 0xFFFFFFFFULL);
            }
            merged.observations_.push_back(std::move(merged_item));
        }
    }
    _merged = std::move(merged);
}

}  // namespace

// Prints the end-of-generation metrics/report and decides the process exit code. Kept out
// of run_generation so the report policy is reviewable on its own. route_latency_* is the
// steady-clock interval from job reservation to result routing; it is not camera-to-output
// latency and excludes FW capture and preview encode.
int vqec_vision_ai_appl_svcmn_report_and_decide(const runtime_executor_metrics& _metrics,
    bool _stopped, bool _enrollment_ok, bool _cascade_ok, std::uint32_t _routed_sources,
    bool _generation_published, status_code _first_error_code, bool _reconcile_requested,
    std::uint32_t _require_sources) {
    const auto route_avg_us = _metrics.end_to_end_samples_ == 0 ? 0ULL :
        _metrics.end_to_end_ns_sum_ / (1000ULL * _metrics.end_to_end_samples_);
    std::printf("metrics steps=%llu routed=%llu accepted=%llu denied=%llu failed=%llu "
        "cascade_tasks=%llu cascade_embeddings=%llu cascade_failed=%llu "
        "route_latency_avg_us=%llu route_latency_min_us=%llu route_latency_max_us=%llu "
        "samples=%u\n",
        static_cast<unsigned long long>(_metrics.steps_),
        static_cast<unsigned long long>(_metrics.results_routed_),
        static_cast<unsigned long long>(_metrics.events_accepted_),
        static_cast<unsigned long long>(_metrics.events_denied_),
        static_cast<unsigned long long>(_metrics.events_failed_),
        static_cast<unsigned long long>(_metrics.cascade_tasks_accepted_),
        static_cast<unsigned long long>(_metrics.cascade_embeddings_),
        static_cast<unsigned long long>(_metrics.cascade_tasks_failed_),
        static_cast<unsigned long long>(route_avg_us),
        static_cast<unsigned long long>(_metrics.end_to_end_samples_ == 0 ? 0ULL :
            _metrics.end_to_end_ns_min_ / 1000ULL),
        static_cast<unsigned long long>(_metrics.end_to_end_ns_max_ / 1000ULL),
        _metrics.end_to_end_samples_);
    std::printf("service stopped=%s routed_sources=%u first_error=%d\n",
        _stopped ? "true" : "false", _routed_sources,
        static_cast<int>(_first_error_code));
    // Owners (feature manager, fan-outs, registries, reference platform) outlive the
    // bundle; the bundle's composition must be stopped before they are destroyed.
    if (!_stopped || !_enrollment_ok || !_cascade_ok) {
        return g_recovery_required_exit_code;
    }
    if (!_generation_published || _first_error_code != status_code::ok) {
        return 1;
    }
    if (_reconcile_requested) {
        return g_reconcile_generation_exit_code;
    }
    return _routed_sources >= _require_sources ? 0 : 1;
}

// Fills the runtime activation entries for every source/model slot. The platform owners and
// the reference graph vector are borrowed here and must outlive the runtime bundle; this
// function only assigns pointers/metadata into _activation. Returns a status; detailed
// diagnostics are printed at the point of failure.
status vqec_vision_ai_appl_svcmn_build_model_activations(
    const parsed_arguments& _args, const deployment_config& _deployment,
    const model_catalog& _catalog, production_platform& _production,
    const std::vector<raw_source_port*>& _sources, bool _use_production_platform,
    const std::string& _tracker_contract,
    std::vector<std::unique_ptr<reference_inference_graph>>& _reference_graphs,
    runtime_composition_activation& _activation) {
    for (std::uint16_t source_slot = 0; source_slot < _activation.source_count_;
         ++source_slot) {
        const auto& source = _deployment.sources_[source_slot];
        auto& source_activation = _activation.sources_[source_slot];
        source_activation.source_id_ = source.source_id_;
        source_activation.source_ = _sources[source_slot];
        source_activation.model_count_ =
            static_cast<std::uint16_t>(source.model_ids_.size());
        for (std::uint16_t model_slot = 0; model_slot < source_activation.model_count_;
             ++model_slot) {
            const auto* model = vqec_vision_ai_appl_svcmn_find_model(
                _catalog, source.model_ids_[model_slot]);
            if (model == nullptr) {
                std::fprintf(stderr, "deployment references unknown model: %s\n",
                    source.model_ids_[model_slot].c_str());
                return {status_code::invalid_argument, "unknown deployment model"};
            }
            auto& model_activation = source_activation.models_[model_slot];
            model_activation.model_id_ = model->model_id_;
            if (_use_production_platform) {
                inference_graph_port* graph =
                    _production.vqec_vision_ai_appl_pdplt_graph(
                        source_slot, model->model_id_);
                image_processor_port* processor =
                    _production.vqec_vision_ai_appl_pdplt_processor(
                        source_slot, model->model_id_);
                const model_outputs* outputs =
                    _production.vqec_vision_ai_appl_pdplt_outputs(model->model_id_);
                if (graph == nullptr || processor == nullptr || outputs == nullptr) {
                    std::fprintf(stderr, "production platform has no graph for model %s\n",
                        model->model_id_.c_str());
                    return {status_code::invalid_argument, "missing production model graph"};
                }
                model_activation.graph_ = graph;
                model_activation.processor_ = processor;
                model_activation.outputs_ = *outputs;
                model_activation.paths_ =
                    _production.vqec_vision_ai_appl_pdplt_paths(model->model_id_);
            } else {
                _reference_graphs.push_back(std::make_unique<reference_inference_graph>());
                model_activation.graph_ = _reference_graphs.back().get();
                model_activation.paths_.model_id_ = model->model_id_;
                model_activation.paths_.target_id_ = model->target_id_;
                model_activation.paths_.artifact_ref_ = model->artifact_ref_;
                model_activation.paths_.model_path_ =
                    vqec_vision_ai_appl_svcmn_dev_model_path(
                        _args.model_root.empty() ? service_harness::g_fixture_model_root
                                                 : _args.model_root,
                        model->model_id_);
                // Reference platform only records the configured paths; the reference graph
                // loads no vendor library. Missing values fall back to device-free fixture
                // placeholders and never to a production default.
                model_activation.paths_.backend_path_ =
                    _args.qnn_backend_library.empty()
                        ? service_harness::g_fixture_backend_library
                        : _args.qnn_backend_library;
                model_activation.paths_.system_path_ =
                    _args.qnn_system_library.empty()
                        ? service_harness::g_fixture_system_library
                        : _args.qnn_system_library;
                model_activation.outputs_ =
                    vqec_vision_ai_appl_svcmn_synthetic_outputs(*model);
            }
            model_activation.resolved_output_manifest_ref_ = model->output_manifest_ref_;
            model_activation.tracker_contract_ = _tracker_contract;
            model_activation.binding_.width_ = source.profile_.width_;
            model_activation.binding_.height_ = source.profile_.height_;
            model_activation.binding_.fps_numerator_ = source.profile_.fps_numerator_;
            model_activation.binding_.fps_denominator_ = source.profile_.fps_denominator_;
            model_activation.binding_.memory_kind_ = source_memory_kind::dmabuf;
            model_activation.binding_.layout_ = source_memory_layout::linear_nv12;
            model_activation.binding_.sync_mode_ = source_sync_mode::implicit_ready;
            model_activation.binding_.color_profile_ = source_color_profile::bt709_limited;
            model_activation.binding_.chroma_site_ = source_chroma_site::mpeg2;
            model_activation.binding_.fw_memory_contract_ =
                service_harness::g_fw_dmabuf_contract;
            model_activation.binding_.backend_memory_contract_ =
                service_harness::g_qcom_dmabuf_contract;
            model_activation.binding_.preprocess_contract_ = model->preprocess_contract_;
            model_activation.cycle_id_ = static_cast<std::uint64_t>(source_slot) *
                    service_harness::g_cycle_id_stride + model_slot + 1U;
            model_activation.job_timeout_ns_ = submission_limits::g_default_job_timeout_ns;
        }
    }
    return {};
}

status vqec_vision_ai_appl_svcmn_resolve_runtime_feature_configuration(
    const service_startup_resolution& _startup, const std::string& _source_id,
    const feature_catalog_entry& _feature, feature_configuration& _configuration,
    std::vector<std::string>& _output_scopes) {
    const app_runtime_association* selected = nullptr;
    for (const auto& usecase : _startup.usecase_control.catalog_.usecases_) {
        if (std::find(usecase.feature_ids_.begin(), usecase.feature_ids_.end(),
                _feature.feature_id_) == usecase.feature_ids_.end()) {
            continue;
        }
        for (const auto& association : _startup.runtime_control.associations_) {
            if (association.app_id_ != usecase.usecase_id_ ||
                association.source_id_ != _source_id || !association.is_effective()) {
                continue;
            }
            if (selected != nullptr &&
                (selected->configuration_schema_id_ !=
                        association.configuration_schema_id_ ||
                    selected->configuration_revision_ !=
                        association.configuration_revision_ ||
                    selected->configuration_sha256_ !=
                        association.configuration_sha256_ ||
                    selected->configuration_payload_ !=
                        association.configuration_payload_ ||
                    selected->output_scopes_ != association.output_scopes_)) {
                return {status_code::invalid_state,
                    "effective applications disagree on shared feature configuration"};
            }
            selected = &association;
        }
    }
    if (selected == nullptr) {
        return {status_code::unauthorized,
            "effective feature has no runtime-control application association"};
    }
    if (selected->configuration_schema_id_ != _feature.configuration_schema_) {
        return {status_code::protocol_error,
            "runtime-control configuration schema differs from feature catalog"};
    }
    try {
        _configuration.schema_id_ = selected->configuration_schema_id_;
        _configuration.revision_ = selected->configuration_revision_;
        _configuration.payload_ = selected->configuration_payload_;
        _output_scopes = selected->output_scopes_;
        return {};
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted,
            "runtime feature configuration allocation failed"};
    }
}

// Appends one FR output-scope rule per deployment source. Shared by the feature-wiring and
// FR-only policy paths so the FR entitlement rule is defined once.
void vqec_vision_ai_appl_svcmn_append_fr_policy_rules(output_policy& _policy,
    const deployment_config& _deployment, const std::string& _feature_id,
    const std::string& _attribute_id) {
    for (const auto& source : _deployment.sources_) {
        output_scope_rule rule;
        rule.source_id_ = source.source_id_;
        rule.feature_id_ = _feature_id;
        rule.attributes_.push_back(_attribute_id);
        _policy.rules_.push_back(std::move(rule));
    }
}

int vqec_vision_ai_appl_svcmn_run_generation(
    int _argc, char** _argv, const deployment_config* _effective_deployment,
    const runtime_control_snapshot* _runtime_control,
    usecase_control_manager* _control_manager,
    const std::function<void()>& _poll_control,
    const std::function<bool()>& _is_runtime_reconcile_requested,
    std::uint64_t _runtime_generation, std::uint64_t _pending_control_revision) {
    parsed_arguments args;
    if (!vqec_vision_ai_appl_svopt_parse(_argc, _argv, args)) {
        std::fprintf(stderr,
            "usage: vqec_ai_vision_applications --deployment <json> --model-catalog <json> "
            "[--feature-catalog <json>] [--usecase-snapshot <json>] "
            "[--app-manager-dbus|--app-manager-dbus-session "
            "--app-manager-service-name <name> --app-manager-client-name <name> "
            "--app-manager-object-path <path> "
            "--app-manager-rpc-timeout-ms <ms> --app-manager-poll-interval-ms <ms>] "
            "[--usecase-dbus|--usecase-dbus-session "
            "--usecase-service-name <name> --usecase-object-path <path> "
            "--usecase-peer-name <name> --usecase-rpc-timeout-ms <ms> "
            "--usecase-callbacks-per-poll <n>] "
            "[--steps <n>] [--require-sources <n>] "
            "[--mode harness|production] [--platform fake|reference|qualcomm] "
            "[--metadata-profile <json>] "
            "[--model-package-registry <json>] "
            "[--output-ring-id <id> [--output-fps <fps>] --output-bitrate <bps> "
            "--output-keyframe-interval <frames> "
            "--output-box-color-rgba <0xRRGGBBAA> "
            "--output-surface-count <count> "
            "--output-colorimetry <gst-colorimetry> "
            "--output-interlace-mode <gst-interlace-mode>] "
            "[--fr-gallery-path <derived-zvec-path> "
            "--fr-protected-directory <absolute-dir> --fr-gallery-file <name> "
            "--fr-key-file <name> --fr-lock-file <name> --fr-gallery-id <id> "
            "--fr-preprocess-revision <n> --fr-store-max-bytes <bytes> "
            "--fr-min-similarity <0..1> "
            "--fr-subject-margin <0..1> --fr-max-templates <n> --fr-top-k <n> "
            "--fr-feature-id <id> --fr-identity-attribute <id> "
            "[--enrollment-dbus|--enrollment-dbus-session "
            "--enrollment-image-root <absolute-dir> "
            "--enrollment-max-image-bytes <bytes> --enrollment-image-timeout-ms <ms> "
            "--enrollment-jpeg-decoder <factory> --enrollment-converter <factory> "
            "--enrollment-scaler <factory> --enrollment-transform <factory> "
            "--enrollment-transform-engine <engine>]]\n");
        return 2;
    }
    std::signal(SIGINT, vqec_vision_ai_appl_svcmn_on_signal);
    std::signal(SIGTERM, vqec_vision_ai_appl_svcmn_on_signal);

    auto startup = vqec_vision_ai_appl_svstr_resolve_startup(
        args, _effective_deployment, _runtime_control, _control_manager, _poll_control,
        _is_runtime_reconcile_requested,
        []() noexcept { return g_stop_requested != 0; }, _runtime_generation,
        _pending_control_revision, g_step_interval_ns,
        g_reconcile_generation_exit_code);
    if (!startup.should_run) {
        return startup.exit_code;
    }
    model_catalog catalog = std::move(startup.catalog);
    deployment_config deployment = std::move(startup.deployment);
    feature_catalog features = std::move(startup.features);
    model_package_registry model_packages = std::move(startup.model_packages);
    const bool use_reference_platform = startup.use_reference_platform;
    const bool use_production_platform = startup.use_production_platform;
    const bool fr_effectively_enabled = startup.fr_effectively_enabled;
    if (use_production_platform && !args.output_ring_id.empty()) {
        for (const auto& source : deployment.sources_) {
            if (args.output_surface_count != source.memory_.preview_surface_count_) {
                std::fprintf(stderr,
                    "preview surface count differs from admitted deployment envelope\n");
                return 1;
            }
        }
    }
    // Platform owners: registered for every catalog contract so the composition has a
    // concrete decoder/tracker/feature set. Selected explicitly, never implicitly.
    const auto source_width = deployment.sources_.front().profile_.width_;
    const auto source_height = deployment.sources_.front().profile_.height_;
    fake_platform platform;
    reference_platform reference;
    production_platform production;
    model_decoder_registry decoders;
    tracker_registry trackers;
    feature_processor_registry feature_registry;
    service_feature_registry compiled_feature_factories;
    feature_catalog platform_features;
    const auto compiled_feature_registration =
        compiled_feature_factories.vqec_vision_ai_appl_sfreg_register_compiled(
            features, feature_registry, platform_features);
    if (compiled_feature_registration.code_ != status_code::ok) {
        std::fprintf(stderr, "compiled feature registration failed (%d): %s\n",
            static_cast<int>(compiled_feature_registration.code_),
            compiled_feature_registration.message_.c_str());
        return 1;
    }
    std::string tracker_contract;
    std::string attribute_schema_id;
    if (use_production_platform) {
        production_platform_config production_config;
        production_config.model_packages_ = model_packages;
        production_config.execution_policy_ = args.execution_policy;
        production_config.backend_library_ = args.qnn_backend_library;
        production_config.system_library_ = args.qnn_system_library;
        production_config.allow_qaic_copy_input_ = args.allow_qaic_copy_input;
        production_config.model_root_ = args.model_root;
        production_config.dsp_v1_skel_dir_ = args.dsp_v1_skel_dir;
        production_config.dsp_legacy_skel_dir_ = args.dsp_legacy_skel_dir;
        production_config.dsp_legacy_clock_corner_ = args.dsp_legacy_clock_corner;
        production_config.dsp_legacy_latency_us_ = args.dsp_legacy_latency_us;
        production_config.dsp_enable_unsigned_pd_ = args.dsp_enable_unsigned_pd;
        production_config.max_artifact_bytes_ = args.max_artifact_bytes > 0
            ? args.max_artifact_bytes
            : production_platform_limits::g_default_max_artifact_bytes;
        production_config.socket_dir_ = args.camera_socket_dir;
        production_config.producer_uid_ = args.camera_producer_uid;
        production_config.nv12_format_value_ = args.nv12_format_value;
        production_config.preprocess_output_timeout_ns_ =
            submission_limits::g_default_job_timeout_ns;
        production_config.tracker_contract_ = args.tracker_contract;
        production_config.event_schema_id_ = args.event_schema_id;
        production_config.event_schema_version_ = args.event_schema_version;
        production_config.consumer_id_prefix_ = args.consumer_id_prefix;
        production_config.output_ring_id_ = args.output_ring_id;
        production_config.output_fps_ = args.output_fps;
        production_config.output_bitrate_bps_ = args.output_bitrate_bps;
        production_config.output_keyframe_interval_frames_ =
            args.output_keyframe_interval_frames;
        production_config.output_box_color_rgba_ = args.output_box_color_rgba;
        production_config.output_surface_count_ = args.output_surface_count;
        production_config.output_colorimetry_ = args.output_colorimetry;
        production_config.output_interlace_mode_ = args.output_interlace_mode;
        const auto configured = production.vqec_vision_ai_appl_pdplt_configure(production_config);
        if (configured.code_ != status_code::ok) {
            std::fprintf(stderr, "production platform configure failed (%d): %s\n",
                static_cast<int>(configured.code_), configured.message_.c_str());
            return 1;
        }
        const auto prepared = production.vqec_vision_ai_appl_pdplt_prepare(deployment, catalog);
        if (prepared.code_ != status_code::ok) {
            std::fprintf(stderr, "production platform prepare failed (%d): %s\n",
                static_cast<int>(prepared.code_), prepared.message_.c_str());
            return 1;
        }
        if (production.vqec_vision_ai_appl_pdplt_register_decoders(catalog, decoders).code_ !=
                status_code::ok ||
            production.vqec_vision_ai_appl_pdplt_register_tracker(trackers).code_ !=
                status_code::ok ||
            production.vqec_vision_ai_appl_pdplt_register_features(
                platform_features, feature_registry)
                    .code_ != status_code::ok) {
            std::fprintf(stderr, "production platform registration failed\n");
            return 1;
        }
        tracker_contract = production.vqec_vision_ai_appl_pdplt_get_tracker_contract();
        attribute_schema_id = production.vqec_vision_ai_appl_pdplt_get_attribute_schema_id();
    } else if (use_reference_platform) {
        const auto configured = reference.vqec_vision_ai_appl_rplat_configure(
            {source_width, source_height});
        if (configured.code_ != status_code::ok) {
            std::fprintf(stderr, "reference platform configure failed (%d): %s\n",
                static_cast<int>(configured.code_), configured.message_.c_str());
            return 1;
        }
        const auto registered_decoders =
            reference.vqec_vision_ai_appl_rplat_register_decoders(catalog, decoders);
        if (registered_decoders.code_ != status_code::ok) {
            std::fprintf(stderr, "cannot register reference platform decoders (%d): %s\n",
                static_cast<int>(registered_decoders.code_),
                registered_decoders.message_.c_str());
            return 1;
        }
        const auto registered_tracker =
            reference.vqec_vision_ai_appl_rplat_register_tracker(trackers);
        if (registered_tracker.code_ != status_code::ok) {
            std::fprintf(stderr, "cannot register reference platform tracker (%d): %s\n",
                static_cast<int>(registered_tracker.code_),
                registered_tracker.message_.c_str());
            return 1;
        }
        const auto registered_features =
            reference.vqec_vision_ai_appl_rplat_register_features(
                platform_features, feature_registry);
        if (registered_features.code_ != status_code::ok) {
            std::fprintf(stderr, "cannot register reference platform features (%d): %s\n",
                static_cast<int>(registered_features.code_),
                registered_features.message_.c_str());
            return 1;
        }
        tracker_contract = reference.vqec_vision_ai_appl_rplat_get_tracker_contract();
        attribute_schema_id = reference.vqec_vision_ai_appl_rplat_get_attribute_schema_id();
    } else {
        const auto configured = platform.vqec_vision_ai_appl_fkplt_configure(
            {source_width, source_height});
        if (configured.code_ != status_code::ok) {
            std::fprintf(stderr, "fake platform configure failed (%d): %s\n",
                static_cast<int>(configured.code_), configured.message_.c_str());
            return 1;
        }
        const auto registered_decoders =
            platform.vqec_vision_ai_appl_fkplt_register_decoders(catalog, decoders);
        if (registered_decoders.code_ != status_code::ok) {
            std::fprintf(stderr, "cannot register fake platform decoders (%d): %s\n",
                static_cast<int>(registered_decoders.code_),
                registered_decoders.message_.c_str());
            return 1;
        }
        const auto registered_tracker =
            platform.vqec_vision_ai_appl_fkplt_register_tracker(trackers);
        if (registered_tracker.code_ != status_code::ok) {
            std::fprintf(stderr, "cannot register fake platform tracker (%d): %s\n",
                static_cast<int>(registered_tracker.code_),
                registered_tracker.message_.c_str());
            return 1;
        }
        const auto registered_features =
            platform.vqec_vision_ai_appl_fkplt_register_features(
                platform_features, feature_registry);
        if (registered_features.code_ != status_code::ok) {
            std::fprintf(stderr, "cannot register fake platform features (%d): %s\n",
                static_cast<int>(registered_features.code_),
                registered_features.message_.c_str());
            return 1;
        }
        tracker_contract = platform.vqec_vision_ai_appl_fkplt_get_tracker_contract();
        attribute_schema_id = platform.vqec_vision_ai_appl_fkplt_get_config().attribute_schema_id_;
    }

    // Output boundary for the harness: a permissive-but-explicit policy plus
    // either a neutral event delivery seam for production or a development reference sink.
    output_gate output_policy_gate;
    reference_event_sink reference_sink;
    event_delivery_seam production_seam;
    feature_event_sink_port& downstream_event_sink =
        use_production_platform
            ? static_cast<feature_event_sink_port&>(production_seam)
            : static_cast<feature_event_sink_port&>(reference_sink);
    std::unique_ptr<metadata_runtime> metadata;
    bool metadata_required = false;
    if (!args.metadata_profile_path.empty()) {
        std::ifstream metadata_profile(args.metadata_profile_path, std::ios::binary);
        metadata_runtime_config metadata_config;
        const auto loaded = metadata_profile
            ? vqec_vision_ai_appl_mdrun_load_config(
                  metadata_profile, deployment, metadata_config)
            : status{status_code::io_error, "cannot open metadata runtime profile"};
        if (loaded.code_ != status_code::ok) {
            std::fprintf(stderr, "metadata profile failed (%d): %s\n",
                static_cast<int>(loaded.code_), loaded.message_.c_str());
            return 1;
        }
        metadata_required = metadata_config.required_;
        metadata = std::make_unique<metadata_runtime>(
            std::move(metadata_config), downstream_event_sink);
        const auto started = metadata->vqec_vision_ai_appl_mdrun_start();
        if (started.code_ != status_code::ok) {
            std::fprintf(stderr, "metadata runtime start failed (%d): %s\n",
                static_cast<int>(started.code_), started.message_.c_str());
            return 1;
        }
    }
    feature_event_sink_port& active_event_sink = metadata != nullptr
        ? static_cast<feature_event_sink_port&>(*metadata)
        : downstream_event_sink;

    // Platform owners: production adapters or the device-free reference backend.
    std::vector<std::unique_ptr<reference_raw_source>> reference_sources;
    std::vector<std::unique_ptr<reference_inference_graph>> reference_graphs;
    std::vector<raw_source_port*> sources;
    std::vector<inference_graph_port*> graphs;
    sources.reserve(deployment.sources_.size());
    graphs.reserve(deployment.sources_.size() * deployment_limits::g_max_models_per_source);
    if (use_production_platform) {
        for (std::uint16_t slot = 0; slot < deployment.sources_.size(); ++slot) {
            raw_source_port* source = production.vqec_vision_ai_appl_pdplt_source(slot);
            if (source == nullptr) {
                std::fprintf(stderr, "production platform has no source for slot %u\n",
                    static_cast<unsigned>(slot));
                return 1;
            }
            sources.push_back(source);
        }
    } else {
        for (const auto& source : deployment.sources_) {
            reference_sources.push_back(std::make_unique<reference_raw_source>(
                reference_source_config{source.profile_.width_, source.profile_.height_,
                    source.profile_.fps_numerator_, source.profile_.fps_denominator_}));
            sources.push_back(reference_sources.back().get());
        }
    }

    runtime_composition_activation activation;
    activation.source_count_ = static_cast<std::uint16_t>(deployment.sources_.size());
    activation.startup_timeout_ns_ = service_harness::g_default_startup_timeout_ns;
    activation.stop_timeout_ns_ = service_harness::g_default_stop_timeout_ns;
    activation.rpc_timeout_ms_ = service_harness::g_default_rpc_timeout_ms;
    activation.use_session_workers_ = args.use_session_workers;
    activation.use_model_workers_ = args.use_model_workers;
    const auto resolved_profile = vqec_vision_ai_appl_svcmn_resolve_hardware_profile(
        args, use_production_platform, activation.hardware_profile_);
    if (resolved_profile.code_ != status_code::ok) {
        std::fprintf(stderr, "hardware admission profile failed (%d): %s\n",
            static_cast<int>(resolved_profile.code_), resolved_profile.message_.c_str());
        return 1;
    }
    const auto built_activations = vqec_vision_ai_appl_svcmn_build_model_activations(
        args, deployment, catalog, production, sources, use_production_platform,
        tracker_contract, reference_graphs, activation);
    if (built_activations.code_ != status_code::ok) {
        return 1;
    }

    // Optional feature activation. Only single_model features are wired by this harness;
    // temporal_join remains a documented activation-time gap.
    feature_activation_manager feature_manager;
    std::array<std::unique_ptr<feature_fanout>,
        deployment_limits::g_max_sources * deployment_limits::g_max_models_per_source>
        fanouts{};
    runtime_feature_activation feature_wiring;
    feature_wiring.deployment_revision_ = deployment.revision_;
    feature_wiring.catalog_revision_ = catalog.revision_;
    feature_wiring.source_count_ = activation.source_count_;
    bool has_feature_wiring = false;
    bool output_policy_applied = false;
    if (!features.features_.empty()) {
        const auto configured = feature_manager.vqec_vision_ai_ftmgr_famgr_configure(
            features, catalog, deployment);
        if (configured.code_ != status_code::ok) {
            std::fprintf(stderr, "feature activation configure failed (%d): %s\n",
                static_cast<int>(configured.code_), configured.message_.c_str());
            return 1;
        }
        std::array<feature_activation_request,
            feature_activation_limits::g_max_associations> requests{};
        std::array<std::pair<std::uint16_t, std::uint16_t>,
            feature_activation_limits::g_max_associations> request_slots{};
        std::array<std::vector<std::string>,
            feature_activation_limits::g_max_associations> authorized_output_scopes{};
        std::uint16_t request_count = 0;
        for (std::uint16_t source_slot = 0; source_slot < activation.source_count_; ++source_slot) {
            const auto& source = deployment.sources_[source_slot];
            for (const auto& feature : features.features_) {
                if (feature.input_mode_ != feature_input_mode::single_model ||
                    feature.model_dependencies_.size() != 1) {
                    continue;
                }
                const auto slot = vqec_vision_ai_appl_svcmn_model_slot(
                    source, feature.model_dependencies_[0].model_id_);
                if (slot == g_invalid_model_slot) {
                    continue;
                }
                auto& request = requests[request_count];
                request.source_id_ = source.source_id_;
                request.feature_id_ = feature.feature_id_;
                if (startup.has_usecase_control) {
                    const auto projected =
                        vqec_vision_ai_core_ucact_project_feature_association(
                            startup.usecase_control.catalog_, startup.usecase_activation,
                            deployment, source.source_id_, feature, request.association_);
                    if (projected.code_ != status_code::ok) {
                        std::fprintf(stderr, "feature association rejected (%d): %s\n",
                            static_cast<int>(projected.code_), projected.message_.c_str());
                        return 1;
                    }
                    request.desired_enabled_ = request.association_.desired_enabled_;
                    request.entitlement_granted_ =
                        request.association_.entitlement_granted_;
                    request.resource_admitted_ = request.association_.resource_admitted_;
                } else {
                    const auto auth = vqec_vision_ai_appl_svstr_resolve_feature_authority(
                        startup, args, source.source_id_, feature.feature_id_);
                    request.desired_enabled_ = auth.desired_enabled_;
                    request.entitlement_granted_ = auth.entitlement_granted_;
                    request.resource_admitted_ = auth.resource_admitted_;
                }
                request.configuration_.schema_id_ = feature.configuration_schema_;
                request.configuration_.revision_ = startup.has_usecase_control ?
                    startup.usecase_activation.config_revision_ :
                    service_harness::g_config_revision;
                if (startup.has_runtime_control && request.desired_enabled_ &&
                    request.entitlement_granted_ && request.resource_admitted_) {
                    const auto resolved =
                        vqec_vision_ai_appl_svcmn_resolve_runtime_feature_configuration(
                            startup, source.source_id_, feature,
                            request.configuration_, authorized_output_scopes[request_count]);
                    if (resolved.code_ != status_code::ok) {
                        std::fprintf(stderr,
                            "runtime feature configuration rejected (%d): %s\n",
                            static_cast<int>(resolved.code_),
                            resolved.message_.c_str());
                        return 1;
                    }
                    request.association_.config_revision_ =
                        request.configuration_.revision_;
                }
                request_slots[request_count] = {source_slot, slot};
                ++request_count;
            }
        }
        if (request_count != 0) {
            feature_activation_snapshot snapshot;
            const auto reconciled = feature_manager.vqec_vision_ai_ftmgr_famgr_reconcile(
                requests, request_count, feature_registry, snapshot);
            if (reconciled.code_ != status_code::ok) {
                std::fprintf(stderr, "feature activation reconcile failed (%d): %s\n",
                    static_cast<int>(reconciled.code_), reconciled.message_.c_str());
                return 1;
            }
            // Explicit output entitlement for the wired associations that are actually ready.
            output_policy policy;
            policy.revision_ = startup.has_usecase_control ?
                startup.usecase_activation.policy_revision_ :
                service_harness::g_policy_revision;
            policy.not_before_ns_ = 0;
            policy.expires_ns_ = service_harness::g_policy_expiry_ns;
            for (std::uint16_t index = 0; index < request_count; ++index) {
                const auto* record = feature_manager.vqec_vision_ai_ftmgr_famgr_get_record(index);
                if (record != nullptr && record->state_ == feature_effective_state::ready) {
                    output_scope_rule rule;
                    rule.source_id_ = record->source_id_;
                    rule.feature_id_ = record->feature_id_;
                    if (startup.has_usecase_control) {
                        rule.attributes_ = startup.has_runtime_control
                            ? authorized_output_scopes[index]
                            : record->association_.attribute_scopes_;
                    } else {
                        rule.attributes_.push_back(attribute_schema_id);
                    }
                    policy.rules_.push_back(std::move(rule));
                }
            }
            for (const auto& source : deployment.sources_) {
                if (!vqec_vision_ai_appl_svstr_is_preview_authorized(
                        startup, args, source.source_id_)) {
                    continue;
                }
                output_scope_rule preview_rule;
                preview_rule.source_id_ = source.source_id_;
                preview_rule.feature_id_ = "preview";
                preview_rule.attributes_.push_back("overlay");
                policy.rules_.push_back(std::move(preview_rule));
            }
            if (fr_effectively_enabled) {
                vqec_vision_ai_appl_svcmn_append_fr_policy_rules(
                    policy, deployment, args.fr_feature_id, args.fr_identity_attribute);
            }
            const auto applied =
                output_policy_gate.vqec_vision_ai_core_otgat_apply_policy(policy, 0);
            if (applied.code_ != status_code::ok) {
                std::fprintf(stderr, "output policy apply failed (%d): %s\n",
                    static_cast<int>(applied.code_), applied.message_.c_str());
                return 1;
            }
            output_policy_applied = true;
            std::array<std::array<std::vector<feature_stage*>,
                deployment_limits::g_max_models_per_source>,
                deployment_limits::g_max_sources> stages_by_slot{};
            for (std::uint16_t index = 0; index < request_count; ++index) {
                auto* stage = feature_manager.vqec_vision_ai_ftmgr_famgr_get_stage(index);
                if (stage != nullptr) {
                    stages_by_slot[request_slots[index].first][request_slots[index].second]
                        .push_back(stage);
                }
            }
            for (std::uint16_t source_slot = 0; source_slot < activation.source_count_;
                 ++source_slot) {
                for (std::uint16_t model_slot = 0;
                     model_slot < deployment_limits::g_max_models_per_source; ++model_slot) {
                    auto& stages = stages_by_slot[source_slot][model_slot];
                    if (stages.empty() ||
                        stages.size() > feature_fanout_limits::g_max_feature_stages) {
                        continue;
                    }
                    std::array<feature_stage*,
                        feature_fanout_limits::g_max_feature_stages> stage_array{};
                    for (std::size_t index = 0; index < stages.size(); ++index) {
                        stage_array[index] = stages[index];
                    }
                    const std::size_t fanout_index =
                        static_cast<std::size_t>(source_slot) *
                            deployment_limits::g_max_models_per_source +
                        model_slot;
                    fanouts[fanout_index] = std::make_unique<feature_fanout>();
                    const auto fanout_configured =
                        fanouts[fanout_index]->vqec_vision_ai_appl_ftfan_configure(
                            stage_array, static_cast<std::uint16_t>(stages.size()));
                    if (fanout_configured.code_ != status_code::ok) {
                        std::fprintf(stderr, "feature fan-out configure failed (%d): %s\n",
                            static_cast<int>(fanout_configured.code_),
                            fanout_configured.message_.c_str());
                        return 1;
                    }
                    feature_wiring.sources_[source_slot].fanouts_[model_slot] =
                        fanouts[fanout_index].get();
                    has_feature_wiring = true;
                }
            }
        }
    }

    if (fr_effectively_enabled && !output_policy_applied) {
        output_policy policy;
        policy.revision_ = startup.has_usecase_control ?
            startup.usecase_activation.policy_revision_ :
            service_harness::g_policy_revision;
        policy.not_before_ns_ = 0;
        policy.expires_ns_ = service_harness::g_policy_expiry_ns;
        for (const auto& source : deployment.sources_) {
            if (!vqec_vision_ai_appl_svstr_is_preview_authorized(
                    startup, args, source.source_id_)) {
                continue;
            }
            output_scope_rule preview_rule;
            preview_rule.source_id_ = source.source_id_;
            preview_rule.feature_id_ = "preview";
            preview_rule.attributes_.push_back("overlay");
            policy.rules_.push_back(std::move(preview_rule));
        }
        vqec_vision_ai_appl_svcmn_append_fr_policy_rules(
            policy, deployment, args.fr_feature_id, args.fr_identity_attribute);
        const auto applied =
            output_policy_gate.vqec_vision_ai_core_otgat_apply_policy(policy, 0);
        if (applied.code_ != status_code::ok) {
            std::fprintf(stderr, "FR output policy apply failed (%d): %s\n",
                static_cast<int>(applied.code_), applied.message_.c_str());
            return 1;
        }
        output_policy_applied = true;
    }

    if (!output_policy_applied) {
        output_policy policy;
        policy.revision_ = startup.has_usecase_control ?
            startup.usecase_activation.policy_revision_ :
            service_harness::g_policy_revision;
        policy.not_before_ns_ = 0;
        policy.expires_ns_ = service_harness::g_policy_expiry_ns;
        for (const auto& source : deployment.sources_) {
            if (!vqec_vision_ai_appl_svstr_is_preview_authorized(
                    startup, args, source.source_id_)) {
                continue;
            }
            output_scope_rule preview_rule;
            preview_rule.source_id_ = source.source_id_;
            preview_rule.feature_id_ = "preview";
            preview_rule.attributes_.push_back("overlay");
            policy.rules_.push_back(std::move(preview_rule));
        }
        const auto applied =
            output_policy_gate.vqec_vision_ai_core_otgat_apply_policy(policy, 0);
        if (applied.code_ != status_code::ok) {
            std::fprintf(stderr, "baseline preview output policy apply failed (%d): %s\n",
                static_cast<int>(applied.code_), applied.message_.c_str());
            return 1;
        }
        output_policy_applied = true;
    }


    if (has_feature_wiring) {
        feature_manager.vqec_vision_ai_ftmgr_famgr_freeze();
    }
    std::unique_ptr<runtime_composition_bundle> bundle;
    const auto created = vqec_vision_ai_appl_rcfac_create_bundle(
        deployment, catalog, activation, decoders, trackers, bundle,
        has_feature_wiring ? &feature_wiring : nullptr);
    if (created.code_ != status_code::ok) {
        std::fprintf(stderr, "runtime composition failed (%d): %s\n",
            static_cast<int>(created.code_), created.message_.c_str());
        return 1;
    }
    auto* executor = bundle->vqec_vision_ai_appl_rcfac_get_executor();
    if (executor == nullptr) {
        std::fprintf(stderr, "runtime composition returned no executor\n");
        return 1;
    }
    std::array<service_cascade_owner, deployment_limits::g_max_sources> cascade_owners;
    std::unique_ptr<embedding_index_port> recognition_index;
    std::unique_ptr<face_gallery_store_port> recognition_store;
    recognition_session recognition;
    std::unique_ptr<face_enrollment_controller> enrollment_controller;
    std::unique_ptr<production_offline_model> enrollment_detector_model;
    std::unique_ptr<production_offline_model> enrollment_embedding_model;
    std::unique_ptr<cascade_graph_session> enrollment_detector_graph_session;
    std::unique_ptr<cascade_graph_session> enrollment_embedding_graph_session;
    std::unique_ptr<single_image_inference> enrollment_detector;
    std::unique_ptr<cascade_coordinator> enrollment_cascade;
    std::unique_ptr<image_path_authorizer> enrollment_path_authorizer;
    std::unique_ptr<qcom_face_enrollment_image_source> enrollment_image_source;
    std::unique_ptr<face_enrollment_image_pipeline> enrollment_image_pipeline;
    face_enrollment_port* enrollment_port = nullptr;
    const auto stop_enrollment_graphs = [&]() {
            return vqec_vision_ai_appl_svcsc_stop_graph_sessions(
            std::array<cascade_graph_session*, 2>{
                enrollment_detector_graph_session.get(),
                enrollment_embedding_graph_session.get()});
    };
#if defined(VQEC_VISION_AI_HAS_FACE_ENROLLMENT_DBUS)
    std::unique_ptr<face_enrollment_dbus_server> enrollment_dbus;
#endif
    bool recognition_enabled = false;
    if (use_production_platform) {
        const auto cascade_prepared = vqec_vision_ai_appl_svcsc_prepare_owners(
            deployment, catalog, production, cascade_owners);
        if (cascade_prepared.code_ != status_code::ok) {
            std::fprintf(stderr, "cascade preparation failed (%d): %s\n",
                static_cast<int>(cascade_prepared.code_),
                cascade_prepared.message_.c_str());
            return 1;
        }
        const auto cascade_started =
            vqec_vision_ai_appl_svcsc_start_graphs(cascade_owners);
        if (cascade_started.code_ != status_code::ok) {
            std::fprintf(stderr, "cascade graph startup failed (%d): %s\n",
                static_cast<int>(cascade_started.code_),
                cascade_started.message_.c_str());
            (void)vqec_vision_ai_appl_svcsc_stop_graphs(cascade_owners);
            return 1;
        }
        for (std::uint16_t source_slot = 0; source_slot < deployment.sources_.size();
             ++source_slot) {
            auto& owner = cascade_owners[source_slot];
            if (owner.graph_session_ == nullptr) {
                continue;
            }
            auto* source_session =
                bundle->vqec_vision_ai_appl_rcfac_get_session(source_slot);
            if (source_session == nullptr || owner.model_ == nullptr) {
                std::fprintf(stderr, "cascade source owner is incomplete\n");
                (void)vqec_vision_ai_appl_svcsc_stop_graphs(cascade_owners);
                return 1;
            }
            cascade_coordinator_config coordinator_config;
            coordinator_config.aligner_ = owner.binding_.aligner_;
            coordinator_config.lease_ = source_session;
            coordinator_config.embedding_graph_ = owner.binding_.graph_;
            coordinator_config.embedding_decoder_ = owner.binding_.decoder_;
            coordinator_config.template_ = owner.binding_.alignment_;
            coordinator_config.normalize_offset_ = owner.binding_.preprocess_.offset_;
            coordinator_config.normalize_scale_ = owner.binding_.preprocess_.scale_;
            coordinator_config.cycle_id_ =
                (static_cast<std::uint64_t>(source_slot) + 1U) *
                service_harness::g_cycle_id_stride;
            coordinator_config.job_timeout_ns_ =
                submission_limits::g_default_job_timeout_ns;
            coordinator_config.max_tasks_per_frame_ =
                deployment.sources_[source_slot].cascade_.tasks_per_frame_;
            coordinator_config.control_budget_ns_ =
                cascade_coordinator_limits::g_default_control_budget_ns;
            coordinator_config.track_refresh_interval_ns_ =
                cascade_coordinator_limits::g_default_track_refresh_interval_ns;
            owner.worker_ = std::make_unique<cascade_execution_worker>();
            const auto worker_configured =
                owner.worker_->vqec_vision_ai_appl_cxwrk_configure(coordinator_config);
            if (worker_configured.code_ != status_code::ok) {
                std::fprintf(stderr, "cascade worker configure failed (%d): %s\n",
                    static_cast<int>(worker_configured.code_),
                    worker_configured.message_.c_str());
                (void)vqec_vision_ai_appl_svcsc_stop_graphs(cascade_owners);
                return 1;
            }
            const auto worker_started = owner.worker_->vqec_vision_ai_appl_cxwrk_start();
            if (worker_started.code_ != status_code::ok) {
                std::fprintf(stderr, "cascade worker start failed (%d): %s\n",
                    static_cast<int>(worker_started.code_), worker_started.message_.c_str());
                (void)vqec_vision_ai_appl_svcsc_stop_graphs(cascade_owners);
                return 1;
            }
            const auto cascade_bound =
                executor->vqec_vision_ai_appl_rtexe_bind_cascade_worker(
                    source_slot, owner.root_model_slot_, *owner.worker_);
            if (cascade_bound.code_ != status_code::ok) {
                std::fprintf(stderr, "runtime cascade binding failed (%d): %s\n",
                    static_cast<int>(cascade_bound.code_),
                    cascade_bound.message_.c_str());
                (void)vqec_vision_ai_appl_svcsc_stop_graphs(cascade_owners);
                return 1;
            }
        }
    }
    if (fr_effectively_enabled) {
        const service_cascade_owner* recognition_owner = nullptr;
        std::uint16_t recognition_source_slot = g_invalid_model_slot;
        std::uint16_t candidate_source_slot = 0;
        for (const auto& owner : cascade_owners) {
            if (owner.model_ != nullptr && owner.binding_.embedding_dimensions_ != 0) {
                recognition_owner = &owner;
                recognition_source_slot = candidate_source_slot;
                break;
            }
            ++candidate_source_slot;
        }
        if (recognition_owner == nullptr) {
            std::fprintf(stderr, "FR configuration requires an active embedding model\n");
            (void)vqec_vision_ai_appl_svcsc_stop_graphs(cascade_owners);
            return 1;
        }
#if defined(VQEC_VISION_AI_HAS_ZVEC)
        if (use_production_platform) {
            recognition_index = std::make_unique<zvec_embedding_index>(
                args.fr_gallery_path, zvec_existing_collection_policy::rebuild);
        } else
#endif
        {
            recognition_index = std::make_unique<exact_embedding_index>();
        }
        recognition_session_config recognition_config;
        recognition_config.index_.model_id_ = recognition_owner->binding_.model_id_;
        recognition_config.index_.model_version_ = recognition_owner->binding_.model_version_;
        recognition_config.index_.dimensions_ = recognition_owner->binding_.embedding_dimensions_;
        if (args.fr_max_templates > face_gallery_limits::g_max_records /
                service_harness::g_fr_max_subjects) {
            std::fprintf(stderr, "FR gallery capacity overflows the supported bound\n");
            (void)vqec_vision_ai_appl_svcsc_stop_graphs(cascade_owners);
            return 1;
        }
        recognition_config.index_.capacity_ = args.fr_max_templates *
            service_harness::g_fr_max_subjects;
        recognition_config.index_.max_results_ = args.fr_top_k;
        recognition_config.index_.initial_revision_ = service_harness::g_initial_gallery_revision;
        recognition_config.policy_.minimum_similarity_ = args.fr_minimum_similarity;
        recognition_config.policy_.minimum_subject_margin_ = args.fr_subject_margin;
        recognition_config.policy_.max_subjects_ = recognition_limits::g_max_subjects;
        recognition_config.max_templates_per_subject_ = args.fr_max_templates;
        recognition_config.search_top_k_ = args.fr_top_k;
        recognition_config.search_minimum_similarity_ = args.fr_minimum_similarity;
        encrypted_face_gallery_store_config store_config;
        store_config.directory_path_ = args.fr_protected_directory;
        store_config.gallery_file_name_ = args.fr_gallery_file_name;
        store_config.key_file_name_ = args.fr_key_file_name;
        store_config.lock_file_name_ = args.fr_lock_file_name;
        store_config.expected_owner_uid_ = static_cast<std::uint32_t>(::geteuid());
        store_config.max_serialized_bytes_ = args.fr_store_max_bytes;
        recognition_store = std::make_unique<encrypted_face_gallery_store>(
            std::move(store_config));
        face_gallery_config gallery_config;
        gallery_config.gallery_id_ = args.fr_gallery_id;
        gallery_config.model_id_ = recognition_owner->binding_.model_id_;
        gallery_config.model_version_ = recognition_owner->binding_.model_version_;
        gallery_config.preprocess_revision_ = args.fr_preprocess_revision;
        gallery_config.dimensions_ = recognition_owner->binding_.embedding_dimensions_;
        gallery_config.capacity_ = recognition_config.index_.capacity_;
        gallery_config.max_templates_per_subject_ = args.fr_max_templates;
        const auto recognition_configured =
            recognition.vqec_vision_ai_embed_rcses_configure_persistent(
                *recognition_index, *recognition_store, recognition_config, gallery_config);
        if (recognition_configured.code_ != status_code::ok) {
            std::fprintf(stderr, "FR configuration failed (%d): %s\n",
                static_cast<int>(recognition_configured.code_),
                recognition_configured.message_.c_str());
            (void)vqec_vision_ai_appl_svcsc_stop_graphs(cascade_owners);
            return 1;
        }
        enrollment_controller = std::make_unique<face_enrollment_controller>(recognition);
        enrollment_port = enrollment_controller.get();
        if (args.enrollment_dbus) {
            const auto& source = deployment.sources_[recognition_source_slot];
            const auto* secondary_model = recognition_owner->model_;
            const auto* primary_model = secondary_model != nullptr &&
                    secondary_model->depends_on_.size() == 1U ?
                vqec_vision_ai_appl_svcmn_find_model(
                    catalog, secondary_model->depends_on_[0].model_id_) : nullptr;
            if (primary_model == nullptr || secondary_model == nullptr) {
                std::fprintf(stderr, "enrollment model dependency is incomplete\n");
                (void)vqec_vision_ai_appl_svcsc_stop_graphs(cascade_owners);
                return 1;
            }
            auto prepared = production.vqec_vision_ai_appl_pdplt_create_offline_model(
                recognition_source_slot, primary_model->model_id_, enrollment_detector_model);
            if (prepared.code_ == status_code::ok) {
                prepared = production.vqec_vision_ai_appl_pdplt_create_offline_model(
                    recognition_source_slot, secondary_model->model_id_,
                    enrollment_embedding_model);
            }
            if (prepared.code_ == status_code::ok) {
                prepared = vqec_vision_ai_appl_svcsc_make_offline_graph_session(
                    source, *primary_model, *enrollment_detector_model,
                    enrollment_detector_graph_session);
            }
            if (prepared.code_ == status_code::ok) {
                prepared = vqec_vision_ai_appl_svcsc_make_offline_graph_session(
                    source, *secondary_model, *enrollment_embedding_model,
                    enrollment_embedding_graph_session);
            }
            const std::array<cascade_graph_session*, 2> enrollment_graph_sessions{
                enrollment_detector_graph_session.get(),
                enrollment_embedding_graph_session.get()};
            if (prepared.code_ == status_code::ok) {
                prepared = vqec_vision_ai_appl_svcsc_start_graph_sessions(
                    enrollment_graph_sessions);
            }
            if (prepared.code_ != status_code::ok) {
                std::fprintf(stderr, "enrollment graph startup failed (%d): %s\n",
                    static_cast<int>(prepared.code_), prepared.message_.c_str());
                (void)vqec_vision_ai_appl_svcsc_stop_graph_sessions(
                    enrollment_graph_sessions);
                (void)vqec_vision_ai_appl_svcsc_stop_graphs(cascade_owners);
                return 1;
            }

            enrollment_detector = std::make_unique<single_image_inference>();
            single_image_inference_config detector_config;
            detector_config.processor_ =
                enrollment_detector_model->vqec_vision_ai_appl_pdplt_get_processor();
            detector_config.graph_ =
                enrollment_detector_model->vqec_vision_ai_appl_pdplt_get_graph();
            detector_config.decoder_ =
                enrollment_detector_model->vqec_vision_ai_appl_pdplt_get_decoder();
            detector_config.plan_ =
                &enrollment_detector_model->vqec_vision_ai_appl_pdplt_get_binding().plan_;
            detector_config.geometry_ = {source.profile_.width_, source.profile_.height_};
            detector_config.camera_id_ = source.camera_id_;
            detector_config.channel_id_ = source.channel_id_;
            detector_config.cycle_id_ = vqec_vision_ai_appl_svcmn_monotonic_ns();
            detector_config.job_timeout_ns_ = submission_limits::g_default_job_timeout_ns;
            prepared = enrollment_detector->vqec_vision_ai_appl_siinf_configure(
                detector_config);

            enrollment_cascade = std::make_unique<cascade_coordinator>();
            cascade_coordinator_config cascade_config;
            const auto& embedding_binding =
                enrollment_embedding_model->vqec_vision_ai_appl_pdplt_get_binding();
            cascade_config.aligner_ =
                enrollment_embedding_model->vqec_vision_ai_appl_pdplt_get_aligner();
            cascade_config.embedding_graph_ =
                enrollment_embedding_model->vqec_vision_ai_appl_pdplt_get_graph();
            cascade_config.embedding_decoder_ = enrollment_embedding_model->
                vqec_vision_ai_appl_pdplt_get_embedding_decoder();
            cascade_config.template_ = embedding_binding.alignment_;
            cascade_config.normalize_offset_ = embedding_binding.preprocess_.offset_;
            cascade_config.normalize_scale_ = embedding_binding.preprocess_.scale_;
            cascade_config.cycle_id_ = detector_config.cycle_id_ + 1U;
            cascade_config.job_timeout_ns_ = submission_limits::g_default_job_timeout_ns;
            cascade_config.max_tasks_per_frame_ = 1U;
            cascade_config.control_budget_ns_ =
                cascade_coordinator_limits::g_default_control_budget_ns;
            if (prepared.code_ == status_code::ok) {
                prepared = enrollment_cascade->vqec_vision_ai_appl_cscrd_configure(
                    cascade_config);
            }

            enrollment_path_authorizer = std::make_unique<image_path_authorizer>();
            if (prepared.code_ == status_code::ok) {
                prepared = enrollment_path_authorizer->vqec_vision_ai_fwctl_ipath_configure(
                    {args.enrollment_image_roots, args.enrollment_max_image_bytes});
            }
            enrollment_image_source = std::make_unique<qcom_face_enrollment_image_source>(
                qcom_face_enrollment_image_source_config{args.enrollment_jpeg_decoder,
                    args.enrollment_converter, args.enrollment_scaler,
                    args.enrollment_transform, args.enrollment_transform_engine,
                    args.enrollment_max_image_bytes, args.enrollment_image_timeout_ms, true});
            enrollment_image_pipeline =
                std::make_unique<face_enrollment_image_pipeline>();
            face_enrollment_image_pipeline_config pipeline_config;
            pipeline_config.controller_ = enrollment_controller.get();
            pipeline_config.path_authorizer_ = enrollment_path_authorizer.get();
            pipeline_config.image_source_ = enrollment_image_source.get();
            pipeline_config.detector_ = enrollment_detector.get();
            pipeline_config.cascade_ = enrollment_cascade.get();
            pipeline_config.geometry_ = detector_config.geometry_;
            pipeline_config.source_epoch_ = detector_config.cycle_id_;
            pipeline_config.source_id_ = source.source_id_;
            pipeline_config.camera_id_ = source.camera_id_;
            pipeline_config.channel_id_ = source.channel_id_;
            if (prepared.code_ == status_code::ok) {
                prepared = enrollment_image_pipeline->vqec_vision_ai_appl_feipl_configure(
                    pipeline_config);
            }
            if (prepared.code_ != status_code::ok) {
                std::fprintf(stderr, "enrollment image pipeline failed (%d): %s\n",
                    static_cast<int>(prepared.code_), prepared.message_.c_str());
                (void)vqec_vision_ai_appl_svcsc_stop_graph_sessions(
                    enrollment_graph_sessions);
                (void)vqec_vision_ai_appl_svcsc_stop_graphs(cascade_owners);
                return 1;
            }
            enrollment_port = enrollment_image_pipeline.get();
        }
#if defined(VQEC_VISION_AI_HAS_FACE_ENROLLMENT_DBUS)
        if (args.enrollment_dbus) {
            enrollment_dbus = std::make_unique<face_enrollment_dbus_server>();
            face_enrollment_dbus_config dbus_config;
            dbus_config.trusted_peer_bus_name_ = args.enrollment_peer_name;
            dbus_config.rpc_timeout_ms_ = args.enrollment_rpc_timeout_ms;
            dbus_config.max_callbacks_per_poll_ = args.enrollment_callbacks_per_poll;
            dbus_config.use_session_bus_ = args.enrollment_dbus_session_bus;
            const auto opened = enrollment_dbus->vqec_vision_ai_fwctl_fedbs_open(
                *enrollment_port, dbus_config);
            if (opened.code_ != status_code::ok) {
                std::fprintf(stderr, "face enrollment DBus failed (%d): %s\n",
                    static_cast<int>(opened.code_), opened.message_.c_str());
                (void)stop_enrollment_graphs();
                (void)vqec_vision_ai_appl_svcsc_stop_graphs(cascade_owners);
                return 1;
            }
        }
#else
        if (args.enrollment_dbus) {
            std::fprintf(stderr,
                "face enrollment DBus was requested but adapter is not built\n");
            (void)stop_enrollment_graphs();
            (void)vqec_vision_ai_appl_svcsc_stop_graphs(cascade_owners);
            return 1;
        }
#endif
        recognition_enabled = true;
    }
    if (output_policy_applied) {
        executor->vqec_vision_ai_appl_rtexe_bind_event_delivery(
            output_policy_gate, active_event_sink);
    }
    const auto activated =
        bundle->vqec_vision_ai_appl_rcfac_get_composition()->vqec_vision_ai_cntr_acomp_activate();
    if (activated.code_ != status_code::ok) {
        std::fprintf(stderr, "composition activation failed (%d): %s\n",
            static_cast<int>(activated.code_), activated.message_.c_str());
        (void)stop_enrollment_graphs();
        (void)vqec_vision_ai_appl_svcsc_stop_graphs(cascade_owners);
        return 1;
    }

    std::uint64_t now_ns = vqec_vision_ai_appl_svcmn_monotonic_ns();
    std::uint64_t steps = 0;
    std::uint32_t routed_source_mask = 0;
    status_code first_error_code = status_code::ok;
    std::array<model_observation_cache, deployment_limits::g_max_sources>
        latest_model_observations;
    std::array<observation_batch, deployment_limits::g_max_sources>
        latest_overlay_observations;
    std::array<bool, deployment_limits::g_max_sources> cascade_error_reported{};
    std::array<std::unordered_map<std::uint64_t, std::string>, deployment_limits::g_max_sources>
        active_track_labels;
    bool reconcile_requested = false;
    bool generation_published = _control_manager == nullptr;
    while (!g_stop_requested && (args.max_steps == 0 || steps < args.max_steps)) {
        if (_poll_control) {
            _poll_control();
        }
        if (_is_runtime_reconcile_requested &&
            _is_runtime_reconcile_requested()) {
            reconcile_requested = true;
            break;
        }
        if (_control_manager != nullptr && generation_published &&
            _control_manager->vqec_vision_ai_ftmgr_ucmgr_has_pending()) {
            reconcile_requested = true;
            break;
        }
        const auto clock_now = vqec_vision_ai_appl_svcmn_monotonic_ns();
        now_ns = clock_now > now_ns ? clock_now :
            now_ns + args.runtime_step_interval_ns;
        runtime_executor_report report;
        const auto stepped = executor->vqec_vision_ai_appl_rtexe_step(now_ns, report);
        // In threaded source mode an OK step consumed the worker completion and did not
        // enqueue a replacement in the same call. Session-owned preview/cascade state is
        // therefore quiescent until the next loop iteration. Pending can mean a worker is
        // active, so the control/output thread must not touch the session mailbox then.
        const bool source_session_quiescent =
            stepped.code_ == status_code::ok && !report.has_cascade_;
        if (report.first_error_code_ != status_code::ok && first_error_code == status_code::ok) {
            first_error_code = report.first_error_code_;
        }
        if (!generation_published && first_error_code == status_code::ok) {
            bool all_sources_running = true;
            for (std::uint16_t source_slot = 0;
                 source_slot < deployment.sources_.size(); ++source_slot) {
                const auto* session = bundle->vqec_vision_ai_appl_rcfac_get_session(source_slot);
                if (session == nullptr ||
                    session->vqec_vision_ai_appl_srcsn_get_health().phase_ !=
                        source_session_phase::running) {
                    all_sources_running = false;
                    break;
                }
            }
            if (all_sources_running) {
                const auto published = _pending_control_revision == 0
                    ? (_runtime_generation == 1
                        ? _control_manager->vqec_vision_ai_ftmgr_ucmgr_publish_initial(
                              _runtime_generation)
                        : status{})
                    : _control_manager->vqec_vision_ai_ftmgr_ucmgr_publish_pending(
                          _pending_control_revision, _runtime_generation);
                if (published.code_ != status_code::ok) {
                    first_error_code = published.code_;
                    break;
                }
                generation_published = true;
            }
        }
        if (stepped.code_ == status_code::ok) {
            std::array<observation_batch, deployment_limits::g_max_models_per_source> tracked;
            std::array<feature_event_batch, feature_fanout_limits::g_max_feature_stages> events;
            std::vector<embedding_result> embeddings;
            if (recognition_enabled) {
                embeddings.reserve(observation_limits::g_max_observations);
            }
            runtime_executor_report taken;
            const auto taken_status = recognition_enabled
                ? executor->vqec_vision_ai_appl_rtexe_take_result_with_embeddings(
                    tracked, events, embeddings, taken)
                : executor->vqec_vision_ai_appl_rtexe_take_result(
                    tracked, events, taken);
            if (taken_status.code_ == status_code::ok) {
                routed_source_mask |= 1U << taken.source_index_;
                feature_dispatch_report dispatch_report;
                if (taken.has_feature_fanout_ && has_feature_wiring) {
                    const auto dispatched =
                        executor->vqec_vision_ai_appl_rtexe_dispatch_events(
                            events, taken.source_index_, taken.model_slot_,
                            taken.captured_policy_revision_, taken.features_.processed_mask_,
                            now_ns, dispatch_report);
                    if (dispatched.code_ != status_code::ok) {
                        std::fprintf(stderr, "event delivery rejected: %s\n",
                            dispatched.message_.c_str());
                        if (metadata_required) {
                            first_error_code = dispatched.code_;
                            break;
                        }
                    }
                }
                if (recognition_enabled && !embeddings.empty() &&
                    taken.source_index_ < deployment_limits::g_max_sources) {
                    if (enrollment_port != nullptr) {
                        face_enrollment_status enrollment_status;
                        const auto root_slot = cascade_owners[taken.source_index_].root_model_slot_;
                        const auto face_count = root_slot < tracked.size()
                            ? tracked[root_slot].observations_.size() : 0;
                        const auto accepted = enrollment_port->
                            vqec_vision_ai_ports_fenrl_accept_batch(
                                deployment.sources_[taken.source_index_].source_id_,
                                embeddings, face_count, enrollment_status);
                        if (accepted.code_ != status_code::ok &&
                            accepted.code_ != status_code::pending &&
                            accepted.code_ != status_code::invalid_argument &&
                            accepted.code_ != status_code::invalid_state) {
                            std::fprintf(stderr, "FR enrollment failed (%d): %s\n",
                                static_cast<int>(accepted.code_), accepted.message_.c_str());
                        }
                    }
                    std::vector<recognition_match_result> recognition_results;
                    recognition_results.reserve(embeddings.size());
                    const auto recognized = recognition.vqec_vision_ai_embed_rcses_recognize_batch(
                        embeddings, recognition_results);
                    if (recognized.code_ != status_code::ok) {
                        std::fprintf(stderr, "FR recognition failed (%d): %s\n",
                            static_cast<int>(recognized.code_), recognized.message_.c_str());
                    } else {
                        const auto& owner = cascade_owners[taken.source_index_];
                        if (owner.root_model_slot_ < deployment_limits::g_max_models_per_source) {
                            const output_authorization identity_scope{
                                taken.captured_policy_revision_,
                                deployment.sources_[taken.source_index_].source_id_,
                                args.fr_feature_id, {args.fr_identity_attribute}};
                            const auto authorized = output_policy_gate
                                .vqec_vision_ai_core_otgat_authorize(identity_scope, now_ns);
                            if (authorized.code_ != status_code::ok) {
                                std::fprintf(stderr,
                                    "FR identity output denied (%d): %s "
                                    "(captured_policy=%llu active_policy=%llu)\n",
                                    static_cast<int>(authorized.code_), authorized.message_.c_str(),
                                    static_cast<unsigned long long>(taken.captured_policy_revision_),
                                    static_cast<unsigned long long>(output_policy_gate
                                        .vqec_vision_ai_core_otgat_get_revision()));
                            } else {
                                const auto labelled = recognition
                                    .vqec_vision_ai_embed_rcses_apply_labels(
                                        recognition_results, tracked[owner.root_model_slot_]);
                                if (labelled.code_ != status_code::ok) {
                                    std::fprintf(stderr, "FR label correlation failed (%d): %s\n",
                                        static_cast<int>(labelled.code_), labelled.message_.c_str());
                                } else {
                                    for (const auto& match : recognition_results) {
                                        if (match.decision_ == recognition_decision::known &&
                                            !match.subject_ref_.empty()) {
                                            active_track_labels[taken.source_index_][match.track_id_] =
                                                match.subject_ref_;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                if (metadata != nullptr &&
                    taken.source_index_ < deployment.sources_.size() &&
                    taken.model_slot_ < tracked.size()) {
                    const auto& metadata_source = deployment.sources_[taken.source_index_];
                    const auto* profile = metadata->vqec_vision_ai_appl_mdrun_find_source(
                        metadata_source.source_id_);
                    const auto* model = taken.model_slot_ < metadata_source.model_ids_.size()
                        ? vqec_vision_ai_appl_svcmn_find_model(
                              catalog, metadata_source.model_ids_[taken.model_slot_])
                        : nullptr;
                    if (profile != nullptr && model != nullptr &&
                        profile->trajectory_model_id_ == model->model_id_) {
                        const output_authorization trajectory_scope{
                            taken.captured_policy_revision_, metadata_source.source_id_,
                            profile->trajectory_authorization_feature_id_,
                            {profile->trajectory_authorization_attribute_id_}};
                        const bool trajectory_authorized = output_policy_gate
                            .vqec_vision_ai_core_otgat_authorize(
                                trajectory_scope, now_ns).code_ == status_code::ok;
                        const auto submitted = metadata->
                            vqec_vision_ai_appl_mdrun_submit_observations(
                                metadata_source.source_id_,
                                model->model_id_ + "." + model->model_version_,
                                tracker_contract, tracked[taken.model_slot_],
                                trajectory_authorized);
                        if (submitted.code_ != status_code::ok &&
                            submitted.code_ != status_code::unauthorized &&
                            submitted.code_ != status_code::unsupported) {
                            std::fprintf(stderr, "metadata trajectory failed (%d): %s\n",
                                static_cast<int>(submitted.code_),
                                submitted.message_.c_str());
                            if (metadata_required) {
                                first_error_code = submitted.code_;
                                break;
                            }
                        }
                        const auto maintained = metadata->vqec_vision_ai_appl_mdrun_maintain(
                            tracked[taken.model_slot_].frame_.source_pts_ns_);
                        if (maintained.code_ != status_code::ok) {
                            std::fprintf(stderr, "metadata maintenance failed (%d): %s\n",
                                static_cast<int>(maintained.code_),
                                maintained.message_.c_str());
                            if (metadata_required) {
                                first_error_code = maintained.code_;
                                break;
                            }
                        }
                    }
                }
                const auto tracked_count = tracked[taken.model_slot_].observations_.size();
                if (use_production_platform &&
                    taken.source_index_ < deployment_limits::g_max_sources &&
                    taken.model_slot_ < deployment_limits::g_max_models_per_source) {
                    const auto& cascade_owner = cascade_owners[taken.source_index_];
                    if (taken.model_slot_ == cascade_owner.root_model_slot_) {
                        for (auto& obs : tracked[taken.model_slot_].observations_) {
                            if (obs.box_.label_.empty()) {
                                const auto it = active_track_labels[taken.source_index_].find(obs.track_id_);
                                if (it != active_track_labels[taken.source_index_].end()) {
                                    obs.box_.label_ = it->second;
                                }
                            }
                        }
                        if (active_track_labels[taken.source_index_].size() > g_max_active_track_labels) {
                            active_track_labels[taken.source_index_].clear();
                        }
                    }
                    latest_model_observations[taken.source_index_][taken.model_slot_] =
                        std::move(tracked[taken.model_slot_]);
                    vqec_vision_ai_appl_svcmn_merge_observations(
                        latest_model_observations[taken.source_index_],
                        static_cast<std::uint16_t>(
                            deployment.sources_[taken.source_index_].model_ids_.size()),
                        now_ns, service_harness::g_overlay_max_age_ns,
                        latest_overlay_observations[taken.source_index_]);
                }
                static std::uint64_t s_last_routed_log_ns = 0;
                if (now_ns - s_last_routed_log_ns >= g_routed_log_interval_ns) {
                    std::printf("routed source=%u model=%u tracked=%zu accepted=%u "
                        "cascade_accepted=%u embedded=%u cascade_failed=%u\n",
                        static_cast<unsigned>(taken.source_index_),
                        static_cast<unsigned>(taken.model_slot_),
                        tracked_count,
                        static_cast<unsigned>(dispatch_report.accepted_),
                        static_cast<unsigned>(taken.cascade_.accepted_),
                        static_cast<unsigned>(taken.cascade_.embedded_),
                        static_cast<unsigned>(taken.cascade_.failed_));
                    s_last_routed_log_ns = now_ns;
                }
                if (taken.cascade_.failed_ != 0 &&
                    taken.source_index_ < cascade_owners.size() &&
                    !cascade_error_reported[taken.source_index_] &&
                    cascade_owners[taken.source_index_].coordinator_ != nullptr) {
                    const auto& cascade_error = cascade_owners[taken.source_index_]
                        .coordinator_->vqec_vision_ai_appl_cscrd_get_last_task_error();
                    std::fprintf(stderr, "cascade task failed (%d): %s\n",
                        static_cast<int>(cascade_error.code_), cascade_error.message_.c_str());
                    cascade_error_reported[taken.source_index_] = true;
                }
            }
        } else if (stepped.code_ != status_code::pending) {
            if (first_error_code == status_code::ok) {
                first_error_code = stepped.code_;
            }
            std::fprintf(stderr, "executor step failed (%d): %s\n",
                static_cast<int>(stepped.code_), stepped.message_.c_str());
            break;
        }
#if defined(VQEC_VISION_AI_HAS_FACE_ENROLLMENT_DBUS)
        if (enrollment_dbus != nullptr) {
            enrollment_dbus->vqec_vision_ai_fwctl_fedbs_poll();
        }
#endif
        if (enrollment_image_pipeline != nullptr &&
            enrollment_image_pipeline->vqec_vision_ai_appl_feipl_has_pending()) {
            const auto enrolled = enrollment_image_pipeline->
                vqec_vision_ai_appl_feipl_step(now_ns);
            if (enrolled.code_ != status_code::ok &&
                enrolled.code_ != status_code::pending) {
                std::fprintf(stderr, "enrollment image job failed (%d): %s\n",
                    static_cast<int>(enrolled.code_), enrolled.message_.c_str());
            }
        }
        if (use_production_platform &&
            (!args.use_session_workers || source_session_quiescent)) {
            for (std::uint16_t source_slot = 0;
                 source_slot < deployment.sources_.size(); ++source_slot) {
                auto* session = bundle->vqec_vision_ai_appl_rcfac_get_session(source_slot);
                raw_frame preview_frame;
                if (session == nullptr ||
                    session->vqec_vision_ai_appl_mmses_take_preview_frame(preview_frame)
                            .code_ != status_code::ok ||
                    !preview_frame.owner_) {
                    continue;
                }
                static std::uint32_t s_preview_phase[deployment_limits::g_max_sources] = {0};
                static bool s_preview_phase_init[deployment_limits::g_max_sources] = {false};
                const std::uint32_t source_fps =
                    (source_slot < deployment.sources_.size() &&
                     deployment.sources_[source_slot].profile_.fps_numerator_ > 0) ?
                        deployment.sources_[source_slot].profile_.fps_numerator_ : 30U;
                if (args.output_fps > 0 && args.output_fps < source_fps &&
                    source_slot < deployment_limits::g_max_sources) {
                    if (!s_preview_phase_init[source_slot]) {
                        s_preview_phase[source_slot] = source_fps;
                        s_preview_phase_init[source_slot] = true;
                    }
                    s_preview_phase[source_slot] += static_cast<std::uint32_t>(args.output_fps);
                    if (s_preview_phase[source_slot] >= source_fps) {
                        s_preview_phase[source_slot] -= source_fps;
                    } else {
                        continue;
                    }
                }
                auto& overlay = latest_overlay_observations[source_slot];
                if (overlay.frame_.source_epoch_ != 0 &&
                    overlay.frame_.source_epoch_ !=
                        preview_frame.descriptor_.session_epoch_) {
                    latest_model_observations[source_slot] = {};
                    overlay = {};
                }
                if (overlay.frame_.source_pts_ns_ != UINT64_MAX &&
                    preview_frame.descriptor_.pts_ns_ != UINT64_MAX &&
                    preview_frame.descriptor_.pts_ns_ != 0 &&
                    preview_frame.descriptor_.pts_ns_ > overlay.frame_.source_pts_ns_ &&
                    preview_frame.descriptor_.pts_ns_ - overlay.frame_.source_pts_ns_ > service_harness::g_overlay_max_age_ns) {
                    static std::uint64_t s_last_pts_drop_ns = 0;
                    if (now_ns - s_last_pts_drop_ns > 2000000000ULL) {
                        std::fprintf(stderr, "overlay dropped due to PTS delta > %llums (preview_pts=%llu, overlay_pts=%llu)\n",
                            static_cast<unsigned long long>(service_harness::g_overlay_max_age_ns / 1000000ULL),
                            static_cast<unsigned long long>(preview_frame.descriptor_.pts_ns_),
                            static_cast<unsigned long long>(overlay.frame_.source_pts_ns_));
                        s_last_pts_drop_ns = now_ns;
                    }
                    overlay = {};
                }
                prepared_overlay prepared;
                if (!output_policy_applied) {
                    continue;
                }
                observation_batch render_observations = overlay;
                if (render_observations.frame_.source_epoch_ == 0) {
                    render_observations.frame_.camera_id_ =
                        deployment.sources_[source_slot].camera_id_;
                    render_observations.frame_.channel_id_ =
                        deployment.sources_[source_slot].channel_id_;
                    render_observations.frame_.source_epoch_ =
                        preview_frame.descriptor_.session_epoch_;
                    render_observations.frame_.frame_id_ =
                        preview_frame.descriptor_.buffer_id_;
                    render_observations.frame_.source_pts_ns_ =
                        preview_frame.descriptor_.pts_ns_;
                    render_observations.geometry_.width_ =
                        preview_frame.descriptor_.width_;
                    render_observations.geometry_.height_ =
                        preview_frame.descriptor_.height_;
                }
                {
                    overlay_preparation_context prep_ctx;
                    prep_ctx.source_id_ = deployment.sources_[source_slot].source_id_;
                    prep_ctx.feature_id_ = "preview";
                    prep_ctx.policy_revision_ =
                        output_policy_gate.vqec_vision_ai_core_otgat_get_revision();
                    prep_ctx.prepared_monotonic_ns_ = now_ns;
                    prep_ctx.max_age_ns_ = service_harness::g_overlay_max_age_ns;
                    prep_ctx.attributes_ = {"overlay"};
                    const auto prep_status = vqec_vision_ai_outpt_ovrpr_prepare_authorized(
                        render_observations, prep_ctx, output_policy_gate, prepared);
                    if (prep_status.code_ != status_code::ok) {
                        static std::uint64_t s_last_prep_fail_ns = 0;
                        if (now_ns - s_last_prep_fail_ns > 2000000000ULL) {
                            std::fprintf(stderr, "overlay prepare failed (%d): %s\n",
                                static_cast<int>(prep_status.code_), prep_status.message_.c_str());
                            s_last_prep_fail_ns = now_ns;
                        }
                        continue;
                    }
                }
                const auto rendered = production.vqec_vision_ai_appl_pdplt_render(
                    source_slot, preview_frame, prepared);
                if (rendered.code_ != status_code::ok &&
                    rendered.code_ != status_code::pending) {
                    std::fprintf(stderr, "render failed (%d): %s\n",
                        static_cast<int>(rendered.code_), rendered.message_.c_str());
                }
            }
        }
        ++steps;
        // Asynchronous cascade completions drain immediately without consuming a frame period.
        if (report.has_cascade_) {
            continue;
        }
        // Pace the supervisor loop to wall time so camera frames, model cadence and the
        // AI-owned output stage progress at the source rate instead of spinning.
        const auto step_end_ns = vqec_vision_ai_appl_svcmn_monotonic_ns();
        const auto step_cost_ns = step_end_ns > clock_now ?
            step_end_ns - clock_now : 0U;
        if (step_cost_ns < args.runtime_step_interval_ns) {
            std::this_thread::sleep_for(
                std::chrono::nanoseconds(args.runtime_step_interval_ns - step_cost_ns));
        }
    }

    std::printf("stopping after %llu steps\n", static_cast<unsigned long long>(steps));
    const auto stop = executor->vqec_vision_ai_appl_rtexe_request_stop(now_ns);
    (void)stop;
    const auto cascade_workers_drained =
        vqec_vision_ai_appl_svcsc_drain_workers(cascade_owners, now_ns);
    if (cascade_workers_drained.code_ != status_code::ok &&
        first_error_code == status_code::ok) {
        first_error_code = cascade_workers_drained.code_;
    }
    bool stopped = false;
    const auto stop_drain_steps =
        service_harness::g_default_stop_timeout_ns / args.runtime_step_interval_ns +
        (service_harness::g_default_stop_timeout_ns % args.runtime_step_interval_ns != 0U
                ? 1U : 0U) +
        g_stop_drain_final_observation_steps;
    for (std::uint64_t drain = 0U; drain < stop_drain_steps && !stopped; ++drain) {
        // Drain must consume/discard a retained result, otherwise the executor refuses to
        // advance and a result arriving at stop would prevent reaching stopped.
        if (executor->vqec_vision_ai_appl_rtexe_has_pending()) {
            executor->vqec_vision_ai_appl_rtexe_discard_pending();
        }
        const auto clock_now = vqec_vision_ai_appl_svcmn_monotonic_ns();
        now_ns = clock_now > now_ns ? clock_now :
            now_ns + args.runtime_step_interval_ns;
        std::this_thread::sleep_for(
            std::chrono::nanoseconds(args.runtime_step_interval_ns));
        runtime_executor_report drain_report;
        const auto progressed = executor->vqec_vision_ai_appl_rtexe_step(now_ns, drain_report);
        if (drain_report.first_error_code_ != status_code::ok &&
            first_error_code == status_code::ok) {
            first_error_code = drain_report.first_error_code_;
        }
        if (progressed.code_ != status_code::ok && progressed.code_ != status_code::pending) {
            if (first_error_code == status_code::ok) {
                first_error_code = progressed.code_;
            }
            break;
        }
        stopped = executor->vqec_vision_ai_appl_rtexe_get_snapshot().state_ ==
            application_composition_state::stopped;
    }
    if (!stopped && first_error_code == status_code::ok) {
        first_error_code = status_code::timeout;
    }
    std::uint32_t routed_sources = 0;
    for (std::uint32_t mask = routed_source_mask; mask != 0; mask &= mask - 1U) {
        ++routed_sources;
    }
    const auto metrics = executor->vqec_vision_ai_appl_rtexe_get_metrics();
    const auto enrollment_stopped = stop_enrollment_graphs();
    if (enrollment_stopped.code_ != status_code::ok &&
        first_error_code == status_code::ok) {
        first_error_code = enrollment_stopped.code_;
    }
    const auto cascade_stopped =
        vqec_vision_ai_appl_svcsc_stop_graphs(cascade_owners);
    if (cascade_stopped.code_ != status_code::ok && first_error_code == status_code::ok) {
        first_error_code = cascade_stopped.code_;
    }
    if (metadata != nullptr) {
        const auto metadata_stopped = metadata->vqec_vision_ai_appl_mdrun_stop(true);
        if (metadata_stopped.code_ != status_code::ok &&
            first_error_code == status_code::ok) {
            first_error_code = metadata_stopped.code_;
        }
        const auto metadata_stats = metadata->vqec_vision_ai_appl_mdrun_get_stats();
        std::printf("metadata accepted=%llu committed=%llu rejected=%llu failed=%llu\n",
            static_cast<unsigned long long>(metadata_stats.accepted_records_),
            static_cast<unsigned long long>(metadata_stats.committed_records_),
            static_cast<unsigned long long>(metadata_stats.rejected_records_),
            static_cast<unsigned long long>(metadata_stats.failed_records_));
    }
    if (use_production_platform) {
        production_seam.vqec_vision_ai_outpt_evdsm_request_stop();
        production_seam.vqec_vision_ai_outpt_evdsm_discard_pending();
    }
    return vqec_vision_ai_appl_svcmn_report_and_decide(metrics, stopped,
        enrollment_stopped.code_ == status_code::ok,
        cascade_stopped.code_ == status_code::ok, routed_sources, generation_published,
        first_error_code, reconcile_requested, args.require_sources);
}

int vqec_vision_ai_appl_svcmn_run_service(int _argc, char** _argv) {
    parsed_arguments args;
    if (!vqec_vision_ai_appl_svopt_parse(_argc, _argv, args)) {
        return vqec_vision_ai_appl_svcmn_run_generation(
            _argc, _argv, nullptr, nullptr, nullptr, {}, {}, 0, 0);
    }
    if (!args.app_manager_dbus &&
        (!args.app_manager_service_name.empty() ||
            !args.app_manager_client_name.empty() ||
            !args.app_manager_object_path.empty() ||
            args.app_manager_rpc_timeout_ms != 0 ||
            args.app_manager_poll_interval_ms != 0)) {
        std::fprintf(stderr,
            "app manager DBus settings require --app-manager-dbus\n");
        return 2;
    }
    if (args.app_manager_dbus) {
        if (args.usecase_dbus || !args.production_mode ||
            args.usecase_snapshot_path.empty() || args.feature_catalog_path.empty() ||
            args.app_manager_service_name.empty() ||
            args.app_manager_client_name.empty() ||
            args.app_manager_object_path.empty() ||
            args.app_manager_rpc_timeout_ms <= 0 ||
            args.app_manager_poll_interval_ms == 0 ||
            args.app_manager_poll_interval_ms >
                service_options_limits::g_max_app_manager_poll_interval_ms) {
            std::fprintf(stderr,
                "app manager runtime control requires production mode, usecase and "
                "feature catalogs, service/object names and RPC timeout; it cannot be "
                "combined with legacy usecase DBus\n");
            return 2;
        }
#if !defined(VQEC_VISION_AI_HAS_APP_MANAGER_DBUS)
        std::fprintf(stderr,
            "app manager DBus was requested but adapter is not built\n");
        return 2;
#else
        app_manager_dbus_client client;
        runtime_control_snapshot runtime_control;
        runtime_control.schema_version_ = app_lifecycle_limits::g_schema_version;
        runtime_control.snapshot_revision_ = 1;
        runtime_control.inventory_revision_ = 1;
        runtime_control.entitlement_revision_ = 1;
        runtime_control.desired_revision_ = 1;
        const app_manager_dbus_client_config client_config{
            args.app_manager_service_name, args.app_manager_client_name,
            args.app_manager_object_path,
            args.app_manager_rpc_timeout_ms,
            args.app_manager_dbus_session_bus};
        runtime_control_snapshot fetched_snapshot;
        const auto fetched = client.vqec_vision_ai_fwctl_amdbs_fetch_snapshot(
            client_config, fetched_snapshot);
        if (fetched.code_ == status_code::ok) {
            runtime_control = std::move(fetched_snapshot);
        } else {
            std::fprintf(stderr,
                "app manager unavailable at startup; runtime remains disabled (%d): %s\n",
                static_cast<int>(fetched.code_), fetched.message_.c_str());
        }
        std::uint64_t generation = 1;
        for (;;) {
            bool reconcile = false;
            runtime_control_snapshot pending = runtime_control;
            std::uint64_t next_poll_ns = 0;
            const std::function<void()> poll_control = [&]() {
                const auto now = vqec_vision_ai_appl_svcmn_monotonic_ns();
                const auto interval_ns = static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::milliseconds(args.app_manager_poll_interval_ms)).count());
                if (next_poll_ns != 0 && now < next_poll_ns) {
                    return;
                }
                next_poll_ns = now <= UINT64_MAX - interval_ns ? now + interval_ns : UINT64_MAX;
                runtime_control_snapshot candidate;
                const auto refreshed = client.vqec_vision_ai_fwctl_amdbs_fetch_snapshot(
                    client_config, candidate);
                if (refreshed.code_ == status_code::ok &&
                    candidate.snapshot_revision_ > runtime_control.snapshot_revision_) {
                    pending = std::move(candidate);
                    reconcile = true;
                    return;
                }
                const auto utc_now_ns = static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count());
                pending = runtime_control;
                for (auto& association : pending.associations_) {
                    if (association.entitled_ &&
                        association.entitlement_expires_utc_ns_ != 0 &&
                        association.entitlement_expires_utc_ns_ <= utc_now_ns) {
                        association.entitled_ = false;
                        association.reason_code_ = "entitlement_expired";
                        reconcile = true;
                    }
                }
            };
            const std::function<bool()> is_reconcile_requested = [&]() {
                return reconcile;
            };
            const int outcome = vqec_vision_ai_appl_svcmn_run_generation(
                _argc, _argv, nullptr, &runtime_control, nullptr, poll_control,
                is_reconcile_requested, generation, 0);
            if (outcome == g_reconcile_generation_exit_code && reconcile &&
                generation != UINT64_MAX) {
                runtime_control = std::move(pending);
                ++generation;
                continue;
            }
            return outcome;
        }
#endif
    }
    if (!args.usecase_dbus) {
        if (!args.usecase_service_name.empty() || !args.usecase_object_path.empty() ||
            !args.usecase_peer_name.empty() || args.usecase_rpc_timeout_ms != 0 ||
            args.usecase_callbacks_per_poll != 0) {
            std::fprintf(stderr, "usecase DBus settings require --usecase-dbus\n");
            return 2;
        }
        return vqec_vision_ai_appl_svcmn_run_generation(
            _argc, _argv, nullptr, nullptr, nullptr, {}, {}, 0, 0);
    }
#if !defined(VQEC_VISION_AI_HAS_USECASE_CONTROL_DBUS)
    std::fprintf(stderr, "usecase DBus was requested but adapter is not built\n");
    return 2;
#else
    if (!args.production_mode || args.usecase_snapshot_path.empty() ||
        args.usecase_service_name.empty() || args.usecase_object_path.empty() ||
        args.usecase_peer_name.empty() || args.usecase_rpc_timeout_ms <= 0 ||
        args.usecase_callbacks_per_poll == 0) {
        std::fprintf(stderr, "usecase DBus requires production mode, trusted snapshot, "
            "service/object/peer names, RPC timeout and callback budget\n");
        return 2;
    }
    model_catalog catalog;
    deployment_config base_deployment;
    usecase_control_snapshot trusted;
    if (!vqec_vision_ai_appl_svstr_load_model_catalog(args.catalog_path, catalog) ||
        !vqec_vision_ai_appl_svstr_load_deployment(
            args.deployment_path, base_deployment) ||
        !vqec_vision_ai_appl_svstr_load_usecase_snapshot(
            args.usecase_snapshot_path, trusted)) {
        return 1;
    }
    usecase_control_manager manager;
    const auto configured = manager.vqec_vision_ai_ftmgr_ucmgr_configure(
        trusted, base_deployment, catalog);
    if (configured.code_ != status_code::ok) {
        std::fprintf(stderr, "usecase control configuration failed (%d): %s\n",
            static_cast<int>(configured.code_), configured.message_.c_str());
        return 1;
    }
    usecase_activation_snapshot activation;
    deployment_config current_deployment;
    const auto composed = vqec_vision_ai_core_ucact_compose_effective_deployment(
        base_deployment, catalog, trusted.catalog_, trusted.requests_,
        activation, current_deployment);
    if (composed.code_ != status_code::ok) {
        std::fprintf(stderr, "initial usecase deployment failed (%d): %s\n",
            static_cast<int>(composed.code_), composed.message_.c_str());
        return 1;
    }
    usecase_control_dbus_server control_dbus;
    const usecase_control_dbus_config dbus_config{
        args.usecase_service_name, args.usecase_object_path,
        args.usecase_peer_name, args.usecase_rpc_timeout_ms,
        args.usecase_callbacks_per_poll, args.usecase_dbus_session_bus};
    const auto opened = control_dbus.vqec_vision_ai_fwctl_ucdbs_open(
        manager, dbus_config);
    if (opened.code_ != status_code::ok) {
        std::fprintf(stderr, "usecase DBus failed (%d): %s\n",
            static_cast<int>(opened.code_), opened.message_.c_str());
        return 1;
    }
    const std::function<void()> poll_control = [&control_dbus]() {
        control_dbus.vqec_vision_ai_fwctl_ucdbs_poll();
    };
    deployment_config last_published_deployment = current_deployment;
    std::uint64_t generation = 1;
    std::uint64_t pending_revision = 0;
    for (;;) {
        const int outcome = vqec_vision_ai_appl_svcmn_run_generation(
            _argc, _argv, &current_deployment, nullptr, &manager, poll_control,
            {}, generation, pending_revision);
        if (outcome == g_reconcile_generation_exit_code) {
            last_published_deployment = current_deployment;
            usecase_control_snapshot pending;
            deployment_config candidate;
            const auto fetched = manager.vqec_vision_ai_ftmgr_ucmgr_get_pending(
                pending, candidate);
            if (fetched.code_ != status_code::ok ||
                generation == UINT64_MAX) {
                std::fprintf(stderr, "cannot fetch pending usecase generation\n");
                return 1;
            }
            current_deployment = std::move(candidate);
            pending_revision = pending.control_revision_;
            ++generation;
            continue;
        }
        if (outcome == g_recovery_required_exit_code) {
            std::fprintf(stderr, "runtime drain requires recovery; replacement blocked\n");
            return outcome;
        }
        if (pending_revision != 0 && manager.vqec_vision_ai_ftmgr_ucmgr_has_pending()) {
            const auto failed = manager.vqec_vision_ai_ftmgr_ucmgr_fail_pending(
                pending_revision, status_code::invalid_state);
            if (failed.code_ != status_code::ok) {
                return 1;
            }
            current_deployment = last_published_deployment;
            pending_revision = 0;
            if (g_stop_requested) {
                return outcome;
            }
            continue;
        }
        return outcome;
    }
#endif
}
