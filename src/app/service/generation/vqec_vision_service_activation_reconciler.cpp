#include "vqec_vision_service_activation_reconciler.hpp"

#include <cstdio>
#include <new>
#include <utility>

#include "vqec/vision/ai/contracts/lifecycle/vqec_vision_activation_delta.hpp"
#include "vqec_vision_service_cascade_runtime.hpp"

namespace vqec::vision::ai {

status service_activation_reconciler::vqec_vision_ai_appl_svacr_complete_pending() {
    if (pending_startup_ == nullptr) {
        return {};
    }
    for (std::uint16_t source_slot = 0;
         source_slot < pending_startup_->activation_plan.source_count_;
         ++source_slot) {
        auto* session = bundle_->vqec_vision_ai_appl_rcfac_get_session(source_slot);
        if (session == nullptr) {
            return {status_code::invalid_state,
                "pending runtime activation source session is absent"};
        }
        const auto snapshot =
            session->vqec_vision_ai_appl_mmses_get_snapshot();
        if (snapshot.is_recovery_required_ ||
            snapshot.delta_phase_ == multi_model_delta_phase::recovery_required) {
            return {status_code::invalid_state,
                "pending model activation requires hardware recovery"};
        }
        const auto expected =
            pending_startup_->activation_plan.sources_[source_slot].active_model_mask_;
        if (snapshot.delta_phase_ != multi_model_delta_phase::idle ||
            snapshot.active_model_mask_ != expected ||
            snapshot.desired_model_mask_ != expected) {
            return {status_code::pending,
                "model activation delta is still in progress"};
        }
    }
    if (cascade_owners_ != nullptr &&
        !vqec_vision_ai_appl_svcsc_is_activation_complete(
            *cascade_owners_)) {
        return {status_code::pending,
            "cascade activation delta is still in progress"};
    }
    const auto committed_revision =
        pending_startup_->runtime_control.snapshot_revision_;
    *startup_ = std::move(*pending_startup_);
    pending_startup_.reset();
    std::printf("activation delta committed snapshot=%llu\n",
        static_cast<unsigned long long>(committed_revision));
    return {};
}

status service_activation_reconciler::vqec_vision_ai_appl_svacr_bind_cascade_owners(
    std::array<service_cascade_owner, deployment_limits::g_max_sources>& _owners)
    noexcept {
    if (startup_ == nullptr || cascade_owners_ != nullptr) {
        return {status_code::invalid_argument,
            "service activation reconciler received invalid cascade owners"};
    }
    cascade_owners_ = &_owners;
    return {};
}

status service_activation_reconciler::vqec_vision_ai_appl_svacr_configure(
    service_startup_resolution& _startup, const parsed_arguments& _arguments,
    runtime_composition_bundle& _bundle,
    std::unique_ptr<service_feature_activation>& _feature_owner,
    const feature_processor_registry& _feature_registry,
    const std::string& _fallback_attribute_schema,
    output_gate& _output_gate) {
    if (startup_ != nullptr || !_startup.has_runtime_control ||
        !_startup.has_usecase_control || _startup.activation_plan.source_count_ == 0 ||
        _feature_owner == nullptr ||
        _bundle.vqec_vision_ai_appl_rcfac_get_source_count() !=
            _startup.activation_plan.source_count_) {
        return {status_code::invalid_state,
            "service activation reconciler cannot bind this generation"};
    }
    startup_ = &_startup;
    arguments_ = &_arguments;
    bundle_ = &_bundle;
    feature_owner_ = &_feature_owner;
    feature_registry_ = &_feature_registry;
    fallback_attribute_schema_ = &_fallback_attribute_schema;
    output_gate_ = &_output_gate;
    return {};
}

status service_activation_reconciler::vqec_vision_ai_appl_svacr_apply_snapshot(
    const runtime_control_snapshot& _runtime, std::uint64_t _steady_now_ns) {
    if (startup_ == nullptr || arguments_ == nullptr || bundle_ == nullptr ||
        feature_owner_ == nullptr || *feature_owner_ == nullptr ||
        feature_registry_ == nullptr || fallback_attribute_schema_ == nullptr ||
        output_gate_ == nullptr) {
        return {status_code::invalid_state,
            "service activation reconciler is not configured"};
    }
    if (pending_startup_ != nullptr) {
        const auto completed = vqec_vision_ai_appl_svacr_complete_pending();
        if (completed.code_ != status_code::ok) {
            return completed;
        }
        if (_runtime.snapshot_revision_ ==
            startup_->runtime_control.snapshot_revision_) {
            return {};
        }
    }
    if (_runtime.snapshot_revision_ <=
        startup_->runtime_control.snapshot_revision_) {
        return {status_code::invalid_state,
            "runtime activation snapshot is not newer"};
    }
    try {
        service_startup_resolution candidate_startup;
        const auto projected = vqec_vision_ai_appl_svstr_reconcile_runtime_control(
            *startup_, _runtime, candidate_startup);
        if (projected.code_ != status_code::ok) {
            return projected;
        }
        app_activation_delta delta;
        const auto planned = vqec_vision_ai_core_acdel_build_delta(
            startup_->activation_plan, candidate_startup.activation_plan, delta);
        if (planned.code_ != status_code::ok) {
            return planned;
        }
        if (delta.requires_capacity_replacement_) {
            return {status_code::unsupported,
                "runtime activation exceeds prepared model capacity"};
        }
        activation_snapshot candidate_admission;
        const auto admitted = vqec_vision_ai_admis_actsp_build_snapshot(
            candidate_startup.active_deployment, candidate_startup.catalog,
            bundle_->vqec_vision_ai_appl_rcfac_get_admission().hardware_profile_,
            candidate_admission);
        if (admitted.code_ != status_code::ok) {
            return admitted;
        }

        for (std::uint16_t source_slot = 0;
             source_slot < candidate_startup.activation_plan.source_count_;
             ++source_slot) {
            auto* session = bundle_->vqec_vision_ai_appl_rcfac_get_session(source_slot);
            if (session == nullptr) {
                return {status_code::invalid_state,
                    "runtime activation source session is absent"};
            }
            const auto valid_mask = session->
                vqec_vision_ai_appl_mmses_validate_model_mask(
                    candidate_startup.activation_plan.sources_[source_slot].
                        active_model_mask_, _steady_now_ns);
            if (valid_mask.code_ == status_code::invalid_state) {
                return {status_code::pending,
                    "previous model activation delta is still in progress"};
            }
            if (valid_mask.code_ != status_code::ok) {
                return valid_mask;
            }
        }
        if (!delta.cascade_dependencies_.empty()) {
            if (cascade_owners_ == nullptr) {
                return {status_code::unsupported,
                    "cascade activation requires a prepared generation owner"};
            }
            const auto valid_cascades =
                vqec_vision_ai_appl_svcsc_validate_activation(
                    *cascade_owners_,
                    candidate_startup.activation_plan, _steady_now_ns);
            if (valid_cascades.code_ == status_code::invalid_state) {
                return {status_code::pending,
                    "previous cascade activation delta is still in progress"};
            }
            if (valid_cascades.code_ != status_code::ok) {
                return valid_cascades;
            }
        }

        auto candidate_features = std::make_unique<service_feature_activation>();
        const auto configured = candidate_features->vqec_vision_ai_appl_svfac_configure(
            candidate_startup, *arguments_, startup_->deployment, startup_->catalog,
            startup_->features, *feature_registry_, *fallback_attribute_schema_,
            *output_gate_, bundle_->vqec_vision_ai_appl_rcfac_get_source_count(), false);
        if (configured.code_ != status_code::ok) {
            return configured;
        }
        auto* candidate_wiring =
            candidate_features->vqec_vision_ai_appl_svfac_get_wiring();
        const auto valid_wiring =
            bundle_->vqec_vision_ai_appl_rcfac_validate_feature_rebind(
                candidate_wiring);
        if (valid_wiring.code_ != status_code::ok) {
            return valid_wiring;
        }
        const auto applied_policy = output_gate_->vqec_vision_ai_core_otgat_apply_policy(
            candidate_features->vqec_vision_ai_appl_svfac_get_output_policy(),
            output_gate_->vqec_vision_ai_core_otgat_get_revision());
        if (applied_policy.code_ != status_code::ok) {
            return applied_policy;
        }
        const auto adopted =
            candidate_features->vqec_vision_ai_appl_svfac_adopt_compatible_owners(
                **feature_owner_);
        if (adopted.code_ != status_code::ok) {
            output_gate_->vqec_vision_ai_core_otgat_invalidate();
            return {status_code::invalid_state,
                "feature owner adoption failed after authority publication"};
        }
        candidate_wiring =
            candidate_features->vqec_vision_ai_appl_svfac_get_wiring();
        const auto rebound = bundle_->vqec_vision_ai_appl_rcfac_rebind_features(
            candidate_wiring);
        if (rebound.code_ != status_code::ok) {
            output_gate_->vqec_vision_ai_core_otgat_invalidate();
            return {status_code::invalid_state,
                "prevalidated feature rebind failed after authority publication"};
        }
        if (cascade_owners_ != nullptr) {
            const auto cascades_requested =
                vqec_vision_ai_appl_svcsc_request_activation(
                    *cascade_owners_,
                    candidate_startup.activation_plan,
                    *bundle_->vqec_vision_ai_appl_rcfac_get_executor(),
                    _steady_now_ns);
            if (cascades_requested.code_ != status_code::ok &&
                cascades_requested.code_ != status_code::pending) {
                output_gate_->vqec_vision_ai_core_otgat_invalidate();
                return {status_code::invalid_state,
                    "prevalidated cascade delta failed after authority publication"};
            }
        }
        for (std::uint16_t source_slot = 0;
             source_slot < candidate_startup.activation_plan.source_count_;
             ++source_slot) {
            auto* session = bundle_->vqec_vision_ai_appl_rcfac_get_session(source_slot);
            const auto requested = session->vqec_vision_ai_appl_mmses_request_model_mask(
                candidate_startup.activation_plan.sources_[source_slot].active_model_mask_,
                _steady_now_ns);
            if (requested.code_ != status_code::ok &&
                requested.code_ != status_code::pending) {
                output_gate_->vqec_vision_ai_core_otgat_invalidate();
                return {status_code::invalid_state,
                    "prevalidated model delta request failed after authority publication"};
            }
        }

        *feature_owner_ = std::move(candidate_features);
        pending_startup_ = std::make_unique<service_startup_resolution>(
            std::move(candidate_startup));
        std::printf("activation delta accepted snapshot=%llu model_changes=%zu "
                    "cascade_changes=%zu feature_changes=%zu\n",
            static_cast<unsigned long long>(
                pending_startup_->runtime_control.snapshot_revision_),
            delta.model_dependencies_.size(), delta.cascade_dependencies_.size(),
            delta.feature_instances_.size());
        return vqec_vision_ai_appl_svacr_complete_pending();
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted,
            "service activation candidate allocation failed"};
    }
}

std::uint64_t service_activation_reconciler::
vqec_vision_ai_appl_svacr_get_applied_revision() const noexcept {
    return startup_ == nullptr ? 0 : startup_->runtime_control.snapshot_revision_;
}

}  // namespace vqec::vision::ai
