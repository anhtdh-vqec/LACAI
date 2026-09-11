#ifndef VQEC_VISION_AI_FTMGR_FEATURE_ACTIVATION_MANAGER_HPP
#define VQEC_VISION_AI_FTMGR_FEATURE_ACTIVATION_MANAGER_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "vqec_vision_feature_processor_registry.hpp"
#include "vqec_vision_feature_stage.hpp"

namespace vqec::vision::ai {

namespace feature_activation_limits {
inline constexpr std::size_t g_max_associations =
    deployment_limits::g_max_sources * feature_catalog_limits::g_max_features;
}  // namespace feature_activation_limits

enum class feature_effective_state {
    disabled,
    denied,
    unsupported,
    resource_limited,
    ready,
    faulted
};

struct feature_activation_request {
    std::string source_id_;
    std::string feature_id_;
    bool desired_enabled_{false};
    bool entitlement_granted_{false};
    bool resource_admitted_{false};
    feature_configuration configuration_;
};

struct feature_activation_record {
    std::string source_id_;
    std::string feature_id_;
    feature_effective_state state_{feature_effective_state::disabled};
    status_code reason_code_{status_code::ok};
    bool desired_enabled_{false};
    bool entitlement_granted_{false};
    bool resource_admitted_{false};
    std::unique_ptr<feature_processor_port> processor_;
    std::unique_ptr<feature_stage> stage_;
};

struct feature_activation_snapshot {
    std::uint64_t feature_catalog_revision_{0};
    std::uint64_t model_catalog_revision_{0};
    std::uint64_t deployment_revision_{0};
    std::uint16_t association_count_{0};
    std::uint16_t ready_count_{0};
    std::uint16_t denied_count_{0};
    std::uint16_t unsupported_count_{0};
    std::uint16_t resource_limited_count_{0};
    std::uint16_t faulted_count_{0};
};

// Cold-path owner for effective feature state. Catalog/deployment/factory references are
// borrowed and immutable; every ready association owns its processor and stage.
class feature_activation_manager final {
public:
    feature_activation_manager() = default;
    feature_activation_manager(const feature_activation_manager& _other) = delete;
    feature_activation_manager& operator=(const feature_activation_manager& _other) = delete;

    [[nodiscard]] status vqec_vision_ai_ftmgr_famgr_configure(
        const feature_catalog& _features, const model_catalog& _models,
        const deployment_config& _deployment);
    [[nodiscard]] status vqec_vision_ai_ftmgr_famgr_reconcile(
        const std::array<feature_activation_request,
            feature_activation_limits::g_max_associations>& _requests,
        std::uint16_t _request_count, const feature_processor_registry& _registry,
        feature_activation_snapshot& _snapshot);
    [[nodiscard]] const feature_activation_record*
    vqec_vision_ai_ftmgr_famgr_get_record(std::uint16_t _slot) const noexcept;
    [[nodiscard]] const feature_catalog_entry*
    vqec_vision_ai_ftmgr_famgr_get_feature(std::uint16_t _slot) const noexcept;
    [[nodiscard]] feature_stage*
    vqec_vision_ai_ftmgr_famgr_get_stage(std::uint16_t _slot) const noexcept;
    [[nodiscard]] std::uint16_t
    vqec_vision_ai_ftmgr_famgr_get_count() const noexcept;
    // Called once the owned processor/stage pointers have been lent to a fan-out. A frozen
    // manager rejects further reconcile so a live borrow cannot be invalidated; a new
    // generation needs a new manager after the old bundle and fan-outs are destroyed.
    void vqec_vision_ai_ftmgr_famgr_freeze() noexcept;
    [[nodiscard]] bool vqec_vision_ai_ftmgr_famgr_is_frozen() const noexcept;

private:
    [[nodiscard]] const feature_catalog_entry*
    vqec_vision_ai_ftmgr_famgr_find_feature(const std::string& _feature_id) const noexcept;
    [[nodiscard]] const source_deployment_config*
    vqec_vision_ai_ftmgr_famgr_find_source(const std::string& _source_id) const noexcept;
    [[nodiscard]] bool vqec_vision_ai_ftmgr_famgr_source_has_models(
        const source_deployment_config& _source,
        const feature_catalog_entry& _feature) const noexcept;

    const feature_catalog* features_{nullptr};
    const model_catalog* models_{nullptr};
    const deployment_config* deployment_{nullptr};
    std::array<feature_activation_record,
        feature_activation_limits::g_max_associations> records_{};
    std::uint16_t record_count_{0};
    bool is_configured_{false};
    bool is_frozen_{false};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_FTMGR_FEATURE_ACTIVATION_MANAGER_HPP
