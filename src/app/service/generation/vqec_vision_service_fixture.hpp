#ifndef VQEC_VISION_AI_APPL_SERVICE_FIXTURE_HPP
#define VQEC_VISION_AI_APPL_SERVICE_FIXTURE_HPP

#include <cstddef>
#include <cstdint>

#include "vqec/vision/ai/contracts/vqec_vision_recognition.hpp"

namespace vqec::vision::ai::service_harness {

// Device-free harness values only. Production receives corresponding policy through
// validated deployment, model, feature and hardware metadata.
inline constexpr std::uint64_t g_default_startup_timeout_ns = 30000000000ULL;
inline constexpr std::uint64_t g_default_stop_timeout_ns = 10000000000ULL;
inline constexpr int g_default_rpc_timeout_ms = 1000;
inline constexpr std::uint64_t g_cycle_id_stride = 100;
inline constexpr std::uint64_t g_policy_revision = 1;
inline constexpr std::uint64_t g_config_revision = 1;
inline constexpr std::uint64_t g_policy_expiry_ns = 1000000000000000000ULL;
inline constexpr std::uint64_t g_initial_gallery_revision = 1;
inline constexpr std::size_t g_fr_max_subjects = recognition_limits::g_max_subjects;
inline constexpr std::uint64_t g_overlay_max_age_ns = 2000000000ULL;
inline constexpr std::uint64_t g_output_bytes = 16;
inline constexpr char g_box_tensor_name[] = "boxes";
inline constexpr std::uint32_t g_box_elements = 4;
inline constexpr char g_fixture_model_root[] = "/nonexistent/lacai-harness/models/";
inline constexpr char g_fixture_backend_library[] =
    "/nonexistent/lacai-harness/libQnnHtp.so";
inline constexpr char g_fixture_system_library[] =
    "/nonexistent/lacai-harness/libQnnSystem.so";
inline constexpr char g_fw_dmabuf_contract[] = "fw.dmabuf.v1";
inline constexpr char g_qcom_dmabuf_contract[] = "qcom.dmabuf.v1";

}  // namespace vqec::vision::ai::service_harness

#endif  // VQEC_VISION_AI_APPL_SERVICE_FIXTURE_HPP
