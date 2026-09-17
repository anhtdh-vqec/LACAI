#ifndef VQEC_VISION_AI_RUNTIME_ADMISSION_HARDWARE_ADMISSION_PROFILE_HPP
#define VQEC_VISION_AI_RUNTIME_ADMISSION_HARDWARE_ADMISSION_PROFILE_HPP

#include <cstddef>
#include <istream>

#include "vqec_vision_activation_snapshot.hpp"

namespace vqec::vision::ai {

namespace hardware_profile_document_limits {
inline constexpr std::size_t g_max_document_bytes = 16U * 1024U;
inline constexpr int g_max_json_depth = 4;
inline constexpr std::size_t g_max_text_bytes = 256;
}  // namespace hardware_profile_document_limits

// Startup-only strict JSON load. The caller owns authentication and opening of the profile.
// A syntactically valid profile records provenance but does not prove its measurements.
[[nodiscard]] status vqec_vision_ai_admis_hwprf_load(
    std::istream& _stream, hardware_admission_profile& _profile);

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_RUNTIME_ADMISSION_HARDWARE_ADMISSION_PROFILE_HPP
