#ifndef VQEC_VISION_AI_APPL_AMOPT_APP_MANAGER_OPTIONS_HPP
#define VQEC_VISION_AI_APPL_AMOPT_APP_MANAGER_OPTIONS_HPP

#include <cstddef>
#include <cstdint>
#include <string>

#include "vqec/vision/ai/contracts/lifecycle/vqec_vision_app_lifecycle.hpp"
#include "vqec/vision/ai/contracts/base/vqec_vision_status.hpp"

namespace vqec::vision::ai {

struct app_manager_options {
    std::string target_id_;
    std::string device_id_;
    app_resource_envelope capacity_;
    std::string database_path_;
    std::uint64_t max_database_bytes_{0};
    std::string content_store_directory_;
    std::uint64_t max_content_store_bytes_{0};
    std::uint64_t max_content_blob_bytes_{0};
    std::size_t max_content_blob_count_{0};
    int busy_timeout_ms_{0};
    std::string public_key_path_;
    std::string key_id_;
    std::string service_bus_name_;
    std::string object_path_;
    std::string trusted_backend_bus_name_;
    std::string trusted_runtime_bus_name_;
    int rpc_timeout_ms_{0};
    std::size_t max_callbacks_per_poll_{0};
    int poll_interval_ms_{0};
    bool use_session_bus_{false};
};

[[nodiscard]] status vqec_vision_ai_appl_amopt_parse(
    int _argc, char** _argv, app_manager_options& _options);
[[nodiscard]] const char* vqec_vision_ai_appl_amopt_usage() noexcept;

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_APPL_AMOPT_APP_MANAGER_OPTIONS_HPP
