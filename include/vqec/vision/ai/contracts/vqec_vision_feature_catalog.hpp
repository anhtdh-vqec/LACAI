#ifndef VQEC_VISION_AI_CONTRACTS_FEATURE_CATALOG_HPP
#define VQEC_VISION_AI_CONTRACTS_FEATURE_CATALOG_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "vqec/vision/ai/contracts/vqec_vision_feature_event.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_model_catalog.hpp"

namespace vqec::vision::ai {

namespace feature_catalog_limits {
inline constexpr std::uint32_t g_schema_version = 1;
inline constexpr std::size_t g_max_features = 64;
inline constexpr std::size_t g_max_model_dependencies =
    deployment_limits::g_max_models_per_source;
inline constexpr std::size_t g_max_attribute_dependencies =
    observation_limits::g_max_attributes;
inline constexpr std::size_t g_max_identifier_bytes = 128;
inline constexpr std::size_t g_max_configuration_bytes = 256U * 1024U;
}  // namespace feature_catalog_limits

enum class feature_input_mode { single_model, temporal_join };

struct feature_model_dependency {
    std::string role_id_;
    std::string model_id_;
};

struct feature_attribute_dependency {
    std::string schema_id_;
    std::string schema_version_;
    std::uint64_t max_age_ns_{0};
};

struct feature_resource_profile {
    std::uint64_t max_temporal_bytes_per_source_{0};
    std::size_t max_events_per_update_{0};
    std::size_t max_track_references_per_event_{0};
    std::size_t max_fields_per_event_{0};
};

struct feature_catalog_entry {
    std::string feature_id_;
    std::string feature_version_;
    std::string processor_contract_;
    std::string configuration_schema_;
    feature_input_mode input_mode_{feature_input_mode::single_model};
    std::vector<feature_model_dependency> model_dependencies_;
    std::vector<feature_attribute_dependency> attribute_dependencies_;
    feature_resource_profile resources_;
};

struct feature_catalog {
    std::uint32_t schema_version_{0};
    std::uint64_t revision_{0};
    std::string catalog_id_;
    std::string model_catalog_ref_;
    std::vector<feature_catalog_entry> features_;
};

// Authenticated cold-path usecase configuration. Payload syntax is owned by the
// feature package named by schema_id; generic runtime only validates the binding/bound.
struct feature_configuration {
    std::string schema_id_;
    std::uint64_t revision_{0};
    std::vector<std::uint8_t> payload_;
};

// Cold-path metadata validation. This does not authenticate catalogs or activate features.
[[nodiscard]] status vqec_vision_ai_core_ftcat_validate_catalog(
    const feature_catalog& _catalog);
[[nodiscard]] status vqec_vision_ai_core_ftcat_validate_model_dependencies(
    const feature_catalog& _features, const model_catalog& _models);
[[nodiscard]] status vqec_vision_ai_core_ftcat_validate_configuration(
    const feature_catalog_entry& _feature,
    const feature_configuration& _configuration);

// Creates the neutral stage configuration from a validated catalog entry and one
// activation/source revision. Feature-specific settings remain owned by its processor.
[[nodiscard]] status vqec_vision_ai_core_ftcat_compose_processor_config(
    const feature_catalog_entry& _feature, const std::string& _source_id,
    std::uint64_t _activation_revision, feature_processor_config& _config);

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_CONTRACTS_FEATURE_CATALOG_HPP
