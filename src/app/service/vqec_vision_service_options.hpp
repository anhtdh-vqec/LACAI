#ifndef VQEC_VISION_AI_APPL_SERVICE_OPTIONS_HPP
#define VQEC_VISION_AI_APPL_SERVICE_OPTIONS_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "vqec/vision/ai/contracts/vqec_vision_inference_execution.hpp"

namespace vqec::vision::ai {

namespace service_options_limits {
// Fallback control-loop period when --runtime-step-interval-us is not supplied. A named
// default, not a per-call literal; the deployment may override it at startup.
inline constexpr std::uint64_t g_default_runtime_step_interval_ns = 10000000;
inline constexpr std::uint64_t g_nanoseconds_per_microsecond = 1000;
inline constexpr std::uint64_t g_default_max_artifact_bytes = 256ULL * 1024 * 1024;
}  // namespace service_options_limits

// Parsed process arguments. This is the cold-path startup contract between main() and the
// service run loop; it owns no runtime resource and performs no I/O itself.
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
    std::uint64_t runtime_step_interval_ns{
        service_options_limits::g_default_runtime_step_interval_ns};
    std::string model_package_registry_path;
    std::string model_package;
    std::string model_library;
    std::string qnn_backend_library;
    std::string qnn_system_library;
    // Explicit device-free fixture mode. Production defaults to registered DMA-BUF input.
    bool allow_qaic_copy_input{false};
    std::string model_root;
    std::string dsp_v1_skel_dir;
    std::string dsp_legacy_skel_dir;
    std::int32_t dsp_legacy_clock_corner{0};
    std::int32_t dsp_legacy_latency_us{0};
    bool dsp_enable_unsigned_pd{false};
    std::string hardware_profile_path;
    std::uint64_t max_artifact_bytes{0};
    std::string camera_socket_dir;
    std::uint32_t camera_producer_uid{0};
    std::uint32_t nv12_format_value{0};
    std::string tracker_contract;
    std::string event_schema_id;
    std::string event_schema_version;
    std::string consumer_id_prefix;
    std::string output_ring_id;
    std::uint32_t output_fps{0};
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

// Parses argv into _args. Returns false and prints the reason to stderr for an unknown or
// incomplete option, or when deployment/catalog paths are missing. On failure the caller
// must not run; _args may hold partial values and must be discarded.
[[nodiscard]] bool vqec_vision_ai_appl_svopt_parse(
    int _argc, char** _argv, parsed_arguments& _args);

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_APPL_SERVICE_OPTIONS_HPP
