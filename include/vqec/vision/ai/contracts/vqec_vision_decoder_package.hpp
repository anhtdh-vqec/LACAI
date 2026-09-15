#ifndef VQEC_VISION_AI_CONTRACTS_DECODER_PACKAGE_HPP
#define VQEC_VISION_AI_CONTRACTS_DECODER_PACKAGE_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "vqec/vision/ai/contracts/vqec_vision_image_enums.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_observation.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_preprocess_spec.hpp"

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
inline constexpr std::size_t g_max_embedding_dimensions = 4096;
inline constexpr std::uint32_t g_min_destination_dimension = 8;
inline constexpr std::uint32_t g_max_destination_dimension = 4096;
}  // namespace decoder_package_limits

enum class decoder_package_kind { yolov8, anchor_distance, embedding };

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
    // Embedding kind: output tensor identity, expected dimension and normalization, plus the
    // landmark alignment template and destination color policy.
    std::string embedding_output_tensor_;
    std::size_t embedding_dimension_{0};
    float min_norm_{0.0F};
    std::uint32_t destination_width_{0};
    std::uint32_t destination_height_{0};
    std::vector<landmark_point> reference_points_;
    color_matrix color_matrix_{color_matrix::unspecified};
    color_range color_range_{color_range::unspecified};
    channel_order channel_order_{channel_order::rgb};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_CONTRACTS_DECODER_PACKAGE_HPP
