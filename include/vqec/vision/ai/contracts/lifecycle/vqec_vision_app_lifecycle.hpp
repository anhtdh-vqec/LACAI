#ifndef VQEC_VISION_AI_CONTRACTS_APP_LIFECYCLE_HPP
#define VQEC_VISION_AI_CONTRACTS_APP_LIFECYCLE_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "vqec/vision/ai/contracts/base/vqec_vision_status.hpp"
#include "vqec/vision/ai/contracts/base/vqec_vision_version_registry.h"

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
inline constexpr std::uint64_t g_max_component_bytes =
    4ULL * 1024ULL * 1024ULL * 1024ULL;
inline constexpr std::size_t g_max_operation_message_bytes = 512;
inline constexpr std::size_t g_max_operations = 1024;
inline constexpr std::size_t g_max_pending_operations = 16;
inline constexpr std::size_t g_max_catalog_code_bytes = 16;
inline constexpr std::size_t g_max_display_name_bytes = 128;
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
enum class app_operation_kind {
    install,
    update,
    rollback,
    configure,
    entitlement,
    desired,
    uninstall
};

struct app_operation_request {
    std::string idempotency_key_;
    std::string payload_sha256_;
    std::string app_id_;
    app_operation_kind kind_{app_operation_kind::install};
};

struct app_operation_record {
    std::string operation_id_;
    std::string idempotency_key_;
    std::string payload_sha256_;
    std::string app_id_;
    app_operation_kind kind_{app_operation_kind::install};
    app_operation_state state_{app_operation_state::queued};
    status_code result_code_{status_code::pending};
    std::string result_message_;
    std::uint64_t snapshot_revision_{0};
};

struct app_component_manifest {
    std::string component_id_;
    std::string component_version_;
    app_component_type type_{app_component_type::model};
    std::string target_id_;
    std::string artifact_sha256_;
    std::uint64_t artifact_bytes_{0};
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

struct app_entitlement_grant {
    std::uint32_t schema_version_{0};
    std::string grant_id_;
    std::uint64_t grant_revision_{0};
    std::uint64_t expected_entitlement_revision_{0};
    std::string issuer_id_;
    std::string key_id_;
    std::string customer_id_;
    std::string device_id_;
    std::string target_id_;
    std::string app_id_;
    std::string source_id_;
    std::uint64_t not_before_utc_ns_{0};
    std::uint64_t expires_utc_ns_{0};
    bool granted_{false};
    std::vector<std::string> output_scopes_;
};

struct app_runtime_component {
    std::string component_id_;
    std::string component_version_;
    app_component_type type_{app_component_type::model};
    std::string target_id_;
    std::string artifact_sha256_;
    std::uint64_t artifact_bytes_{0};
    std::string semantic_contract_sha256_;
    app_model_role model_role_{app_model_role::none};
    std::string immutable_location_;
};

struct app_runtime_association {
    std::string app_id_;
    std::string source_id_;
    std::string app_version_;
    std::uint64_t release_sequence_{0};
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
    std::vector<app_runtime_component> components_;
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

struct app_catalog_entry {
    std::string catalog_code_;
    std::string app_id_;
    std::string display_name_;
    std::string app_version_;
    bool published_{false};
};

struct usecase_app_catalog {
    std::uint32_t schema_version_{0};
    std::string catalog_id_;
    std::uint64_t revision_{0};
    std::vector<app_catalog_entry> applications_;
};

struct app_catalog_status {
    app_catalog_entry catalog_;
    std::string source_id_;
    bool supported_{false};
    bool installed_{false};
    bool entitled_{false};
    bool desired_{false};
    bool effective_{false};
    app_install_state state_{app_install_state::not_installed};
    std::string reason_code_;
    std::uint64_t snapshot_revision_{0};
};

[[nodiscard]] status vqec_vision_ai_core_applc_validate_manifest(
    const usecase_app_manifest& _manifest);
[[nodiscard]] status vqec_vision_ai_core_applc_validate_entitlement(
    const app_entitlement_grant& _grant);
[[nodiscard]] status vqec_vision_ai_core_applc_validate_runtime_snapshot(
    const runtime_control_snapshot& _snapshot);
[[nodiscard]] status vqec_vision_ai_core_applc_validate_catalog(
    const usecase_app_catalog& _catalog);

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_CONTRACTS_APP_LIFECYCLE_HPP
