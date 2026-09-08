#ifndef VQEC_VISION_AI_CONTRACTS_SOURCE_BINDING_HPP
#define VQEC_VISION_AI_CONTRACTS_SOURCE_BINDING_HPP

#include <cstdint>
#include <string>

#include "vqec/vision/ai/contracts/vqec_vision_inference_plan.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_status.hpp"

namespace vqec::vision::ai {

enum class source_memory_kind { unspecified, dmabuf, opaque_fd };
enum class source_memory_layout { unspecified, linear_nv12, ubwc };
enum class source_sync_mode { unspecified, implicit_ready, explicit_fence };
enum class source_color_profile { unspecified, bt601_limited, bt709_limited };
enum class source_chroma_site { unspecified, mpeg2, jpeg };

// Trusted deployment policy, not legacy wire metadata or a hardware capability probe.
// Evidence IDs refer to approved FW/BSP/model reports; syntax validation is not approval.
struct source_binding {
    std::uint32_t width_{0};
    std::uint32_t height_{0};
    std::uint32_t fps_numerator_{0};
    std::uint32_t fps_denominator_{0};
    source_memory_kind memory_kind_{source_memory_kind::unspecified};
    source_memory_layout layout_{source_memory_layout::unspecified};
    source_sync_mode sync_mode_{source_sync_mode::unspecified};
    source_color_profile color_profile_{source_color_profile::unspecified};
    source_chroma_site chroma_site_{source_chroma_site::unspecified};
    std::string fw_memory_contract_;
    std::string backend_memory_contract_;
    std::string preprocess_contract_;
};

// Pure validation, no I/O or mutation. Rejects unsupported policies and plan mismatch.
[[nodiscard]] status vqec_vision_ai_core_srcbd_validate_binding(
    const source_binding& _binding, const inference_plan& _plan);

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_CONTRACTS_SOURCE_BINDING_HPP
