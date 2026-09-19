#include "vqec_vision_service_startup.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <new>
#include <thread>
#include <utility>
#include <vector>

#include "vqec_vision_deployment_config.hpp"

namespace vqec::vision::ai {
namespace {

status vqec_vision_ai_appl_svstr_project_runtime_control(
    const runtime_control_snapshot& _runtime, const deployment_config& _deployment,
    usecase_control_snapshot& _control) {
    const auto valid = vqec_vision_ai_core_applc_validate_runtime_snapshot(_runtime);
    if (valid.code_ != status_code::ok) {
        return valid;
    }
    const auto utc_now_ns = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    std::vector<usecase_activation_request> requests;
    try {
        requests = _control.requests_;
        for (auto& request : requests) {
            request.desired_enabled_ = false;
            request.installed_ = false;
            request.entitlement_granted_ = false;
            request.supported_ = false;
            request.compatible_ = false;
            request.resource_admitted_ = false;
        }
        for (const auto& association : _runtime.associations_) {
            const auto usecase = std::find_if(_control.catalog_.usecases_.begin(),
                _control.catalog_.usecases_.end(), [&association](const auto& _entry) {
                    return _entry.usecase_id_ == association.app_id_;
                });
            const auto source = std::find_if(_deployment.sources_.begin(),
                _deployment.sources_.end(), [&association](const auto& _entry) {
                    return _entry.source_id_ == association.source_id_;
                });
            if (usecase == _control.catalog_.usecases_.end() ||
                source == _deployment.sources_.end()) {
                return {status_code::invalid_argument,
                    "runtime control association is absent from deployment catalog"};
            }
            const auto request = std::find_if(requests.begin(), requests.end(),
                [&association](const auto& _request) {
                    return _request.source_id_ == association.source_id_ &&
                        _request.usecase_id_ == association.app_id_;
                });
            if (request == requests.end()) {
                return {status_code::invalid_argument,
                    "runtime control association is absent from trusted usecase plan"};
            }
            request->desired_enabled_ = association.desired_;
            request->installed_ = association.installed_;
            request->entitlement_granted_ = association.entitled_ &&
                association.entitlement_expires_utc_ns_ > utc_now_ns;
            request->supported_ = association.supported_;
            request->compatible_ = association.compatible_;
            request->resource_admitted_ = association.admitted_;
        }
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted,
            "runtime control projection allocation failed"};
    }
    _control.control_revision_ = _runtime.snapshot_revision_;
    _control.entitlement_revision_ = _runtime.entitlement_revision_;
    _control.requests_ = std::move(requests);
    return {};
}

}  // namespace

bool vqec_vision_ai_appl_svstr_load_model_catalog(
    const std::string& _path, model_catalog& _catalog) {
    std::ifstream stream(_path);
    if (!stream.is_open()) {
        std::fprintf(stderr, "cannot open model catalog: %s\n", _path.c_str());
        return false;
    }
    std::uint64_t resident_bytes = 0;
    const auto loaded =
        vqec_vision_ai_mreg_mdcat_load_catalog(stream, _catalog, resident_bytes);
    if (loaded.code_ != status_code::ok) {
        std::fprintf(stderr, "model catalog rejected (%d): %s\n",
            static_cast<int>(loaded.code_), loaded.message_.c_str());
        return false;
    }
    return true;
}

bool vqec_vision_ai_appl_svstr_load_model_packages(
    const std::string& _path, model_package_registry& _registry) {
    std::ifstream stream(_path);
    if (!stream.is_open()) {
        std::fprintf(stderr, "cannot open model package registry: %s\n", _path.c_str());
        return false;
    }
    const auto loaded = vqec_vision_ai_mreg_mprld_load_registry(stream, _registry);
    if (loaded.code_ != status_code::ok) {
        std::fprintf(stderr, "model package registry rejected (%d): %s\n",
            static_cast<int>(loaded.code_), loaded.message_.c_str());
        return false;
    }
    return true;
}

bool vqec_vision_ai_appl_svstr_load_deployment(
    const std::string& _path, deployment_config& _deployment) {
    std::ifstream stream(_path);
    if (!stream.is_open()) {
        std::fprintf(stderr, "cannot open deployment config: %s\n", _path.c_str());
        return false;
    }
    std::uint64_t resident_bytes = 0;
    const auto loaded =
        vqec_vision_ai_life_dpcfg_load(stream, _deployment, resident_bytes);
    if (loaded.code_ != status_code::ok) {
        std::fprintf(stderr, "deployment config rejected (%d): %s\n",
            static_cast<int>(loaded.code_), loaded.message_.c_str());
        return false;
    }
    return true;
}

bool vqec_vision_ai_appl_svstr_load_feature_catalog(
    const std::string& _path, feature_catalog& _features) {
    std::ifstream stream(_path);
    if (!stream.is_open()) {
        std::fprintf(stderr, "cannot open feature catalog: %s\n", _path.c_str());
        return false;
    }
    const auto loaded = vqec_vision_ai_ftmgr_ftcat_load_catalog(stream, _features);
    if (loaded.code_ != status_code::ok) {
        std::fprintf(stderr, "feature catalog rejected (%d): %s\n",
            static_cast<int>(loaded.code_), loaded.message_.c_str());
        return false;
    }
    return true;
}

bool vqec_vision_ai_appl_svstr_load_usecase_snapshot(
    const std::string& _path, usecase_control_snapshot& _snapshot) {
    std::ifstream stream(_path);
    if (!stream.is_open()) {
        std::fprintf(stderr, "cannot open usecase snapshot: %s\n", _path.c_str());
        return false;
    }
    const auto loaded = vqec_vision_ai_ftmgr_ucfg_load_snapshot(stream, _snapshot);
    if (loaded.code_ != status_code::ok) {
        std::fprintf(stderr, "usecase snapshot rejected (%d): %s\n",
            static_cast<int>(loaded.code_), loaded.message_.c_str());
        return false;
    }
    return true;
}

service_feature_authority_state
vqec_vision_ai_appl_svstr_resolve_feature_authority(
    const service_startup_resolution& _startup, const parsed_arguments& _args,
    const std::string& _source_id, const std::string& _feature_id) {
    if (_startup.has_usecase_control) {
        feature_authority_projection projection;
        const auto projected = vqec_vision_ai_core_ucact_project_feature_authority(
            _startup.usecase_control.catalog_, _startup.usecase_activation,
            _source_id, _feature_id, projection);
        if (projected.code_ != status_code::ok || !projection.has_association_) {
            return {};
        }
        return {projection.desired_enabled_, projection.entitlement_granted_,
            projection.resource_admitted_};
    }
    if (!_args.production_mode || _args.platform == "fake" ||
        _args.platform == "reference") {
        return {true, true, true};
    }
    return {};
}

bool vqec_vision_ai_appl_svstr_is_preview_authorized(
    const service_startup_resolution& _startup, const parsed_arguments& _args,
    const std::string& _source_id) noexcept {
    if (!_args.production_mode || _args.platform == "fake" ||
        _args.platform == "reference") {
        return true;
    }
    if (!_startup.has_usecase_control) {
        return false;
    }
    for (const auto& record : _startup.usecase_activation.records_) {
        if (record.source_id_ == _source_id &&
            record.state_ == usecase_effective_state::ready &&
            record.desired_enabled_ && record.entitlement_granted_ &&
            record.resource_admitted_) {
            return true;
        }
    }
    return false;
}

service_startup_resolution vqec_vision_ai_appl_svstr_resolve_startup(
    const parsed_arguments& _args, const deployment_config* _effective_deployment,
    const runtime_control_snapshot* _runtime_control,
    usecase_control_manager* _control_manager,
    const std::function<void()>& _poll_control,
    const std::function<bool()>& _is_runtime_reconcile_requested,
    const std::function<bool()>& _is_stop_requested,
    std::uint64_t _runtime_generation, std::uint64_t _pending_control_revision,
    std::uint64_t _idle_step_interval_ns, int _reconcile_generation_exit_code) {
    service_startup_resolution result;
    if (!vqec_vision_ai_appl_svstr_load_model_catalog(
            _args.catalog_path, result.catalog) ||
        !vqec_vision_ai_appl_svstr_load_deployment(
            _args.deployment_path, result.deployment)) {
        result.exit_code = 1;
        return result;
    }
    if (!_args.feature_catalog_path.empty() &&
        !vqec_vision_ai_appl_svstr_load_feature_catalog(
            _args.feature_catalog_path, result.features)) {
        result.exit_code = 1;
        return result;
    }
    if (!_args.usecase_snapshot_path.empty()) {
        usecase_control_snapshot control;
        if (!vqec_vision_ai_appl_svstr_load_usecase_snapshot(
                _args.usecase_snapshot_path, control)) {
            result.exit_code = 1;
            return result;
        }
        if (control.deployment_revision_ != result.deployment.revision_) {
            std::fprintf(stderr,
                "usecase snapshot deployment revision does not match deployment\n");
            result.exit_code = 1;
            return result;
        }
        if (_runtime_control != nullptr) {
            const auto projected = vqec_vision_ai_appl_svstr_project_runtime_control(
                *_runtime_control, result.deployment, control);
            if (projected.code_ != status_code::ok) {
                std::fprintf(stderr, "runtime control projection rejected (%d): %s\n",
                    static_cast<int>(projected.code_), projected.message_.c_str());
                result.exit_code = 1;
                return result;
            }
            result.runtime_control = *_runtime_control;
            result.has_runtime_control = true;
        }
        usecase_activation_snapshot activation_snapshot;
        deployment_config effective_deployment;
        const auto composed = vqec_vision_ai_core_ucact_compose_effective_deployment(
            result.deployment, result.catalog, control.catalog_, control.requests_,
            activation_snapshot, effective_deployment);
        if (composed.code_ != status_code::ok) {
            std::fprintf(stderr, "usecase composition rejected (%d): %s\n",
                static_cast<int>(composed.code_), composed.message_.c_str());
            result.exit_code = 1;
            return result;
        }
        activation_snapshot.policy_revision_ = control.control_revision_;
        activation_snapshot.config_revision_ = _runtime_control == nullptr ?
            control.control_revision_ : _runtime_control->snapshot_revision_;
        result.deployment = _effective_deployment == nullptr
            ? std::move(effective_deployment) : *_effective_deployment;
        result.usecase_control = control;
        result.usecase_activation = std::move(activation_snapshot);
        result.has_usecase_control = true;
        std::printf("usecase plan control_revision=%llu entitlement_revision=%llu "
                    "active_sources=%zu\n",
            static_cast<unsigned long long>(control.control_revision_),
            static_cast<unsigned long long>(control.entitlement_revision_),
            result.deployment.sources_.size());
    } else if (_effective_deployment != nullptr) {
        result.deployment = *_effective_deployment;
    }
    if (result.deployment.sources_.empty()) {
        if (_control_manager != nullptr) {
            const auto published = _pending_control_revision == 0
                ? (_runtime_generation == 1
                    ? _control_manager->vqec_vision_ai_ftmgr_ucmgr_publish_initial(
                          _runtime_generation)
                    : status{})
                : _control_manager->vqec_vision_ai_ftmgr_ucmgr_publish_pending(
                      _pending_control_revision, _runtime_generation);
            if (published.code_ != status_code::ok) {
                result.exit_code = 1;
                return result;
            }
        }
        std::uint64_t idle_steps = 0;
        while (!_is_stop_requested() &&
               (_args.max_steps == 0 || idle_steps < _args.max_steps)) {
            if (_poll_control) {
                _poll_control();
            }
            if (_is_runtime_reconcile_requested &&
                _is_runtime_reconcile_requested()) {
                result.exit_code = _reconcile_generation_exit_code;
                return result;
            }
            if (_control_manager != nullptr &&
                _control_manager->vqec_vision_ai_ftmgr_ucmgr_has_pending()) {
                result.exit_code = _reconcile_generation_exit_code;
                return result;
            }
            std::this_thread::sleep_for(
                std::chrono::nanoseconds(_idle_step_interval_ns));
            ++idle_steps;
        }
        return result;
    }
    const bool has_fr_arguments = !_args.fr_gallery_path.empty() ||
        !_args.fr_protected_directory.empty() || !_args.fr_gallery_file_name.empty() ||
        !_args.fr_key_file_name.empty() || !_args.fr_lock_file_name.empty() ||
        !_args.fr_gallery_id.empty() || _args.fr_preprocess_revision != 0 ||
        _args.fr_store_max_bytes != 0 || _args.fr_similarity_set ||
        _args.fr_margin_set || _args.fr_templates_set || _args.fr_top_k_set ||
        !_args.fr_feature_id.empty() || !_args.fr_identity_attribute.empty() ||
        _args.enrollment_dbus;
    const bool has_complete_fr_arguments = !_args.fr_gallery_path.empty() &&
        !_args.fr_protected_directory.empty() && !_args.fr_gallery_file_name.empty() &&
        !_args.fr_key_file_name.empty() && !_args.fr_lock_file_name.empty() &&
        !_args.fr_gallery_id.empty() && _args.fr_preprocess_revision != 0 &&
        _args.fr_store_max_bytes != 0 && _args.fr_similarity_set &&
        _args.fr_margin_set && _args.fr_templates_set && _args.fr_top_k_set &&
        !_args.fr_feature_id.empty() && !_args.fr_identity_attribute.empty();
    if (has_fr_arguments && !has_complete_fr_arguments) {
        std::fprintf(stderr,
            "FR requires derived index path, protected-store directory/files, gallery "
            "identity/preprocess revision/quota, similarity, margin, max templates, "
            "top-k, feature id and identity attribute\n");
        result.exit_code = 1;
        return result;
    }
    bool has_active_secondary_model = false;
    for (const auto& source : result.deployment.sources_) {
        for (const auto& model : result.catalog.models_) {
            if (model.role_ == model_role::secondary &&
                vqec_vision_ai_core_mdcat_source_activates_model(source, model)) {
                has_active_secondary_model = true;
                break;
            }
        }
        if (has_active_secondary_model) {
            break;
        }
    }
    result.fr_effectively_enabled =
        has_complete_fr_arguments && has_active_secondary_model;
    const bool has_complete_image_enrollment = !_args.enrollment_image_roots.empty() &&
        _args.enrollment_max_image_bytes != 0 && _args.enrollment_image_timeout_ms != 0 &&
        !_args.enrollment_jpeg_decoder.empty() && !_args.enrollment_converter.empty() &&
        !_args.enrollment_scaler.empty() && !_args.enrollment_transform.empty() &&
        !_args.enrollment_transform_engine.empty();
    if (_args.enrollment_dbus && !has_complete_image_enrollment) {
        std::fprintf(stderr,
            "enrollment DBus requires image roots, byte/time limits and every image "
            "pipeline factory\n");
        result.exit_code = 1;
        return result;
    }
    if (!_args.model_package_registry_path.empty()) {
        if (!_args.model_package.empty() || !_args.model_library.empty()) {
            std::fprintf(stderr,
                "model package registry cannot be combined with legacy package arguments\n");
            result.exit_code = 1;
            return result;
        }
        if (!vqec_vision_ai_appl_svstr_load_model_packages(
                _args.model_package_registry_path, result.model_packages)) {
            result.exit_code = 1;
            return result;
        }
    } else if (!_args.model_package.empty() || !_args.model_library.empty()) {
        if (result.catalog.models_.size() != 1 || _args.model_package.empty() ||
            _args.model_library.empty()) {
            std::fprintf(stderr,
                "legacy model package arguments require exactly one catalog model\n");
            result.exit_code = 1;
            return result;
        }
        const auto& model = result.catalog.models_.front();
        result.model_packages.schema_version_ =
            model_package_registry_limits::g_schema_version;
        result.model_packages.bindings_.push_back({model.model_id_, model.model_version_,
            model.target_id_, model.artifact_ref_, _args.model_package,
            _args.model_library});
    }
    if (result.deployment.sources_.size() > deployment_limits::g_max_sources) {
        std::fprintf(stderr, "deployment source count exceeds runtime support\n");
        result.exit_code = 1;
        return result;
    }
    result.use_reference_platform = _args.platform == "reference";
    result.use_production_platform = _args.platform == "qualcomm";
    const bool platform_named = _args.platform == "fake" ||
        result.use_reference_platform || result.use_production_platform;
    if (_args.enrollment_dbus &&
        (!_args.production_mode || !result.use_production_platform)) {
        std::fprintf(stderr,
            "file enrollment DBus requires production mode with the Qualcomm platform\n");
        result.exit_code = 1;
        return result;
    }
    if (_args.production_mode && !platform_named) {
        std::fprintf(stderr,
            "production mode: --platform %s is not wired; use --platform fake or "
            "--platform reference for a device-free platform. Refusing fixture fallback\n",
            _args.platform.c_str());
        result.exit_code = 3;
        return result;
    }
    result.should_run = true;
    return result;
}

}  // namespace vqec::vision::ai
