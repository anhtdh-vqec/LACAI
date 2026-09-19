#ifndef VQEC_VISION_AI_APPL_SERVICE_STARTUP_HPP
#define VQEC_VISION_AI_APPL_SERVICE_STARTUP_HPP

#include <cstdint>
#include <functional>
#include <string>

#include "vqec_vision_feature_catalog.hpp"
#include "vqec_vision_model_catalog.hpp"
#include "vqec_vision_model_package_registry.hpp"
#include "vqec_vision_service_options.hpp"
#include "vqec_vision_usecase_config.hpp"
#include "vqec_vision_usecase_control_manager.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_app_lifecycle.hpp"

namespace vqec::vision::ai {

// Cold-path result. It owns all parsed metadata used by one runtime generation.
struct service_startup_resolution {
    bool should_run{false};
    int exit_code{0};
    model_catalog catalog;
    deployment_config deployment;
    feature_catalog features;
    model_package_registry model_packages;
    usecase_control_snapshot usecase_control;
    usecase_activation_snapshot usecase_activation;
    runtime_control_snapshot runtime_control;
    bool has_usecase_control{false};
    bool has_runtime_control{false};
    bool use_reference_platform{false};
    bool use_production_platform{false};
    bool fr_effectively_enabled{false};
};

struct service_feature_authority_state {
    bool desired_enabled_{false};
    bool entitlement_granted_{false};
    bool resource_admitted_{false};
};

[[nodiscard]] bool vqec_vision_ai_appl_svstr_load_model_catalog(
    const std::string& _path, model_catalog& _catalog);
[[nodiscard]] bool vqec_vision_ai_appl_svstr_load_model_packages(
    const std::string& _path, model_package_registry& _registry);
[[nodiscard]] bool vqec_vision_ai_appl_svstr_load_deployment(
    const std::string& _path, deployment_config& _deployment);
[[nodiscard]] bool vqec_vision_ai_appl_svstr_load_feature_catalog(
    const std::string& _path, feature_catalog& _features);
[[nodiscard]] bool vqec_vision_ai_appl_svstr_load_usecase_snapshot(
    const std::string& _path, usecase_control_snapshot& _snapshot);

[[nodiscard]] service_feature_authority_state
vqec_vision_ai_appl_svstr_resolve_feature_authority(
    const service_startup_resolution& _startup, const parsed_arguments& _args,
    const std::string& _source_id, const std::string& _feature_id);
[[nodiscard]] bool vqec_vision_ai_appl_svstr_is_preview_authorized(
    const service_startup_resolution& _startup, const parsed_arguments& _args,
    const std::string& _source_id) noexcept;

[[nodiscard]] service_startup_resolution vqec_vision_ai_appl_svstr_resolve_startup(
    const parsed_arguments& _args, const deployment_config* _effective_deployment,
    const runtime_control_snapshot* _runtime_control,
    usecase_control_manager* _control_manager,
    const std::function<void()>& _poll_control,
    const std::function<bool()>& _is_stop_requested,
    std::uint64_t _runtime_generation, std::uint64_t _pending_control_revision,
    std::uint64_t _idle_step_interval_ns, int _reconcile_generation_exit_code);

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_APPL_SERVICE_STARTUP_HPP
