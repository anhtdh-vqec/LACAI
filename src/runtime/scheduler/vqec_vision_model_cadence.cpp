#include "vqec_vision_model_cadence.hpp"

#include <limits>

namespace vqec::vision::ai {
namespace {

const model_catalog_entry* vqec_vision_ai_sched_mdcad_find_model(
    const model_catalog& _catalog, const std::string& _model_id) noexcept {
    for (const auto& model : _catalog.models_) {
        if (model.model_id_ == _model_id) {
            return &model;
        }
    }
    return nullptr;
}

bool vqec_vision_ai_sched_mdcad_multiply(
    std::uint64_t _left, std::uint64_t _right, std::uint64_t& _product) noexcept {
    if (_right != 0 && _left > std::numeric_limits<std::uint64_t>::max() / _right) {
        return false;
    }
    _product = _left * _right;
    return true;
}

}  // namespace

status vqec_vision_ai_sched_mdcad_compose_config(
    const source_deployment_config& _source, const model_catalog& _catalog,
    model_cadence_config& _config) {
    if (_source.model_ids_.empty() ||
        _source.model_ids_.size() > deployment_limits::g_max_models_per_source) {
        return {status_code::invalid_argument, "invalid source model assignment count"};
    }
    model_cadence_config candidate;
    candidate.source_fps_numerator_ = _source.profile_.fps_numerator_;
    candidate.source_fps_denominator_ = _source.profile_.fps_denominator_;
    candidate.model_count_ = static_cast<std::uint16_t>(_source.model_ids_.size());
    for (std::uint16_t slot = 0; slot < candidate.model_count_; ++slot) {
        const auto* model = vqec_vision_ai_sched_mdcad_find_model(
            _catalog, _source.model_ids_[slot]);
        if (model == nullptr) {
            return {status_code::invalid_argument, "cadence references unknown model"};
        }
        candidate.model_fps_numerators_[slot] = model->inference_fps_numerator_;
        candidate.model_fps_denominators_[slot] = model->inference_fps_denominator_;
    }
    model_cadence_scheduler validator;
    const auto valid = validator.vqec_vision_ai_sched_mdcad_configure(candidate);
    if (valid.code_ != status_code::ok) {
        return valid;
    }
    _config = candidate;
    return {};
}

bool vqec_vision_ai_sched_mdcad_is_model_due(
    const model_cadence_selection& _selection, std::uint16_t _model_slot) noexcept {
    return _model_slot < _selection.model_count_ &&
        (_selection.due_model_mask_ & (1U << _model_slot)) != 0;
}

status model_cadence_scheduler::vqec_vision_ai_sched_mdcad_configure(
    const model_cadence_config& _config) {
    if (_config.source_fps_numerator_ == 0 || _config.source_fps_denominator_ == 0 ||
        _config.model_count_ == 0 ||
        _config.model_count_ > deployment_limits::g_max_models_per_source) {
        return {status_code::invalid_argument, "invalid source cadence configuration"};
    }
    std::array<std::uint64_t, deployment_limits::g_max_models_per_source> increments{};
    std::array<std::uint64_t, deployment_limits::g_max_models_per_source> thresholds{};
    for (std::uint16_t slot = 0; slot < _config.model_count_; ++slot) {
        const auto model_numerator = _config.model_fps_numerators_[slot];
        const auto model_denominator = _config.model_fps_denominators_[slot];
        if (model_numerator == 0 || model_denominator == 0 ||
            !vqec_vision_ai_sched_mdcad_multiply(
                model_numerator, _config.source_fps_denominator_, increments[slot]) ||
            !vqec_vision_ai_sched_mdcad_multiply(
                _config.source_fps_numerator_, model_denominator, thresholds[slot]) ||
            increments[slot] > thresholds[slot]) {
            return {status_code::invalid_argument,
                    "model cadence is invalid or exceeds source rate"};
        }
    }
    increments_ = increments;
    thresholds_ = thresholds;
    phases_ = {};
    last_frame_sequence_ = 0;
    model_count_ = _config.model_count_;
    is_configured_ = true;
    return {};
}

status model_cadence_scheduler::vqec_vision_ai_sched_mdcad_select(
    std::uint64_t _frame_sequence, model_cadence_selection& _selection) {
    if (!is_configured_) {
        return {status_code::invalid_state, "cadence scheduler is not ready"};
    }
    if (_frame_sequence == 0 ||
        _frame_sequence == std::numeric_limits<std::uint64_t>::max()) {
        return {status_code::invalid_argument, "frame sequence is invalid"};
    }
    if (last_frame_sequence_ == 0) {
        model_cadence_selection candidate;
        candidate.frame_sequence_ = _frame_sequence;
        candidate.model_count_ = model_count_;
        candidate.due_model_mask_ = static_cast<std::uint16_t>(
            (1U << model_count_) - 1U);
        last_frame_sequence_ = _frame_sequence;
        _selection = candidate;
        return {};
    }
    if (_frame_sequence <= last_frame_sequence_) {
        return {status_code::invalid_argument, "frame sequence must increase"};
    }

    const auto frame_delta = _frame_sequence - last_frame_sequence_;
    auto phases = phases_;
    model_cadence_selection candidate;
    candidate.frame_sequence_ = _frame_sequence;
    candidate.model_count_ = model_count_;
    for (std::uint16_t slot = 0; slot < model_count_; ++slot) {
        std::uint64_t added_phase = 0;
        if (!vqec_vision_ai_sched_mdcad_multiply(
                frame_delta, increments_[slot], added_phase) ||
            added_phase > std::numeric_limits<std::uint64_t>::max() - phases[slot]) {
            return {status_code::resource_exhausted, "cadence phase arithmetic overflow"};
        }
        phases[slot] += added_phase;
        const auto elapsed_intervals = phases[slot] / thresholds_[slot];
        phases[slot] %= thresholds_[slot];
        if (elapsed_intervals == 0) {
            continue;
        }
        candidate.due_model_mask_ = static_cast<std::uint16_t>(
            candidate.due_model_mask_ | (1U << slot));
        const auto skipped = elapsed_intervals - 1;
        if (skipped > std::numeric_limits<std::uint64_t>::max() -
                candidate.skipped_intervals_) {
            return {status_code::resource_exhausted, "cadence skip counter overflow"};
        }
        candidate.skipped_intervals_ += skipped;
    }
    phases_ = phases;
    last_frame_sequence_ = _frame_sequence;
    _selection = candidate;
    return {};
}

bool model_cadence_scheduler::vqec_vision_ai_sched_mdcad_is_configured()
    const noexcept {
    return is_configured_;
}

}  // namespace vqec::vision::ai
