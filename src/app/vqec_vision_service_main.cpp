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
#include <string>
#include <utility>
#include <vector>
#include <unistd.h>

#include "vqec_vision_deployment_config.hpp"
#include "vqec_vision_cascade_coordinator.hpp"
#include "vqec_vision_cascade_graph_session.hpp"
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

using namespace vqec::vision::ai;

namespace service_harness {
// Development-harness constants. Production values are supplied by validated deployment/
// model/feature catalogs and a trusted resolver; these are not product defaults.
inline constexpr std::uint64_t g_default_startup_timeout_ns = 30000000000ULL;
inline constexpr std::uint64_t g_default_stop_timeout_ns = 10000000000ULL;
inline constexpr int g_default_rpc_timeout_ms = 1000;
inline constexpr std::uint64_t g_cycle_id_stride = 100;
inline constexpr std::uint64_t g_policy_revision = 1;
inline constexpr std::uint64_t g_config_revision = 1;
inline constexpr std::uint64_t g_policy_expiry_ns = 1000000000000000000ULL;
inline constexpr std::uint64_t g_initial_gallery_revision = 1;
inline constexpr std::size_t g_fr_max_subjects = recognition_limits::g_max_subjects;
inline constexpr char g_model_root[] = "/opt/vqec/models/";
inline constexpr char g_backend_library[] = "/usr/lib/libQnnHtp.so";
inline constexpr char g_system_library[] = "/usr/lib/libQnnSystem.so";
inline constexpr std::uint64_t g_output_bytes = 16;
inline constexpr char g_box_tensor_name[] = "boxes";
inline constexpr std::uint32_t g_box_elements = 4;
inline constexpr char g_fw_dmabuf_contract[] = "fw.dmabuf.v1";
inline constexpr char g_qcom_dmabuf_contract[] = "qcom.dmabuf.v1";
}  // namespace service_harness

namespace {

volatile std::sig_atomic_t g_stop_requested = 0;
constexpr std::uint64_t g_mib = 1024ULL * 1024ULL;
constexpr std::uint64_t g_step_interval_ns = 1000000;
constexpr std::uint64_t g_nanoseconds_per_microsecond = 1000;
// Internal generation outcomes; recovery-required must never enter candidate rollback.
constexpr int g_reconcile_generation_exit_code = 4;
constexpr int g_recovery_required_exit_code = 5;

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

struct service_cascade_owner {
    production_cascade_binding binding_;
    const model_catalog_entry* model_{nullptr};
    std::unique_ptr<cascade_graph_session> graph_session_;
    std::unique_ptr<cascade_coordinator> coordinator_;
    std::uint16_t root_model_slot_{g_invalid_model_slot};
};

status vqec_vision_ai_appl_svcmn_start_graph_sessions(
    const std::array<cascade_graph_session*, 2>& _sessions) {
    bool all_running = false;
    while (!all_running) {
        all_running = true;
        const auto now_ns = vqec_vision_ai_appl_svcmn_monotonic_ns();
        for (auto* session : _sessions) {
            if (session == nullptr || session->vqec_vision_ai_appl_cgses_get_state() ==
                    cascade_graph_session_state::running) {
                continue;
            }
            all_running = false;
            const auto stepped = session->vqec_vision_ai_appl_cgses_step(now_ns);
            if (stepped.code_ != status_code::ok && stepped.code_ != status_code::pending) {
                return stepped;
            }
        }
        if (!all_running) {
            std::this_thread::sleep_for(std::chrono::nanoseconds(g_step_interval_ns));
        }
    }
    return {};
}

status vqec_vision_ai_appl_svcmn_stop_graph_sessions(
    const std::array<cascade_graph_session*, 2>& _sessions) {
    status first_error;
    auto now_ns = vqec_vision_ai_appl_svcmn_monotonic_ns();
    for (auto* session : _sessions) {
        if (session == nullptr) {
            continue;
        }
        const auto requested = session->vqec_vision_ai_appl_cgses_request_stop(now_ns);
        if (requested.code_ != status_code::ok && first_error.code_ == status_code::ok) {
            first_error = requested;
        }
    }
    bool all_stopped = false;
    while (!all_stopped) {
        all_stopped = true;
        now_ns = vqec_vision_ai_appl_svcmn_monotonic_ns();
        for (auto* session : _sessions) {
            if (session == nullptr) {
                continue;
            }
            const auto state = session->vqec_vision_ai_appl_cgses_get_state();
            if (state == cascade_graph_session_state::stopped ||
                state == cascade_graph_session_state::faulted) {
                continue;
            }
            all_stopped = false;
            const auto stepped = session->vqec_vision_ai_appl_cgses_step(now_ns);
            if (stepped.code_ != status_code::ok && stepped.code_ != status_code::pending &&
                first_error.code_ == status_code::ok) {
                first_error = stepped;
            }
        }
        if (!all_stopped) {
            std::this_thread::sleep_for(std::chrono::nanoseconds(g_step_interval_ns));
        }
    }
    return first_error;
}

status vqec_vision_ai_appl_svcmn_make_source_binding(
    const source_deployment_config& _source, const model_catalog_entry& _model,
    source_binding& _binding) {
    source_color_profile color_profile{source_color_profile::unspecified};
    if (_model.preprocess_.matrix_ == color_matrix::bt601 &&
        _model.preprocess_.range_ == color_range::limited) {
        color_profile = source_color_profile::bt601_limited;
    } else if (_model.preprocess_.matrix_ == color_matrix::bt709 &&
               _model.preprocess_.range_ == color_range::limited) {
        color_profile = source_color_profile::bt709_limited;
    } else {
        return {status_code::unsupported,
            "source binding requires a supported limited-range color profile"};
    }
    source_binding candidate;
    candidate.width_ = _source.profile_.width_;
    candidate.height_ = _source.profile_.height_;
    candidate.fps_numerator_ = _source.profile_.fps_numerator_;
    candidate.fps_denominator_ = _source.profile_.fps_denominator_;
    candidate.memory_kind_ = source_memory_kind::dmabuf;
    candidate.layout_ = source_memory_layout::linear_nv12;
    candidate.sync_mode_ = source_sync_mode::implicit_ready;
    candidate.color_profile_ = color_profile;
    candidate.chroma_site_ = source_chroma_site::mpeg2;
    candidate.fw_memory_contract_ = service_harness::g_fw_dmabuf_contract;
    candidate.backend_memory_contract_ = service_harness::g_qcom_dmabuf_contract;
    candidate.preprocess_contract_ = _model.preprocess_contract_;
    _binding = std::move(candidate);
    return {};
}

status vqec_vision_ai_appl_svcmn_make_offline_graph_session(
    const source_deployment_config& _source, const model_catalog_entry& _model,
    production_offline_model& _offline,
    std::unique_ptr<cascade_graph_session>& _session) {
    const auto& binding = _offline.vqec_vision_ai_appl_pdplt_get_binding();
    cascade_graph_session_config config;
    config.graph_ = _offline.vqec_vision_ai_appl_pdplt_get_graph();
    config.plan_ = binding.plan_;
    config.outputs_ = binding.outputs_;
    config.max_output_bytes_ = binding.max_output_bytes_;
    config.startup_timeout_ns_ = service_harness::g_default_startup_timeout_ns;
    config.stop_timeout_ns_ = service_harness::g_default_stop_timeout_ns;
    const auto bound = vqec_vision_ai_appl_svcmn_make_source_binding(
        _source, _model, config.binding_);
    if (bound.code_ != status_code::ok) {
        return bound;
    }
    if (config.graph_ == nullptr || config.plan_.model_path_.empty() ||
        config.outputs_.empty() || config.max_output_bytes_ == 0) {
        return {status_code::invalid_state, "offline graph binding is incomplete"};
    }
    _session = std::make_unique<cascade_graph_session>(std::move(config));
    return {};
}

status vqec_vision_ai_appl_svcmn_prepare_cascade_owners(
    const deployment_config& _deployment, const model_catalog& _catalog,
    production_platform& _platform,
    std::array<service_cascade_owner, deployment_limits::g_max_sources>& _owners) {
    for (std::uint16_t source_slot = 0; source_slot < _deployment.sources_.size();
         ++source_slot) {
        const auto& source = _deployment.sources_[source_slot];
        const model_catalog_entry* secondary = nullptr;
        for (const auto& model : _catalog.models_) {
            if (model.role_ != model_role::secondary ||
                !vqec_vision_ai_core_mdcat_source_activates_model(source, model)) {
                continue;
            }
            if (secondary != nullptr) {
                return {status_code::unsupported,
                    "one source currently supports one active secondary model"};
            }
            secondary = &model;
        }
        if (secondary == nullptr) {
            continue;
        }
        if (secondary->depends_on_.size() != 1U) {
            return {status_code::unsupported,
                "cascade coordinator currently requires one primary dependency"};
        }
        std::uint16_t root_slot = g_invalid_model_slot;
        for (std::uint16_t model_slot = 0; model_slot < source.model_ids_.size();
             ++model_slot) {
            if (source.model_ids_[model_slot] == secondary->depends_on_[0].model_id_) {
                root_slot = model_slot;
                break;
            }
        }
        if (root_slot == g_invalid_model_slot) {
            return {status_code::invalid_argument,
                "secondary dependency has no primary source slot"};
        }
        production_cascade_binding binding;
        const auto resolved = _platform.vqec_vision_ai_appl_pdplt_cascade_binding(
            source_slot, secondary->model_id_, binding);
        if (resolved.code_ != status_code::ok) {
            return resolved;
        }
        cascade_graph_session_config graph_config;
        graph_config.graph_ = binding.graph_;
        graph_config.plan_ = binding.plan_;
        graph_config.outputs_ = binding.outputs_;
        graph_config.max_output_bytes_ = binding.max_output_bytes_;
        graph_config.startup_timeout_ns_ = service_harness::g_default_startup_timeout_ns;
        graph_config.stop_timeout_ns_ = service_harness::g_default_stop_timeout_ns;
        const auto source_bound = vqec_vision_ai_appl_svcmn_make_source_binding(
            source, *secondary, graph_config.binding_);
        if (source_bound.code_ != status_code::ok) {
            return source_bound;
        }
        auto& owner = _owners[source_slot];
        owner.binding_ = std::move(binding);
        owner.model_ = secondary;
        owner.root_model_slot_ = root_slot;
        owner.graph_session_ =
            std::make_unique<cascade_graph_session>(std::move(graph_config));
    }
    return {};
}

status vqec_vision_ai_appl_svcmn_start_cascade_graphs(
    std::array<service_cascade_owner, deployment_limits::g_max_sources>& _owners) {
    bool all_running = false;
    while (!all_running) {
        all_running = true;
        const auto now_ns = vqec_vision_ai_appl_svcmn_monotonic_ns();
        for (auto& owner : _owners) {
            if (owner.graph_session_ == nullptr ||
                owner.graph_session_->vqec_vision_ai_appl_cgses_get_state() ==
                    cascade_graph_session_state::running) {
                continue;
            }
            all_running = false;
            const auto stepped =
                owner.graph_session_->vqec_vision_ai_appl_cgses_step(now_ns);
            if (stepped.code_ != status_code::ok &&
                stepped.code_ != status_code::pending) {
                return stepped;
            }
        }
        if (!all_running) {
            std::this_thread::sleep_for(std::chrono::nanoseconds(g_step_interval_ns));
        }
    }
    return {};
}

status vqec_vision_ai_appl_svcmn_stop_cascade_graphs(
    std::array<service_cascade_owner, deployment_limits::g_max_sources>& _owners) {
    auto now_ns = vqec_vision_ai_appl_svcmn_monotonic_ns();
    status first_error;
    for (auto& owner : _owners) {
        if (owner.graph_session_ == nullptr) {
            continue;
        }
        const auto requested =
            owner.graph_session_->vqec_vision_ai_appl_cgses_request_stop(now_ns);
        if (requested.code_ != status_code::ok && first_error.code_ == status_code::ok) {
            first_error = requested;
        }
    }
    bool all_stopped = false;
    while (!all_stopped) {
        all_stopped = true;
        now_ns = vqec_vision_ai_appl_svcmn_monotonic_ns();
        for (auto& owner : _owners) {
            if (owner.graph_session_ == nullptr) {
                continue;
            }
            const auto state =
                owner.graph_session_->vqec_vision_ai_appl_cgses_get_state();
            if (state == cascade_graph_session_state::stopped ||
                state == cascade_graph_session_state::faulted) {
                continue;
            }
            all_stopped = false;
            const auto stepped =
                owner.graph_session_->vqec_vision_ai_appl_cgses_step(now_ns);
            if (stepped.code_ != status_code::ok &&
                stepped.code_ != status_code::pending) {
                if (first_error.code_ == status_code::ok) {
                    first_error = stepped;
                }
            }
        }
        if (!all_stopped) {
            std::this_thread::sleep_for(std::chrono::nanoseconds(g_step_interval_ns));
        }
    }
    return first_error;
}

// --- loading helpers --------------------------------------------------------------------

bool vqec_vision_ai_appl_svcmn_load_model_catalog(
    const std::string& _path, model_catalog& _catalog) {
    std::ifstream stream(_path);
    if (!stream.is_open()) {
        std::fprintf(stderr, "cannot open model catalog: %s\n", _path.c_str());
        return false;
    }
    std::uint64_t resident_bytes = 0;
    const auto loaded = vqec_vision_ai_mreg_mdcat_load_catalog(stream, _catalog, resident_bytes);
    if (loaded.code_ != status_code::ok) {
        std::fprintf(stderr, "model catalog rejected (%d): %s\n",
            static_cast<int>(loaded.code_), loaded.message_.c_str());
        return false;
    }
    return true;
}

bool vqec_vision_ai_appl_svcmn_load_model_packages(
    const std::string& _path, model_package_registry& _registry) {
    std::ifstream stream(_path);
    if (!stream.is_open()) {
        std::fprintf(stderr, "cannot open model package registry: %s\n", _path.c_str());
        return false;
    }
    const auto loaded = vqec_vision_ai_mreg_mprld_load_registry(stream, _registry);
    if (loaded.code_ != status_code::ok) {
        std::fprintf(stderr, "model package registry rejected (%d): %s\n",
            static_cast<int>(loaded.code_), loaded.message_.c_str());
        return false;
    }
    return true;
}

bool vqec_vision_ai_appl_svcmn_load_deployment(
    const std::string& _path, deployment_config& _deployment) {
    std::ifstream stream(_path);
    if (!stream.is_open()) {
        std::fprintf(stderr, "cannot open deployment config: %s\n", _path.c_str());
        return false;
    }
    std::uint64_t resident_bytes = 0;
    const auto loaded = vqec_vision_ai_life_dpcfg_load(stream, _deployment, resident_bytes);
    if (loaded.code_ != status_code::ok) {
        std::fprintf(stderr, "deployment config rejected (%d): %s\n",
            static_cast<int>(loaded.code_), loaded.message_.c_str());
        return false;
    }
    return true;
}

bool vqec_vision_ai_appl_svcmn_load_feature_catalog(
    const std::string& _path, feature_catalog& _features) {
    std::ifstream stream(_path);
    if (!stream.is_open()) {
        std::fprintf(stderr, "cannot open feature catalog: %s\n", _path.c_str());
        return false;
    }
    const auto loaded = vqec_vision_ai_ftmgr_ftcat_load_catalog(stream, _features);
    if (loaded.code_ != status_code::ok) {
        std::fprintf(stderr, "feature catalog rejected (%d): %s\n",
            static_cast<int>(loaded.code_), loaded.message_.c_str());
        return false;
    }
    return true;
}

bool vqec_vision_ai_appl_svcmn_load_usecase_snapshot(
    const std::string& _path, usecase_control_snapshot& _snapshot) {
    std::ifstream stream(_path);
    if (!stream.is_open()) {
        std::fprintf(stderr, "cannot open usecase snapshot: %s\n", _path.c_str());
        return false;
    }
    const auto loaded = vqec_vision_ai_ftmgr_ucfg_load_snapshot(stream, _snapshot);
    if (loaded.code_ != status_code::ok) {
        std::fprintf(stderr, "usecase snapshot rejected (%d): %s\n",
            static_cast<int>(loaded.code_), loaded.message_.c_str());
        return false;
    }
    return true;
}

std::string vqec_vision_ai_appl_svcmn_dev_model_path(const std::string& _model_id) {
    return std::string(service_harness::g_model_root) + _model_id + ".bin";
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

struct parsed_arguments {
    std::string deployment_path;
    std::string catalog_path;
    std::string feature_catalog_path;
    std::string usecase_snapshot_path;
    bool usecase_dbus{false};
    bool usecase_dbus_session_bus{false};
    std::string usecase_service_name;
    std::string usecase_object_path;
    std::string usecase_peer_name;
    int usecase_rpc_timeout_ms{0};
    std::size_t usecase_callbacks_per_poll{0};
    std::uint64_t max_steps{0};
    std::uint32_t require_sources{0};
    // Production requires an explicit wired platform. Harness selects the device-free fake
    // platform implicitly for development only.
    std::string platform{"none"};
    bool production_mode{false};
    // Production platform owner inputs (Qualcomm).
    inference_execution_policy execution_policy;
    bool use_session_workers{false};
    bool use_model_workers{false};
    std::uint64_t runtime_step_interval_ns{g_step_interval_ns};
    std::string model_package_registry_path;
    std::string model_package;
    std::string model_library;
    std::string camera_socket_dir{"/run/camera_ai"};
    std::uint32_t camera_producer_uid{0};
    std::uint32_t nv12_format_value{23};
    std::string output_ring_id;
    std::uint32_t output_bitrate_bps{0};
    std::uint32_t output_keyframe_interval_frames{0};
    std::uint32_t output_box_color_rgba{0};
    std::uint32_t output_surface_count{0};
    std::string output_colorimetry;
    std::string output_interlace_mode;
    std::string fr_gallery_path;
    std::string fr_protected_directory;
    std::string fr_gallery_file_name;
    std::string fr_key_file_name;
    std::string fr_lock_file_name;
    std::string fr_gallery_id;
    std::uint64_t fr_preprocess_revision{0};
    std::size_t fr_store_max_bytes{0};
    float fr_minimum_similarity{0.0F};
    float fr_subject_margin{0.0F};
    std::size_t fr_max_templates{0};
    std::size_t fr_top_k{0};
    bool fr_similarity_set{false};
    bool fr_margin_set{false};
    bool fr_templates_set{false};
    bool fr_top_k_set{false};
    std::string fr_feature_id;
    std::string fr_identity_attribute;
    bool enrollment_dbus{false};
    bool enrollment_dbus_session_bus{false};
    std::string enrollment_peer_name;
    int enrollment_rpc_timeout_ms{0};
    std::size_t enrollment_callbacks_per_poll{0};
    std::vector<std::string> enrollment_image_roots;
    std::uint64_t enrollment_max_image_bytes{0};
    std::uint32_t enrollment_image_timeout_ms{0};
    std::string enrollment_jpeg_decoder;
    std::string enrollment_converter;
    std::string enrollment_scaler;
    std::string enrollment_transform;
    std::string enrollment_transform_engine;
};

bool vqec_vision_ai_appl_svcmn_parse(int _argc, char** _argv, parsed_arguments& _args) {
    for (int index = 1; index < _argc; ++index) {
        const std::string option = _argv[index];
        const bool has_value = index + 1 < _argc;
        if (option == "--deployment" && has_value) {
            _args.deployment_path = _argv[++index];
        } else if (option == "--source-execution" && has_value) {
            const std::string execution = _argv[++index];
            if (execution == "serialized") {
                _args.use_session_workers = false;
            } else if (execution == "threaded") {
                _args.use_session_workers = true;
            } else {
                std::fprintf(stderr, "unsupported source execution policy\n");
                return false;
            }
        } else if (option == "--inference-perf-profile" && has_value) {
            const std::string profile = _argv[++index];
            if (profile == "balanced") {
                _args.execution_policy.profile_ = inference_perf_profile::balanced;
            } else if (profile == "low_latency") {
                _args.execution_policy.profile_ = inference_perf_profile::low_latency;
            } else {
                std::fprintf(stderr, "unsupported inference performance profile\n");
                return false;
            }
        } else if (option == "--model-execution" && has_value) {
            const std::string execution = _argv[++index];
            if (execution == "serialized") {
                _args.use_model_workers = false;
            } else if (execution == "parallel") {
                _args.use_model_workers = true;
            } else {
                std::fprintf(stderr, "unsupported model execution policy\n");
                return false;
            }
        } else if (option == "--runtime-step-interval-us" && has_value) {
            const auto interval_us = std::strtoull(_argv[++index], nullptr, 10);
            if (interval_us == 0 || interval_us >
                    std::numeric_limits<std::uint64_t>::max() /
                        g_nanoseconds_per_microsecond) {
                std::fprintf(stderr, "invalid runtime step interval\n");
                return false;
            }
            _args.runtime_step_interval_ns =
                interval_us * g_nanoseconds_per_microsecond;
        } else if (option == "--model-catalog" && has_value) {
            _args.catalog_path = _argv[++index];
        } else if (option == "--feature-catalog" && has_value) {
            _args.feature_catalog_path = _argv[++index];
        } else if (option == "--usecase-snapshot" && has_value) {
            _args.usecase_snapshot_path = _argv[++index];
        } else if (option == "--usecase-dbus") {
            _args.usecase_dbus = true;
        } else if (option == "--usecase-dbus-session") {
            _args.usecase_dbus = true;
            _args.usecase_dbus_session_bus = true;
        } else if (option == "--usecase-service-name" && has_value) {
            _args.usecase_service_name = _argv[++index];
        } else if (option == "--usecase-object-path" && has_value) {
            _args.usecase_object_path = _argv[++index];
        } else if (option == "--usecase-peer-name" && has_value) {
            _args.usecase_peer_name = _argv[++index];
        } else if (option == "--usecase-rpc-timeout-ms" && has_value) {
            _args.usecase_rpc_timeout_ms = static_cast<int>(
                std::strtol(_argv[++index], nullptr, 10));
        } else if (option == "--usecase-callbacks-per-poll" && has_value) {
            _args.usecase_callbacks_per_poll = static_cast<std::size_t>(
                std::strtoull(_argv[++index], nullptr, 10));
        } else if (option == "--steps" && has_value) {
            _args.max_steps = std::strtoull(_argv[++index], nullptr, 10);
        } else if (option == "--require-sources" && has_value) {
            _args.require_sources = static_cast<std::uint32_t>(
                std::strtoul(_argv[++index], nullptr, 10));
        } else if (option == "--mode" && has_value) {
            const std::string mode = _argv[++index];
            if (mode == "harness") {
                _args.production_mode = false;
            } else if (mode == "production") {
                _args.production_mode = true;
            } else {
                std::fprintf(stderr, "unknown mode: %s\n", mode.c_str());
                return false;
            }
        } else if (option == "--platform" && has_value) {
            _args.platform = _argv[++index];
        } else if (option == "--model-package-registry" && has_value) {
            _args.model_package_registry_path = _argv[++index];
        } else if (option == "--model-package" && has_value) {
            _args.model_package = _argv[++index];
        } else if (option == "--model-library" && has_value) {
            _args.model_library = _argv[++index];
        } else if (option == "--camera-socket-dir" && has_value) {
            _args.camera_socket_dir = _argv[++index];
        } else if (option == "--camera-producer-uid" && has_value) {
            _args.camera_producer_uid = static_cast<std::uint32_t>(
                std::strtoul(_argv[++index], nullptr, 10));
        } else if (option == "--nv12-format" && has_value) {
            _args.nv12_format_value = static_cast<std::uint32_t>(
                std::strtoul(_argv[++index], nullptr, 10));
        } else if (option == "--output-ring-id" && has_value) {
            _args.output_ring_id = _argv[++index];
        } else if (option == "--output-bitrate" && has_value) {
            _args.output_bitrate_bps = static_cast<std::uint32_t>(
                std::strtoul(_argv[++index], nullptr, 10));
        } else if (option == "--output-keyframe-interval" && has_value) {
            _args.output_keyframe_interval_frames = static_cast<std::uint32_t>(
                std::strtoul(_argv[++index], nullptr, 10));
        } else if (option == "--output-box-color-rgba" && has_value) {
            _args.output_box_color_rgba = static_cast<std::uint32_t>(
                std::strtoul(_argv[++index], nullptr, 0));
        } else if (option == "--output-surface-count" && has_value) {
            _args.output_surface_count = static_cast<std::uint32_t>(
                std::strtoul(_argv[++index], nullptr, 10));
        } else if (option == "--output-colorimetry" && has_value) {
            _args.output_colorimetry = _argv[++index];
        } else if (option == "--output-interlace-mode" && has_value) {
            _args.output_interlace_mode = _argv[++index];
        } else if (option == "--fr-gallery-path" && has_value) {
            _args.fr_gallery_path = _argv[++index];
        } else if (option == "--fr-protected-directory" && has_value) {
            _args.fr_protected_directory = _argv[++index];
        } else if (option == "--fr-gallery-file" && has_value) {
            _args.fr_gallery_file_name = _argv[++index];
        } else if (option == "--fr-key-file" && has_value) {
            _args.fr_key_file_name = _argv[++index];
        } else if (option == "--fr-lock-file" && has_value) {
            _args.fr_lock_file_name = _argv[++index];
        } else if (option == "--fr-gallery-id" && has_value) {
            _args.fr_gallery_id = _argv[++index];
        } else if (option == "--fr-preprocess-revision" && has_value) {
            _args.fr_preprocess_revision = std::strtoull(_argv[++index], nullptr, 10);
        } else if (option == "--fr-store-max-bytes" && has_value) {
            _args.fr_store_max_bytes = static_cast<std::size_t>(
                std::strtoull(_argv[++index], nullptr, 10));
        } else if (option == "--fr-min-similarity" && has_value) {
            _args.fr_minimum_similarity = std::strtof(_argv[++index], nullptr);
            _args.fr_similarity_set = true;
        } else if (option == "--fr-subject-margin" && has_value) {
            _args.fr_subject_margin = std::strtof(_argv[++index], nullptr);
            _args.fr_margin_set = true;
        } else if (option == "--fr-max-templates" && has_value) {
            _args.fr_max_templates = static_cast<std::size_t>(
                std::strtoull(_argv[++index], nullptr, 10));
            _args.fr_templates_set = true;
        } else if (option == "--fr-top-k" && has_value) {
            _args.fr_top_k = static_cast<std::size_t>(
                std::strtoull(_argv[++index], nullptr, 10));
            _args.fr_top_k_set = true;
        } else if (option == "--fr-feature-id" && has_value) {
            _args.fr_feature_id = _argv[++index];
        } else if (option == "--fr-identity-attribute" && has_value) {
            _args.fr_identity_attribute = _argv[++index];
        } else if (option == "--enrollment-dbus") {
            _args.enrollment_dbus = true;
        } else if (option == "--enrollment-dbus-session") {
            _args.enrollment_dbus = true;
            _args.enrollment_dbus_session_bus = true;
        } else if (option == "--enrollment-peer-name" && has_value) {
            _args.enrollment_peer_name = _argv[++index];
        } else if (option == "--enrollment-rpc-timeout-ms" && has_value) {
            _args.enrollment_rpc_timeout_ms = static_cast<int>(
                std::strtol(_argv[++index], nullptr, 10));
        } else if (option == "--enrollment-callbacks-per-poll" && has_value) {
            _args.enrollment_callbacks_per_poll = static_cast<std::size_t>(
                std::strtoull(_argv[++index], nullptr, 10));
        } else if (option == "--enrollment-image-root" && has_value) {
            _args.enrollment_image_roots.emplace_back(_argv[++index]);
        } else if (option == "--enrollment-max-image-bytes" && has_value) {
            _args.enrollment_max_image_bytes = std::strtoull(_argv[++index], nullptr, 10);
        } else if (option == "--enrollment-image-timeout-ms" && has_value) {
            _args.enrollment_image_timeout_ms = static_cast<std::uint32_t>(
                std::strtoul(_argv[++index], nullptr, 10));
        } else if (option == "--enrollment-jpeg-decoder" && has_value) {
            _args.enrollment_jpeg_decoder = _argv[++index];
        } else if (option == "--enrollment-converter" && has_value) {
            _args.enrollment_converter = _argv[++index];
        } else if (option == "--enrollment-scaler" && has_value) {
            _args.enrollment_scaler = _argv[++index];
        } else if (option == "--enrollment-transform" && has_value) {
            _args.enrollment_transform = _argv[++index];
        } else if (option == "--enrollment-transform-engine" && has_value) {
            _args.enrollment_transform_engine = _argv[++index];
        } else {
            std::fprintf(stderr, "unknown or incomplete argument: %s\n", option.c_str());
            return false;
        }
    }
    return !_args.deployment_path.empty() && !_args.catalog_path.empty();
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
    observation_batch& _merged) {
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
        }
        for (const auto& item : batch.observations_) {
            if (merged.observations_.size() >= observation_limits::g_max_observations) {
                break;
            }
            merged.observations_.push_back(item);
        }
    }
    _merged = std::move(merged);
}

}  // namespace

int vqec_vision_ai_appl_svcmn_run_generation(
    int _argc, char** _argv, const deployment_config* _effective_deployment,
    usecase_control_manager* _control_manager,
    const std::function<void()>& _poll_control,
    std::uint64_t _runtime_generation, std::uint64_t _pending_control_revision) {
    parsed_arguments args;
    if (!vqec_vision_ai_appl_svcmn_parse(_argc, _argv, args)) {
        std::fprintf(stderr,
            "usage: vqec_ai_vision_applications --deployment <json> --model-catalog <json> "
            "[--feature-catalog <json>] [--usecase-snapshot <json>] "
            "[--usecase-dbus|--usecase-dbus-session "
            "--usecase-service-name <name> --usecase-object-path <path> "
            "--usecase-peer-name <name> --usecase-rpc-timeout-ms <ms> "
            "--usecase-callbacks-per-poll <n>] "
            "[--steps <n>] [--require-sources <n>] "
            "[--mode harness|production] [--platform fake|reference|qualcomm] "
            "[--model-package-registry <json>] "
            "[--output-ring-id <id> --output-bitrate <bps> "
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

    model_catalog catalog;
    deployment_config deployment;
    feature_catalog features;
    if (!vqec_vision_ai_appl_svcmn_load_model_catalog(args.catalog_path, catalog) ||
        !vqec_vision_ai_appl_svcmn_load_deployment(args.deployment_path, deployment)) {
        return 1;
    }
    if (!args.feature_catalog_path.empty() &&
        !vqec_vision_ai_appl_svcmn_load_feature_catalog(args.feature_catalog_path, features)) {
        return 1;
    }
    if (_effective_deployment != nullptr) {
        deployment = *_effective_deployment;
    } else if (!args.usecase_snapshot_path.empty()) {
        usecase_control_snapshot control;
        if (!vqec_vision_ai_appl_svcmn_load_usecase_snapshot(
                args.usecase_snapshot_path, control)) {
            return 1;
        }
        if (control.deployment_revision_ != deployment.revision_) {
            std::fprintf(stderr,
                "usecase snapshot deployment revision does not match deployment\n");
            return 1;
        }
        usecase_activation_snapshot activation_snapshot;
        deployment_config effective_deployment;
        const auto composed = vqec_vision_ai_core_ucact_compose_effective_deployment(
            deployment, catalog, control.catalog_, control.requests_,
            activation_snapshot, effective_deployment);
        if (composed.code_ != status_code::ok) {
            std::fprintf(stderr, "usecase composition rejected (%d): %s\n",
                static_cast<int>(composed.code_), composed.message_.c_str());
            return 1;
        }
        deployment = std::move(effective_deployment);
        std::printf("usecase plan control_revision=%llu entitlement_revision=%llu "
                    "active_sources=%zu\n",
            static_cast<unsigned long long>(control.control_revision_),
            static_cast<unsigned long long>(control.entitlement_revision_),
            deployment.sources_.size());
    }
    if (deployment.sources_.empty()) {
        if (_control_manager != nullptr) {
            const auto published = _pending_control_revision == 0
                ? (_runtime_generation == 1
                    ? _control_manager->vqec_vision_ai_ftmgr_ucmgr_publish_initial(
                          _runtime_generation)
                    : status{})
                : _control_manager->vqec_vision_ai_ftmgr_ucmgr_publish_pending(
                      _pending_control_revision, _runtime_generation);
            if (published.code_ != status_code::ok) {
                return 1;
            }
        }
        std::uint64_t idle_steps = 0;
        while (!g_stop_requested &&
               (args.max_steps == 0 || idle_steps < args.max_steps)) {
            if (_poll_control) {
                _poll_control();
            }
            if (_control_manager != nullptr &&
                _control_manager->vqec_vision_ai_ftmgr_ucmgr_has_pending()) {
                return g_reconcile_generation_exit_code;
            }
            std::this_thread::sleep_for(std::chrono::nanoseconds(g_step_interval_ns));
            ++idle_steps;
        }
        return 0;
    }
    const bool has_fr_arguments = !args.fr_gallery_path.empty() ||
        !args.fr_protected_directory.empty() || !args.fr_gallery_file_name.empty() ||
        !args.fr_key_file_name.empty() || !args.fr_lock_file_name.empty() ||
        !args.fr_gallery_id.empty() || args.fr_preprocess_revision != 0 ||
        args.fr_store_max_bytes != 0 || args.fr_similarity_set ||
        args.fr_margin_set || args.fr_templates_set || args.fr_top_k_set ||
        !args.fr_feature_id.empty() || !args.fr_identity_attribute.empty() ||
        args.enrollment_dbus;
    const bool has_complete_fr_arguments = !args.fr_gallery_path.empty() &&
        !args.fr_protected_directory.empty() && !args.fr_gallery_file_name.empty() &&
        !args.fr_key_file_name.empty() && !args.fr_lock_file_name.empty() &&
        !args.fr_gallery_id.empty() && args.fr_preprocess_revision != 0 &&
        args.fr_store_max_bytes != 0 &&
        args.fr_similarity_set && args.fr_margin_set && args.fr_templates_set &&
        args.fr_top_k_set && !args.fr_feature_id.empty() &&
        !args.fr_identity_attribute.empty();
    if (has_fr_arguments && !has_complete_fr_arguments) {
        std::fprintf(stderr,
            "FR requires derived index path, protected-store directory/files, gallery "
            "identity/preprocess revision/quota, similarity, margin, max templates, "
            "top-k, feature id and identity attribute\n");
        return 1;
    }
    bool has_active_secondary_model = false;
    for (const auto& source : deployment.sources_) {
        for (const auto& model : catalog.models_) {
            if (model.role_ == model_role::secondary &&
                vqec_vision_ai_core_mdcat_source_activates_model(source, model)) {
                has_active_secondary_model = true;
                break;
            }
        }
        if (has_active_secondary_model) {
            break;
        }
    }
    const bool fr_effectively_enabled =
        has_complete_fr_arguments && has_active_secondary_model;
    const bool has_complete_image_enrollment = !args.enrollment_image_roots.empty() &&
        args.enrollment_max_image_bytes != 0 && args.enrollment_image_timeout_ms != 0 &&
        !args.enrollment_jpeg_decoder.empty() && !args.enrollment_converter.empty() &&
        !args.enrollment_scaler.empty() && !args.enrollment_transform.empty() &&
        !args.enrollment_transform_engine.empty();
    if (args.enrollment_dbus && !has_complete_image_enrollment) {
        std::fprintf(stderr,
            "enrollment DBus requires image roots, byte/time limits and every image "
            "pipeline factory\n");
        return 1;
    }
    model_package_registry model_packages;
    if (!args.model_package_registry_path.empty()) {
        if (!args.model_package.empty() || !args.model_library.empty()) {
            std::fprintf(stderr,
                "model package registry cannot be combined with legacy package arguments\n");
            return 1;
        }
        if (!vqec_vision_ai_appl_svcmn_load_model_packages(
                args.model_package_registry_path, model_packages)) {
            return 1;
        }
    } else if (!args.model_package.empty() || !args.model_library.empty()) {
        if (catalog.models_.size() != 1 || args.model_package.empty() ||
            args.model_library.empty()) {
            std::fprintf(stderr,
                "legacy model package arguments require exactly one catalog model\n");
            return 1;
        }
        const auto& model = catalog.models_.front();
        model_packages.schema_version_ = model_package_registry_limits::g_schema_version;
        model_packages.bindings_.push_back({model.model_id_, model.model_version_,
            model.target_id_, model.artifact_ref_, args.model_package, args.model_library});
    }

    if (deployment.sources_.size() > deployment_limits::g_max_sources) {
        std::fprintf(stderr, "deployment source count exceeds runtime support\n");
        return 1;
    }
    // Platform selection. Production requires an explicit device-free platform; an unset
    // or unwired platform fails closed instead of substituting the development harness.
    // `fake` proves wiring only; `reference` wires the real reference tracker and zone
    // feature. Both are selected by name and never fall back implicitly.
    const bool use_reference_platform = args.platform == "reference";
    const bool use_production_platform = args.platform == "qualcomm";
    const bool platform_named = args.platform == "fake" || args.platform == "reference" ||
        args.platform == "qualcomm";
    if (args.enrollment_dbus && (!args.production_mode || !use_production_platform)) {
        std::fprintf(stderr,
            "file enrollment DBus requires production mode with the Qualcomm platform\n");
        return 1;
    }
    if (args.production_mode && !platform_named) {
        std::fprintf(stderr,
            "production mode: --platform %s is not wired; use --platform fake or "
            "--platform reference for a device-free platform. Refusing fixture fallback\n",
            args.platform.c_str());
        return 3;
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
    std::string tracker_contract;
    std::string attribute_schema_id;
    if (use_production_platform) {
        production_platform_config production_config;
        production_config.model_packages_ = model_packages;
        production_config.execution_policy_ = args.execution_policy;
        production_config.backend_library_ = service_harness::g_backend_library;
        production_config.system_library_ = service_harness::g_system_library;
        production_config.socket_dir_ = args.camera_socket_dir;
        production_config.producer_uid_ = args.camera_producer_uid;
        production_config.nv12_format_value_ = args.nv12_format_value;
        production_config.preprocess_output_timeout_ns_ =
            submission_limits::g_default_job_timeout_ns;
        production_config.output_ring_id_ = args.output_ring_id;
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
            production.vqec_vision_ai_appl_pdplt_register_features(features, feature_registry)
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
            reference.vqec_vision_ai_appl_rplat_register_features(features, feature_registry);
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
            platform.vqec_vision_ai_appl_fkplt_register_features(features, feature_registry);
        if (registered_features.code_ != status_code::ok) {
            std::fprintf(stderr, "cannot register fake platform features (%d): %s\n",
                static_cast<int>(registered_features.code_),
                registered_features.message_.c_str());
            return 1;
        }
        tracker_contract = platform.vqec_vision_ai_appl_fkplt_get_tracker_contract();
        attribute_schema_id = platform.vqec_vision_ai_appl_fkplt_get_config().attribute_schema_id_;
    }

    // Output boundary for the harness: a permissive-but-explicit policy plus a
    // development sink. The gate still denies any event whose attributes are unlisted.
    output_gate output_policy_gate;
    reference_event_sink event_sink;

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
    for (std::uint16_t source_slot = 0; source_slot < activation.source_count_; ++source_slot) {
        const auto& source = deployment.sources_[source_slot];
        auto& source_activation = activation.sources_[source_slot];
        source_activation.source_id_ = source.source_id_;
        source_activation.source_ = sources[source_slot];
        source_activation.model_count_ = static_cast<std::uint16_t>(source.model_ids_.size());
        for (std::uint16_t model_slot = 0; model_slot < source_activation.model_count_;
             ++model_slot) {
            const auto* model = vqec_vision_ai_appl_svcmn_find_model(
                catalog, source.model_ids_[model_slot]);
            if (model == nullptr) {
                std::fprintf(stderr, "deployment references unknown model: %s\n",
                    source.model_ids_[model_slot].c_str());
                return 1;
            }
            auto& model_activation = source_activation.models_[model_slot];
            model_activation.model_id_ = model->model_id_;
            if (use_production_platform) {
                inference_graph_port* graph =
                    production.vqec_vision_ai_appl_pdplt_graph(source_slot, model->model_id_);
                image_processor_port* processor =
                    production.vqec_vision_ai_appl_pdplt_processor(
                        source_slot, model->model_id_);
                const model_outputs* outputs =
                    production.vqec_vision_ai_appl_pdplt_outputs(model->model_id_);
                if (graph == nullptr || processor == nullptr || outputs == nullptr) {
                    std::fprintf(stderr, "production platform has no graph for model %s\n",
                        model->model_id_.c_str());
                    return 1;
                }
                model_activation.graph_ = graph;
                model_activation.processor_ = processor;
                model_activation.outputs_ = *outputs;
                model_activation.paths_ =
                    production.vqec_vision_ai_appl_pdplt_paths(model->model_id_);
            } else {
                reference_graphs.push_back(std::make_unique<reference_inference_graph>());
                model_activation.graph_ = reference_graphs.back().get();
                model_activation.paths_.model_id_ = model->model_id_;
                model_activation.paths_.target_id_ = model->target_id_;
                model_activation.paths_.artifact_ref_ = model->artifact_ref_;
                model_activation.paths_.model_path_ =
                    vqec_vision_ai_appl_svcmn_dev_model_path(model->model_id_);
                model_activation.paths_.backend_path_ = service_harness::g_backend_library;
                model_activation.paths_.system_path_ = service_harness::g_system_library;
                model_activation.outputs_ = vqec_vision_ai_appl_svcmn_synthetic_outputs(*model);
            }
            model_activation.resolved_output_manifest_ref_ = model->output_manifest_ref_;
            model_activation.tracker_contract_ = tracker_contract;
            model_activation.binding_.width_ = source.profile_.width_;
            model_activation.binding_.height_ = source.profile_.height_;
            model_activation.binding_.fps_numerator_ = source.profile_.fps_numerator_;
            model_activation.binding_.fps_denominator_ = source.profile_.fps_denominator_;
            model_activation.binding_.memory_kind_ = source_memory_kind::dmabuf;
            model_activation.binding_.layout_ = source_memory_layout::linear_nv12;
            model_activation.binding_.sync_mode_ = source_sync_mode::implicit_ready;
            model_activation.binding_.color_profile_ = source_color_profile::bt709_limited;
            model_activation.binding_.chroma_site_ = source_chroma_site::mpeg2;
            model_activation.binding_.fw_memory_contract_ = "fw.dmabuf.v1";
            model_activation.binding_.backend_memory_contract_ = "qcom.dmabuf.v1";
            model_activation.binding_.preprocess_contract_ = model->preprocess_contract_;
            model_activation.cycle_id_ = static_cast<std::uint64_t>(source_slot) *
                    service_harness::g_cycle_id_stride + model_slot + 1U;
            model_activation.job_timeout_ns_ = submission_limits::g_default_job_timeout_ns;
        }
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
                request.desired_enabled_ = true;
                request.entitlement_granted_ = true;
                request.resource_admitted_ = true;
                request.configuration_.schema_id_ = feature.configuration_schema_;
                request.configuration_.revision_ = service_harness::g_config_revision;
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
            // Explicit output entitlement for the wired associations. The fixture feature
            // emits one field, so the rule must list it or delivery is denied.
            output_policy policy;
            policy.revision_ = service_harness::g_policy_revision;
            policy.not_before_ns_ = 0;
            policy.expires_ns_ = service_harness::g_policy_expiry_ns;
            for (std::uint16_t index = 0; index < request_count; ++index) {
                output_scope_rule rule;
                rule.source_id_ = requests[index].source_id_;
                rule.feature_id_ = requests[index].feature_id_;
                rule.attributes_.push_back(attribute_schema_id);
                policy.rules_.push_back(std::move(rule));
            }
            if (fr_effectively_enabled) {
                for (const auto& source : deployment.sources_) {
                    output_scope_rule rule;
                    rule.source_id_ = source.source_id_;
                    rule.feature_id_ = args.fr_feature_id;
                    rule.attributes_.push_back(args.fr_identity_attribute);
                    policy.rules_.push_back(std::move(rule));
                }
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
        policy.revision_ = service_harness::g_policy_revision;
        policy.not_before_ns_ = 0;
        policy.expires_ns_ = service_harness::g_policy_expiry_ns;
        for (const auto& source : deployment.sources_) {
            output_scope_rule rule;
            rule.source_id_ = source.source_id_;
            rule.feature_id_ = args.fr_feature_id;
            rule.attributes_.push_back(args.fr_identity_attribute);
            policy.rules_.push_back(std::move(rule));
        }
        const auto applied =
            output_policy_gate.vqec_vision_ai_core_otgat_apply_policy(policy, 0);
        if (applied.code_ != status_code::ok) {
            std::fprintf(stderr, "FR output policy apply failed (%d): %s\n",
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
        return vqec_vision_ai_appl_svcmn_stop_graph_sessions(
            std::array<cascade_graph_session*, 2>{
                enrollment_detector_graph_session.get(),
                enrollment_embedding_graph_session.get()});
    };
#if defined(VQEC_VISION_AI_HAS_FACE_ENROLLMENT_DBUS)
    std::unique_ptr<face_enrollment_dbus_server> enrollment_dbus;
#endif
    bool recognition_enabled = false;
    if (use_production_platform) {
        const auto cascade_prepared = vqec_vision_ai_appl_svcmn_prepare_cascade_owners(
            deployment, catalog, production, cascade_owners);
        if (cascade_prepared.code_ != status_code::ok) {
            std::fprintf(stderr, "cascade preparation failed (%d): %s\n",
                static_cast<int>(cascade_prepared.code_),
                cascade_prepared.message_.c_str());
            return 1;
        }
        const auto cascade_started =
            vqec_vision_ai_appl_svcmn_start_cascade_graphs(cascade_owners);
        if (cascade_started.code_ != status_code::ok) {
            std::fprintf(stderr, "cascade graph startup failed (%d): %s\n",
                static_cast<int>(cascade_started.code_),
                cascade_started.message_.c_str());
            (void)vqec_vision_ai_appl_svcmn_stop_cascade_graphs(cascade_owners);
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
                (void)vqec_vision_ai_appl_svcmn_stop_cascade_graphs(cascade_owners);
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
            owner.coordinator_ = std::make_unique<cascade_coordinator>();
            const auto coordinator_configured =
                owner.coordinator_->vqec_vision_ai_appl_cscrd_configure(
                    coordinator_config);
            if (coordinator_configured.code_ != status_code::ok) {
                std::fprintf(stderr, "cascade coordinator configure failed (%d): %s\n",
                    static_cast<int>(coordinator_configured.code_),
                    coordinator_configured.message_.c_str());
                (void)vqec_vision_ai_appl_svcmn_stop_cascade_graphs(cascade_owners);
                return 1;
            }
            const auto cascade_bound = executor->vqec_vision_ai_appl_rtexe_bind_cascade(
                source_slot, owner.root_model_slot_, *owner.coordinator_,
                coordinator_config.max_tasks_per_frame_);
            if (cascade_bound.code_ != status_code::ok) {
                std::fprintf(stderr, "runtime cascade binding failed (%d): %s\n",
                    static_cast<int>(cascade_bound.code_),
                    cascade_bound.message_.c_str());
                (void)vqec_vision_ai_appl_svcmn_stop_cascade_graphs(cascade_owners);
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
            (void)vqec_vision_ai_appl_svcmn_stop_cascade_graphs(cascade_owners);
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
            (void)vqec_vision_ai_appl_svcmn_stop_cascade_graphs(cascade_owners);
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
            (void)vqec_vision_ai_appl_svcmn_stop_cascade_graphs(cascade_owners);
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
                (void)vqec_vision_ai_appl_svcmn_stop_cascade_graphs(cascade_owners);
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
                prepared = vqec_vision_ai_appl_svcmn_make_offline_graph_session(
                    source, *primary_model, *enrollment_detector_model,
                    enrollment_detector_graph_session);
            }
            if (prepared.code_ == status_code::ok) {
                prepared = vqec_vision_ai_appl_svcmn_make_offline_graph_session(
                    source, *secondary_model, *enrollment_embedding_model,
                    enrollment_embedding_graph_session);
            }
            const std::array<cascade_graph_session*, 2> enrollment_graph_sessions{
                enrollment_detector_graph_session.get(),
                enrollment_embedding_graph_session.get()};
            if (prepared.code_ == status_code::ok) {
                prepared = vqec_vision_ai_appl_svcmn_start_graph_sessions(
                    enrollment_graph_sessions);
            }
            if (prepared.code_ != status_code::ok) {
                std::fprintf(stderr, "enrollment graph startup failed (%d): %s\n",
                    static_cast<int>(prepared.code_), prepared.message_.c_str());
                (void)vqec_vision_ai_appl_svcmn_stop_graph_sessions(
                    enrollment_graph_sessions);
                (void)vqec_vision_ai_appl_svcmn_stop_cascade_graphs(cascade_owners);
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
                (void)vqec_vision_ai_appl_svcmn_stop_graph_sessions(
                    enrollment_graph_sessions);
                (void)vqec_vision_ai_appl_svcmn_stop_cascade_graphs(cascade_owners);
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
                (void)vqec_vision_ai_appl_svcmn_stop_cascade_graphs(cascade_owners);
                return 1;
            }
        }
#else
        if (args.enrollment_dbus) {
            std::fprintf(stderr,
                "face enrollment DBus was requested but adapter is not built\n");
            (void)stop_enrollment_graphs();
            (void)vqec_vision_ai_appl_svcmn_stop_cascade_graphs(cascade_owners);
            return 1;
        }
#endif
        recognition_enabled = true;
    }
    if (has_feature_wiring) {
        executor->vqec_vision_ai_appl_rtexe_bind_event_delivery(output_policy_gate, event_sink);
    }
    const auto activated =
        bundle->vqec_vision_ai_appl_rcfac_get_composition()->vqec_vision_ai_cntr_acomp_activate();
    if (activated.code_ != status_code::ok) {
        std::fprintf(stderr, "composition activation failed (%d): %s\n",
            static_cast<int>(activated.code_), activated.message_.c_str());
        (void)stop_enrollment_graphs();
        (void)vqec_vision_ai_appl_svcmn_stop_cascade_graphs(cascade_owners);
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
    bool reconcile_requested = false;
    bool generation_published = _control_manager == nullptr;
    while (!g_stop_requested && (args.max_steps == 0 || steps < args.max_steps)) {
        if (_poll_control) {
            _poll_control();
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
        const bool source_session_quiescent = stepped.code_ == status_code::ok;
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
                                service_harness::g_policy_revision,
                                deployment.sources_[taken.source_index_].source_id_,
                                args.fr_feature_id, {args.fr_identity_attribute}};
                            const auto authorized = output_policy_gate
                                .vqec_vision_ai_core_otgat_authorize(identity_scope, now_ns);
                            if (authorized.code_ != status_code::ok) {
                                std::fprintf(stderr, "FR identity output denied (%d): %s\n",
                                    static_cast<int>(authorized.code_), authorized.message_.c_str());
                            } else {
                                const auto labelled = recognition
                                    .vqec_vision_ai_embed_rcses_apply_labels(
                                        recognition_results, tracked[owner.root_model_slot_]);
                                if (labelled.code_ != status_code::ok) {
                                    std::fprintf(stderr, "FR label correlation failed (%d): %s\n",
                                        static_cast<int>(labelled.code_), labelled.message_.c_str());
                                }
                            }
                        }
                    }
                }
                const auto tracked_count = tracked[taken.model_slot_].observations_.size();
                if (use_production_platform &&
                    taken.source_index_ < deployment_limits::g_max_sources &&
                    taken.model_slot_ < deployment_limits::g_max_models_per_source) {
                    latest_model_observations[taken.source_index_][taken.model_slot_] =
                        std::move(tracked[taken.model_slot_]);
                    vqec_vision_ai_appl_svcmn_merge_observations(
                        latest_model_observations[taken.source_index_],
                        static_cast<std::uint16_t>(
                            deployment.sources_[taken.source_index_].model_ids_.size()),
                        latest_overlay_observations[taken.source_index_]);
                }
                std::printf("routed source=%u model=%u tracked=%zu delivered=%u "
                    "cascade_accepted=%u embedded=%u cascade_failed=%u\n",
                    static_cast<unsigned>(taken.source_index_),
                    static_cast<unsigned>(taken.model_slot_),
                    tracked_count,
                    static_cast<unsigned>(dispatch_report.delivered_),
                    static_cast<unsigned>(taken.cascade_.accepted_),
                    static_cast<unsigned>(taken.cascade_.embedded_),
                    static_cast<unsigned>(taken.cascade_.failed_));
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
                auto& overlay = latest_overlay_observations[source_slot];
                if (overlay.frame_.source_epoch_ != 0 &&
                    overlay.frame_.source_epoch_ !=
                        preview_frame.descriptor_.session_epoch_) {
                    latest_model_observations[source_slot] = {};
                    overlay = {};
                }
                const auto rendered = production.vqec_vision_ai_appl_pdplt_render(
                    source_slot, preview_frame, overlay);
                if (rendered.code_ != status_code::ok &&
                    rendered.code_ != status_code::pending) {
                    std::fprintf(stderr, "render failed (%d): %s\n",
                        static_cast<int>(rendered.code_), rendered.message_.c_str());
                }
            }
        }
        ++steps;
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
    bool stopped = false;
    for (unsigned drain = 0; drain < 1000 && !stopped; ++drain) {
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
        vqec_vision_ai_appl_svcmn_stop_cascade_graphs(cascade_owners);
    if (cascade_stopped.code_ != status_code::ok && first_error_code == status_code::ok) {
        first_error_code = cascade_stopped.code_;
    }
    const auto e2e_avg_us = metrics.end_to_end_samples_ == 0 ? 0ULL :
        metrics.end_to_end_ns_sum_ / (1000ULL * metrics.end_to_end_samples_);
    std::printf("metrics steps=%llu routed=%llu delivered=%llu denied=%llu failed=%llu "
        "cascade_tasks=%llu cascade_embeddings=%llu cascade_failed=%llu "
        "e2e_avg_us=%llu e2e_min_us=%llu e2e_max_us=%llu samples=%u\n",
        static_cast<unsigned long long>(metrics.steps_),
        static_cast<unsigned long long>(metrics.results_routed_),
        static_cast<unsigned long long>(metrics.events_delivered_),
        static_cast<unsigned long long>(metrics.events_denied_),
        static_cast<unsigned long long>(metrics.events_failed_),
        static_cast<unsigned long long>(metrics.cascade_tasks_accepted_),
        static_cast<unsigned long long>(metrics.cascade_embeddings_),
        static_cast<unsigned long long>(metrics.cascade_tasks_failed_),
        static_cast<unsigned long long>(e2e_avg_us),
        static_cast<unsigned long long>(metrics.end_to_end_samples_ == 0 ? 0ULL :
            metrics.end_to_end_ns_min_ / 1000ULL),
        static_cast<unsigned long long>(metrics.end_to_end_ns_max_ / 1000ULL),
        metrics.end_to_end_samples_);
    std::printf("service stopped=%s routed_sources=%u first_error=%d\n",
        stopped ? "true" : "false", routed_sources, static_cast<int>(first_error_code));
    // Owners (feature manager, fan-outs, registries, reference platform) outlive the
    // bundle; the bundle's composition must be stopped before they are destroyed.
    if (!stopped || enrollment_stopped.code_ != status_code::ok ||
        cascade_stopped.code_ != status_code::ok) {
        return g_recovery_required_exit_code;
    }
    if (!generation_published || first_error_code != status_code::ok) {
        return 1;
    }
    if (reconcile_requested) {
        return g_reconcile_generation_exit_code;
    }
    return routed_sources >= args.require_sources ? 0 : 1;
}

int main(int _argc, char** _argv) {
    parsed_arguments args;
    if (!vqec_vision_ai_appl_svcmn_parse(_argc, _argv, args)) {
        return vqec_vision_ai_appl_svcmn_run_generation(
            _argc, _argv, nullptr, nullptr, {}, 0, 0);
    }
    if (!args.usecase_dbus) {
        if (!args.usecase_service_name.empty() || !args.usecase_object_path.empty() ||
            !args.usecase_peer_name.empty() || args.usecase_rpc_timeout_ms != 0 ||
            args.usecase_callbacks_per_poll != 0) {
            std::fprintf(stderr, "usecase DBus settings require --usecase-dbus\n");
            return 2;
        }
        return vqec_vision_ai_appl_svcmn_run_generation(
            _argc, _argv, nullptr, nullptr, {}, 0, 0);
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
    if (!vqec_vision_ai_appl_svcmn_load_model_catalog(args.catalog_path, catalog) ||
        !vqec_vision_ai_appl_svcmn_load_deployment(
            args.deployment_path, base_deployment) ||
        !vqec_vision_ai_appl_svcmn_load_usecase_snapshot(
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
            _argc, _argv, &current_deployment, &manager, poll_control,
            generation, pending_revision);
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
