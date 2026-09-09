#ifndef VQEC_VISION_AI_DETEC_MODEL_DECODER_REGISTRY_HPP
#define VQEC_VISION_AI_DETEC_MODEL_DECODER_REGISTRY_HPP

#include <array>
#include <cstddef>
#include <string>

#include "vqec/vision/ai/contracts/vqec_vision_model_decoder.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_model_catalog.hpp"

namespace vqec::vision::ai {

namespace model_decoder_limits {
inline constexpr std::size_t g_max_registered_decoders = 64;
inline constexpr std::size_t g_max_contract_bytes = 128;
}  // namespace model_decoder_limits

// Non-owning registry populated during activation. Decoder implementations must
// outlive the registry and remain serialized by their owning executor.
class model_decoder_registry final {
public:
    model_decoder_registry() = default;
    model_decoder_registry(const model_decoder_registry& _other) = delete;
    model_decoder_registry& operator=(const model_decoder_registry& _other) = delete;

    [[nodiscard]] status vqec_vision_ai_detec_mdreg_register_decoder(
        const std::string& _contract, model_decoder_port& _decoder);
    [[nodiscard]] status vqec_vision_ai_detec_mdreg_resolve_decoder(
        const std::string& _contract, model_decoder_port*& _decoder) const noexcept;
    [[nodiscard]] status vqec_vision_ai_detec_mdreg_validate_model_outputs(
        const model_catalog_entry& _model, const model_outputs& _outputs) const;
    void vqec_vision_ai_detec_mdreg_clear() noexcept;
    [[nodiscard]] std::size_t
    vqec_vision_ai_detec_mdreg_get_count() const noexcept;

private:
    struct entry {
        std::string contract_;
        model_decoder_port* decoder_{nullptr};
    };
    std::array<entry, model_decoder_limits::g_max_registered_decoders> entries_{};
    std::size_t count_{0};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_DETEC_MODEL_DECODER_REGISTRY_HPP
