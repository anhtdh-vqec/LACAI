#ifndef VQEC_VISION_AI_CONTRACTS_ACTIVATION_DELTA_HPP
#define VQEC_VISION_AI_CONTRACTS_ACTIVATION_DELTA_HPP

#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "vqec/vision/ai/contracts/features/vqec_vision_feature_catalog.hpp"
#include "vqec/vision/ai/contracts/features/vqec_vision_usecase_activation.hpp"
#include "vqec/vision/ai/contracts/lifecycle/vqec_vision_app_lifecycle.hpp"

namespace vqec::vision::ai {

namespace activation_delta_limits {
inline constexpr std::size_t g_max_model_dependencies =
    deployment_limits::g_max_sources * deployment_limits::g_max_models_per_source;
inline constexpr std::size_t g_max_feature_instances =
    usecase_activation_limits::g_max_associations * app_lifecycle_limits::g_max_features;
inline constexpr std::uint16_t g_invalid_slot =
    std::numeric_limits<std::uint16_t>::max();
}  // namespace activation_delta_limits

struct model_dependency_reference {
    std::string source_id_;
    std::string model_id_;
    std::string model_version_;
    std::string target_id_;
    std::string artifact_sha256_;
    std::string semantic_contract_sha256_;
    std::string preprocess_contract_;
    std::uint16_t source_slot_{activation_delta_limits::g_invalid_slot};
    std::uint16_t model_slot_{activation_delta_limits::g_invalid_slot};
    std::uint16_t consumer_count_{0};
    std::vector<std::string> consumer_app_ids_;
};

struct app_feature_instance {
    std::string source_id_;
    std::string app_id_;
    std::string feature_id_;
    std::string configuration_schema_id_;
    std::string configuration_sha256_;
    std::vector<std::uint8_t> configuration_payload_;
    std::vector<std::string> output_scopes_;
    std::uint64_t configuration_revision_{0};
    std::uint16_t source_slot_{activation_delta_limits::g_invalid_slot};
    std::uint16_t model_dependency_mask_{0};
};

struct app_activation_source_state {
    std::string source_id_;
    std::uint16_t active_model_mask_{0};
    std::uint16_t prepared_model_mask_{0};
};

struct app_activation_plan {
    std::array<app_activation_source_state, deployment_limits::g_max_sources> sources_{};
    std::vector<model_dependency_reference> model_dependencies_;
    std::vector<app_feature_instance> feature_instances_;
    std::uint64_t snapshot_revision_{0};
    std::uint64_t deployment_revision_{0};
    std::uint64_t model_catalog_revision_{0};
    std::uint64_t feature_catalog_revision_{0};
    std::uint64_t usecase_catalog_revision_{0};
    std::uint16_t source_count_{0};
};

enum class model_dependency_delta_kind { acquire, retain, release };
enum class feature_instance_delta_kind { add, replace, remove };

struct model_dependency_delta {
    std::string source_id_;
    std::string model_id_;
    model_dependency_delta_kind kind_{model_dependency_delta_kind::retain};
    std::uint16_t source_slot_{activation_delta_limits::g_invalid_slot};
    std::uint16_t model_slot_{activation_delta_limits::g_invalid_slot};
    std::uint16_t previous_consumer_count_{0};
    std::uint16_t candidate_consumer_count_{0};
};

struct feature_instance_delta {
    std::string source_id_;
    std::string app_id_;
    std::string feature_id_;
    feature_instance_delta_kind kind_{feature_instance_delta_kind::replace};
};

struct app_activation_delta {
    std::array<std::uint16_t, deployment_limits::g_max_sources>
        previous_active_model_masks_{};
    std::array<std::uint16_t, deployment_limits::g_max_sources>
        candidate_active_model_masks_{};
    std::vector<model_dependency_delta> model_dependencies_;
    std::vector<feature_instance_delta> feature_instances_;
    std::uint64_t previous_snapshot_revision_{0};
    std::uint64_t candidate_snapshot_revision_{0};
    bool requires_capacity_replacement_{false};
};

// Builds reference counts and feature instances from one complete trusted snapshot. The
// caller-supplied output is changed only after the whole plan validates.
[[nodiscard]] status vqec_vision_ai_core_acdel_build_plan(
    const deployment_config& _deployment, const model_catalog& _models,
    const feature_catalog& _features, const usecase_catalog& _usecases,
    const runtime_control_snapshot& _runtime, app_activation_plan& _plan);

// Compares two validated plans. A candidate must be strictly newer and have identical immutable
// catalog/deployment revisions. Failure preserves _delta.
[[nodiscard]] status vqec_vision_ai_core_acdel_build_delta(
    const app_activation_plan& _previous, const app_activation_plan& _candidate,
    app_activation_delta& _delta);

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_CONTRACTS_ACTIVATION_DELTA_HPP
