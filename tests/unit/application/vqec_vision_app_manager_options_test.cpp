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
        "--database", "/var/lib/lacai/apps.db", "--max-database-bytes",
        "67108864", "--busy-timeout-ms", "5000", "--public-key",
        "/etc/lacai/release.pem", "--key-id", "release.primary",
        "--service-name", "com.vqec.AiVision.AppManager", "--object-path",
        "/com/vqec/AiVision/AppManager", "--trusted-peer-name",
        "com.vqec.Backend", "--rpc-timeout-ms", "5000",
        "--callbacks-per-poll", "16", "--poll-interval-ms", "10", "--session"};
    app_manager_options options;
    assert(vqec_vision_ai_unit_amotst_parse(valid, options).code_ == status_code::ok);
    assert(options.use_session_bus_ && options.poll_interval_ms_ == 10 &&
        options.max_database_bytes_ == 67108864U);
    auto unknown = valid;
    unknown.push_back("--unknown");
    assert(vqec_vision_ai_unit_amotst_parse(unknown, options).code_ ==
        status_code::invalid_argument);
    auto relative = valid;
    relative[4] = "relative.db";
    assert(vqec_vision_ai_unit_amotst_parse(relative, options).code_ ==
        status_code::invalid_argument);
    auto busy_spin = valid;
    busy_spin[busy_spin.size() - 2U] = "0";
    assert(vqec_vision_ai_unit_amotst_parse(busy_spin, options).code_ ==
        status_code::invalid_argument);
    return 0;
}
