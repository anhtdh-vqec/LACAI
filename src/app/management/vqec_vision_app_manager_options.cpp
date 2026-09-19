#include "vqec_vision_app_manager_options.hpp"

#include <charconv>
#include <filesystem>
#include <limits>
#include <string_view>
#include <utility>

#include "vqec/vision/ai/contracts/vqec_vision_app_lifecycle.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_identifier.hpp"

namespace vqec::vision::ai {
namespace {

constexpr std::uint64_t g_min_database_bytes = 1024U * 1024U;
constexpr std::uint64_t g_max_database_bytes = 256U * 1024U * 1024U;
constexpr int g_max_timeout_ms = 60000;
constexpr std::size_t g_max_callbacks_per_poll = 64;
constexpr int g_max_poll_interval_ms = 1000;

bool vqec_vision_ai_appl_amopt_read_u64(
    std::string_view _text, std::uint64_t& _value) {
    if (_text.empty()) {
        return false;
    }
    std::uint64_t candidate = 0;
    const auto parsed = std::from_chars(
        _text.data(), _text.data() + _text.size(), candidate);
    if (parsed.ec != std::errc{} || parsed.ptr != _text.data() + _text.size()) {
        return false;
    }
    _value = candidate;
    return true;
}

bool vqec_vision_ai_appl_amopt_take_value(
    int _argc, char** _argv, int& _index, std::string& _value) {
    if (_index + 1 >= _argc || _argv[_index + 1] == nullptr) {
        return false;
    }
    _value = _argv[++_index];
    return !_value.empty();
}

}  // namespace

const char* vqec_vision_ai_appl_amopt_usage() noexcept {
    return "vqec_vision_app_manager --target <id> --database <absolute-path> "
           "--max-database-bytes <bytes> --busy-timeout-ms <ms> "
           "--public-key <absolute-pem-path> --key-id <id> "
           "--service-name <dbus-name> --object-path <dbus-path> "
           "--trusted-peer-name <dbus-name> --rpc-timeout-ms <ms> "
           "--callbacks-per-poll <count> --poll-interval-ms <ms> [--session]";
}

status vqec_vision_ai_appl_amopt_parse(
    int _argc, char** _argv, app_manager_options& _options) {
    app_manager_options candidate;
    for (int index = 1; index < _argc; ++index) {
        if (_argv[index] == nullptr) {
            return {status_code::invalid_argument, "null App Manager argument"};
        }
        const std::string_view option(_argv[index]);
        std::string value;
        if (option == "--session") {
            candidate.use_session_bus_ = true;
            continue;
        }
        if (!vqec_vision_ai_appl_amopt_take_value(_argc, _argv, index, value)) {
            return {status_code::invalid_argument, "missing App Manager option value"};
        }
        std::uint64_t number = 0;
        if (option == "--target") {
            candidate.target_id_ = value;
        } else if (option == "--database") {
            candidate.database_path_ = value;
        } else if (option == "--max-database-bytes" &&
                   vqec_vision_ai_appl_amopt_read_u64(value, number)) {
            candidate.max_database_bytes_ = number;
        } else if (option == "--busy-timeout-ms" &&
                   vqec_vision_ai_appl_amopt_read_u64(value, number) &&
                   number <= static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
            candidate.busy_timeout_ms_ = static_cast<int>(number);
        } else if (option == "--public-key") {
            candidate.public_key_path_ = value;
        } else if (option == "--key-id") {
            candidate.key_id_ = value;
        } else if (option == "--service-name") {
            candidate.service_bus_name_ = value;
        } else if (option == "--object-path") {
            candidate.object_path_ = value;
        } else if (option == "--trusted-peer-name") {
            candidate.trusted_peer_bus_name_ = value;
        } else if (option == "--rpc-timeout-ms" &&
                   vqec_vision_ai_appl_amopt_read_u64(value, number) &&
                   number <= static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
            candidate.rpc_timeout_ms_ = static_cast<int>(number);
        } else if (option == "--callbacks-per-poll" &&
                   vqec_vision_ai_appl_amopt_read_u64(value, number)) {
            candidate.max_callbacks_per_poll_ = static_cast<std::size_t>(number);
        } else if (option == "--poll-interval-ms" &&
                   vqec_vision_ai_appl_amopt_read_u64(value, number) &&
                   number <= static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
            candidate.poll_interval_ms_ = static_cast<int>(number);
        } else {
            return {status_code::invalid_argument, "unknown or invalid App Manager option"};
        }
    }
    if (!vqec_vision_ai_cntr_ident_is_valid(
            candidate.target_id_, app_lifecycle_limits::g_max_identifier_bytes) ||
        !std::filesystem::path(candidate.database_path_).is_absolute() ||
        !std::filesystem::path(candidate.public_key_path_).is_absolute() ||
        !vqec_vision_ai_cntr_ident_is_valid(
            candidate.key_id_, app_lifecycle_limits::g_max_identifier_bytes) ||
        candidate.service_bus_name_.empty() || candidate.object_path_.empty() ||
        candidate.trusted_peer_bus_name_.empty() ||
        candidate.max_database_bytes_ < g_min_database_bytes ||
        candidate.max_database_bytes_ > g_max_database_bytes ||
        candidate.busy_timeout_ms_ <= 0 || candidate.busy_timeout_ms_ > g_max_timeout_ms ||
        candidate.rpc_timeout_ms_ <= 0 || candidate.rpc_timeout_ms_ > g_max_timeout_ms ||
        candidate.max_callbacks_per_poll_ == 0 ||
        candidate.max_callbacks_per_poll_ > g_max_callbacks_per_poll ||
        candidate.poll_interval_ms_ <= 0 ||
        candidate.poll_interval_ms_ > g_max_poll_interval_ms) {
        return {status_code::invalid_argument,
            "incomplete or out-of-range App Manager configuration"};
    }
    _options = std::move(candidate);
    return {};
}

}  // namespace vqec::vision::ai
