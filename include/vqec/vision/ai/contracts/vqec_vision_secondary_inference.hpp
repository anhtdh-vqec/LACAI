#ifndef VQEC_VISION_AI_CONTRACTS_SECONDARY_INFERENCE_HPP
#define VQEC_VISION_AI_CONTRACTS_SECONDARY_INFERENCE_HPP

#include <cstddef>
#include <cstdint>
#include <vector>

#include "vqec/vision/ai/contracts/vqec_vision_status.hpp"

namespace vqec::vision::ai {

namespace secondary_inference_limits {
inline constexpr std::size_t g_max_queue = 64;
inline constexpr std::size_t g_max_payload_bytes = 4096;
inline constexpr std::uint16_t g_max_sources = 16;
inline constexpr std::uint16_t g_max_models = 16;
}  // namespace secondary_inference_limits

enum class secondary_priority { low, normal, high };

// Region of interest in source pixel coordinates for a cascade stage (face, plate, pose,
// embedding). A task without an ROI is a whole-frame secondary inference.
struct secondary_roi {
    float x_{0};
    float y_{0};
    float width_{0};
    float height_{0};
};

// A cascade request produced by a feature/decoder from a primary result. It never names a
// vendor backend: the scheduler maps it to a registered model slot.
struct secondary_inference_request {
    std::uint64_t task_id_{0};
    std::uint16_t source_slot_{0};
    std::uint16_t model_slot_{0};
    std::uint64_t source_epoch_{0};
    std::uint64_t source_frame_id_{0};
    std::uint64_t source_pts_ns_{0};
    bool has_roi_{false};
    secondary_roi roi_{};
    // 0 means no track association.
    std::uint64_t track_id_{0};
    // 0 means no deadline.
    std::uint64_t deadline_ns_{0};
    secondary_priority priority_{secondary_priority::normal};
};

struct secondary_inference_result {
    secondary_inference_request request_;
    status result_;
    bool cancelled_{false};
    // True when the source epoch changed before the consumer observed the result.
    bool stale_epoch_{false};
    std::vector<std::uint8_t> payload_;
};

// Structural validation only; no model trust, entitlement or backend decision.
[[nodiscard]] status vqec_vision_ai_core_secin_validate_request(
    const secondary_inference_request& _request);

[[nodiscard]] status vqec_vision_ai_core_secin_validate_result(
    const secondary_inference_result& _result);

// Backend executed by the scheduler on the caller's serialized thread. It returns a bounded
// opaque payload; it does not own queues, tasks or result lifetime.
class secondary_inference_backend {
public:
    virtual ~secondary_inference_backend() = default;
    [[nodiscard]] virtual status vqec_vision_ai_cntr_secin_execute(
        const secondary_inference_request& _request,
        std::vector<std::uint8_t>& _payload) = 0;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_CONTRACTS_SECONDARY_INFERENCE_HPP
