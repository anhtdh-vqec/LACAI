#ifndef VQEC_VISION_AI_ATTR_ATTRIBUTE_READER_HPP
#define VQEC_VISION_AI_ATTR_ATTRIBUTE_READER_HPP

#include <cstdint>
#include <string>

#include "vqec/vision/ai/contracts/vqec_vision_observation.hpp"

namespace vqec::vision::ai {

// Returns a pointer borrowed from _batch. Failure leaves _attribute unchanged.
[[nodiscard]] status vqec_vision_ai_attr_atrdr_find_current_attribute(
    const observation_batch& _batch, std::uint64_t _track_id,
    const std::string& _schema_id, const std::string& _schema_version,
    std::uint64_t _now_source_ns, const observation_attribute*& _attribute);

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_ATTR_ATTRIBUTE_READER_HPP
