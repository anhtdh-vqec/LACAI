#ifndef VQEC_VISION_AI_STOR_APP_CONTENT_STORE_HPP
#define VQEC_VISION_AI_STOR_APP_CONTENT_STORE_HPP

#include <cstddef>
#include <cstdint>
#include <string>

#include "vqec/vision/ai/ports/management/vqec_vision_app_content_store.hpp"

namespace vqec::vision::ai {

struct app_content_store_config {
    std::string root_directory_;
    std::uint64_t max_store_bytes_{0};
    std::uint64_t max_blob_bytes_{0};
    std::size_t max_blob_count_{0};
};

class app_content_store final : public app_content_store_port {
public:
    explicit app_content_store(app_content_store_config _config);

    [[nodiscard]] status vqec_vision_ai_ports_apcst_open() override;
    [[nodiscard]] status vqec_vision_ai_ports_apcst_put(
        std::istream& _source, const std::string& _expected_sha256,
        std::uint64_t _expected_bytes, app_content_record& _record) override;
    [[nodiscard]] status vqec_vision_ai_ports_apcst_get(
        const std::string& _sha256, app_content_record& _record) const override;
    [[nodiscard]] status vqec_vision_ai_ports_apcst_remove(
        const std::string& _sha256) override;

private:
    app_content_store_config config_;
    std::uint64_t stored_bytes_{0};
    std::size_t stored_blob_count_{0};
    bool open_{false};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_STOR_APP_CONTENT_STORE_HPP
