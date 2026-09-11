#ifndef VQEC_VISION_AI_CONTRACTS_VQEC_VISION_IDENTIFIER_HPP
#define VQEC_VISION_AI_CONTRACTS_VQEC_VISION_IDENTIFIER_HPP

#include <cstddef>
#include <string>

namespace vqec::vision::ai {

// Shared bounded ASCII identifier check for neutral config/catalog/wire names.
// Accepts A-Z a-z 0-9 and _ - . : ; rejects empty and over-length values.
// The caller supplies its own ceiling because limits are owned per contract,
// but the accepted character set must not diverge between validators.
[[nodiscard]] inline bool vqec_vision_ai_cntr_ident_is_valid(
    const std::string& _value, std::size_t _max_bytes) noexcept {
    if (_value.empty() || _value.size() > _max_bytes) {
        return false;
    }
    for (const unsigned char character : _value) {
        const bool is_alphanumeric = (character >= 'a' && character <= 'z') ||
            (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9');
        if (!is_alphanumeric && character != '_' && character != '-' &&
            character != '.' && character != ':') {
            return false;
        }
    }
    return true;
}

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_CONTRACTS_VQEC_VISION_IDENTIFIER_HPP
