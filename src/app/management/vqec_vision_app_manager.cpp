#include "vqec_vision_app_manager.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <new>
#include <sstream>
#include <utility>

#include <fcntl.h>
#include <unistd.h>

#include "vqec/vision/ai/contracts/base/vqec_vision_identifier.hpp"
#include "vqec_vision_artifact_digest.hpp"

namespace vqec::vision::ai {

struct app_manager::operation_job {
    app_operation_request request_;
    app_package_candidate candidate_;
    std::vector<int> owned_descriptors_;
    std::uint64_t expected_inventory_revision_{0};

    ~operation_job() noexcept {
        for (const int descriptor : owned_descriptors_) {
            if (descriptor >= 0) {
                (void)::close(descriptor);
            }
        }
    }
};

app_manager::app_manager(app_manager_config _config,
    app_package_verifier_port& _package_verifier,
    app_entitlement_verifier_port& _entitlement_verifier,
    app_configuration_registry& _configuration_registry,
    const usecase_app_catalog& _catalog, app_content_store_port& _content_store,
    app_inventory_port& _inventory)
    : config_(std::move(_config)),
      package_verifier_(_package_verifier),
      entitlement_verifier_(_entitlement_verifier),
      configuration_registry_(_configuration_registry),
      catalog_(_catalog),
      content_store_(_content_store),
      inventory_(_inventory) {}

app_manager::~app_manager() noexcept {
    {
        std::lock_guard<std::mutex> guard(queue_mutex_);
        stop_worker_ = true;
    }
    queue_ready_.notify_all();
    if (operation_worker_.joinable()) {
        operation_worker_.join();
    }
}

status app_manager::vqec_vision_ai_appl_appmn_verify_configuration_digest(
    const std::vector<std::uint8_t>& _payload, const std::string& _sha256) const {
    if (_payload.empty() ||
        _payload.size() > app_lifecycle_limits::g_max_document_bytes ||
        !vqec_vision_ai_cntr_ident_is_sha256_hex(_sha256)) {
        return {status_code::invalid_argument, "invalid configuration artifact"};
    }
    const std::string document(_payload.begin(), _payload.end());
    std::istringstream stream(document);
    artifact_digest_receipt receipt;
    return vqec_vision_ai_mreg_ardgt_verify_stream(stream, _sha256,
        app_lifecycle_limits::g_max_document_bytes, receipt);
}

const app_runtime_association* app_manager::vqec_vision_ai_appl_appmn_find_association(
    const runtime_control_snapshot& _snapshot,
    const std::string& _app_id) const noexcept {
    const auto found = std::find_if(_snapshot.associations_.begin(),
        _snapshot.associations_.end(), [&_app_id](const auto& _association) {
            return _association.app_id_ == _app_id;
        });
    return found == _snapshot.associations_.end() ? nullptr : &*found;
}

bool app_manager::vqec_vision_ai_appl_appmn_is_catalogued(
    const std::string& _app_id) const noexcept {
    return std::any_of(catalog_.applications_.begin(), catalog_.applications_.end(),
        [&_app_id](const auto& _application) {
            return _application.app_id_ == _app_id && _application.published_;
        });
}

status app_manager::vqec_vision_ai_appl_appmn_stage_package_content(
    const app_package_candidate& _candidate,
    const verified_app_package& _package,
    std::vector<app_installed_component>& _components) {
    std::size_t component_index = 0;
    std::vector<app_installed_component> components;
    components.reserve(_package.manifest_.components_.size());
    for (const auto& component : _package.manifest_.components_) {
        if (component.target_id_ != config_.target_id_) {
            continue;
        }
        app_content_record record;
        status staged;
        if (component.type_ == app_component_type::configuration) {
            if (component.artifact_sha256_ != _package.configuration_sha256_ ||
                component.artifact_bytes_ != _package.configuration_payload_.size()) {
                return {status_code::protocol_error,
                    "configuration component differs from signed package defaults"};
            }
            const std::string document(_package.configuration_payload_.begin(),
                _package.configuration_payload_.end());
            std::istringstream stream(document);
            staged = content_store_.vqec_vision_ai_ports_apcst_put(stream,
                component.artifact_sha256_, component.artifact_bytes_, record);
        } else {
            if (component_index >= _candidate.components_.size() ||
                _candidate.components_[component_index].descriptor_ < 0) {
                return {status_code::invalid_argument,
                    "package component descriptor is missing"};
            }
            staged = content_store_.vqec_vision_ai_ports_apcst_put_descriptor(
                _candidate.components_[component_index].descriptor_,
                component.artifact_sha256_, component.artifact_bytes_, record);
            ++component_index;
        }
        if (staged.code_ != status_code::ok) {
            return staged;
        }
        components.push_back({component, record.immutable_location_});
    }
    if (component_index != _candidate.components_.size()) {
        return {status_code::invalid_argument,
            "package contains undeclared component descriptors"};
    }
    _components = std::move(components);
    return {};
}

status app_manager::vqec_vision_ai_appl_appmn_open(
    runtime_control_snapshot& _snapshot) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (open_) {
        return {status_code::invalid_state, "app manager is already open"};
    }
    if (!vqec_vision_ai_cntr_ident_is_valid(
            config_.target_id_, app_lifecycle_limits::g_max_identifier_bytes) ||
        !vqec_vision_ai_cntr_ident_is_valid(
            config_.device_id_, app_lifecycle_limits::g_max_identifier_bytes) ||
        config_.capacity_.max_resident_bytes_ == 0 ||
        config_.capacity_.max_tensor_bytes_ == 0 ||
        config_.capacity_.max_tensor_bytes_ > config_.capacity_.max_resident_bytes_ ||
        config_.capacity_.max_active_incidents_ == 0 ||
        !std::isfinite(config_.capacity_.max_events_per_second_) ||
        config_.capacity_.max_events_per_second_ <= 0.0) {
        return {status_code::invalid_argument, "invalid app manager target or capacity"};
    }
    auto current = vqec_vision_ai_core_applc_validate_catalog(catalog_);
    if (current.code_ != status_code::ok) {
        return current;
    }
    current = content_store_.vqec_vision_ai_ports_apcst_open();
    if (current.code_ == status_code::ok) {
        current = inventory_.vqec_vision_ai_ports_apinv_open();
    }
    if (current.code_ != status_code::ok) {
        return current;
    }
    current = inventory_.vqec_vision_ai_ports_apinv_load_snapshot(_snapshot);
    if (current.code_ == status_code::ok) {
        open_ = true;
        try {
            operation_worker_ = std::thread(
                &app_manager::vqec_vision_ai_appl_appmn_run_operations, this);
        } catch (...) {
            open_ = false;
            return {status_code::resource_exhausted,
                "cannot start app operation worker"};
        }
    }
    return current;
}

status app_manager::vqec_vision_ai_appl_appmn_commit_package(
    const app_package_candidate& _candidate,
    std::uint64_t _expected_inventory_revision,
    bool _is_update, const std::string& _expected_app_id,
    runtime_control_snapshot& _snapshot) {
    if (!open_) {
        return {status_code::invalid_state, "app manager is not open"};
    }
    if (_expected_inventory_revision == 0) {
        return {status_code::invalid_argument, "invalid expected inventory revision"};
    }
    verified_app_package package;
    auto current = package_verifier_.vqec_vision_ai_ports_apver_verify(
        _candidate, package);
    if (current.code_ != status_code::ok) {
        return current;
    }
    if (package.verification_receipt_id_.empty() ||
        package.manifest_sha256_ != _candidate.manifest_sha256_ ||
        package.configuration_sha256_ != _candidate.configuration_sha256_ ||
        (!_expected_app_id.empty() &&
            package.manifest_.app_id_ != _expected_app_id)) {
        return {status_code::unauthorized, "invalid verified package receipt"};
    }
    current = vqec_vision_ai_core_applc_validate_manifest(package.manifest_);
    if (current.code_ != status_code::ok) {
        return current;
    }
    if (!vqec_vision_ai_appl_appmn_is_catalogued(package.manifest_.app_id_)) {
        return {status_code::unauthorized,
            "application is not published in the product catalog"};
    }
    if (std::find(package.manifest_.target_ids_.begin(),
            package.manifest_.target_ids_.end(), config_.target_id_) ==
        package.manifest_.target_ids_.end()) {
        return {status_code::unsupported, "application does not support this target"};
    }
    for (const auto& component : package.manifest_.components_) {
        if (component.required_ && component.target_id_ != config_.target_id_) {
            return {status_code::unsupported,
                "required application component targets another platform"};
        }
    }
    if (!configuration_registry_.vqec_vision_ai_appl_apcrg_supports_manifest(
            package.manifest_)) {
        return {status_code::unsupported,
            "application requests an unavailable processor or schema"};
    }
    if (package.configuration_sha256_ !=
        package.manifest_.configuration_defaults_sha256_) {
        return {status_code::protocol_error,
            "configuration digest differs from manifest defaults"};
    }
    current = vqec_vision_ai_appl_appmn_verify_configuration_digest(
        package.configuration_payload_, package.configuration_sha256_);
    if (current.code_ != status_code::ok) {
        return current;
    }
    current = configuration_registry_.vqec_vision_ai_appl_apcrg_validate(
        package.manifest_.app_id_, package.manifest_.configuration_schema_id_,
        package.manifest_.configuration_schema_version_,
        package.configuration_payload_);
    if (current.code_ != status_code::ok) {
        return current;
    }
    current = inventory_.vqec_vision_ai_ports_apinv_authorize_install(
        package.manifest_, _expected_inventory_revision);
    if (current.code_ != status_code::ok) {
        return current;
    }
    std::vector<app_installed_component> installed_components;
    current = vqec_vision_ai_appl_appmn_stage_package_content(
        _candidate, package, installed_components);
    if (current.code_ != status_code::ok) {
        return current;
    }

    app_install_request request;
    request.manifest_ = std::move(package.manifest_);
    request.manifest_sha256_ = std::move(package.manifest_sha256_);
    request.expected_inventory_revision_ = _expected_inventory_revision;
    request.configuration_revision_ = 1;
    if (_is_update) {
        runtime_control_snapshot current_snapshot;
        current = inventory_.vqec_vision_ai_ports_apinv_load_snapshot(current_snapshot);
        const auto* association = current.code_ == status_code::ok ?
            vqec_vision_ai_appl_appmn_find_association(
                current_snapshot, request.manifest_.app_id_) : nullptr;
        if (current.code_ != status_code::ok || association == nullptr ||
            association->configuration_revision_ ==
                std::numeric_limits<std::uint64_t>::max()) {
            return current.code_ != status_code::ok ? current :
                status{status_code::invalid_state,
                    "app update has no valid installed generation"};
        }
        request.configuration_revision_ = association->configuration_revision_ + 1U;
    }
    request.configuration_sha256_ = std::move(package.configuration_sha256_);
    request.configuration_payload_ = std::move(package.configuration_payload_);
    request.components_ = std::move(installed_components);
    request.supported_ = true;
    request.compatible_ = true;
    request.admitted_ =
        request.manifest_.resources_.max_resident_bytes_ <=
            config_.capacity_.max_resident_bytes_ &&
        request.manifest_.resources_.max_tensor_bytes_ <=
            config_.capacity_.max_tensor_bytes_ &&
        request.manifest_.resources_.max_active_incidents_ <=
            config_.capacity_.max_active_incidents_ &&
        request.manifest_.resources_.max_events_per_second_ <=
            config_.capacity_.max_events_per_second_;
    return _is_update ?
        inventory_.vqec_vision_ai_ports_apinv_update(request, _snapshot) :
        inventory_.vqec_vision_ai_ports_apinv_install(request, _snapshot);
}

status app_manager::vqec_vision_ai_appl_appmn_install(
    const app_package_candidate& _candidate,
    std::uint64_t _expected_inventory_revision,
    runtime_control_snapshot& _snapshot) {
    std::lock_guard<std::mutex> guard(mutex_);
    return vqec_vision_ai_appl_appmn_commit_package(
        _candidate, _expected_inventory_revision, false, "", _snapshot);
}

status app_manager::vqec_vision_ai_appl_appmn_update(
    const app_package_candidate& _candidate,
    std::uint64_t _expected_inventory_revision,
    runtime_control_snapshot& _snapshot) {
    std::lock_guard<std::mutex> guard(mutex_);
    return vqec_vision_ai_appl_appmn_commit_package(
        _candidate, _expected_inventory_revision, true, "", _snapshot);
}

status app_manager::vqec_vision_ai_appl_appmn_submit_package(
    const app_operation_request& _operation_request,
    const app_package_candidate& _candidate,
    std::uint64_t _expected_inventory_revision, bool _is_update,
    app_operation_record& _operation) {
    if (_operation_request.kind_ != (_is_update ?
            app_operation_kind::update : app_operation_kind::install)) {
        return {status_code::invalid_argument,
            "invalid package operation submission"};
    }
    std::unique_ptr<operation_job> job;
    try {
        job = std::make_unique<operation_job>();
        job->request_ = _operation_request;
        job->candidate_ = _candidate;
        job->candidate_.components_.clear();
        job->expected_inventory_revision_ = _expected_inventory_revision;
        job->owned_descriptors_.reserve(_candidate.components_.size());
        job->candidate_.components_.reserve(_candidate.components_.size());
        for (const auto& component : _candidate.components_) {
            if (component.descriptor_ < 0) {
                return {status_code::invalid_argument,
                    "package component descriptor is invalid"};
            }
            const int duplicate = ::fcntl(
                component.descriptor_, F_DUPFD_CLOEXEC, 0);
            if (duplicate < 0) {
                return {status_code::io_error,
                    "cannot retain package component descriptor"};
            }
            job->owned_descriptors_.push_back(duplicate);
            job->candidate_.components_.push_back({duplicate});
        }
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted,
            "cannot allocate package operation"};
    }
    {
        std::lock_guard<std::mutex> queue_guard(queue_mutex_);
        if (operation_queue_.size() >=
            app_lifecycle_limits::g_max_pending_operations) {
            return {status_code::resource_exhausted,
                "app operation queue is full"};
        }
    }
    std::lock_guard<std::mutex> guard(mutex_);
    if (!open_) {
        return {status_code::invalid_state, "app manager is not open"};
    }
    bool is_new = false;
    auto current = inventory_.vqec_vision_ai_ports_apinv_begin_operation(
        _operation_request, _operation, is_new);
    if (current.code_ != status_code::ok || !is_new) {
        return current;
    }
    try {
        {
            std::lock_guard<std::mutex> queue_guard(queue_mutex_);
            operation_queue_.push_back(std::move(job));
        }
        queue_ready_.notify_one();
        return {};
    } catch (const std::bad_alloc&) {
        return inventory_.vqec_vision_ai_ports_apinv_finish_operation(
            _operation.operation_id_, app_operation_state::failed,
            {status_code::resource_exhausted,
                "cannot queue package operation"}, 0, _operation);
    }
}

status app_manager::vqec_vision_ai_appl_appmn_submit_rollback(
    const app_operation_request& _operation_request,
    std::uint64_t _expected_inventory_revision,
    app_operation_record& _operation) {
    if (_operation_request.kind_ != app_operation_kind::rollback) {
        return {status_code::invalid_argument, "invalid rollback operation submission"};
    }
    std::unique_ptr<operation_job> job;
    try {
        job = std::make_unique<operation_job>();
        job->request_ = _operation_request;
        job->expected_inventory_revision_ = _expected_inventory_revision;
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted,
            "cannot allocate rollback operation"};
    }
    {
        std::lock_guard<std::mutex> queue_guard(queue_mutex_);
        if (operation_queue_.size() >=
            app_lifecycle_limits::g_max_pending_operations) {
            return {status_code::resource_exhausted,
                "app operation queue is full"};
        }
    }
    std::lock_guard<std::mutex> guard(mutex_);
    if (!open_) {
        return {status_code::invalid_state, "app manager is not open"};
    }
    bool is_new = false;
    auto current = inventory_.vqec_vision_ai_ports_apinv_begin_operation(
        _operation_request, _operation, is_new);
    if (current.code_ != status_code::ok || !is_new) {
        return current;
    }
    try {
        {
            std::lock_guard<std::mutex> queue_guard(queue_mutex_);
            operation_queue_.push_back(std::move(job));
        }
        queue_ready_.notify_one();
        return {};
    } catch (const std::bad_alloc&) {
        return inventory_.vqec_vision_ai_ports_apinv_finish_operation(
            _operation.operation_id_, app_operation_state::failed,
            {status_code::resource_exhausted,
                "cannot queue rollback operation"}, 0, _operation);
    }
}

void app_manager::vqec_vision_ai_appl_appmn_run_operations() noexcept {
    for (;;) {
        std::unique_ptr<operation_job> job;
        {
            std::unique_lock<std::mutex> queue_guard(queue_mutex_);
            queue_ready_.wait(queue_guard, [this]() {
                return stop_worker_ || !operation_queue_.empty();
            });
            if (operation_queue_.empty()) {
                if (stop_worker_) {
                    return;
                }
                continue;
            }
            job = std::move(operation_queue_.front());
            operation_queue_.pop_front();
        }
        std::lock_guard<std::mutex> guard(mutex_);
        app_operation_record operation;
        const auto queried = inventory_.vqec_vision_ai_ports_apinv_get_operation(
            job->request_.idempotency_key_, operation);
        if (queried.code_ != status_code::ok ||
            operation.state_ != app_operation_state::queued) {
            continue;
        }
        runtime_control_snapshot snapshot;
        status executed;
        app_operation_state terminal_state = app_operation_state::failed;
        if (job->request_.kind_ == app_operation_kind::rollback) {
            executed = inventory_.vqec_vision_ai_ports_apinv_rollback(
                job->request_.app_id_, job->expected_inventory_revision_, snapshot);
            if (executed.code_ == status_code::ok) {
                terminal_state = app_operation_state::rolled_back;
            }
        } else {
            const bool is_update =
                job->request_.kind_ == app_operation_kind::update;
            executed = vqec_vision_ai_appl_appmn_commit_package(job->candidate_,
                job->expected_inventory_revision_, is_update,
                job->request_.app_id_, snapshot);
            if (executed.code_ == status_code::ok) {
                terminal_state = app_operation_state::committed;
            }
        }
        app_operation_record completed;
        (void)inventory_.vqec_vision_ai_ports_apinv_finish_operation(
            operation.operation_id_, terminal_state, executed,
            executed.code_ == status_code::ok ? snapshot.snapshot_revision_ : 0,
            completed);
    }
}

status app_manager::vqec_vision_ai_appl_appmn_rollback(
    const std::string& _app_id, std::uint64_t _expected_inventory_revision,
    runtime_control_snapshot& _snapshot) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!open_) {
        return {status_code::invalid_state, "app manager is not open"};
    }
    return inventory_.vqec_vision_ai_ports_apinv_rollback(
        _app_id, _expected_inventory_revision, _snapshot);
}

status app_manager::vqec_vision_ai_appl_appmn_update_configuration(
    const std::string& _app_id, std::uint64_t _expected_configuration_revision,
    const std::vector<std::uint8_t>& _configuration_payload,
    const std::string& _configuration_sha256,
    runtime_control_snapshot& _snapshot) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!open_) {
        return {status_code::invalid_state, "app manager is not open"};
    }
    runtime_control_snapshot current_snapshot;
    auto current = inventory_.vqec_vision_ai_ports_apinv_load_snapshot(current_snapshot);
    if (current.code_ != status_code::ok) {
        return current;
    }
    const auto* association = vqec_vision_ai_appl_appmn_find_association(
        current_snapshot, _app_id);
    if (association == nullptr || !association->installed_) {
        return {status_code::invalid_state, "application is not installed"};
    }
    if (association->configuration_revision_ != _expected_configuration_revision) {
        return {status_code::invalid_state, "stale app configuration revision"};
    }
    if (_expected_configuration_revision ==
        std::numeric_limits<std::uint64_t>::max()) {
        return {status_code::resource_exhausted,
            "app configuration revision is exhausted"};
    }
    current = vqec_vision_ai_appl_appmn_verify_configuration_digest(
        _configuration_payload, _configuration_sha256);
    if (current.code_ != status_code::ok) {
        return current;
    }
    current = configuration_registry_.vqec_vision_ai_appl_apcrg_validate(
        _app_id, association->configuration_schema_id_,
        _expected_configuration_revision + 1U, _configuration_payload);
    if (current.code_ != status_code::ok) {
        return current;
    }
    app_configuration_update update;
    update.app_id_ = _app_id;
    update.expected_configuration_revision_ = _expected_configuration_revision;
    update.configuration_revision_ = _expected_configuration_revision + 1U;
    update.configuration_sha256_ = _configuration_sha256;
    update.configuration_payload_ = _configuration_payload;
    return inventory_.vqec_vision_ai_ports_apinv_update_configuration(update, _snapshot);
}

status app_manager::vqec_vision_ai_appl_appmn_apply_verified_authority(
    const app_authority_update& _update,
    runtime_control_snapshot& _snapshot) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!open_) {
        return {status_code::invalid_state, "app manager is not open"};
    }
    return inventory_.vqec_vision_ai_ports_apinv_update_authority(_update, _snapshot);
}

status app_manager::vqec_vision_ai_appl_appmn_apply_entitlement(
    const app_entitlement_candidate& _candidate,
    runtime_control_snapshot& _snapshot) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!open_) {
        return {status_code::invalid_state, "app manager is not open"};
    }
    verified_app_entitlement entitlement;
    auto current = entitlement_verifier_.vqec_vision_ai_ports_entvr_verify(
        _candidate, entitlement);
    if (current.code_ != status_code::ok) {
        return current;
    }
    const auto& grant = entitlement.grant_;
    if (entitlement.verification_receipt_id_.empty() ||
        entitlement.grant_sha256_ != _candidate.grant_sha256_ ||
        grant.device_id_ != config_.device_id_ ||
        grant.target_id_ != config_.target_id_) {
        return {status_code::unauthorized,
            "entitlement scope or verification receipt is invalid"};
    }
    if (!vqec_vision_ai_appl_appmn_is_catalogued(grant.app_id_)) {
        return {status_code::unauthorized,
            "entitlement application is not published in the product catalog"};
    }
    const auto utc_now_ns = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    if (utc_now_ns < grant.not_before_utc_ns_ || utc_now_ns >= grant.expires_utc_ns_) {
        return {status_code::unauthorized, "entitlement is not currently valid"};
    }
    runtime_control_snapshot current_snapshot;
    current = inventory_.vqec_vision_ai_ports_apinv_load_snapshot(current_snapshot);
    if (current.code_ != status_code::ok) {
        return current;
    }
    const auto association = std::find_if(current_snapshot.associations_.begin(),
        current_snapshot.associations_.end(), [&grant](const auto& _association) {
            return _association.app_id_ == grant.app_id_ &&
                _association.source_id_ == grant.source_id_;
        });
    app_authority_update update;
    update.app_id_ = grant.app_id_;
    update.source_id_ = grant.source_id_;
    update.expected_entitlement_revision_ = grant.expected_entitlement_revision_;
    update.entitled_ = grant.granted_;
    update.supported_ = association != current_snapshot.associations_.end() &&
        association->installed_ && association->supported_;
    update.compatible_ = association != current_snapshot.associations_.end() &&
        association->installed_ && association->compatible_;
    update.admitted_ = association != current_snapshot.associations_.end() &&
        association->installed_ && association->admitted_;
    update.entitlement_expires_utc_ns_ = grant.granted_ ? grant.expires_utc_ns_ : 0;
    update.output_scopes_ = grant.output_scopes_;
    if (!grant.granted_) {
        update.reason_code_ = "revoked";
    } else if (association == current_snapshot.associations_.end() ||
               !association->installed_) {
        update.reason_code_ = "verified_preinstall";
    } else if (!association->supported_) {
        update.reason_code_ = "unsupported";
    } else if (!association->compatible_) {
        update.reason_code_ = "incompatible";
    } else if (!association->admitted_) {
        update.reason_code_ = "resource_limited";
    } else {
        update.reason_code_ = "verified";
    }
    return inventory_.vqec_vision_ai_ports_apinv_update_authority(update, _snapshot);
}

status app_manager::vqec_vision_ai_appl_appmn_set_desired(
    const app_desired_update& _update,
    runtime_control_snapshot& _snapshot) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!open_) {
        return {status_code::invalid_state, "app manager is not open"};
    }
    return inventory_.vqec_vision_ai_ports_apinv_set_desired(_update, _snapshot);
}

status app_manager::vqec_vision_ai_appl_appmn_uninstall(
    const std::string& _app_id, std::uint64_t _expected_inventory_revision,
    runtime_control_snapshot& _snapshot) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!open_) {
        return {status_code::invalid_state, "app manager is not open"};
    }
    return inventory_.vqec_vision_ai_ports_apinv_uninstall(
        _app_id, _expected_inventory_revision, _snapshot);
}

status app_manager::vqec_vision_ai_appl_appmn_get_snapshot(
    runtime_control_snapshot& _snapshot) const {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!open_) {
        return {status_code::invalid_state, "app manager is not open"};
    }
    return inventory_.vqec_vision_ai_ports_apinv_load_snapshot(_snapshot);
}

status app_manager::vqec_vision_ai_appl_appmn_list_applications(
    const std::string& _source_id,
    std::vector<app_catalog_status>& _applications) const {
    if (!vqec_vision_ai_cntr_ident_is_valid(
            _source_id, app_lifecycle_limits::g_max_identifier_bytes)) {
        return {status_code::invalid_argument, "invalid app catalog source identity"};
    }
    std::lock_guard<std::mutex> guard(mutex_);
    if (!open_) {
        return {status_code::invalid_state, "app manager is not open"};
    }
    runtime_control_snapshot snapshot;
    auto current = inventory_.vqec_vision_ai_ports_apinv_load_snapshot(snapshot);
    if (current.code_ != status_code::ok) {
        return current;
    }
    std::vector<app_catalog_status> applications;
    try {
        applications.reserve(catalog_.applications_.size());
        for (const auto& entry : catalog_.applications_) {
            app_catalog_status item;
            item.catalog_ = entry;
            item.source_id_ = _source_id;
            item.supported_ = entry.published_ &&
                configuration_registry_.vqec_vision_ai_appl_apcrg_supports_app(
                    entry.app_id_);
            item.snapshot_revision_ = snapshot.snapshot_revision_;
            const auto association = std::find_if(snapshot.associations_.begin(),
                snapshot.associations_.end(), [&entry, &_source_id](const auto& _value) {
                    return _value.app_id_ == entry.app_id_ &&
                        _value.source_id_ == _source_id;
                });
            if (!entry.published_) {
                item.reason_code_ = "not_published";
            } else if (association == snapshot.associations_.end() ||
                       !association->installed_) {
                item.entitled_ = association != snapshot.associations_.end() &&
                    association->entitled_;
                item.state_ = app_install_state::not_installed;
                item.reason_code_ = item.supported_ ? "not_installed" : "unsupported";
            } else {
                item.installed_ = true;
                item.entitled_ = association->entitled_;
                item.desired_ = association->desired_;
                item.effective_ = association->is_effective();
                if (!item.supported_ || !association->supported_) {
                    item.state_ = app_install_state::incompatible;
                    item.reason_code_ = "unsupported";
                } else if (!association->entitled_) {
                    item.state_ = app_install_state::locked;
                    item.reason_code_ = "entitlement_required";
                } else if (!association->compatible_) {
                    item.state_ = app_install_state::incompatible;
                    item.reason_code_ = "incompatible";
                } else if (!association->admitted_) {
                    item.state_ = app_install_state::resource_limited;
                    item.reason_code_ = "resource_limited";
                } else if (item.effective_) {
                    item.state_ = app_install_state::running;
                    item.reason_code_ = "running";
                } else {
                    item.state_ = app_install_state::installed_disabled;
                    item.reason_code_ = association->reason_code_.empty() ?
                        "installed_disabled" : association->reason_code_;
                }
            }
            applications.push_back(std::move(item));
        }
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted,
            "cannot allocate app catalog status"};
    }
    _applications = std::move(applications);
    return {};
}

status app_manager::vqec_vision_ai_ports_apmgr_install(
    const app_package_candidate& _candidate,
    std::uint64_t _expected_inventory_revision,
    runtime_control_snapshot& _snapshot) {
    return vqec_vision_ai_appl_appmn_install(
        _candidate, _expected_inventory_revision, _snapshot);
}

status app_manager::vqec_vision_ai_ports_apmgr_update(
    const app_package_candidate& _candidate,
    std::uint64_t _expected_inventory_revision,
    runtime_control_snapshot& _snapshot) {
    return vqec_vision_ai_appl_appmn_update(
        _candidate, _expected_inventory_revision, _snapshot);
}

status app_manager::vqec_vision_ai_ports_apmgr_rollback(
    const std::string& _app_id, std::uint64_t _expected_inventory_revision,
    runtime_control_snapshot& _snapshot) {
    return vqec_vision_ai_appl_appmn_rollback(
        _app_id, _expected_inventory_revision, _snapshot);
}

status app_manager::vqec_vision_ai_ports_apmgr_update_configuration(
    const std::string& _app_id, std::uint64_t _expected_configuration_revision,
    const std::vector<std::uint8_t>& _configuration_payload,
    const std::string& _configuration_sha256,
    runtime_control_snapshot& _snapshot) {
    return vqec_vision_ai_appl_appmn_update_configuration(_app_id,
        _expected_configuration_revision, _configuration_payload,
        _configuration_sha256, _snapshot);
}

status app_manager::vqec_vision_ai_ports_apmgr_apply_entitlement(
    const app_entitlement_candidate& _candidate,
    runtime_control_snapshot& _snapshot) {
    return vqec_vision_ai_appl_appmn_apply_entitlement(_candidate, _snapshot);
}

status app_manager::vqec_vision_ai_ports_apmgr_set_desired(
    const app_desired_update& _update,
    runtime_control_snapshot& _snapshot) {
    return vqec_vision_ai_appl_appmn_set_desired(_update, _snapshot);
}

status app_manager::vqec_vision_ai_ports_apmgr_uninstall(
    const std::string& _app_id, std::uint64_t _expected_inventory_revision,
    runtime_control_snapshot& _snapshot) {
    return vqec_vision_ai_appl_appmn_uninstall(
        _app_id, _expected_inventory_revision, _snapshot);
}

status app_manager::vqec_vision_ai_ports_apmgr_get_snapshot(
    runtime_control_snapshot& _snapshot) const {
    return vqec_vision_ai_appl_appmn_get_snapshot(_snapshot);
}

status app_manager::vqec_vision_ai_ports_apmgr_list_applications(
    const std::string& _source_id,
    std::vector<app_catalog_status>& _applications) const {
    return vqec_vision_ai_appl_appmn_list_applications(
        _source_id, _applications);
}

status app_manager::vqec_vision_ai_ports_apmgr_submit_install(
    const app_operation_request& _operation_request,
    const app_package_candidate& _candidate,
    std::uint64_t _expected_inventory_revision,
    app_operation_record& _operation) {
    return vqec_vision_ai_appl_appmn_submit_package(_operation_request,
        _candidate, _expected_inventory_revision, false, _operation);
}

status app_manager::vqec_vision_ai_ports_apmgr_submit_update(
    const app_operation_request& _operation_request,
    const app_package_candidate& _candidate,
    std::uint64_t _expected_inventory_revision,
    app_operation_record& _operation) {
    return vqec_vision_ai_appl_appmn_submit_package(_operation_request,
        _candidate, _expected_inventory_revision, true, _operation);
}

status app_manager::vqec_vision_ai_ports_apmgr_submit_rollback(
    const app_operation_request& _operation_request,
    std::uint64_t _expected_inventory_revision,
    app_operation_record& _operation) {
    return vqec_vision_ai_appl_appmn_submit_rollback(
        _operation_request, _expected_inventory_revision, _operation);
}

status app_manager::vqec_vision_ai_ports_apmgr_get_operation(
    const std::string& _operation_id, app_operation_record& _operation) const {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!open_) {
        return {status_code::invalid_state, "app manager is not open"};
    }
    return inventory_.vqec_vision_ai_ports_apinv_get_operation(
        _operation_id, _operation);
}

status app_manager::vqec_vision_ai_ports_apmgr_cancel_operation(
    const std::string& _operation_id, app_operation_record& _operation) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!open_) {
        return {status_code::invalid_state, "app manager is not open"};
    }
    return inventory_.vqec_vision_ai_ports_apinv_cancel_operation(
        _operation_id, _operation);
}

}  // namespace vqec::vision::ai
