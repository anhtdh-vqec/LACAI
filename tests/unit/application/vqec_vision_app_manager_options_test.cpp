#include "vqec_vision_app_manager_options.hpp"

#include <cassert>
#include <string>
#include <vector>

namespace {

vqec::vision::ai::status vqec_vision_ai_unit_amotst_parse(
    const std::vector<std::string>& _arguments,
    vqec::vision::ai::app_manager_options& _options) {
    std::vector<char*> pointers;
    pointers.reserve(_arguments.size());
    for (const auto& argument : _arguments) {
        pointers.push_back(const_cast<char*>(argument.c_str()));
    }
    return vqec::vision::ai::vqec_vision_ai_appl_amopt_parse(
        static_cast<int>(pointers.size()), pointers.data(), _options);
}

}  // namespace

int main() {
    using namespace vqec::vision::ai;
    const std::vector<std::string> valid{
        "vqec_vision_app_manager", "--target", "qcs6490_qlinux_1_8",
        "--device-id", "09c89b1858f54955a3d13f2767622448",
        "--max-resident-bytes", "536870912", "--max-tensor-bytes", "134217728",
        "--max-active-incidents", "32", "--max-events-per-second", "64",
        "--database", "/var/lib/lacai/apps.db", "--max-database-bytes",
        "67108864", "--content-store", "/var/lib/lacai/content",
        "--max-content-store-bytes", "1073741824", "--max-content-blob-bytes",
        "268435456", "--max-content-blob-count", "2048",
        "--busy-timeout-ms", "5000", "--public-key",
        "/etc/lacai/release.pem", "--key-id", "release.primary",
        "--service-name", "com.vqec.AiVision.AppManager", "--object-path",
        "/com/vqec/AiVision/AppManager", "--trusted-backend-name",
        "com.vqec.Backend", "--trusted-runtime-name", "com.vqec.AiRuntime",
        "--rpc-timeout-ms", "5000",
        "--callbacks-per-poll", "16", "--poll-interval-ms", "10", "--session"};
    app_manager_options options;
    assert(vqec_vision_ai_unit_amotst_parse(valid, options).code_ == status_code::ok);
    assert(options.use_session_bus_ && options.poll_interval_ms_ == 10 &&
        options.max_database_bytes_ == 67108864U &&
        options.max_content_blob_count_ == 2048U);
    auto unknown = valid;
    unknown.push_back("--unknown");
    assert(vqec_vision_ai_unit_amotst_parse(unknown, options).code_ ==
        status_code::invalid_argument);
    auto relative = valid;
    relative[14] = "relative.db";
    assert(vqec_vision_ai_unit_amotst_parse(relative, options).code_ ==
        status_code::invalid_argument);
    auto busy_spin = valid;
    busy_spin[busy_spin.size() - 2U] = "0";
    assert(vqec_vision_ai_unit_amotst_parse(busy_spin, options).code_ ==
        status_code::invalid_argument);
    return 0;
}
