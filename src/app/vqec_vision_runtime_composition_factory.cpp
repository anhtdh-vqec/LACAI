#include "vqec_vision_runtime_composition_factory.hpp"

#include <cstddef>
#include <limits>
#include <new>
#include <utility>

namespace vqec::vision::ai {
namespace {

constexpr std::size_t g_max_runtime_graphs =
    deployment_limits::g_max_sources * deployment_limits::g_max_models_per_source;

status vqec_vision_ai_appl_rcfac_validate_policy(
    const runtime_composition_activation& _activation) {
    if (_activation.source_count_ == 0 ||
        _activation.source_count_ > deployment_limits::g_max_sources ||
        _activation.startup_timeout_ns_ == 0 ||
        _activation.startup_timeout_ns_ == std::numeric_limits<std::uint64_t>::max() ||
        _activation.stop_timeout_ns_ == 0 ||
        _activation.stop_timeout_ns_ == std::numeric_limits<std::uint64_t>::max() ||
        _activation.rpc_timeout_ms_ < 1 || _activation.rpc_timeout_ms_ > 60000) {
        return {status_code::invalid_argument,
            "invalid runtime source count or lifecycle policy"};
    }
    return {};
}

status vqec_vision_ai_appl_rcfac_register_graph_identity(
    inference_graph_port& _graph, std::uint64_t _cycle_id,
    std::array<inference_graph_port*, g_max_runtime_graphs>& _graphs,
    std::array<std::uint64_t, g_max_runtime_graphs>& _cycles,
    std::size_t& _registered) {
    if (_cycle_id == 0 || _cycle_id == std::numeric_limits<std::uint64_t>::max() ||
        _registered >= _graphs.size()) {
        return {status_code::invalid_argument,
            "invalid runtime graph cycle identity"};
    }
    for (std::size_t index = 0; index < _registered; ++index) {
        if (_graphs[index] == &_graph || _cycles[index] == _cycle_id) {
            return {status_code::invalid_argument,
                "runtime graph port or cycle identity is duplicated"};
        }
    }
    _graphs[_registered] = &_graph;
    _cycles[_registered] = _cycle_id;
    ++_registered;
    return {};
}

status vqec_vision_ai_appl_rcfac_compose_model(
    const source_deployment_config& _source, const model_catalog_entry& _model,
    const runtime_model_activation& _activation,
    std::array<inference_graph_port*, g_max_runtime_graphs>& _graphs,
    std::array<std::uint64_t, g_max_runtime_graphs>& _cycles,
    std::size_t& _registered, multi_model_graph_config& _graph_config,
    model_cadence_config& _cadence, std::uint16_t _model_slot,
    perception_model_activation& _perception) {
    if (_activation.model_id_ != _model.model_id_ || _activation.graph_ == nullptr ||
        _activation.job_timeout_ns_ == 0 ||
        _activation.job_timeout_ns_ == std::numeric_limits<std::uint64_t>::max()) {
        return {status_code::invalid_argument,
            "runtime model activation does not match its catalog slot"};
    }
    const auto identity = vqec_vision_ai_appl_rcfac_register_graph_identity(
        *_activation.graph_, _activation.cycle_id_, _graphs, _cycles, _registered);
    if (identity.code_ != status_code::ok) {
        return identity;
    }
    if (_activation.graph_->vqec_vision_ai_ports_infgr_get_state() !=
        inference_graph_state::empty) {
        return {status_code::invalid_state,
            "runtime composition requires every graph to be empty"};
    }
    const auto graph_activation =
        _activation.graph_->vqec_vision_ai_ports_infgr_validate_activation();
    if (graph_activation.code_ != status_code::ok) {
        return graph_activation;
    }

    inference_plan plan;
    const auto composed_plan = vqec_vision_ai_core_mdcat_compose_inference_plan(
        _source, _model, _activation.paths_, plan);
    if (composed_plan.code_ != status_code::ok) {
        return composed_plan;
    }
    if (_activation.binding_.preprocess_contract_ != _model.preprocess_contract_) {
        return {status_code::invalid_argument,
            "source binding preprocess contract differs from model catalog"};
    }
    const auto valid_binding = vqec_vision_ai_core_srcbd_validate_binding(
        _activation.binding_, plan);
    if (valid_binding.code_ != status_code::ok) {
        return valid_binding;
    }
    std::uint64_t required_output_bytes = 0;
    const auto valid_outputs = vqec_vision_ai_core_mdcat_validate_model_outputs(
        _model, _activation.resolved_output_manifest_ref_,
        _activation.outputs_, required_output_bytes);
    if (valid_outputs.code_ != status_code::ok) {
        return valid_outputs;
    }

    _graph_config.graph_ = _activation.graph_;
    _graph_config.plan_ = std::move(plan);
    _graph_config.binding_ = _activation.binding_;
    _graph_config.outputs_ = _activation.outputs_.outputs_;
    _graph_config.max_output_bytes_ = _activation.outputs_.max_output_bytes_;
    _graph_config.cycle_id_ = _activation.cycle_id_;
    _graph_config.job_timeout_ns_ = _activation.job_timeout_ns_;
    _cadence.model_fps_numerators_[_model_slot] =
        _model.inference_fps_numerator_;
    _cadence.model_fps_denominators_[_model_slot] =
        _model.inference_fps_denominator_;
    _perception.model_id_ = _model.model_id_;
    _perception.tracker_contract_ = _activation.tracker_contract_;
    _perception.resolved_output_manifest_ref_ =
        _activation.resolved_output_manifest_ref_;
    _perception.outputs_ = _activation.outputs_;
    return {};
}

}  // namespace

application_composition* runtime_composition_bundle::
vqec_vision_ai_appl_rcfac_get_composition() noexcept {
    return composition_.get();
}

multi_model_session* runtime_composition_bundle::
vqec_vision_ai_appl_rcfac_get_session(std::uint16_t _source_slot) noexcept {
    return _source_slot < source_count_ ? sessions_[_source_slot].get() : nullptr;
}

source_perception_bundle* runtime_composition_bundle::
vqec_vision_ai_appl_rcfac_get_perception(std::uint16_t _source_slot) noexcept {
    return _source_slot < source_count_ ? perceptions_[_source_slot].get() : nullptr;
}

const activation_snapshot& runtime_composition_bundle::
vqec_vision_ai_appl_rcfac_get_admission() const noexcept {
    return admission_;
}

std::uint16_t runtime_composition_bundle::
vqec_vision_ai_appl_rcfac_get_source_count() const noexcept {
    return source_count_;
}

status vqec_vision_ai_appl_rcfac_create_bundle(
    const deployment_config& _deployment, const model_catalog& _catalog,
    const runtime_composition_activation& _activation,
    const model_decoder_registry& _decoder_registry,
    const tracker_registry& _tracker_registry,
    std::unique_ptr<runtime_composition_bundle>& _bundle) {
    try {
        const auto valid_policy =
            vqec_vision_ai_appl_rcfac_validate_policy(_activation);
        if (valid_policy.code_ != status_code::ok) {
            return valid_policy;
        }

        activation_snapshot admission;
        const auto admitted = vqec_vision_ai_admis_actsp_build_snapshot(
            _deployment, _catalog, admission);
        if (admitted.code_ != status_code::ok) {
            return admitted;
        }
        if (_activation.source_count_ != admission.source_count_ ||
            _activation.source_count_ != _deployment.sources_.size()) {
            return {status_code::invalid_argument,
                "runtime activation source count differs from admission"};
        }

        std::array<multi_model_session_config,
            deployment_limits::g_max_sources> session_configs{};
        std::array<std::array<perception_model_activation,
            deployment_limits::g_max_models_per_source>,
            deployment_limits::g_max_sources> perception_activations{};
        std::array<inference_graph_port*, g_max_runtime_graphs> graphs{};
        std::array<std::uint64_t, g_max_runtime_graphs> cycles{};
        std::size_t registered_graphs = 0;

        for (std::uint16_t source_slot = 0;
             source_slot < _activation.source_count_; ++source_slot) {
            const auto& source = _deployment.sources_[source_slot];
            const auto& admitted_source = admission.sources_[source_slot];
            const auto& source_activation = _activation.sources_[source_slot];
            if (admitted_source.deployment_index_ != source_slot ||
                source_activation.source_id_ != source.source_id_ ||
                source_activation.source_ == nullptr ||
                source_activation.model_count_ != admitted_source.model_count_ ||
                source_activation.model_count_ != source.model_ids_.size()) {
                return {status_code::invalid_argument,
                    "runtime source activation differs from deployment slot"};
            }
            if (source_activation.source_->vqec_vision_ai_ports_rawsr_get_state() !=
                raw_source_state::idle) {
                return {status_code::invalid_state,
                    "runtime composition requires every RAW source to be idle"};
            }

            auto& session_config = session_configs[source_slot];
            session_config.graph_count_ = source_activation.model_count_;
            session_config.cadence_.source_fps_numerator_ =
                source.profile_.fps_numerator_;
            session_config.cadence_.source_fps_denominator_ =
                source.profile_.fps_denominator_;
            session_config.cadence_.model_count_ = source_activation.model_count_;
            session_config.startup_timeout_ns_ = _activation.startup_timeout_ns_;
            session_config.stop_timeout_ns_ = _activation.stop_timeout_ns_;
            session_config.rpc_timeout_ms_ = _activation.rpc_timeout_ms_;

            for (std::uint16_t model_slot = 0;
                 model_slot < source_activation.model_count_; ++model_slot) {
                const auto catalog_index =
                    admitted_source.catalog_model_indices_[model_slot];
                if (catalog_index >= _catalog.models_.size() ||
                    _catalog.models_[catalog_index].model_id_ !=
                        source.model_ids_[model_slot]) {
                    return {status_code::invalid_argument,
                        "admitted model index differs from immutable catalog"};
                }
                const auto composed = vqec_vision_ai_appl_rcfac_compose_model(
                    source, _catalog.models_[catalog_index],
                    source_activation.models_[model_slot], graphs, cycles,
                    registered_graphs, session_config.graphs_[model_slot],
                    session_config.cadence_, model_slot,
                    perception_activations[source_slot][model_slot]);
                if (composed.code_ != status_code::ok) {
                    return composed;
                }
            }
        }

        auto candidate = std::unique_ptr<runtime_composition_bundle>(
            new runtime_composition_bundle());
        candidate->admission_ = admission;
        candidate->source_count_ = _activation.source_count_;
        for (std::uint16_t source_slot = 0;
             source_slot < _activation.source_count_; ++source_slot) {
            const auto& source = _deployment.sources_[source_slot];
            const auto perception =
                vqec_vision_ai_appl_spfac_create_source_bundle(
                    _catalog, source, perception_activations[source_slot],
                    session_configs[source_slot].graph_count_, _decoder_registry,
                    _tracker_registry, candidate->perceptions_[source_slot]);
            if (perception.code_ != status_code::ok) {
                return perception;
            }
            candidate->sessions_[source_slot] = std::make_unique<multi_model_session>(
                *_activation.sources_[source_slot].source_,
                std::move(session_configs[source_slot]));
        }

        candidate->composition_ = std::make_unique<application_composition>(
            _deployment.revision_, _catalog.revision_, _activation.source_count_);
        for (std::uint16_t source_slot = 0;
             source_slot < _activation.source_count_; ++source_slot) {
            const auto bound = candidate->composition_->
                vqec_vision_ai_appl_acomp_bind_session(
                    source_slot, *candidate->sessions_[source_slot]);
            if (bound.code_ != status_code::ok) {
                return bound;
            }
        }
        const auto validated =
            candidate->composition_->vqec_vision_ai_cntr_acomp_validate();
        if (validated.code_ != status_code::ok) {
            return validated;
        }
        _bundle = std::move(candidate);
        return {};
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted,
            "runtime composition allocation failed"};
    } catch (...) {
        return {status_code::io_error,
            "runtime composition raised an exception"};
    }
}

}  // namespace vqec::vision::ai
