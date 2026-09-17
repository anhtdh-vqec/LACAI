#include "vqec_vision_service_options.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>

namespace vqec::vision::ai {

bool vqec_vision_ai_appl_svopt_parse(int _argc, char** _argv, parsed_arguments& _args) {
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
                        service_options_limits::g_nanoseconds_per_microsecond) {
                std::fprintf(stderr, "invalid runtime step interval\n");
                return false;
            }
            _args.runtime_step_interval_ns =
                interval_us * service_options_limits::g_nanoseconds_per_microsecond;
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
        } else if (option == "--qnn-backend-library" && has_value) {
            _args.qnn_backend_library = _argv[++index];
        } else if (option == "--qnn-system-library" && has_value) {
            _args.qnn_system_library = _argv[++index];
        } else if (option == "--model-root" && has_value) {
            _args.model_root = _argv[++index];
        } else if (option == "--hardware-profile" && has_value) {
            _args.hardware_profile_path = _argv[++index];
        } else if (option == "--max-artifact-bytes" && has_value) {
            _args.max_artifact_bytes = static_cast<std::uint64_t>(
                std::strtoull(_argv[++index], nullptr, 10));
        } else if (option == "--tracker-contract" && has_value) {
            _args.tracker_contract = _argv[++index];
        } else if (option == "--event-schema-id" && has_value) {
            _args.event_schema_id = _argv[++index];
        } else if (option == "--event-schema-version" && has_value) {
            _args.event_schema_version = _argv[++index];
        } else if (option == "--consumer-id-prefix" && has_value) {
            _args.consumer_id_prefix = _argv[++index];
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

}  // namespace vqec::vision::ai
