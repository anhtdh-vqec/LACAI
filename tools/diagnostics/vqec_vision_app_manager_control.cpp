#include <charconv>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include "vqec_vision_app_manager_dbus.hpp"
#include "vqec_vision_runtime_control_snapshot.hpp"

namespace vqec::vision::ai {
namespace {

struct app_manager_control_options {
    app_manager_dbus_client_config dbus_;
    std::string command_;
    std::string manifest_path_;
    std::string configuration_path_;
    std::string grant_path_;
    std::string signature_path_;
    std::string manifest_sha256_;
    std::string configuration_sha256_;
    std::string grant_sha256_;
    std::string app_id_;
    std::string source_id_;
    std::uint64_t expected_revision_{0};
    bool desired_{false};
    bool has_desired_{false};
};

bool vqec_vision_ai_tools_amctl_read_u64(
    std::string_view _text, std::uint64_t& _value) {
    if (_text.empty()) {
        return false;
    }
    const auto result = std::from_chars(
        _text.data(), _text.data() + _text.size(), _value);
    return result.ec == std::errc{} && result.ptr == _text.data() + _text.size();
}

bool vqec_vision_ai_tools_amctl_parse(
    int _argc, char** _argv, app_manager_control_options& _options) {
    if (_argc < 2 || _argv[1] == nullptr) {
        return false;
    }
    app_manager_control_options candidate;
    candidate.command_ = _argv[1];
    for (int index = 2; index < _argc; ++index) {
        if (_argv[index] == nullptr) {
            return false;
        }
        const std::string_view option(_argv[index]);
        if (option == "--session") {
            candidate.dbus_.use_session_bus_ = true;
            continue;
        }
        if (index + 1 >= _argc || _argv[index + 1] == nullptr) {
            return false;
        }
        const std::string value(_argv[++index]);
        if (option == "--service-name") {
            candidate.dbus_.service_bus_name_ = value;
        } else if (option == "--client-name") {
            candidate.dbus_.client_bus_name_ = value;
        } else if (option == "--object-path") {
            candidate.dbus_.object_path_ = value;
        } else if (option == "--rpc-timeout-ms") {
            std::uint64_t timeout = 0;
            if (!vqec_vision_ai_tools_amctl_read_u64(value, timeout) ||
                timeout == 0 || timeout > static_cast<std::uint64_t>(INT32_MAX)) {
                return false;
            }
            candidate.dbus_.rpc_timeout_ms_ = static_cast<int>(timeout);
        } else if (option == "--manifest") {
            candidate.manifest_path_ = value;
        } else if (option == "--configuration") {
            candidate.configuration_path_ = value;
        } else if (option == "--grant") {
            candidate.grant_path_ = value;
        } else if (option == "--signature") {
            candidate.signature_path_ = value;
        } else if (option == "--manifest-sha256") {
            candidate.manifest_sha256_ = value;
        } else if (option == "--configuration-sha256") {
            candidate.configuration_sha256_ = value;
        } else if (option == "--grant-sha256") {
            candidate.grant_sha256_ = value;
        } else if (option == "--app-id") {
            candidate.app_id_ = value;
        } else if (option == "--source-id") {
            candidate.source_id_ = value;
        } else if (option == "--expected-revision") {
            if (!vqec_vision_ai_tools_amctl_read_u64(
                    value, candidate.expected_revision_)) {
                return false;
            }
        } else if (option == "--enabled" && (value == "true" || value == "false")) {
            candidate.desired_ = value == "true";
            candidate.has_desired_ = true;
        } else {
            return false;
        }
    }
    if (candidate.dbus_.service_bus_name_.empty() ||
        candidate.dbus_.client_bus_name_.empty() ||
        candidate.dbus_.object_path_.empty() || candidate.dbus_.rpc_timeout_ms_ <= 0) {
        return false;
    }
    _options = std::move(candidate);
    return true;
}

status vqec_vision_ai_tools_amctl_read_file(
    const std::string& _path, std::size_t _max_bytes,
    std::vector<std::uint8_t>& _payload) {
    std::ifstream stream(_path, std::ios::binary);
    if (!stream.is_open()) {
        return {status_code::io_error, "cannot open control input file"};
    }
    std::vector<std::uint8_t> candidate;
    char byte = 0;
    while (stream.get(byte)) {
        if (candidate.size() == _max_bytes) {
            return {status_code::resource_exhausted,
                "control input file exceeds byte limit"};
        }
        candidate.push_back(static_cast<std::uint8_t>(byte));
    }
    if (stream.bad() || (!stream.eof() && stream.fail())) {
        return {status_code::io_error, "control input file read failed"};
    }
    if (candidate.empty()) {
        return {status_code::invalid_argument, "control input file is empty"};
    }
    _payload = std::move(candidate);
    return {};
}

void vqec_vision_ai_tools_amctl_usage() {
    std::cerr << "usage: vqec_vision_app_manager_control <snapshot|install|entitlement|"
                 "desired|uninstall> --service-name <name> --client-name <name> "
                 "--object-path <path> --rpc-timeout-ms <ms> [--session] [command options]\n";
}

}  // namespace
}  // namespace vqec::vision::ai

int main(int argc, char** argv) {
    using namespace vqec::vision::ai;
    app_manager_control_options options;
    if (!vqec_vision_ai_tools_amctl_parse(argc, argv, options)) {
        vqec_vision_ai_tools_amctl_usage();
        return 2;
    }
    app_manager_dbus_client client;
    status outcome;
    std::uint64_t revision = 0;
    if (options.command_ == "snapshot") {
        runtime_control_snapshot snapshot;
        outcome = client.vqec_vision_ai_fwctl_amdbs_fetch_snapshot(
            options.dbus_, snapshot);
        if (outcome.code_ == status_code::ok) {
            outcome = vqec_vision_ai_lifec_rcsnp_write(snapshot, std::cout);
            std::cout << '\n';
        }
    } else if (options.command_ == "install") {
        app_package_candidate candidate;
        outcome = vqec_vision_ai_tools_amctl_read_file(options.manifest_path_,
            app_lifecycle_limits::g_max_document_bytes, candidate.manifest_payload_);
        if (outcome.code_ == status_code::ok) {
            outcome = vqec_vision_ai_tools_amctl_read_file(options.configuration_path_,
                app_lifecycle_limits::g_max_document_bytes,
                candidate.configuration_payload_);
        }
        if (outcome.code_ == status_code::ok) {
            outcome = vqec_vision_ai_tools_amctl_read_file(options.signature_path_,
                app_lifecycle_limits::g_max_document_bytes,
                candidate.signature_payload_);
        }
        candidate.manifest_sha256_ = options.manifest_sha256_;
        candidate.configuration_sha256_ = options.configuration_sha256_;
        if (outcome.code_ == status_code::ok) {
            outcome = client.vqec_vision_ai_fwctl_amdbs_install(options.dbus_, candidate,
                options.expected_revision_, revision);
        }
    } else if (options.command_ == "entitlement") {
        app_entitlement_candidate candidate;
        outcome = vqec_vision_ai_tools_amctl_read_file(options.grant_path_,
            app_lifecycle_limits::g_max_document_bytes, candidate.grant_payload_);
        if (outcome.code_ == status_code::ok) {
            outcome = vqec_vision_ai_tools_amctl_read_file(options.signature_path_,
                app_lifecycle_limits::g_max_document_bytes,
                candidate.signature_payload_);
        }
        candidate.grant_sha256_ = options.grant_sha256_;
        if (outcome.code_ == status_code::ok) {
            outcome = client.vqec_vision_ai_fwctl_amdbs_apply_entitlement(
                options.dbus_, candidate, revision);
        }
    } else if (options.command_ == "desired" && options.has_desired_) {
        app_desired_update update{options.app_id_, options.source_id_,
            options.expected_revision_, options.desired_};
        outcome = client.vqec_vision_ai_fwctl_amdbs_set_desired(
            options.dbus_, update, revision);
    } else if (options.command_ == "uninstall") {
        outcome = client.vqec_vision_ai_fwctl_amdbs_uninstall(options.dbus_,
            options.app_id_, options.expected_revision_, revision);
    } else {
        vqec_vision_ai_tools_amctl_usage();
        return 2;
    }
    if (outcome.code_ != status_code::ok) {
        std::cerr << "App Manager control failed (" << static_cast<int>(outcome.code_)
                  << "): " << outcome.message_ << '\n';
        return 1;
    }
    if (revision != 0) {
        std::cout << "snapshot_revision=" << revision << '\n';
    }
    return 0;
}
