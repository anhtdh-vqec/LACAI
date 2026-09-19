#ifndef VQEC_VISION_AI_CONTRACTS_USECASE_ACTIVATION_HPP
#define VQEC_VISION_AI_CONTRACTS_USECASE_ACTIVATION_HPP

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "vqec/vision/ai/contracts/base/vqec_vision_version_registry.h"

#include "vqec/vision/ai/contracts/features/vqec_vision_feature_catalog.hpp"

namespace vqec::vision::ai {

namespace usecase_activation_limits {
inline constexpr std::uint32_t g_schema_version = VQEC_VISION_AI_BASELINE_SCHEMA_VERSION;
inline constexpr std::size_t g_max_usecases = 64;
inline constexpr std::size_t g_max_root_models = deployment_limits::g_max_models_per_source;
inline constexpr std::size_t g_max_feature_ids = 64;
inline constexpr std::size_t g_max_identifier_bytes = deployment_limits::g_max_identifier_bytes;
inline constexpr std::size_t g_max_associations =
    deployment_limits::g_max_sources * g_max_usecases;
}  // namespace usecase_activation_limits

struct usecase_catalog_entry {
    std::string usecase_id_;
    std::string usecase_version_;
    std::vector<std::string> root_model_ids_;
    std::vector<std::string> feature_ids_;
};

struct usecase_catalog {
    std::uint32_t schema_version_{0};
    std::uint64_t revision_{0};
    std::string catalog_id_;
    std::string model_catalog_ref_;
    std::vector<usecase_catalog_entry> usecases_;
};

enum class usecase_effective_state {
    disabled,
    not_installed,
    denied,
    unsupported,
    incompatible,
    resource_limited,
    ready
};

struct usecase_activation_request {
    std::string source_id_;
    std::string usecase_id_;
    bool desired_enabled_{false};
    bool installed_{false};
    bool entitlement_granted_{false};
    bool supported_{false};
    bool compatible_{false};
    bool resource_admitted_{false};
};

struct usecase_activation_record {
    std::string source_id_;
    std::string usecase_id_;
    usecase_effective_state state_{usecase_effective_state::disabled};
    status_code reason_code_{status_code::ok};
    bool desired_enabled_{false};
    bool installed_{false};
    bool entitlement_granted_{false};
    bool supported_{false};
    bool compatible_{false};
    bool resource_admitted_{false};
};

struct usecase_activation_snapshot {
    std::uint64_t usecase_catalog_revision_{0};
    std::uint64_t model_catalog_revision_{0};
    std::uint64_t deployment_revision_{0};
    std::uint64_t policy_revision_{0};
    std::uint64_t config_revision_{0};
    std::vector<usecase_activation_record> records_;
};

// Scoped association record that carries source, usecase, feature, model slot,
// attribute scopes and revisions together in one immutable record.
struct feature_scoped_association_record {
    std::string source_id_;
    std::string usecase_id_;
    std::string feature_id_;
    std::uint16_t model_slot_{std::numeric_limits<std::uint16_t>::max()};
    std::vector<std::string> attribute_scopes_;
    std::uint64_t deployment_revision_{0};
    std::uint64_t usecase_catalog_revision_{0};
    std::uint64_t model_catalog_revision_{0};
    std::uint64_t policy_revision_{0};
    std::uint64_t config_revision_{0};
    usecase_effective_state state_{usecase_effective_state::disabled};
    status_code reason_code_{status_code::ok};
    bool desired_enabled_{false};
    bool installed_{false};
    bool entitlement_granted_{false};
    bool supported_{false};
    bool compatible_{false};
    bool resource_admitted_{false};

    [[nodiscard]] bool is_ready() const noexcept {
        return state_ == usecase_effective_state::ready &&
               desired_enabled_ && installed_ && entitlement_granted_ &&
               supported_ && compatible_ && resource_admitted_;
    }
};

// Projection used when one runtime feature is referenced by more than one usecase. Gates
// are monotonic: entitlement is considered only for a desired association and resource
// admission only for a desired, entitled association. This prevents combining unrelated
// grants from different usecases into one effective ready feature.
struct feature_authority_projection {
    bool has_association_{false};
    bool desired_enabled_{false};
    bool entitlement_granted_{false};
    bool resource_admitted_{false};
};

// Validates product usecase metadata against primary model roots. Authentication and
// installation provenance remain outside this pure contract.
[[nodiscard]] status vqec_vision_ai_core_ucact_validate_catalog(
    const usecase_catalog& _usecases, const model_catalog& _models);

// Resolves a complete association snapshot before any platform graph is loaded. The base
// deployment is the maximum authorized model assignment; the effective deployment keeps
// only roots referenced by ready usecases, preserving base source/model order. An empty
// result is the valid idle plan and must bypass platform prepare/acquisition.
[[nodiscard]] status vqec_vision_ai_core_ucact_compose_effective_deployment(
    const deployment_config& _base_deployment, const model_catalog& _models,
    const usecase_catalog& _usecases,
    const std::vector<usecase_activation_request>& _requests,
    usecase_activation_snapshot& _snapshot, deployment_config& _effective_deployment);

// Resolves the authority gates for one source/feature from one immutable activation
// snapshot. Failure preserves _projection. A feature is ready only when at least one
// single usecase association supplies the complete desired+entitled+admitted chain.
[[nodiscard]] status vqec_vision_ai_core_ucact_project_feature_authority(
    const usecase_catalog& _usecases, const usecase_activation_snapshot& _snapshot,
    const std::string& _source_id, const std::string& _feature_id,
    feature_authority_projection& _projection);

// Resolves the complete immutable scoped association record for one source/feature from the
// activation snapshot and base deployment. A feature is ready only when at least one single
// usecase association supplies the complete desired+entitled+admitted chain.
[[nodiscard]] status vqec_vision_ai_core_ucact_project_feature_association(
    const usecase_catalog& _usecases, const usecase_activation_snapshot& _snapshot,
    const deployment_config& _deployment, const std::string& _source_id,
    const feature_catalog_entry& _feature, feature_scoped_association_record& _association);

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_CONTRACTS_USECASE_ACTIVATION_HPP
