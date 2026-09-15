#ifndef VQEC_VISION_AI_CONTRACTS_IMAGE_ALIGNMENT_HPP
#define VQEC_VISION_AI_CONTRACTS_IMAGE_ALIGNMENT_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "vqec/vision/ai/contracts/vqec_vision_observation.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_preview_contract.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_status.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_tensor_result.hpp"

namespace vqec::vision::ai {

namespace image_alignment_limits {
inline constexpr std::size_t g_max_points = 128;
inline constexpr std::uint32_t g_min_destination_dimension = 8;
inline constexpr std::uint32_t g_max_destination_dimension = 4096;
}  // namespace image_alignment_limits

// Ordered landmark template for a secondary crop. The reference point order is the source
// landmark order the model was trained with; both lists must use the same schema id/version
// and point count.
struct alignment_template {
    std::string schema_id_;
    std::string schema_version_;
    std::uint32_t destination_width_{0};
    std::uint32_t destination_height_{0};
    std::vector<landmark_point> reference_points_;
};

enum class alignment_transform_kind { none, similarity };

// Row-major 2x3 transform mapping source pixels into the destination aligned space. It is
// returned as provenance so downstream results can be mapped back to the source frame; a
// transform is not an accuracy or calibration claim.
struct alignment_transform {
    alignment_transform_kind kind_{alignment_transform_kind::none};
    float m00_{1.0F};
    float m01_{0.0F};
    float m02_{0.0F};
    float m10_{0.0F};
    float m11_{1.0F};
    float m12_{0.0F};
    std::uint32_t source_width_{0};
    std::uint32_t source_height_{0};
    std::uint32_t destination_width_{0};
    std::uint32_t destination_height_{0};
};

// One alignment task. The source frame owner is borrowed separately by the port call and
// must stay alive until device completion; this struct only carries identity and landmarks.
struct alignment_request {
    preview_frame_key frame_;
    observation_landmarks landmarks_;
    // 0 means no deadline; the deadline is a caller steady-clock value, never a PTS.
    std::uint64_t deadline_ns_{0};
};

// Owned destination tensor and its transform. Completion is reported separately by the
// port; this struct does not assert device completion.
struct alignment_result {
    tensor_blob tensor_;
    alignment_transform transform_;
    bool has_transform_{false};
};

// Probed backend capability. A backend that cannot honor a template fails activation; there
// is no silent CPU fallback.
struct alignment_capabilities {
    bool supports_similarity_{false};
    std::size_t max_points_{0};
    std::uint32_t max_destination_dimension_{0};
};

// Structural validation only. No capability, ownership or device-completion decision.
[[nodiscard]] status vqec_vision_ai_core_imaln_validate_template(
    const alignment_template& _template);

[[nodiscard]] status vqec_vision_ai_core_imaln_validate_request(
    const alignment_request& _request, const alignment_template& _template);

// Fail-closed capability check for a declared template.
[[nodiscard]] status vqec_vision_ai_core_imaln_require_capability(
    const alignment_capabilities& _capabilities, const alignment_template& _template);

// Least-squares 4-DOF similarity (scale, rotation, translation) mapping source landmark
// pixels into destination (template) pixels. Requires equal sets of at least two points and
// rejects non-finite or degenerate (coincident) source points. Source/destination geometry
// in the transform is left for the caller to fill from the frame and template.
[[nodiscard]] status vqec_vision_ai_core_imaln_compute_similarity(
    const std::vector<landmark_point>& _source_points,
    const std::vector<landmark_point>& _reference_points,
    alignment_transform& _transform);

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_CONTRACTS_IMAGE_ALIGNMENT_HPP
