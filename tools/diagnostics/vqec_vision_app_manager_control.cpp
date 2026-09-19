#include <charconv>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#include <fcntl.h>

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
    std::vector<std::string> component_paths_;
    std::string manifest_sha256_;
    std::string configuration_sha256_;
    std::string grant_sha256_;
    std::string idempotency_key_;
    std::string request_sha256_;
    std::string operation_id_;
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
        } else if (option == "--component" &&
                   candidate.component_paths_.size() <
                       app_lifecycle_limits::g_max_components) {
            candidate.component_paths_.push_back(value);
        } else if (option == "--manifest-sha256") {
            candidate.manifest_sha256_ = value;
        } else if (option == "--configuration-sha256") {
            candidate.configuration_sha256_ = value;
        } else if (option == "--grant-sha256") {
            candidate.grant_sha256_ = value;
        } else if (option == "--idempotency-key") {
            candidate.idempotency_key_ = value;
        } else if (option == "--request-sha256") {
            candidate.request_sha256_ = value;
        } else if (option == "--operation-id") {
            candidate.operation_id_ = value;
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

struct component_descriptor_owner {
    std::vector<int> descriptors_;
    ~component_descriptor_owner() noexcept {
        for (const int descriptor : descriptors_) {
            if (descriptor >= 0) {
                (void)::close(descriptor);
            }
        }
    }
};

status vqec_vision_ai_tools_amctl_open_components(
    const std::vector<std::string>& _paths, component_descriptor_owner& _owner,
    app_package_candidate& _candidate) {
    _owner.descriptors_.reserve(_paths.size());
    _candidate.components_.reserve(_paths.size());
    for (const auto& path : _paths) {
        const int descriptor = ::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
        struct stat file_status {};
        if (descriptor < 0 || ::fstat(descriptor, &file_status) != 0 ||
            !S_ISREG(file_status.st_mode) || file_status.st_size <= 0) {
            if (descriptor >= 0) {
                (void)::close(descriptor);
            }
            return {status_code::io_error,
                "cannot open regular package component input"};
        }
        _owner.descriptors_.push_back(descriptor);
        _candidate.components_.push_back({descriptor});
    }
    return {};
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
    std::cerr << "usage: vqec_vision_app_manager_control <snapshot|list|install|update|rollback|"
                 "submit-install|submit-update|submit-rollback|operation|cancel-operation|"
                 "configure|entitlement|desired|uninstall> --service-name <name> "
                 "--client-name <name> "
                 "--object-path <path> --rpc-timeout-ms <ms> [--session] [command options]\n";
}

void vqec_vision_ai_tools_amctl_write_applications(
    const std::vector<app_catalog_status>& _applications) {
    for (const auto& item : _applications) {
        std::cout << "catalog_code=" << item.catalog_.catalog_code_
                  << " app_id=" << item.catalog_.app_id_
                  << " display_name=\"" << item.catalog_.display_name_ << '"'
                  << " app_version=" << item.catalog_.app_version_
                  << " published=" << (item.catalog_.published_ ? "true" : "false")
                  << " supported=" << (item.supported_ ? "true" : "false")
                  << " installed=" << (item.installed_ ? "true" : "false")
                  << " entitled=" << (item.entitled_ ? "true" : "false")
                  << " desired=" << (item.desired_ ? "true" : "false")
                  << " effective=" << (item.effective_ ? "true" : "false")
                  << " state=" << static_cast<unsigned>(item.state_)
                  << " reason=" << item.reason_code_
                  << " snapshot_revision=" << item.snapshot_revision_ << '\n';
    }
}

void vqec_vision_ai_tools_amctl_write_operation(
    const app_operation_record& _operation) {
    std::cout << "operation_id=" << _operation.operation_id_ << '\n'
              << "app_id=" << _operation.app_id_ << '\n'
              << "payload_sha256=" << _operation.payload_sha256_ << '\n'
              << "kind=" << static_cast<unsigned>(_operation.kind_) << '\n'
              << "state=" << static_cast<unsigned>(_operation.state_) << '\n'
              << "result_code=" << static_cast<unsigned>(_operation.result_code_) << '\n'
              << "snapshot_revision=" << _operation.snapshot_revision_ << '\n'
              << "message=" << _operation.result_message_ << '\n';
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
    if (options.command_ == "snapshot") {
        runtime_control_snapshot snapshot;
        outcome = client.vqec_vision_ai_fwctl_amdbs_fetch_snapshot(
            options.dbus_, snapshot);
        if (outcome.code_ == status_code::ok) {
            outcome = vqec_vision_ai_lifec_rcsnp_write(snapshot, std::cout);
            std::cout << '\n';
        }
    } else if (options.command_ == "list" && !options.source_id_.empty()) {
        std::vector<app_catalog_status> applications;
        outcome = client.vqec_vision_ai_fwctl_amdbs_list_applications(
            options.dbus_, options.source_id_, applications);
        if (outcome.code_ == status_code::ok) {
            vqec_vision_ai_tools_amctl_write_applications(applications);
        }
    } else if (options.command_ == "install" || options.command_ == "update" ||
               options.command_ == "submit-install" ||
               options.command_ == "submit-update") {
        app_package_candidate candidate;
        component_descriptor_owner component_descriptors;
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
            outcome = vqec_vision_ai_tools_amctl_open_components(
                options.component_paths_, component_descriptors, candidate);
        }
        if (outcome.code_ == status_code::ok) {
            const bool is_update = options.command_ == "update" ||
                options.command_ == "submit-update";
            app_operation_request operation_request{
                options.idempotency_key_, options.request_sha256_, options.app_id_,
                is_update ? app_operation_kind::update : app_operation_kind::install};
            outcome = is_update ?
                client.vqec_vision_ai_fwctl_amdbs_submit_update(options.dbus_,
                    operation_request, candidate, options.expected_revision_,
                    options.operation_id_) :
                client.vqec_vision_ai_fwctl_amdbs_submit_install(options.dbus_,
                    operation_request, candidate, options.expected_revision_,
                    options.operation_id_);
        }
    } else if (options.command_ == "rollback" ||
               options.command_ == "submit-rollback") {
        const app_operation_request operation_request{
            options.idempotency_key_, options.request_sha256_, options.app_id_,
            app_operation_kind::rollback};
        outcome = client.vqec_vision_ai_fwctl_amdbs_submit_rollback(options.dbus_,
            operation_request, options.expected_revision_, options.operation_id_);
    } else if (options.command_ == "operation" ||
               options.command_ == "cancel-operation") {
        app_operation_record operation;
        outcome = options.command_ == "operation" ?
            client.vqec_vision_ai_fwctl_amdbs_get_operation(options.dbus_,
                options.operation_id_, operation) :
            client.vqec_vision_ai_fwctl_amdbs_cancel_operation(options.dbus_,
                options.operation_id_, operation);
        if (outcome.code_ == status_code::ok) {
            vqec_vision_ai_tools_amctl_write_operation(operation);
        }
    } else if (options.command_ == "configure") {
        std::vector<std::uint8_t> configuration;
        outcome = vqec_vision_ai_tools_amctl_read_file(options.configuration_path_,
            app_lifecycle_limits::g_max_document_bytes, configuration);
        if (outcome.code_ == status_code::ok) {
            const app_operation_request operation_request{
                options.idempotency_key_, options.request_sha256_, options.app_id_,
                app_operation_kind::configure};
            outcome = client.vqec_vision_ai_fwctl_amdbs_submit_configuration(
                options.dbus_, operation_request, options.expected_revision_,
                configuration, options.configuration_sha256_, options.operation_id_);
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
            const app_operation_request operation_request{
                options.idempotency_key_, options.request_sha256_, options.app_id_,
                app_operation_kind::entitlement};
            outcome = client.vqec_vision_ai_fwctl_amdbs_submit_entitlement(
                options.dbus_, operation_request, candidate, options.operation_id_);
        }
    } else if (options.command_ == "desired" && options.has_desired_) {
        app_desired_update update{options.app_id_, options.source_id_,
            options.expected_revision_, options.desired_};
        const app_operation_request operation_request{
            options.idempotency_key_, options.request_sha256_, options.app_id_,
            app_operation_kind::desired};
        outcome = client.vqec_vision_ai_fwctl_amdbs_submit_desired(
            options.dbus_, operation_request, update, options.operation_id_);
    } else if (options.command_ == "uninstall") {
        const app_operation_request operation_request{
            options.idempotency_key_, options.request_sha256_, options.app_id_,
            app_operation_kind::uninstall};
        outcome = client.vqec_vision_ai_fwctl_amdbs_submit_uninstall(options.dbus_,
            operation_request, options.expected_revision_, options.operation_id_);
    } else {
        vqec_vision_ai_tools_amctl_usage();
        return 2;
    }
    if (outcome.code_ != status_code::ok) {
        std::cerr << "App Manager control failed (" << static_cast<int>(outcome.code_)
                  << "): " << outcome.message_ << '\n';
        return 1;
    }
    if (!options.operation_id_.empty() &&
        options.command_ != "operation" &&
        options.command_ != "cancel-operation") {
        std::cout << "operation_id=" << options.operation_id_ << '\n';
    }
    return 0;
}
