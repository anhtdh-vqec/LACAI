#include <chrono>
#include <csignal>
#include <cstdio>
#include <thread>

#include "vqec_vision_app_configuration_registry.hpp"
#include "vqec_vision_app_manager.hpp"
#include "vqec_vision_app_manager_dbus.hpp"
#include "vqec_vision_app_manager_options.hpp"
#include "vqec_vision_ed25519_app_entitlement_verifier.hpp"
#include "vqec_vision_ed25519_app_package_verifier.hpp"
#include "vqec_vision_fire_smoke_factory.hpp"
#include "vqec_vision_sqlite_app_inventory.hpp"
#include "vqec_vision_app_content_store.hpp"

int main(int argc, char** argv) {
    using namespace vqec::vision::ai;
    app_manager_options options;
    const auto parsed = vqec_vision_ai_appl_amopt_parse(argc, argv, options);
    if (parsed.code_ != status_code::ok) {
        std::fprintf(stderr, "%s: %s\n", parsed.message_.c_str(),
            vqec_vision_ai_appl_amopt_usage());
        return 2;
    }
    app_configuration_registry registry;
    fire_smoke_factory fire_smoke;
    auto current = registry.vqec_vision_ai_appl_apcrg_register(
        fire_smoke_app_contract::g_app_id,
        fire_smoke_app_contract::g_configuration_schema_id,
        fire_smoke_app_contract::g_processor_contract, fire_smoke);
    if (current.code_ != status_code::ok) {
        std::fprintf(stderr, "App Manager registry failed: %s\n", current.message_.c_str());
        return 3;
    }
    sqlite_app_inventory inventory({options.database_path_,
        options.max_database_bytes_, options.busy_timeout_ms_});
    app_content_store content_store({options.content_store_directory_,
        options.max_content_store_bytes_, options.max_content_blob_bytes_,
        options.max_content_blob_count_});
    ed25519_app_package_verifier verifier({options.public_key_path_, options.key_id_});
    ed25519_app_entitlement_verifier entitlement_verifier(
        {options.public_key_path_, options.key_id_});
    app_manager manager({options.target_id_, options.device_id_, options.capacity_},
        verifier, entitlement_verifier, registry, content_store, inventory);
    runtime_control_snapshot snapshot;
    current = manager.vqec_vision_ai_appl_appmn_open(snapshot);
    if (current.code_ != status_code::ok) {
        std::fprintf(stderr, "App Manager inventory failed: %s\n", current.message_.c_str());
        return 4;
    }
    app_manager_dbus_server server;
    current = server.vqec_vision_ai_fwctl_amdbs_open(manager,
        {options.service_bus_name_, options.object_path_,
            options.trusted_backend_bus_name_, options.trusted_runtime_bus_name_,
            options.rpc_timeout_ms_,
            options.max_callbacks_per_poll_, options.use_session_bus_});
    if (current.code_ != status_code::ok) {
        std::fprintf(stderr, "App Manager D-Bus failed: %s\n", current.message_.c_str());
        return 5;
    }
    static volatile std::sig_atomic_t stop_requested = 0;
    const auto stop = +[](int) { stop_requested = 1; };
    std::signal(SIGINT, stop);
    std::signal(SIGTERM, stop);
    while (stop_requested == 0) {
        server.vqec_vision_ai_fwctl_amdbs_poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(options.poll_interval_ms_));
    }
    return 0;
}
