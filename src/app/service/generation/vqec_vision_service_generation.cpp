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
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <unistd.h>

#include "vqec_vision_service_generation.hpp"

#include "vqec_vision_service_options.hpp"
#include "vqec_vision_service_fixture.hpp"
#include "vqec_vision_service_feature_activation.hpp"
#include "vqec_vision_service_execution_loop.hpp"
#include "vqec_vision_service_platform.hpp"
#include "vqec_vision_service_shutdown.hpp"
#include "vqec_vision_service_enrollment_runtime.hpp"
#include "vqec_vision_service_startup.hpp"
#include "vqec_vision_deployment_config.hpp"
#include "vqec_vision_cascade_coordinator.hpp"
#include "vqec_vision_cascade_execution_worker.hpp"
#include "vqec_vision_service_cascade_runtime.hpp"
#include "vqec_vision_feature_catalog.hpp"
#include "vqec_vision_model_catalog.hpp"
#include "vqec_vision_model_package_registry.hpp"
#include "vqec_vision_reference_sink.hpp"
#include "vqec_vision_production_platform.hpp"
#include "vqec_vision_runtime_composition_factory.hpp"
#include "vqec_vision_exact_embedding_index.hpp"
#include "vqec_vision_recognition_session.hpp"
#include "vqec_vision_encrypted_face_gallery_store.hpp"
#if defined(VQEC_VISION_AI_HAS_ZVEC)
#include "vqec_vision_zvec_embedding_index.hpp"
#endif
#include "vqec_vision_event_delivery_seam.hpp"
#include "vqec_vision_service_output_runtime.hpp"

using namespace vqec::vision::ai;

namespace {

volatile std::sig_atomic_t g_stop_requested = 0;
constexpr std::uint64_t g_step_interval_ns =
    service_options_limits::g_default_runtime_step_interval_ns;
// Internal generation outcomes; recovery-required must never enter candidate rollback.
constexpr int g_reconcile_generation_exit_code = 4;

void vqec_vision_ai_appl_svgen_on_signal(int) {
    g_stop_requested = 1;
}

// Real monotonic clock for the executor loop. Never UTC; never a fabricated counter.
std::uint64_t vqec_vision_ai_appl_svgen_monotonic_ns() noexcept {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

}  // namespace

std::uint64_t vqec_vision_ai_appl_svgen_current_monotonic_ns() noexcept {
    return vqec_vision_ai_appl_svgen_monotonic_ns();
}

bool vqec_vision_ai_appl_svgen_is_stop_requested() noexcept {
    return g_stop_requested != 0;
}

int vqec_vision_ai_appl_svgen_run_generation(
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
            "[--source-recovery-backoff-ms <ms>] "
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
    std::signal(SIGINT, vqec_vision_ai_appl_svgen_on_signal);
    std::signal(SIGTERM, vqec_vision_ai_appl_svgen_on_signal);

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
    service_platform platform;
    const auto platform_prepared = platform.vqec_vision_ai_appl_svplt_prepare(
        args, deployment, catalog, features, model_packages,
        use_reference_platform, use_production_platform);
    if (platform_prepared.code_ != status_code::ok) {
        std::fprintf(stderr, "service platform preparation failed (%d): %s\n",
            static_cast<int>(platform_prepared.code_),
            platform_prepared.message_.c_str());
        return 1;
    }
    auto& production = platform.vqec_vision_ai_appl_svplt_get_production();
    auto& decoders = platform.vqec_vision_ai_appl_svplt_get_decoders();
    auto& trackers = platform.vqec_vision_ai_appl_svplt_get_trackers();
    auto& feature_registry = platform.vqec_vision_ai_appl_svplt_get_features();
    auto& activation = platform.vqec_vision_ai_appl_svplt_get_activation();
    const auto& tracker_contract =
        platform.vqec_vision_ai_appl_svplt_get_tracker_contract();
    const auto& attribute_schema_id =
        platform.vqec_vision_ai_appl_svplt_get_attribute_schema();

    // Output boundary for the harness: a permissive-but-explicit policy plus
    // either a neutral event delivery seam for production or a development reference sink.
    reference_event_sink reference_sink;
    event_delivery_seam production_seam;
    feature_event_sink_port& fallback_event_sink = use_production_platform
        ? static_cast<feature_event_sink_port&>(production_seam)
        : static_cast<feature_event_sink_port&>(reference_sink);
    service_output_runtime output_runtime;
    const auto output_started = output_runtime.vqec_vision_ai_appl_svout_start(
        args, deployment, fallback_event_sink);
    if (output_started.code_ != status_code::ok) {
        std::fprintf(stderr, "service output startup failed (%d): %s\n",
            static_cast<int>(output_started.code_), output_started.message_.c_str());
        return output_started.code_ == status_code::invalid_argument ? 2 : 1;
    }
    auto& output_policy_gate = output_runtime.vqec_vision_ai_appl_svout_get_gate();
    auto* metadata = output_runtime.vqec_vision_ai_appl_svout_get_metadata();
    const bool metadata_required =
        output_runtime.vqec_vision_ai_appl_svout_is_metadata_required();
    auto& active_event_sink = output_runtime.vqec_vision_ai_appl_svout_get_sink();

    service_feature_activation feature_activation;
    const auto feature_configured = feature_activation.vqec_vision_ai_appl_svfac_configure(
        startup, args, deployment, catalog, features, feature_registry,
        attribute_schema_id, output_policy_gate, activation.source_count_);
    if (feature_configured.code_ != status_code::ok) {
        std::fprintf(stderr, "service feature activation failed (%d): %s\n",
            static_cast<int>(feature_configured.code_),
            feature_configured.message_.c_str());
        return 1;
    }
    const auto* feature_wiring =
        feature_activation.vqec_vision_ai_appl_svfac_get_wiring();
    const auto delivery_started =
        output_runtime.vqec_vision_ai_appl_svout_start_delivery();
    if (delivery_started.code_ != status_code::ok) {
        std::fprintf(stderr, "service output delivery start failed (%d): %s\n",
            static_cast<int>(delivery_started.code_),
            delivery_started.message_.c_str());
        return 1;
    }


    std::unique_ptr<runtime_composition_bundle> bundle;
    const auto created = vqec_vision_ai_appl_rcfac_create_bundle(
        deployment, catalog, activation, decoders, trackers, bundle,
        feature_wiring);
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
    service_enrollment_runtime enrollment_runtime;
    face_enrollment_port* enrollment_port = nullptr;
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
        const auto enrollment_configured =
            enrollment_runtime.vqec_vision_ai_appl_svenr_configure(
                args, recognition, production, recognition_source_slot,
                deployment.sources_[recognition_source_slot], catalog,
                *recognition_owner->model_,
                vqec_vision_ai_appl_svgen_monotonic_ns());
        if (enrollment_configured.code_ != status_code::ok) {
            std::fprintf(stderr, "face enrollment runtime failed (%d): %s\n",
                static_cast<int>(enrollment_configured.code_),
                enrollment_configured.message_.c_str());
            (void)enrollment_runtime.vqec_vision_ai_appl_svenr_stop();
            (void)vqec_vision_ai_appl_svcsc_stop_graphs(cascade_owners);
            return 1;
        }
        enrollment_port = enrollment_runtime.vqec_vision_ai_appl_svenr_get_port();
        recognition_enabled = true;
    }
    executor->vqec_vision_ai_appl_rtexe_bind_event_delivery(
        output_policy_gate, active_event_sink);
    const auto activated =
        bundle->vqec_vision_ai_appl_rcfac_get_composition()->vqec_vision_ai_cntr_acomp_activate();
    if (activated.code_ != status_code::ok) {
        std::fprintf(stderr, "composition activation failed (%d): %s\n",
            static_cast<int>(activated.code_), activated.message_.c_str());
        (void)enrollment_runtime.vqec_vision_ai_appl_svenr_stop();
        (void)vqec_vision_ai_appl_svcsc_stop_graphs(cascade_owners);
        return 1;
    }

    service_execution_context execution;
    execution.arguments_ = &args;
    execution.deployment_ = &deployment;
    execution.catalog_ = &catalog;
    execution.tracker_contract_ = &tracker_contract;
    execution.bundle_ = bundle.get();
    execution.executor_ = executor;
    execution.production_ = &production;
    execution.output_policy_gate_ = &output_policy_gate;
    execution.metadata_ = metadata;
    execution.recognition_ = &recognition;
    execution.enrollment_ = &enrollment_runtime;
    execution.enrollment_port_ = enrollment_port;
    execution.cascade_owners_ = &cascade_owners;
    execution.control_manager_ = _control_manager;
    execution.feature_wiring_ = feature_wiring;
    execution.poll_control_ = _poll_control;
    execution.reconcile_requested_ = _is_runtime_reconcile_requested;
    execution.stop_requested_ = []() noexcept {
        return g_stop_requested != 0;
    };
    execution.runtime_generation_ = _runtime_generation;
    execution.pending_control_revision_ = _pending_control_revision;
    execution.production_platform_enabled_ = use_production_platform;
    execution.recognition_enabled_ = recognition_enabled;
    execution.metadata_required_ = metadata_required;

    service_execution_result execution_result;
    const auto executed =
        vqec_vision_ai_appl_svxlp_run(execution, execution_result);
    if (executed.code_ != status_code::ok) {
        std::fprintf(stderr, "service execution loop failed (%d): %s\n",
            static_cast<int>(executed.code_), executed.message_.c_str());
        execution_result.steady_now_ns_ =
            vqec_vision_ai_appl_svgen_monotonic_ns();
        execution_result.first_error_code_ = executed.code_;
        execution_result.generation_published_ =
            _control_manager == nullptr;
    }

    service_shutdown_context shutdown;
    shutdown.arguments_ = &args;
    shutdown.executor_ = executor;
    shutdown.cascade_owners_ = &cascade_owners;
    shutdown.enrollment_ = &enrollment_runtime;
    shutdown.output_ = &output_runtime;
    shutdown.production_seam_ = &production_seam;
    shutdown.steady_now_ns_ = execution_result.steady_now_ns_;
    shutdown.steps_ = execution_result.steps_;
    shutdown.routed_source_mask_ = execution_result.routed_source_mask_;
    shutdown.first_error_code_ = execution_result.first_error_code_;
    shutdown.recognition_enabled_ = recognition_enabled;
    shutdown.production_platform_ = use_production_platform;
    shutdown.generation_published_ = execution_result.generation_published_;
    shutdown.reconcile_requested_ = execution_result.replacement_requested_;
    return vqec_vision_ai_appl_svshd_stop_and_report(shutdown);
}
