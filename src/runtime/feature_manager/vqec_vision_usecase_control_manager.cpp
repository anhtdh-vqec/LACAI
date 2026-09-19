#include "vqec_vision_usecase_control_manager.hpp"

#include <algorithm>
#include <limits>
#include <new>
#include <tuple>
#include <utility>

#include "vqec/vision/ai/contracts/base/vqec_vision_identifier.hpp"

namespace vqec::vision::ai {
namespace {

bool vqec_vision_ai_ftmgr_ucmgr_entry_less(
    const usecase_desired_entry& _left, const usecase_desired_entry& _right) noexcept {
    return std::tie(_left.source_id_, _left.usecase_id_) <
        std::tie(_right.source_id_, _right.usecase_id_);
}

bool vqec_vision_ai_ftmgr_ucmgr_entries_equal(
    const std::vector<usecase_desired_entry>& _left,
    const std::vector<usecase_desired_entry>& _right) noexcept {
    if (_left.size() != _right.size()) {
        return false;
    }
    for (std::size_t index = 0; index < _left.size(); ++index) {
        if (_left[index].source_id_ != _right[index].source_id_ ||
            _left[index].usecase_id_ != _right[index].usecase_id_ ||
            _left[index].desired_enabled_ != _right[index].desired_enabled_) {
            return false;
        }
    }
    return true;
}

const usecase_activation_record* vqec_vision_ai_ftmgr_ucmgr_find_record(
    const usecase_activation_snapshot& _snapshot, const std::string& _source_id,
    const std::string& _usecase_id) noexcept {
    for (const auto& record : _snapshot.records_) {
        if (record.source_id_ == _source_id && record.usecase_id_ == _usecase_id) {
            return &record;
        }
    }
    return nullptr;
}

usecase_runtime_state vqec_vision_ai_ftmgr_ucmgr_runtime_state(
    usecase_effective_state _state, bool _pending, bool _failed) noexcept {
    if (_failed && _state == usecase_effective_state::ready) {
        return usecase_runtime_state::faulted;
    }
    switch (_state) {
    case usecase_effective_state::disabled: return usecase_runtime_state::disabled;
    case usecase_effective_state::not_installed: return usecase_runtime_state::unsupported;
    case usecase_effective_state::denied: return usecase_runtime_state::denied;
    case usecase_effective_state::unsupported: return usecase_runtime_state::unsupported;
    case usecase_effective_state::incompatible: return usecase_runtime_state::incompatible;
    case usecase_effective_state::resource_limited:
        return usecase_runtime_state::resource_limited;
    case usecase_effective_state::ready:
        return _pending ? usecase_runtime_state::loading : usecase_runtime_state::running;
    }
    return usecase_runtime_state::faulted;
}

const char* vqec_vision_ai_ftmgr_ucmgr_reason(
    usecase_effective_state _state, bool _pending, bool _failed) noexcept {
    if (_failed && _state == usecase_effective_state::ready) {
        return "runtime_reconcile_failed";
    }
    if (_pending && _state == usecase_effective_state::ready) {
        return "runtime_reconciling";
    }
    switch (_state) {
    case usecase_effective_state::disabled: return "desired_disabled";
    case usecase_effective_state::not_installed: return "not_installed";
    case usecase_effective_state::denied: return "entitlement_denied";
    case usecase_effective_state::unsupported: return "unsupported";
    case usecase_effective_state::incompatible: return "incompatible";
    case usecase_effective_state::resource_limited: return "resource_limited";
    case usecase_effective_state::ready: return "running";
    }
    return "unknown";
}

}  // namespace

status usecase_control_manager::vqec_vision_ai_ftmgr_ucmgr_configure(
    const usecase_control_snapshot& _trusted_snapshot,
    const deployment_config& _base_deployment, const model_catalog& _models) {
    if (configured_) {
        return {status_code::invalid_state, "usecase control manager is already configured"};
    }
    if (_trusted_snapshot.control_revision_ == 0 ||
        _trusted_snapshot.entitlement_revision_ == 0 ||
        _trusted_snapshot.deployment_revision_ != _base_deployment.revision_) {
        return {status_code::invalid_argument, "trusted usecase snapshot revision is invalid"};
    }
    usecase_activation_snapshot activation;
    deployment_config effective;
    const auto composed = vqec_vision_ai_core_ucact_compose_effective_deployment(
        _base_deployment, _models, _trusted_snapshot.catalog_,
        _trusted_snapshot.requests_, activation, effective);
    if (composed.code_ != status_code::ok) {
        return composed;
    }
    try {
        control_ = _trusted_snapshot;
        base_deployment_ = _base_deployment;
        models_ = _models;
        activation_ = std::move(activation);
        active_deployment_ = std::move(effective);
        receipts_.reserve(usecase_control_limits::g_max_idempotency_receipts);
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted, "usecase control configuration allocation failed"};
    }
    configured_ = true;
    return {};
}

status usecase_control_manager::vqec_vision_ai_ports_ucctl_apply_desired_plan(
    const usecase_desired_plan& _plan, usecase_apply_receipt& _receipt) {
    if (!configured_) {
        return {status_code::invalid_state, "usecase control manager is not configured"};
    }
    if (!vqec_vision_ai_cntr_ident_is_valid(
            _plan.request_id_, usecase_control_limits::g_max_request_id_bytes) ||
        _plan.entries_.size() > usecase_activation_limits::g_max_associations) {
        return {status_code::invalid_argument, "desired usecase plan is invalid"};
    }
    std::vector<usecase_desired_entry> entries;
    try {
        entries = _plan.entries_;
        std::sort(entries.begin(), entries.end(), vqec_vision_ai_ftmgr_ucmgr_entry_less);
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted, "desired usecase plan allocation failed"};
    }
    for (std::size_t index = 0; index < entries.size(); ++index) {
        if (!vqec_vision_ai_cntr_ident_is_valid(
                entries[index].source_id_, usecase_activation_limits::g_max_identifier_bytes) ||
            !vqec_vision_ai_cntr_ident_is_valid(
                entries[index].usecase_id_, usecase_activation_limits::g_max_identifier_bytes) ||
            (index != 0 && entries[index].source_id_ == entries[index - 1].source_id_ &&
             entries[index].usecase_id_ == entries[index - 1].usecase_id_)) {
            return {status_code::invalid_argument, "desired usecase plan contains an invalid entry"};
        }
    }
    for (const auto& record : receipts_) {
        if (record.request_id_ != _plan.request_id_) {
            continue;
        }
        if (record.expected_control_revision_ != _plan.expected_control_revision_ ||
            !vqec_vision_ai_ftmgr_ucmgr_entries_equal(record.entries_, entries)) {
            return {status_code::invalid_argument, "usecase request ID payload conflict"};
        }
        _receipt = record.receipt_;
        return {};
    }
    if (_plan.expected_control_revision_ != control_.control_revision_) {
        return {status_code::invalid_state, "usecase control revision is stale"};
    }
    if (runtime_generation_ == 0) {
        return {status_code::invalid_state, "initial usecase runtime is still loading"};
    }
    if (has_pending_) {
        return {status_code::invalid_state, "a usecase runtime generation is reconciling"};
    }

    receipt_record stored_receipt;
    try {
        stored_receipt.request_id_ = _plan.request_id_;
        stored_receipt.expected_control_revision_ = _plan.expected_control_revision_;
        stored_receipt.entries_ = entries;
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted, "usecase receipt allocation failed"};
    }

    usecase_control_snapshot candidate;
    try {
        candidate = control_;
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted, "usecase candidate allocation failed"};
    }
    for (auto& request : candidate.requests_) {
        request.desired_enabled_ = false;
    }
    for (const auto& entry : entries) {
        auto request = std::find_if(candidate.requests_.begin(), candidate.requests_.end(),
            [&](const usecase_activation_request& _item) {
                return _item.source_id_ == entry.source_id_ &&
                    _item.usecase_id_ == entry.usecase_id_;
            });
        if (request == candidate.requests_.end()) {
            return {status_code::invalid_argument, "desired plan references an unknown association"};
        }
        request->desired_enabled_ = entry.desired_enabled_;
    }
    bool changed = false;
    for (std::size_t index = 0; index < candidate.requests_.size(); ++index) {
        if (candidate.requests_[index].desired_enabled_ !=
            control_.requests_[index].desired_enabled_) {
            changed = true;
            break;
        }
    }
    usecase_apply_receipt receipt;
    receipt.accepted_ = true;
    receipt.control_revision_ = control_.control_revision_;
    receipt.apply_state_ = usecase_apply_state::unchanged;
    if (changed) {
        if (control_.control_revision_ == std::numeric_limits<std::uint64_t>::max()) {
            return {status_code::resource_exhausted, "usecase control revision exhausted"};
        }
        ++candidate.control_revision_;
        usecase_activation_snapshot activation;
        deployment_config effective;
        const auto composed = vqec_vision_ai_core_ucact_compose_effective_deployment(
            base_deployment_, models_, candidate.catalog_, candidate.requests_,
            activation, effective);
        if (composed.code_ != status_code::ok) {
            return composed;
        }
        try {
            pending_control_ = candidate;
            pending_activation_ = std::move(activation);
            pending_deployment_ = std::move(effective);
            control_ = std::move(candidate);
        } catch (const std::bad_alloc&) {
            pending_control_ = {};
            pending_activation_ = {};
            pending_deployment_ = {};
            return {status_code::resource_exhausted,
                "usecase candidate publication allocation failed"};
        }
        has_pending_ = true;
        pending_failure_ = status_code::ok;
        receipt.control_revision_ = control_.control_revision_;
        receipt.apply_state_ = usecase_apply_state::reconciling;
    }
    try {
        if (receipts_.size() == usecase_control_limits::g_max_idempotency_receipts) {
            receipts_.erase(receipts_.begin());
        }
        stored_receipt.receipt_ = receipt;
        receipts_.push_back(std::move(stored_receipt));
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted, "usecase receipt allocation failed"};
    }
    _receipt = receipt;
    return {};
}

status usecase_control_manager::vqec_vision_ai_ports_ucctl_get_status(
    usecase_control_status& _status) const {
    if (!configured_) {
        return {status_code::invalid_state, "usecase control manager is not configured"};
    }
    const auto& selected = (has_pending_ || pending_failure_ != status_code::ok)
        ? pending_activation_ : activation_;
    usecase_control_status result;
    result.control_revision_ = control_.control_revision_;
    result.entitlement_revision_ = control_.entitlement_revision_;
    result.runtime_generation_ = runtime_generation_;
    try {
        result.entries_.reserve(selected.records_.size());
        for (const auto& record : selected.records_) {
            const bool failed = pending_failure_ != status_code::ok;
            const auto active = vqec_vision_ai_ftmgr_ucmgr_find_record(
                activation_, record.source_id_, record.usecase_id_);
            const bool active_ready = active != nullptr &&
                active->state_ == usecase_effective_state::ready && runtime_generation_ != 0;
            const bool candidate_ready = record.state_ == usecase_effective_state::ready;
            const bool loaded = active_ready ||
                (!has_pending_ && !failed && candidate_ready && runtime_generation_ != 0);
            bool running = !has_pending_ && active_ready;
            const bool awaiting_publication = has_pending_ || runtime_generation_ == 0;
            auto runtime_state = vqec_vision_ai_ftmgr_ucmgr_runtime_state(
                record.state_, awaiting_publication, failed);
            const char* reason = vqec_vision_ai_ftmgr_ucmgr_reason(
                record.state_, awaiting_publication, failed);
            if (has_pending_ && active_ready && candidate_ready) {
                running = true;
                runtime_state = usecase_runtime_state::running;
                reason = "running_during_reconcile";
            } else if (has_pending_ && active_ready && !candidate_ready) {
                runtime_state = usecase_runtime_state::draining;
                reason = "runtime_draining";
            }
            result.entries_.push_back({record.source_id_, record.usecase_id_,
                record.installed_, record.entitlement_granted_, record.desired_enabled_,
                record.supported_, record.compatible_, record.resource_admitted_, loaded,
                running, runtime_state, reason});
        }
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted, "usecase status allocation failed"};
    }
    _status = std::move(result);
    return {};
}

status usecase_control_manager::vqec_vision_ai_ports_ucctl_get_capabilities(
    usecase_capability_snapshot& _capabilities) const {
    if (!configured_) {
        return {status_code::invalid_state, "usecase control manager is not configured"};
    }
    usecase_capability_snapshot result;
    result.catalog_revision_ = control_.catalog_.revision_;
    try {
        result.usecases_.reserve(control_.catalog_.usecases_.size());
        for (const auto& usecase : control_.catalog_.usecases_) {
            result.usecases_.push_back({usecase.usecase_id_, usecase.usecase_version_});
        }
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted, "usecase capability allocation failed"};
    }
    _capabilities = std::move(result);
    return {};
}

status usecase_control_manager::vqec_vision_ai_ftmgr_ucmgr_get_pending(
    usecase_control_snapshot& _snapshot, deployment_config& _deployment) const {
    if (!has_pending_) {
        return {status_code::invalid_state, "no usecase runtime generation is pending"};
    }
    try {
        _snapshot = pending_control_;
        _deployment = pending_deployment_;
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted, "pending usecase plan allocation failed"};
    }
    return {};
}

status usecase_control_manager::vqec_vision_ai_ftmgr_ucmgr_publish_initial(
    std::uint64_t _runtime_generation) {
    if (!configured_ || has_pending_ || runtime_generation_ != 0 ||
        _runtime_generation == 0) {
        return {status_code::invalid_state, "initial usecase runtime publish is invalid"};
    }
    runtime_generation_ = _runtime_generation;
    return {};
}

status usecase_control_manager::vqec_vision_ai_ftmgr_ucmgr_publish_pending(
    std::uint64_t _control_revision, std::uint64_t _runtime_generation) {
    if (!has_pending_ || _control_revision != pending_control_.control_revision_ ||
        _runtime_generation == 0 || _runtime_generation <= runtime_generation_) {
        return {status_code::invalid_state, "pending usecase generation publish is stale"};
    }
    activation_ = std::move(pending_activation_);
    active_deployment_ = std::move(pending_deployment_);
    pending_control_ = {};
    pending_failure_ = status_code::ok;
    runtime_generation_ = _runtime_generation;
    has_pending_ = false;
    return {};
}

status usecase_control_manager::vqec_vision_ai_ftmgr_ucmgr_fail_pending(
    std::uint64_t _control_revision, status_code _reason_code) {
    if (!has_pending_ || _control_revision != pending_control_.control_revision_ ||
        _reason_code == status_code::ok || _reason_code == status_code::pending) {
        return {status_code::invalid_argument, "pending usecase generation failure is invalid"};
    }
    pending_failure_ = _reason_code;
    has_pending_ = false;
    pending_control_ = {};
    pending_deployment_ = {};
    return {};
}

}  // namespace vqec::vision::ai
