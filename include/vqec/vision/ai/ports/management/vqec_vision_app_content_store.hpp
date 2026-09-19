#ifndef VQEC_VISION_AI_PORTS_APP_CONTENT_STORE_HPP
#define VQEC_VISION_AI_PORTS_APP_CONTENT_STORE_HPP

#include <cstdint>
#include <istream>
#include <string>

#include "vqec/vision/ai/contracts/base/vqec_vision_status.hpp"

namespace vqec::vision::ai {

struct app_content_record {
    std::string sha256_;
    std::uint64_t byte_size_{0};
    std::string immutable_location_;
};

class app_content_store_port {
public:
    virtual ~app_content_store_port() = default;

    [[nodiscard]] virtual status vqec_vision_ai_ports_apcst_open() = 0;
    [[nodiscard]] virtual status vqec_vision_ai_ports_apcst_put(
        std::istream& _source, const std::string& _expected_sha256,
        std::uint64_t _expected_bytes, app_content_record& _record) = 0;
    [[nodiscard]] virtual status vqec_vision_ai_ports_apcst_get(
        const std::string& _sha256, app_content_record& _record) const = 0;
    // The inventory owner may remove a blob only after proving no current or rollback
    // generation references it. The store deliberately does not infer references.
    [[nodiscard]] virtual status vqec_vision_ai_ports_apcst_remove(
        const std::string& _sha256) = 0;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_PORTS_APP_CONTENT_STORE_HPP
