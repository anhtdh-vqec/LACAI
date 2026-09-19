#include "vqec_vision_service_runtime.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <thread>
#include <utility>

#include "vqec_vision_service_generation.hpp"
#include "vqec_vision_service_options.hpp"
#include "vqec_vision_service_startup.hpp"
#include "vqec/vision/ai/contracts/features/vqec_vision_usecase_activation.hpp"
#include "vqec_vision_usecase_control_manager.hpp"
#if defined(VQEC_VISION_AI_HAS_USECASE_CONTROL_DBUS)
#include "vqec_vision_usecase_control_dbus.hpp"
#endif
#if defined(VQEC_VISION_AI_HAS_APP_MANAGER_DBUS)
#include "vqec_vision_app_manager_dbus.hpp"
#endif

using namespace vqec::vision::ai;

namespace {

constexpr int g_reconcile_generation_exit_code = 4;
constexpr int g_recovery_required_exit_code = 5;

}  // namespace

int vqec_vision_ai_appl_svcmn_run_service(int _argc, char** _argv) {
    parsed_arguments args;
    if (!vqec_vision_ai_appl_svopt_parse(_argc, _argv, args)) {
        return vqec_vision_ai_appl_svgen_run_generation(
            _argc, _argv, nullptr, nullptr, nullptr, {}, {}, 0, 0);
    }
    if (!args.app_manager_dbus &&
        (!args.app_manager_service_name.empty() ||
            !args.app_manager_client_name.empty() ||
            !args.app_manager_object_path.empty() ||
            args.app_manager_rpc_timeout_ms != 0 ||
            args.app_manager_poll_interval_ms != 0)) {
        std::fprintf(stderr,
            "app manager DBus settings require --app-manager-dbus\n");
        return 2;
    }
    if (args.app_manager_dbus) {
        if (args.usecase_dbus || !args.production_mode ||
            args.usecase_snapshot_path.empty() || args.feature_catalog_path.empty() ||
            args.app_manager_service_name.empty() ||
            args.app_manager_client_name.empty() ||
            args.app_manager_object_path.empty() ||
            args.app_manager_rpc_timeout_ms <= 0 ||
            args.app_manager_poll_interval_ms == 0 ||
            args.app_manager_poll_interval_ms >
                service_options_limits::g_max_app_manager_poll_interval_ms) {
            std::fprintf(stderr,
                "app manager runtime control requires production mode, usecase and "
                "feature catalogs, service/object names and RPC timeout; it cannot be "
                "combined with legacy usecase DBus\n");
            return 2;
        }
#if !defined(VQEC_VISION_AI_HAS_APP_MANAGER_DBUS)
        std::fprintf(stderr,
            "app manager DBus was requested but adapter is not built\n");
        return 2;
#else
        app_manager_dbus_client client;
        runtime_control_snapshot runtime_control;
        runtime_control.schema_version_ = app_lifecycle_limits::g_schema_version;
        runtime_control.snapshot_revision_ = 1;
        runtime_control.inventory_revision_ = 1;
        runtime_control.entitlement_revision_ = 1;
        runtime_control.desired_revision_ = 1;
        const app_manager_dbus_client_config client_config{
            args.app_manager_service_name, args.app_manager_client_name,
            args.app_manager_object_path,
            args.app_manager_rpc_timeout_ms,
            args.app_manager_dbus_session_bus};
        runtime_control_snapshot fetched_snapshot;
        const auto fetched = client.vqec_vision_ai_fwctl_amdbs_fetch_snapshot(
            client_config, fetched_snapshot);
        if (fetched.code_ == status_code::ok) {
            runtime_control = std::move(fetched_snapshot);
        } else {
            std::fprintf(stderr,
                "app manager unavailable at startup; runtime remains disabled (%d): %s\n",
                static_cast<int>(fetched.code_), fetched.message_.c_str());
        }
        std::uint64_t generation = 1;
        for (;;) {
            bool reconcile = false;
            runtime_control_snapshot pending = runtime_control;
            std::uint64_t next_poll_ns = 0;
            const std::function<void()> poll_control = [&]() {
                const auto now = vqec_vision_ai_appl_svgen_current_monotonic_ns();
                const auto interval_ns = static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::milliseconds(args.app_manager_poll_interval_ms)).count());
                if (next_poll_ns != 0 && now < next_poll_ns) {
                    return;
                }
                next_poll_ns = now <= UINT64_MAX - interval_ns ? now + interval_ns : UINT64_MAX;
                runtime_control_snapshot candidate;
                const auto refreshed = client.vqec_vision_ai_fwctl_amdbs_fetch_snapshot(
                    client_config, candidate);
                if (refreshed.code_ == status_code::ok &&
                    candidate.snapshot_revision_ > runtime_control.snapshot_revision_) {
                    pending = std::move(candidate);
                    reconcile = true;
                    return;
                }
                const auto utc_now_ns = static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count());
                pending = runtime_control;
                for (auto& association : pending.associations_) {
                    if (association.entitled_ &&
                        association.entitlement_expires_utc_ns_ != 0 &&
                        association.entitlement_expires_utc_ns_ <= utc_now_ns) {
                        association.entitled_ = false;
                        association.reason_code_ = "entitlement_expired";
                        reconcile = true;
                    }
                }
            };
            const std::function<bool()> is_reconcile_requested = [&]() {
                return reconcile;
            };
            const int outcome = vqec_vision_ai_appl_svgen_run_generation(
                _argc, _argv, nullptr, &runtime_control, nullptr, poll_control,
                is_reconcile_requested, generation, 0);
            if (outcome == g_reconcile_generation_exit_code &&
                generation != UINT64_MAX) {
                if (reconcile) {
                    runtime_control = std::move(pending);
                } else {
                    std::fprintf(stderr,
                        "\nretrying source generation after %u ms backoff\n",
                        args.source_recovery_backoff_ms);
                    std::this_thread::sleep_for(std::chrono::milliseconds(
                        args.source_recovery_backoff_ms));
                }
                ++generation;
                continue;
            }
            return outcome;
        }
#endif
    }
    if (!args.usecase_dbus) {
        if (!args.usecase_service_name.empty() || !args.usecase_object_path.empty() ||
            !args.usecase_peer_name.empty() || args.usecase_rpc_timeout_ms != 0 ||
            args.usecase_callbacks_per_poll != 0) {
            std::fprintf(stderr, "usecase DBus settings require --usecase-dbus\n");
            return 2;
        }
        return vqec_vision_ai_appl_svgen_run_generation(
            _argc, _argv, nullptr, nullptr, nullptr, {}, {}, 0, 0);
    }
#if !defined(VQEC_VISION_AI_HAS_USECASE_CONTROL_DBUS)
    std::fprintf(stderr, "usecase DBus was requested but adapter is not built\n");
    return 2;
#else
    if (!args.production_mode || args.usecase_snapshot_path.empty() ||
        args.usecase_service_name.empty() || args.usecase_object_path.empty() ||
        args.usecase_peer_name.empty() || args.usecase_rpc_timeout_ms <= 0 ||
        args.usecase_callbacks_per_poll == 0) {
        std::fprintf(stderr, "usecase DBus requires production mode, trusted snapshot, "
            "service/object/peer names, RPC timeout and callback budget\n");
        return 2;
    }
    model_catalog catalog;
    deployment_config base_deployment;
    usecase_control_snapshot trusted;
    if (!vqec_vision_ai_appl_svstr_load_model_catalog(args.catalog_path, catalog) ||
        !vqec_vision_ai_appl_svstr_load_deployment(
            args.deployment_path, base_deployment) ||
        !vqec_vision_ai_appl_svstr_load_usecase_snapshot(
            args.usecase_snapshot_path, trusted)) {
        return 1;
    }
    usecase_control_manager manager;
    const auto configured = manager.vqec_vision_ai_ftmgr_ucmgr_configure(
        trusted, base_deployment, catalog);
    if (configured.code_ != status_code::ok) {
        std::fprintf(stderr, "usecase control configuration failed (%d): %s\n",
            static_cast<int>(configured.code_), configured.message_.c_str());
        return 1;
    }
    usecase_activation_snapshot activation;
    deployment_config current_deployment;
    const auto composed = vqec_vision_ai_core_ucact_compose_effective_deployment(
        base_deployment, catalog, trusted.catalog_, trusted.requests_,
        activation, current_deployment);
    if (composed.code_ != status_code::ok) {
        std::fprintf(stderr, "initial usecase deployment failed (%d): %s\n",
            static_cast<int>(composed.code_), composed.message_.c_str());
        return 1;
    }
    usecase_control_dbus_server control_dbus;
    const usecase_control_dbus_config dbus_config{
        args.usecase_service_name, args.usecase_object_path,
        args.usecase_peer_name, args.usecase_rpc_timeout_ms,
        args.usecase_callbacks_per_poll, args.usecase_dbus_session_bus};
    const auto opened = control_dbus.vqec_vision_ai_fwctl_ucdbs_open(
        manager, dbus_config);
    if (opened.code_ != status_code::ok) {
        std::fprintf(stderr, "usecase DBus failed (%d): %s\n",
            static_cast<int>(opened.code_), opened.message_.c_str());
        return 1;
    }
    const std::function<void()> poll_control = [&control_dbus]() {
        control_dbus.vqec_vision_ai_fwctl_ucdbs_poll();
    };
    deployment_config last_published_deployment = current_deployment;
    std::uint64_t generation = 1;
    std::uint64_t pending_revision = 0;
    for (;;) {
        const int outcome = vqec_vision_ai_appl_svgen_run_generation(
            _argc, _argv, &current_deployment, nullptr, &manager, poll_control,
            {}, generation, pending_revision);
        if (outcome == g_reconcile_generation_exit_code) {
            last_published_deployment = current_deployment;
            usecase_control_snapshot pending;
            deployment_config candidate;
            const auto fetched = manager.vqec_vision_ai_ftmgr_ucmgr_get_pending(
                pending, candidate);
            if (fetched.code_ != status_code::ok ||
                generation == UINT64_MAX) {
                std::fprintf(stderr, "cannot fetch pending usecase generation\n");
                return 1;
            }
            current_deployment = std::move(candidate);
            pending_revision = pending.control_revision_;
            ++generation;
            continue;
        }
        if (outcome == g_recovery_required_exit_code) {
            std::fprintf(stderr, "runtime drain requires recovery; replacement blocked\n");
            return outcome;
        }
        if (pending_revision != 0 && manager.vqec_vision_ai_ftmgr_ucmgr_has_pending()) {
            const auto failed = manager.vqec_vision_ai_ftmgr_ucmgr_fail_pending(
                pending_revision, status_code::invalid_state);
            if (failed.code_ != status_code::ok) {
                return 1;
            }
            current_deployment = last_published_deployment;
            pending_revision = 0;
            if (vqec_vision_ai_appl_svgen_is_stop_requested()) {
                return outcome;
            }
            continue;
        }
        return outcome;
    }
#endif
}
