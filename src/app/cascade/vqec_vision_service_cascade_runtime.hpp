#ifndef VQEC_VISION_AI_APPL_SERVICE_CASCADE_RUNTIME_HPP
#define VQEC_VISION_AI_APPL_SERVICE_CASCADE_RUNTIME_HPP

#include <array>
#include <cstdint>
#include <memory>

#include "vqec_vision_cascade_coordinator.hpp"
#include "vqec_vision_cascade_execution_worker.hpp"
#include "vqec_vision_cascade_graph_session.hpp"
#include "vqec_vision_production_platform.hpp"
#include "vqec_vision_source_session.hpp"
#include "vqec/vision/ai/contracts/lifecycle/vqec_vision_activation_delta.hpp"

namespace vqec::vision::ai {

class runtime_executor;

struct service_cascade_owner {
    production_cascade_binding binding_;
    const model_catalog_entry* model_{nullptr};
    std::unique_ptr<cascade_graph_session> graph_session_;
    std::unique_ptr<cascade_coordinator> coordinator_;
    std::unique_ptr<cascade_execution_worker> worker_;
    std::uint16_t root_model_slot_{g_invalid_model_slot};
    bool desired_active_{true};
    bool active_{false};
};

[[nodiscard]] status vqec_vision_ai_appl_svcsc_start_graph_sessions(
    const std::array<cascade_graph_session*, 2>& _sessions);
[[nodiscard]] status vqec_vision_ai_appl_svcsc_stop_graph_sessions(
    const std::array<cascade_graph_session*, 2>& _sessions);
[[nodiscard]] status vqec_vision_ai_appl_svcsc_make_source_binding(
    const source_deployment_config& _source, const model_catalog_entry& _model,
    source_binding& _binding);
[[nodiscard]] status vqec_vision_ai_appl_svcsc_make_offline_graph_session(
    const source_deployment_config& _source, const model_catalog_entry& _model,
    production_offline_model& _offline,
    std::unique_ptr<cascade_graph_session>& _session);
[[nodiscard]] status vqec_vision_ai_appl_svcsc_prepare_owners(
    const deployment_config& _deployment, const model_catalog& _catalog,
    production_platform& _platform,
    std::array<service_cascade_owner, deployment_limits::g_max_sources>& _owners);
// Starts only sources that have crossed the primary session first-frame gate. A false slot
// is a hard prohibition on secondary graph configure/load/QNN/HTP activation.
[[nodiscard]] status vqec_vision_ai_appl_svcsc_start_ready_graphs(
    std::array<service_cascade_owner, deployment_limits::g_max_sources>& _owners,
    const std::array<bool, deployment_limits::g_max_sources>& _source_ready);
[[nodiscard]] status vqec_vision_ai_appl_svcsc_initialize_activation(
    std::array<service_cascade_owner, deployment_limits::g_max_sources>& _owners,
    const app_activation_plan& _plan, runtime_executor& _executor,
    std::uint64_t _steady_now_ns);
[[nodiscard]] status vqec_vision_ai_appl_svcsc_validate_activation(
    const std::array<service_cascade_owner, deployment_limits::g_max_sources>& _owners,
    const app_activation_plan& _plan, std::uint64_t _steady_now_ns);
[[nodiscard]] status vqec_vision_ai_appl_svcsc_request_activation(
    std::array<service_cascade_owner, deployment_limits::g_max_sources>& _owners,
    const app_activation_plan& _plan, runtime_executor& _executor,
    std::uint64_t _steady_now_ns);
[[nodiscard]] status vqec_vision_ai_appl_svcsc_sync_executor(
    std::array<service_cascade_owner, deployment_limits::g_max_sources>& _owners,
    runtime_executor& _executor);
[[nodiscard]] bool vqec_vision_ai_appl_svcsc_is_activation_complete(
    const std::array<service_cascade_owner, deployment_limits::g_max_sources>& _owners)
    noexcept;
[[nodiscard]] status vqec_vision_ai_appl_svcsc_stop_graphs(
    std::array<service_cascade_owner, deployment_limits::g_max_sources>& _owners);
[[nodiscard]] status vqec_vision_ai_appl_svcsc_drain_workers(
    std::array<service_cascade_owner, deployment_limits::g_max_sources>& _owners,
    std::uint64_t _steady_now_ns);

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_APPL_SERVICE_CASCADE_RUNTIME_HPP
