#include <atomic>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "vqec_vision_app_manager_dbus.hpp"
#include "vqec_vision_artifact_digest.hpp"
#include "vqec_vision_runtime_control_snapshot.hpp"

using namespace vqec::vision::ai;

namespace {

constexpr std::uint64_t g_default_poll_interval_ms = 10;
constexpr std::uint64_t g_max_poll_interval_ms = 1000;
constexpr std::size_t g_max_fixture_operations = 256;
constexpr char g_fixture_catalog_code[] = "FIXTURE";
constexpr char g_fixture_operation_prefix[] = "fixture_operation_";
constexpr char g_fixture_desired_result_message[] = "fixture desired state committed";
constexpr char g_fixture_configuration_result_message[] =
    "fixture configuration committed";
std::atomic<bool> g_stop_requested{false};

struct vqec_vision_ai_tools_amsfx_options {
    std::string snapshot_path_;
    app_manager_dbus_config dbus_;
    std::uint64_t poll_interval_ms_{g_default_poll_interval_ms};
};

void vqec_vision_ai_tools_amsfx_on_signal(int) noexcept {
    g_stop_requested.store(true, std::memory_order_relaxed);
}

bool vqec_vision_ai_tools_amsfx_parse_positive(
    const char* _text, std::uint64_t& _value) noexcept {
    if (_text == nullptr || *_text == '\0') {
        return false;
    }
    char* end = nullptr;
    errno = 0;
    const auto parsed = std::strtoull(_text, &end, 10);
    if (errno == ERANGE || end == _text || *end != '\0' || parsed == 0) {
        return false;
    }
    _value = parsed;
    return true;
}

bool vqec_vision_ai_tools_amsfx_parse(
    int _argc, char** _argv, vqec_vision_ai_tools_amsfx_options& _options) {
    vqec_vision_ai_tools_amsfx_options candidate;
    candidate.dbus_.use_session_bus_ = true;
    for (int index = 1; index < _argc; ++index) {
        const std::string argument = _argv[index];
        if (index + 1 >= _argc) {
            return false;
        }
        const std::string value = _argv[++index];
        if (argument == "--snapshot") {
            candidate.snapshot_path_ = value;
        } else if (argument == "--service-name") {
            candidate.dbus_.service_bus_name_ = value;
        } else if (argument == "--backend-name") {
            candidate.dbus_.trusted_backend_bus_name_ = value;
        } else if (argument == "--runtime-name") {
            candidate.dbus_.trusted_runtime_bus_name_ = value;
        } else if (argument == "--object-path") {
            candidate.dbus_.object_path_ = value;
        } else if (argument == "--rpc-timeout-ms") {
            std::uint64_t parsed = 0;
            if (!vqec_vision_ai_tools_amsfx_parse_positive(value.c_str(), parsed) ||
                parsed > static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
                return false;
            }
            candidate.dbus_.rpc_timeout_ms_ = static_cast<int>(parsed);
        } else if (argument == "--callbacks") {
            std::uint64_t parsed = 0;
            if (!vqec_vision_ai_tools_amsfx_parse_positive(value.c_str(), parsed) ||
                parsed > std::numeric_limits<std::size_t>::max()) {
                return false;
            }
            candidate.dbus_.max_callbacks_per_poll_ = static_cast<std::size_t>(parsed);
        } else if (argument == "--poll-ms") {
            if (!vqec_vision_ai_tools_amsfx_parse_positive(
                    value.c_str(), candidate.poll_interval_ms_) ||
                candidate.poll_interval_ms_ > g_max_poll_interval_ms) {
                return false;
            }
        } else {
            return false;
        }
    }
    if (candidate.snapshot_path_.empty() ||
        candidate.dbus_.service_bus_name_.empty() ||
        candidate.dbus_.trusted_backend_bus_name_.empty() ||
        candidate.dbus_.trusted_runtime_bus_name_.empty() ||
        candidate.dbus_.object_path_.empty() ||
        candidate.dbus_.rpc_timeout_ms_ <= 0 ||
        candidate.dbus_.max_callbacks_per_poll_ == 0) {
        return false;
    }
    _options = std::move(candidate);
    return true;
}

class vqec_vision_ai_tools_amsfx_manager final : public app_manager_port {
public:
    explicit vqec_vision_ai_tools_amsfx_manager(runtime_control_snapshot _snapshot)
        : snapshot_(std::move(_snapshot)) {}

    [[nodiscard]] status vqec_vision_ai_ports_apmgr_get_snapshot(
        runtime_control_snapshot& _snapshot) const override {
        _snapshot = snapshot_;
        return {};
    }

    [[nodiscard]] status vqec_vision_ai_ports_apmgr_list_applications(
        const std::string& _source_id,
        std::vector<app_catalog_status>& _applications) const override {
        std::vector<app_catalog_status> candidate;
        for (const auto& association : snapshot_.associations_) {
            if (association.source_id_ != _source_id) {
                continue;
            }
            app_catalog_status entry;
            entry.catalog_.catalog_code_ = g_fixture_catalog_code;
            entry.catalog_.app_id_ = association.app_id_;
            entry.catalog_.display_name_ = association.app_id_;
            entry.catalog_.app_version_ = association.app_version_;
            entry.catalog_.published_ = true;
            entry.source_id_ = association.source_id_;
            entry.supported_ = association.supported_;
            entry.installed_ = association.installed_;
            entry.entitled_ = association.entitled_;
            entry.desired_ = association.desired_;
            entry.effective_ = association.is_effective();
            entry.state_ = entry.effective_ ? app_install_state::running :
                app_install_state::installed_disabled;
            entry.reason_code_ = association.reason_code_;
            entry.snapshot_revision_ = snapshot_.snapshot_revision_;
            candidate.push_back(std::move(entry));
        }
        _applications = std::move(candidate);
        return {};
    }

    [[nodiscard]] status vqec_vision_ai_ports_apmgr_submit_install(
        const app_operation_request&, const app_package_candidate&, std::uint64_t,
        app_operation_record&) override {
        return vqec_vision_ai_tools_amsfx_unsupported();
    }
    [[nodiscard]] status vqec_vision_ai_ports_apmgr_submit_update(
        const app_operation_request&, const app_package_candidate&, std::uint64_t,
        app_operation_record&) override {
        return vqec_vision_ai_tools_amsfx_unsupported();
    }
    [[nodiscard]] status vqec_vision_ai_ports_apmgr_submit_rollback(
        const app_operation_request&, std::uint64_t,
        app_operation_record&) override {
        return vqec_vision_ai_tools_amsfx_unsupported();
    }
    [[nodiscard]] status vqec_vision_ai_ports_apmgr_submit_configuration(
        const app_operation_request& _request,
        std::uint64_t _expected_configuration_revision,
        const std::vector<std::uint8_t>& _configuration_payload,
        const std::string& _configuration_sha256,
        app_operation_record& _operation) override {
        const auto repeated = vqec_vision_ai_tools_amsfx_find_repeated(
            _request, _operation);
        if (repeated.code_ != status_code::source_lost) {
            return repeated;
        }
        if (_request.kind_ != app_operation_kind::configure ||
            _request.payload_sha256_ != _configuration_sha256 ||
            _expected_configuration_revision == 0 ||
            _expected_configuration_revision == UINT64_MAX ||
            snapshot_.snapshot_revision_ == UINT64_MAX ||
            snapshot_.inventory_revision_ == UINT64_MAX ||
            operations_.size() >= g_max_fixture_operations) {
            return {status_code::invalid_argument,
                "fixture configuration request is invalid or over capacity"};
        }
        const std::string document(
            _configuration_payload.begin(), _configuration_payload.end());
        std::istringstream stream(document);
        artifact_digest_receipt receipt;
        const auto verified = vqec_vision_ai_mreg_ardgt_verify_stream(
            stream, _configuration_sha256,
            app_lifecycle_limits::g_max_document_bytes, receipt);
        if (verified.code_ != status_code::ok) {
            return verified;
        }
        auto candidate = snapshot_;
        bool found = false;
        for (auto& association : candidate.associations_) {
            if (association.app_id_ != _request.app_id_) {
                continue;
            }
            if (!association.installed_ ||
                association.configuration_revision_ !=
                    _expected_configuration_revision) {
                return {status_code::invalid_state,
                    "fixture application is not installed or configuration is stale"};
            }
            association.configuration_revision_ =
                _expected_configuration_revision + 1U;
            association.configuration_sha256_ = _configuration_sha256;
            association.configuration_payload_ = _configuration_payload;
            found = true;
        }
        if (!found) {
            return {status_code::invalid_argument,
                "fixture configuration application is unknown"};
        }
        ++candidate.inventory_revision_;
        ++candidate.snapshot_revision_;
        const auto valid = vqec_vision_ai_core_applc_validate_runtime_snapshot(candidate);
        if (valid.code_ != status_code::ok) {
            return valid;
        }
        return vqec_vision_ai_tools_amsfx_commit(_request,
            g_fixture_configuration_result_message, std::move(candidate), _operation);
    }
    [[nodiscard]] status vqec_vision_ai_ports_apmgr_submit_entitlement(
        const app_operation_request&, const app_entitlement_candidate&,
        app_operation_record&) override {
        return vqec_vision_ai_tools_amsfx_unsupported();
    }

    [[nodiscard]] status vqec_vision_ai_ports_apmgr_submit_desired(
        const app_operation_request& _request, const app_desired_update& _update,
        app_operation_record& _operation) override {
        const auto repeated = vqec_vision_ai_tools_amsfx_find_repeated(
            _request, _operation);
        if (repeated.code_ != status_code::source_lost) {
            return repeated;
        }
        if (_request.kind_ != app_operation_kind::desired ||
            _request.app_id_ != _update.app_id_ ||
            _update.expected_desired_revision_ != snapshot_.desired_revision_ ||
            snapshot_.desired_revision_ == UINT64_MAX ||
            snapshot_.snapshot_revision_ == UINT64_MAX ||
            operations_.size() >= g_max_fixture_operations) {
            return {status_code::invalid_argument,
                "fixture desired request is stale, invalid or over capacity"};
        }
        auto found = snapshot_.associations_.end();
        for (auto iterator = snapshot_.associations_.begin();
             iterator != snapshot_.associations_.end(); ++iterator) {
            if (iterator->app_id_ == _update.app_id_ &&
                iterator->source_id_ == _update.source_id_) {
                if (found != snapshot_.associations_.end()) {
                    return {status_code::invalid_state,
                        "fixture desired association is ambiguous"};
                }
                found = iterator;
            }
        }
        if (found == snapshot_.associations_.end()) {
            return {status_code::invalid_argument,
                "fixture desired association is unknown"};
        }
        auto candidate = snapshot_;
        const auto offset = static_cast<std::size_t>(
            std::distance(snapshot_.associations_.begin(), found));
        candidate.associations_[offset].desired_ = _update.desired_;
        ++candidate.desired_revision_;
        ++candidate.snapshot_revision_;
        const auto valid = vqec_vision_ai_core_applc_validate_runtime_snapshot(candidate);
        if (valid.code_ != status_code::ok) {
            return valid;
        }
        return vqec_vision_ai_tools_amsfx_commit(_request,
            g_fixture_desired_result_message, std::move(candidate), _operation);
    }

    [[nodiscard]] status vqec_vision_ai_ports_apmgr_submit_uninstall(
        const app_operation_request&, std::uint64_t,
        app_operation_record&) override {
        return vqec_vision_ai_tools_amsfx_unsupported();
    }

    [[nodiscard]] status vqec_vision_ai_ports_apmgr_get_operation(
        const std::string& _operation_id,
        app_operation_record& _operation) const override {
        for (const auto& operation : operations_) {
            if (operation.operation_id_ == _operation_id) {
                _operation = operation;
                return {};
            }
        }
        return {status_code::source_lost, "fixture operation is unknown"};
    }

    [[nodiscard]] status vqec_vision_ai_ports_apmgr_cancel_operation(
        const std::string& _operation_id,
        app_operation_record& _operation) override {
        const auto found = vqec_vision_ai_ports_apmgr_get_operation(
            _operation_id, _operation);
        return found.code_ == status_code::ok
            ? status{status_code::invalid_state,
                "fixture operations commit synchronously and cannot be cancelled"}
            : found;
    }

private:
    [[nodiscard]] status vqec_vision_ai_tools_amsfx_find_repeated(
        const app_operation_request& _request,
        app_operation_record& _operation) const {
        for (const auto& operation : operations_) {
            if (operation.idempotency_key_ != _request.idempotency_key_) {
                continue;
            }
            if (operation.payload_sha256_ != _request.payload_sha256_ ||
                operation.app_id_ != _request.app_id_ ||
                operation.kind_ != _request.kind_) {
                return {status_code::invalid_argument,
                    "fixture idempotency key conflicts with an earlier request"};
            }
            _operation = operation;
            return {};
        }
        return {status_code::source_lost,
            "fixture idempotency key has not been used"};
    }

    [[nodiscard]] status vqec_vision_ai_tools_amsfx_commit(
        const app_operation_request& _request, const char* _result_message,
        runtime_control_snapshot _candidate,
        app_operation_record& _operation) {
        app_operation_record operation;
        operation.operation_id_ = g_fixture_operation_prefix +
            std::to_string(operations_.size() + 1U);
        operation.idempotency_key_ = _request.idempotency_key_;
        operation.payload_sha256_ = _request.payload_sha256_;
        operation.app_id_ = _request.app_id_;
        operation.kind_ = _request.kind_;
        operation.state_ = app_operation_state::committed;
        operation.result_code_ = status_code::ok;
        operation.result_message_ = _result_message;
        operation.snapshot_revision_ = _candidate.snapshot_revision_;
        snapshot_ = std::move(_candidate);
        operations_.push_back(operation);
        _operation = std::move(operation);
        return {};
    }

    [[nodiscard]] static status vqec_vision_ai_tools_amsfx_unsupported() {
        return {status_code::unsupported,
            "snapshot fixture supports desired-state mutation only"};
    }

    runtime_control_snapshot snapshot_;
    std::vector<app_operation_record> operations_;
};

}  // namespace

int main(int argc, char** argv) {
    vqec_vision_ai_tools_amsfx_options options;
    if (!vqec_vision_ai_tools_amsfx_parse(argc, argv, options)) {
        return 2;
    }
    std::ifstream input(options.snapshot_path_);
    runtime_control_snapshot snapshot;
    if (!input.is_open() ||
        vqec_vision_ai_lifec_rcsnp_load(input, snapshot).code_ != status_code::ok) {
        return 1;
    }
    vqec_vision_ai_tools_amsfx_manager manager(std::move(snapshot));
    app_manager_dbus_server server;
    if (server.vqec_vision_ai_fwctl_amdbs_open(manager, options.dbus_).code_ !=
        status_code::ok) {
        return 1;
    }
    std::signal(SIGINT, vqec_vision_ai_tools_amsfx_on_signal);
    std::signal(SIGTERM, vqec_vision_ai_tools_amsfx_on_signal);
    while (!g_stop_requested.load(std::memory_order_relaxed)) {
        server.vqec_vision_ai_fwctl_amdbs_poll();
        std::this_thread::sleep_for(
            std::chrono::milliseconds(options.poll_interval_ms_));
    }
    return 0;
}
