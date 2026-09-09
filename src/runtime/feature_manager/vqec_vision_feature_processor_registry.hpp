#ifndef VQEC_VISION_AI_FTMGR_FEATURE_PROCESSOR_REGISTRY_HPP
#define VQEC_VISION_AI_FTMGR_FEATURE_PROCESSOR_REGISTRY_HPP

#include <array>
#include <cstddef>
#include <memory>
#include <string>

#include "vqec/vision/ai/ports/vqec_vision_feature_processor_factory.hpp"

namespace vqec::vision::ai {

namespace feature_processor_registry_limits {
inline constexpr std::size_t g_max_factories = feature_catalog_limits::g_max_features;
}  // namespace feature_processor_registry_limits

// Cold-path registry. Factories are borrowed and must outlive registry operations;
// every created processor owns its state independently. Calls are serialized with
// activation and package lifecycle.
class feature_processor_registry final {
public:
    feature_processor_registry() = default;
    feature_processor_registry(const feature_processor_registry& _other) = delete;
    feature_processor_registry& operator=(const feature_processor_registry& _other) = delete;

    [[nodiscard]] status vqec_vision_ai_ftmgr_ftreg_register_factory(
        const std::string& _processor_contract,
        feature_processor_factory_port& _factory);
    [[nodiscard]] status vqec_vision_ai_ftmgr_ftreg_resolve_factory(
        const std::string& _processor_contract,
        feature_processor_factory_port*& _factory) const noexcept;
    [[nodiscard]] status vqec_vision_ai_ftmgr_ftreg_create_processor(
        const feature_catalog_entry& _feature, const std::string& _source_id,
        const feature_configuration& _configuration,
        std::unique_ptr<feature_processor_port>& _processor) const;
    void vqec_vision_ai_ftmgr_ftreg_clear() noexcept;
    [[nodiscard]] std::size_t
    vqec_vision_ai_ftmgr_ftreg_get_count() const noexcept;

private:
    struct entry {
        std::string processor_contract_;
        feature_processor_factory_port* factory_{nullptr};
    };
    std::array<entry, feature_processor_registry_limits::g_max_factories> entries_{};
    std::size_t count_{0};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_FTMGR_FEATURE_PROCESSOR_REGISTRY_HPP
