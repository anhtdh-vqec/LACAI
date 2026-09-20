#include "vqec_vision_service_cascade_runtime.hpp"

#include <algorithm>
#include <chrono>
#include <thread>
#include <utility>

#include "vqec_vision_service_fixture.hpp"
#include "vqec_vision_service_options.hpp"
#include "vqec_vision_runtime_executor.hpp"

namespace vqec::vision::ai {
namespace {

std::uint64_t vqec_vision_ai_appl_svcsc_monotonic_ns() noexcept {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

void vqec_vision_ai_appl_svcsc_wait_step() {
    std::this_thread::sleep_for(std::chrono::nanoseconds(
        service_options_limits::g_default_runtime_step_interval_ns));
}

const cascade_dependency_reference* vqec_vision_ai_appl_svcsc_find_dependency(
    const app_activation_plan& _plan, std::uint16_t _source_slot) noexcept {
    const auto found = std::find_if(_plan.cascade_dependencies_.begin(),
        _plan.cascade_dependencies_.end(), [_source_slot](const auto& _dependency) {
            return _dependency.source_slot_ == _source_slot;
        });
    return found == _plan.cascade_dependencies_.end() ? nullptr : &*found;
}

status vqec_vision_ai_appl_svcsc_validate_owner_dependency(
    const service_cascade_owner& _owner,
    const cascade_dependency_reference* _dependency) noexcept {
    if (_dependency == nullptr) {
        return {};
    }
    if (_owner.graph_session_ == nullptr || _owner.model_ == nullptr ||
        _owner.model_->model_id_ != _dependency->model_id_ ||
        _owner.model_->model_version_ != _dependency->model_version_ ||
        _owner.model_->target_id_ != _dependency->target_id_ ||
        _owner.root_model_slot_ != _dependency->root_model_slot_) {
        return {status_code::invalid_state,
            "cascade activation dependency has no matching prepared owner"};
    }
    return {};
}

}  // namespace

status vqec_vision_ai_appl_svcsc_start_graph_sessions(
    const std::array<cascade_graph_session*, 2>& _sessions) {
    bool all_running = false;
    while (!all_running) {
        all_running = true;
        const auto now_ns = vqec_vision_ai_appl_svcsc_monotonic_ns();
        for (auto* session : _sessions) {
            if (session == nullptr ||
                session->vqec_vision_ai_appl_cgses_get_state() ==
                    cascade_graph_session_state::running) {
                continue;
            }
            all_running = false;
            const auto stepped = session->vqec_vision_ai_appl_cgses_step(now_ns);
            if (stepped.code_ != status_code::ok &&
                stepped.code_ != status_code::pending) {
                return stepped;
            }
        }
        if (!all_running) {
            vqec_vision_ai_appl_svcsc_wait_step();
        }
    }
    return {};
}

status vqec_vision_ai_appl_svcsc_stop_graph_sessions(
    const std::array<cascade_graph_session*, 2>& _sessions) {
    status first_error;
    auto now_ns = vqec_vision_ai_appl_svcsc_monotonic_ns();
    for (auto* session : _sessions) {
        if (session == nullptr) {
            continue;
        }
        const auto requested =
            session->vqec_vision_ai_appl_cgses_request_stop(now_ns);
        if (requested.code_ != status_code::ok &&
            first_error.code_ == status_code::ok) {
            first_error = requested;
        }
    }
    bool all_stopped = false;
    while (!all_stopped) {
        all_stopped = true;
        now_ns = vqec_vision_ai_appl_svcsc_monotonic_ns();
        for (auto* session : _sessions) {
            if (session == nullptr) {
                continue;
            }
            const auto state = session->vqec_vision_ai_appl_cgses_get_state();
            if (state == cascade_graph_session_state::stopped ||
                state == cascade_graph_session_state::faulted) {
                continue;
            }
            all_stopped = false;
            const auto stepped = session->vqec_vision_ai_appl_cgses_step(now_ns);
            if (stepped.code_ != status_code::ok &&
                stepped.code_ != status_code::pending &&
                first_error.code_ == status_code::ok) {
                first_error = stepped;
            }
        }
        if (!all_stopped) {
            vqec_vision_ai_appl_svcsc_wait_step();
        }
    }
    return first_error;
}

status vqec_vision_ai_appl_svcsc_make_source_binding(
    const source_deployment_config& _source, const model_catalog_entry& _model,
    source_binding& _binding) {
    source_color_profile color_profile{source_color_profile::unspecified};
    if (_model.preprocess_.matrix_ == color_matrix::bt601 &&
        _model.preprocess_.range_ == color_range::limited) {
        color_profile = source_color_profile::bt601_limited;
    } else if (_model.preprocess_.matrix_ == color_matrix::bt709 &&
               _model.preprocess_.range_ == color_range::limited) {
        color_profile = source_color_profile::bt709_limited;
    } else {
        return {status_code::unsupported,
            "source binding requires a supported limited-range color profile"};
    }
    source_binding candidate;
    candidate.width_ = _source.profile_.width_;
    candidate.height_ = _source.profile_.height_;
    candidate.fps_numerator_ = _source.profile_.fps_numerator_;
    candidate.fps_denominator_ = _source.profile_.fps_denominator_;
    candidate.memory_kind_ = source_memory_kind::dmabuf;
    candidate.layout_ = source_memory_layout::linear_nv12;
    candidate.sync_mode_ = source_sync_mode::implicit_ready;
    candidate.color_profile_ = color_profile;
    candidate.chroma_site_ = source_chroma_site::mpeg2;
    candidate.fw_memory_contract_ = service_harness::g_fw_dmabuf_contract;
    candidate.backend_memory_contract_ = service_harness::g_qcom_dmabuf_contract;
    candidate.preprocess_contract_ = _model.preprocess_contract_;
    _binding = std::move(candidate);
    return {};
}

status vqec_vision_ai_appl_svcsc_make_offline_graph_session(
    const source_deployment_config& _source, const model_catalog_entry& _model,
    production_offline_model& _offline,
    std::unique_ptr<cascade_graph_session>& _session) {
    const auto& binding = _offline.vqec_vision_ai_appl_pdplt_get_binding();
    cascade_graph_session_config config;
    config.graph_ = _offline.vqec_vision_ai_appl_pdplt_get_graph();
    config.plan_ = binding.plan_;
    config.outputs_ = binding.outputs_;
    config.max_output_bytes_ = binding.max_output_bytes_;
    config.startup_timeout_ns_ = service_harness::g_default_startup_timeout_ns;
    config.stop_timeout_ns_ = service_harness::g_default_stop_timeout_ns;
    const auto bound = vqec_vision_ai_appl_svcsc_make_source_binding(
        _source, _model, config.binding_);
    if (bound.code_ != status_code::ok) {
        return bound;
    }
    if (config.graph_ == nullptr || config.plan_.model_path_.empty() ||
        config.outputs_.empty() || config.max_output_bytes_ == 0) {
        return {status_code::invalid_state, "offline graph binding is incomplete"};
    }
    _session = std::make_unique<cascade_graph_session>(std::move(config));
    return {};
}

status vqec_vision_ai_appl_svcsc_prepare_owners(
    const deployment_config& _deployment, const model_catalog& _catalog,
    production_platform& _platform,
    std::array<service_cascade_owner, deployment_limits::g_max_sources>& _owners) {
    for (std::uint16_t source_slot = 0; source_slot < _deployment.sources_.size();
         ++source_slot) {
        const auto& source = _deployment.sources_[source_slot];
        const model_catalog_entry* secondary = nullptr;
        for (const auto& model : _catalog.models_) {
            if (model.role_ != model_role::secondary ||
                !vqec_vision_ai_core_mdcat_source_activates_model(source, model)) {
                continue;
            }
            if (secondary != nullptr) {
                return {status_code::unsupported,
                    "one source currently supports one active secondary model"};
            }
            secondary = &model;
        }
        if (secondary == nullptr) {
            continue;
        }
        if (secondary->depends_on_.size() != 1U) {
            return {status_code::unsupported,
                "cascade coordinator currently requires one primary dependency"};
        }
        std::uint16_t root_slot = g_invalid_model_slot;
        for (std::uint16_t model_slot = 0; model_slot < source.model_ids_.size();
             ++model_slot) {
            if (source.model_ids_[model_slot] == secondary->depends_on_[0].model_id_) {
                root_slot = model_slot;
                break;
            }
        }
        if (root_slot == g_invalid_model_slot) {
            return {status_code::invalid_argument,
                "secondary dependency has no primary source slot"};
        }
        production_cascade_binding binding;
        const auto resolved = _platform.vqec_vision_ai_appl_pdplt_cascade_binding(
            source_slot, secondary->model_id_, binding);
        if (resolved.code_ != status_code::ok) {
            return resolved;
        }
        cascade_graph_session_config graph_config;
        graph_config.graph_ = binding.graph_;
        graph_config.plan_ = binding.plan_;
        graph_config.outputs_ = binding.outputs_;
        graph_config.max_output_bytes_ = binding.max_output_bytes_;
        graph_config.startup_timeout_ns_ =
            service_harness::g_default_startup_timeout_ns;
        graph_config.stop_timeout_ns_ = service_harness::g_default_stop_timeout_ns;
        const auto source_bound = vqec_vision_ai_appl_svcsc_make_source_binding(
            source, *secondary, graph_config.binding_);
        if (source_bound.code_ != status_code::ok) {
            return source_bound;
        }
        auto& owner = _owners[source_slot];
        owner.binding_ = std::move(binding);
        owner.model_ = secondary;
        owner.root_model_slot_ = root_slot;
        owner.graph_session_ =
            std::make_unique<cascade_graph_session>(std::move(graph_config));
    }
    return {};
}

status vqec_vision_ai_appl_svcsc_start_ready_graphs(
    std::array<service_cascade_owner, deployment_limits::g_max_sources>& _owners,
    const std::array<bool, deployment_limits::g_max_sources>& _source_ready) {
    bool all_ready_graphs_reconciled = false;
    while (!all_ready_graphs_reconciled) {
        all_ready_graphs_reconciled = true;
        const auto now_ns = vqec_vision_ai_appl_svcsc_monotonic_ns();
        for (std::size_t slot = 0; slot < _owners.size(); ++slot) {
            auto& owner = _owners[slot];
            if (owner.graph_session_ == nullptr) {
                continue;
            }
            const auto state = owner.graph_session_->
                vqec_vision_ai_appl_cgses_get_state();
            const auto target = owner.desired_active_
                ? cascade_graph_session_state::running
                : cascade_graph_session_state::stopped;
            if (state == target) {
                owner.active_ = owner.desired_active_;
                continue;
            }
            if (owner.desired_active_ && !_source_ready[slot]) {
                continue;
            }
            all_ready_graphs_reconciled = false;
            const auto stepped =
                owner.graph_session_->vqec_vision_ai_appl_cgses_step(now_ns);
            if (stepped.code_ != status_code::ok &&
                stepped.code_ != status_code::pending) {
                return stepped;
            }
        }
        if (!all_ready_graphs_reconciled) {
            vqec_vision_ai_appl_svcsc_wait_step();
        }
    }
    return {};
}

status vqec_vision_ai_appl_svcsc_initialize_activation(
    std::array<service_cascade_owner, deployment_limits::g_max_sources>& _owners,
    const app_activation_plan& _plan, runtime_executor& _executor,
    std::uint64_t _steady_now_ns) {
    for (std::uint16_t source_slot = 0; source_slot < _plan.source_count_;
         ++source_slot) {
        auto& owner = _owners[source_slot];
        const auto* dependency = vqec_vision_ai_appl_svcsc_find_dependency(
            _plan, source_slot);
        const auto valid = vqec_vision_ai_appl_svcsc_validate_owner_dependency(
            owner, dependency);
        if (valid.code_ != status_code::ok) {
            return valid;
        }
        if (owner.graph_session_ == nullptr) {
            continue;
        }
        owner.desired_active_ = dependency != nullptr &&
            dependency->consumer_count_ != 0;
        owner.active_ = false;
        const auto gated = _executor.vqec_vision_ai_appl_rtexe_set_cascade_active(
            source_slot, false);
        if (gated.code_ != status_code::ok) {
            return gated;
        }
        const auto requested = owner.graph_session_->
            vqec_vision_ai_appl_cgses_request_active(
                owner.desired_active_, _steady_now_ns);
        if (requested.code_ != status_code::ok &&
            requested.code_ != status_code::pending) {
            return requested;
        }
    }
    return {};
}

status vqec_vision_ai_appl_svcsc_validate_activation(
    const std::array<service_cascade_owner, deployment_limits::g_max_sources>& _owners,
    const app_activation_plan& _plan, std::uint64_t _steady_now_ns) {
    for (std::uint16_t source_slot = 0; source_slot < _plan.source_count_;
         ++source_slot) {
        const auto& owner = _owners[source_slot];
        const auto* dependency = vqec_vision_ai_appl_svcsc_find_dependency(
            _plan, source_slot);
        const auto valid = vqec_vision_ai_appl_svcsc_validate_owner_dependency(
            owner, dependency);
        if (valid.code_ != status_code::ok) {
            return valid;
        }
        if (owner.graph_session_ == nullptr) {
            continue;
        }
        const bool desired = dependency != nullptr &&
            dependency->consumer_count_ != 0;
        const auto accepted = owner.graph_session_->
            vqec_vision_ai_appl_cgses_validate_active(desired, _steady_now_ns);
        if (accepted.code_ != status_code::ok) {
            return accepted;
        }
    }
    return {};
}

status vqec_vision_ai_appl_svcsc_request_activation(
    std::array<service_cascade_owner, deployment_limits::g_max_sources>& _owners,
    const app_activation_plan& _plan, runtime_executor& _executor,
    std::uint64_t _steady_now_ns) {
    for (std::uint16_t source_slot = 0; source_slot < _plan.source_count_;
         ++source_slot) {
        auto& owner = _owners[source_slot];
        if (owner.graph_session_ == nullptr) {
            continue;
        }
        const auto* dependency = vqec_vision_ai_appl_svcsc_find_dependency(
            _plan, source_slot);
        const bool desired = dependency != nullptr &&
            dependency->consumer_count_ != 0;
        if (desired == owner.desired_active_) {
            continue;
        }
        const auto gated = _executor.vqec_vision_ai_appl_rtexe_set_cascade_active(
            source_slot, false);
        if (gated.code_ != status_code::ok) {
            return gated;
        }
        if (!desired && owner.worker_ != nullptr) {
            const auto quiesced = owner.worker_->vqec_vision_ai_appl_cxwrk_quiescent_reset(
                _steady_now_ns, service_harness::g_default_stop_timeout_ns);
            if (quiesced.code_ != status_code::ok) {
                return quiesced;
            }
        }
        const auto requested = owner.graph_session_->
            vqec_vision_ai_appl_cgses_request_active(desired, _steady_now_ns);
        if (requested.code_ != status_code::ok &&
            requested.code_ != status_code::pending) {
            return requested;
        }
        owner.desired_active_ = desired;
        owner.active_ = false;
    }
    return {};
}

status vqec_vision_ai_appl_svcsc_sync_executor(
    std::array<service_cascade_owner, deployment_limits::g_max_sources>& _owners,
    runtime_executor& _executor) {
    for (std::uint16_t source_slot = 0; source_slot < _owners.size();
         ++source_slot) {
        auto& owner = _owners[source_slot];
        if (owner.graph_session_ == nullptr) {
            continue;
        }
        owner.active_ = owner.desired_active_ &&
            owner.graph_session_->vqec_vision_ai_appl_cgses_get_state() ==
                cascade_graph_session_state::running;
        const auto synced = _executor.vqec_vision_ai_appl_rtexe_set_cascade_active(
            source_slot, owner.active_);
        if (synced.code_ != status_code::ok) {
            return synced;
        }
    }
    return {};
}

bool vqec_vision_ai_appl_svcsc_is_activation_complete(
    const std::array<service_cascade_owner, deployment_limits::g_max_sources>& _owners)
    noexcept {
    for (const auto& owner : _owners) {
        if (owner.graph_session_ == nullptr) {
            continue;
        }
        const auto expected = owner.desired_active_
            ? cascade_graph_session_state::running
            : cascade_graph_session_state::stopped;
        if (owner.graph_session_->vqec_vision_ai_appl_cgses_get_state() != expected ||
            owner.active_ != owner.desired_active_) {
            return false;
        }
    }
    return true;
}

status vqec_vision_ai_appl_svcsc_stop_graphs(
    std::array<service_cascade_owner, deployment_limits::g_max_sources>& _owners) {
    auto now_ns = vqec_vision_ai_appl_svcsc_monotonic_ns();
    status first_error;
    for (auto& owner : _owners) {
        if (owner.worker_ == nullptr) {
            continue;
        }
        (void)owner.worker_->vqec_vision_ai_appl_cxwrk_request_stop(now_ns);
        const auto drained = owner.worker_->vqec_vision_ai_appl_cxwrk_drain_and_join(
            service_harness::g_default_stop_timeout_ns);
        if (drained.code_ != status_code::ok) {
            return drained;
        }
    }
    for (auto& owner : _owners) {
        if (owner.graph_session_ == nullptr) {
            continue;
        }
        const auto requested =
            owner.graph_session_->vqec_vision_ai_appl_cgses_request_stop(now_ns);
        if (requested.code_ != status_code::ok &&
            first_error.code_ == status_code::ok) {
            first_error = requested;
        }
    }
    bool all_stopped = false;
    while (!all_stopped) {
        all_stopped = true;
        now_ns = vqec_vision_ai_appl_svcsc_monotonic_ns();
        for (auto& owner : _owners) {
            if (owner.graph_session_ == nullptr) {
                continue;
            }
            const auto state =
                owner.graph_session_->vqec_vision_ai_appl_cgses_get_state();
            if (state == cascade_graph_session_state::stopped ||
                state == cascade_graph_session_state::faulted) {
                continue;
            }
            all_stopped = false;
            const auto stepped =
                owner.graph_session_->vqec_vision_ai_appl_cgses_step(now_ns);
            if (stepped.code_ != status_code::ok &&
                stepped.code_ != status_code::pending &&
                first_error.code_ == status_code::ok) {
                first_error = stepped;
            }
        }
        if (!all_stopped) {
            vqec_vision_ai_appl_svcsc_wait_step();
        }
    }
    return first_error;
}

status vqec_vision_ai_appl_svcsc_drain_workers(
    std::array<service_cascade_owner, deployment_limits::g_max_sources>& _owners,
    std::uint64_t _steady_now_ns) {
    status first_error;
    for (auto& owner : _owners) {
        if (owner.worker_ != nullptr) {
            (void)owner.worker_->vqec_vision_ai_appl_cxwrk_request_stop(_steady_now_ns);
        }
    }
    for (auto& owner : _owners) {
        if (owner.worker_ == nullptr) {
            continue;
        }
        const auto drained = owner.worker_->vqec_vision_ai_appl_cxwrk_drain_and_join(
            service_harness::g_default_stop_timeout_ns);
        if (drained.code_ != status_code::ok &&
            first_error.code_ == status_code::ok) {
            first_error = drained;
        }
    }
    return first_error;
}

}  // namespace vqec::vision::ai
