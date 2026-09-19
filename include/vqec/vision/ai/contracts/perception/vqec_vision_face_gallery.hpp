#ifndef VQEC_VISION_AI_CONTRACTS_FACE_GALLERY_HPP
#define VQEC_VISION_AI_CONTRACTS_FACE_GALLERY_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "vqec/vision/ai/contracts/base/vqec_vision_version_registry.h"

#include "vqec/vision/ai/contracts/perception/vqec_vision_embedding.hpp"

namespace vqec::vision::ai {

namespace face_gallery_limits {
inline constexpr std::uint32_t g_schema_version = VQEC_VISION_AI_BASELINE_SCHEMA_VERSION;
inline constexpr std::size_t g_max_records = 4096;
}

struct face_gallery_config {
    std::string gallery_id_;
    std::string model_id_;
    std::string model_version_;
    std::uint64_t preprocess_revision_{0};
    std::size_t dimensions_{0};
    std::size_t capacity_{0};
    std::size_t max_templates_per_subject_{0};
};

struct face_gallery_template {
    std::uint64_t record_id_{0};
    std::string subject_ref_;
    std::vector<float> values_;
};

// Complete authoritative state. The protected-store adapter atomically replaces one
// validated snapshot under revision CAS; Zvec is rebuilt/synchronized from this state.
struct face_gallery_snapshot {
    std::uint32_t schema_version_{0};
    std::uint64_t revision_{0};
    std::uint64_t next_record_id_{0};
    std::string gallery_id_;
    std::string model_id_;
    std::string model_version_;
    std::uint64_t preprocess_revision_{0};
    std::size_t dimensions_{0};
    std::vector<face_gallery_template> templates_;
};

[[nodiscard]] status vqec_vision_ai_core_fgalr_validate_config(
    const face_gallery_config& _config);
[[nodiscard]] status vqec_vision_ai_core_fgalr_validate_snapshot(
    const face_gallery_config& _config, const face_gallery_snapshot& _snapshot);
[[nodiscard]] status vqec_vision_ai_core_fgalr_validate_replacement(
    const face_gallery_config& _config, std::uint64_t _expected_revision,
    const face_gallery_snapshot& _replacement);

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_CONTRACTS_FACE_GALLERY_HPP
