#ifndef VQEC_VISION_AI_CONTRACTS_INFERENCE_PLAN_HPP
#define VQEC_VISION_AI_CONTRACTS_INFERENCE_PLAN_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include <vqec/vision/ai/contracts/vqec_vision_status.hpp>

namespace vqec::vision::ai {

namespace inference_limits {
inline constexpr std::uint32_t g_min_tensor_dimension = 8;
inline constexpr std::uint32_t g_max_tensor_dimension = 4096;
inline constexpr std::uint32_t g_max_fps_numerator = 240000;
inline constexpr std::uint32_t g_max_fps_denominator = 10000;
inline constexpr std::uint32_t g_max_frames_per_second = 240;
inline constexpr std::uint64_t g_max_input_queue_bytes = 256ULL * 1024 * 1024;
inline constexpr std::uint32_t g_max_output_queue_buffers = 16;
inline constexpr std::size_t g_max_path_bytes = 4096;
}  // namespace inference_limits

enum class tensor_type { uint8, float32 };
enum class channel_order { rgb, bgr };
enum class image_placement { unspecified, top_left, centre, stretch };

// This describes a single-image plan, not the general multi-input model contract.
// Coefficients are explicit plugin coefficients; no implicit mean/std conversion.
struct inference_plan {
    // Required from the validated per-source deployment profile; no implicit 4K/FPS.
    std::uint32_t source_width_{0};
    std::uint32_t source_height_{0};
    std::uint32_t fps_numerator_{0};
    std::uint32_t fps_denominator_{0};
    std::uint32_t tensor_width_{0};
    std::uint32_t tensor_height_{0};
    tensor_type input_type_{tensor_type::uint8};
    channel_order channel_order_{channel_order::rgb};
    image_placement placement_{image_placement::unspecified};
    std::array<double, 3> mean_{0.0, 0.0, 0.0};
    std::array<double, 3> sigma_{1.0, 1.0, 1.0};
    std::string model_path_;
    std::string backend_path_;
    std::string system_path_;
    std::uint64_t input_queue_bytes_{0};
    std::uint32_t output_queue_buffers_{0};
};

// Nonblocking, thread-safe pure validation; does not touch target files or SDK.
// Checks structural limits only; success does not qualify a model or a board.
// Model paths must already be authorized and integrity-checked by the caller.
[[nodiscard]] status
vqec_vision_ai_core_infpl_validate_plan(const inference_plan& _plan);

// Returns packed NV12 bytes after validated even dimensions, or zero on invalid
// geometry. Allocation/stride-aware size must be used by the future frame bridge.
[[nodiscard]] std::uint64_t
vqec_vision_ai_core_infpl_get_packed_frame_bytes(const inference_plan& _plan) noexcept;

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_CONTRACTS_INFERENCE_PLAN_HPP
