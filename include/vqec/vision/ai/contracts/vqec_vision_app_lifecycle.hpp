#ifndef VQEC_VISION_AI_CONTRACTS_APP_LIFECYCLE_HPP
#define VQEC_VISION_AI_CONTRACTS_APP_LIFECYCLE_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "vqec/vision/ai/contracts/vqec_vision_status.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_version_registry.h"

namespace vqec::vision::ai {

namespace app_lifecycle_limits {
inline constexpr std::uint32_t g_schema_version =
    VQEC_VISION_AI_BASELINE_SCHEMA_VERSION;
inline constexpr std::uint32_t g_runtime_abi_major = VQEC_VISION_AI_BASELINE_ABI_MAJOR;
inline constexpr std::uint32_t g_runtime_abi_minor = VQEC_VISION_AI_BASELINE_ABI_MINOR;
inline constexpr std::size_t g_max_document_bytes = 512U * 1024U;
inline constexpr std::size_t g_max_json_depth = 16;
inline constexpr std::size_t g_max_identifier_bytes = 128;
inline constexpr std::size_t g_max_version_bytes = 64;
inline constexpr std::size_t g_max_reference_bytes = 256;
inline constexpr std::size_t g_max_targets = 16;
inline constexpr std::size_t g_max_components = 32;
inline constexpr std::size_t g_max_features = 16;
inline constexpr std::size_t g_max_scopes = 32;
inline constexpr std::size_t g_max_applications = 64;
inline constexpr std::size_t g_max_sources = 16;
inline constexpr std::size_t g_max_associations =
    g_max_applications * g_max_sources;
}  // namespace app_lifecycle_limits

enum class app_component_type { model, labels, ontology, rules, configuration };
enum class app_model_role { none, primary, secondary, offline };
enum class app_install_state {
    not_installed,
    installed_disabled,
    running,
    locked,
    incompatible,
    resource_limited,
    faulted
};
enum class app_operation_state {
    queued,
    staging,
    verifying,
    installing,
    reconciling,
    committed,
    rolled_back,
    cancelled,
    failed,
    recovery_required
};

struct app_component_manifest {
    std::string component_id_;
    std::string component_version_;
    app_component_type type_{app_component_type::model};
    std::string target_id_;
    std::string artifact_sha256_;
    std::string semantic_contract_sha256_;
    bool required_{false};
    app_model_role model_role_{app_model_role::none};
    std::string quality_receipt_ref_;
};

struct app_feature_manifest {
    std::string feature_id_;
    std::string processor_contract_;
    std::string configuration_schema_id_;
};

struct app_requested_scopes {
    std::vector<std::string> sources_;
    std::vector<std::string> outputs_;
    std::vector<std::string> queries_;
    std::vector<std::string> evidence_;
};

struct app_resource_envelope {
    std::uint64_t max_resident_bytes_{0};
    std::uint64_t max_tensor_bytes_{0};
    std::size_t max_active_incidents_{0};
    double max_events_per_second_{0.0};
};

struct usecase_app_manifest {
    std::uint32_t schema_version_{0};
    std::string app_id_;
    std::string usecase_id_;
    std::string app_version_;
    std::string usecase_version_;
    std::uint64_t release_sequence_{0};
    std::uint32_t runtime_abi_major_{0};
    std::uint32_t runtime_abi_minor_{0};
    std::vector<std::string> target_ids_;
    std::vector<app_component_manifest> components_;
    std::vector<app_feature_manifest> features_;
    std::string configuration_schema_id_;
    std::uint32_t configuration_schema_version_{0};
    std::string configuration_defaults_sha256_;
    app_requested_scopes requested_scopes_;
    app_resource_envelope resources_;
    std::string metadata_profile_ref_;
    bool uninstall_purges_data_{false};
    std::string sbom_ref_;
    std::string provenance_ref_;
    std::string known_limits_ref_;
    std::string rollback_predecessor_;
};

struct app_runtime_association {
    std::string app_id_;
    std::string source_id_;
    bool installed_{false};
    bool entitled_{false};
    bool desired_{false};
    bool supported_{false};
    bool compatible_{false};
    bool admitted_{false};
    std::uint64_t configuration_revision_{0};
    std::string configuration_sha256_;
    std::string configuration_schema_id_;
    std::vector<std::uint8_t> configuration_payload_;
    std::vector<std::string> output_scopes_;
    std::string reason_code_;
    std::uint64_t entitlement_expires_utc_ns_{0};

    [[nodiscard]] bool is_effective() const noexcept {
        return installed_ && entitled_ && desired_ && supported_ && compatible_ && admitted_;
    }
};

struct runtime_control_snapshot {
    std::uint32_t schema_version_{0};
    std::uint64_t snapshot_revision_{0};
    std::uint64_t inventory_revision_{0};
    std::uint64_t entitlement_revision_{0};
    std::uint64_t desired_revision_{0};
    std::vector<app_runtime_association> associations_;
};

[[nodiscard]] status vqec_vision_ai_core_applc_validate_manifest(
    const usecase_app_manifest& _manifest);
[[nodiscard]] status vqec_vision_ai_core_applc_validate_runtime_snapshot(
    const runtime_control_snapshot& _snapshot);

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_CONTRACTS_APP_LIFECYCLE_HPP

