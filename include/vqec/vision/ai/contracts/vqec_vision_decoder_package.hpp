#ifndef VQEC_VISION_AI_CONTRACTS_DECODER_PACKAGE_HPP
#define VQEC_VISION_AI_CONTRACTS_DECODER_PACKAGE_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace vqec::vision::ai {

namespace decoder_package_limits {
inline constexpr std::size_t g_max_identifier_bytes = 128;
inline constexpr std::size_t g_max_classes = 4096;
inline constexpr std::size_t g_max_labels = 4096;
inline constexpr std::size_t g_max_stages = 8;
inline constexpr std::size_t g_max_landmarks = 16;
inline constexpr std::size_t g_max_candidates = 4096;
inline constexpr std::uint32_t g_max_stride = 1024;
inline constexpr std::size_t g_max_metadata_items = 8;
}  // namespace decoder_package_limits

enum class decoder_package_kind { yolov8, anchor_distance };

struct decoder_package_stage {
    std::string score_tensor_;
    std::string box_tensor_;
    std::string landmark_tensor_;
    std::uint32_t stride_{0};
    std::uint32_t grid_width_{0};
    std::uint32_t grid_height_{0};
    std::uint32_t anchors_per_cell_{0};
};

// Strictly validated decoder.json payload. No field is defaulted at load time: the package
// must declare every policy value this struct carries. It is model metadata only and does
// not authenticate the artifact or the referenced model library.
struct decoder_package {
    decoder_package_kind kind_{decoder_package_kind::yolov8};
    std::string decoder_contract_;
    std::string box_tensor_;
    std::string score_tensor_;
    std::size_t class_count_{0};
    std::vector<std::string> labels_;
    std::string labels_ref_;
    float confidence_threshold_{0.0F};
    float iou_threshold_{0.0F};
    std::string class_id_;
    std::string landmark_schema_id_;
    std::string landmark_schema_version_;
    std::size_t landmark_count_{0};
    float anchor_offset_cells_{0.0F};
    std::size_t max_candidates_{0};
    std::vector<decoder_package_stage> stages_;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_CONTRACTS_DECODER_PACKAGE_HPP
