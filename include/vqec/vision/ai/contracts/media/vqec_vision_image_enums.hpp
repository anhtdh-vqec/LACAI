#ifndef VQEC_VISION_AI_CONTRACTS_IMAGE_ENUMS_HPP
#define VQEC_VISION_AI_CONTRACTS_IMAGE_ENUMS_HPP

namespace vqec::vision::ai {

// Shared image descriptors used by the inference plan and the preprocess spec. Kept in one
// header so both contracts include each other without a cycle.
enum class channel_order { rgb, bgr };
enum class image_placement { unspecified, top_left, centre, stretch };

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_CONTRACTS_IMAGE_ENUMS_HPP
