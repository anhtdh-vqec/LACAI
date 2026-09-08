#include "vqec_vision_encoded_dispatch.hpp"

#include <utility>

namespace vqec::vision::ai {

status vqec_vision_ai_outpt_encdp_poll_event(
    encoder_backend& _backend, encoder_window& _window, encoder_event& _event) {
    if (_event.output_ || _event.token_.cycle_id_ != 0 || _event.token_.job_id_ != 0 ||
        _event.kind_ != encoder_event_kind::fault || _event.detail_.code_ != status_code::ok ||
        !_event.detail_.message_.empty()) {
        return {status_code::invalid_state, "encoder event destination must be reset"};
    }
    try {
        encoder_event pending;
        const auto polled = _backend.vqec_vision_ai_cntr_encbk_poll(pending);
        if (polled.code_ != status_code::ok) {
            if (polled.code_ != status_code::pending) {
                _window.vqec_vision_ai_core_encwn_begin_drain();
            }
            return polled;
        }
        const auto valid = _window.vqec_vision_ai_core_encwn_validate_event(pending);
        if (valid.code_ != status_code::ok) {
            _window.vqec_vision_ai_core_encwn_begin_drain();
            return valid; // Invalid output is discarded, never delivered or completed.
        }
        _event = std::move(pending);
        return {};
    } catch (...) {
        _window.vqec_vision_ai_core_encwn_begin_drain();
        throw;
    }
}

status vqec_vision_ai_outpt_encdp_handle_event(
    const encoder_event& _event, encoder_window& _window,
    const encoded_dispatch_context& _context, std::uint64_t _now_ns,
    output_gate& _gate, encoded_sink& _sink, status& _delivery) {
    const auto valid = _window.vqec_vision_ai_core_encwn_validate_event(_event);
    if (valid.code_ != status_code::ok) {
        // Revalidation latches admission fault; malformed events cannot release accounting.
        return _window.vqec_vision_ai_core_encwn_apply_event(_event);
    }
    if (_event.kind_ == encoder_event_kind::output_ready) {
        try {
            _delivery = vqec_vision_ai_outpt_encdp_dispatch(
                *_event.output_, _context, _now_ns, _gate, _sink);
        } catch (...) {
            _window.vqec_vision_ai_core_encwn_begin_drain();
            throw; // Ambiguous write: no retry or result completion is inferred.
        }
        // Preview policy: terminal discard on denied/stale/no-consumer/write-error outcomes.
        // This acknowledges result handling, not network delivery or input completion.
    }
    return _window.vqec_vision_ai_core_encwn_apply_event(_event);
}

namespace {

status vqec_vision_ai_outpt_encdp_authorize_scopes(
    const encoded_dispatch_context& _context, std::uint64_t _now_ns, output_gate& _gate) {
    if (_context.rendered_scopes_.empty() ||
        _context.rendered_scopes_.size() > output_policy_limits::g_max_rendered_scopes ||
        _context.expected_source_id_.empty() ||
        _context.expected_source_id_.size() > output_policy_limits::g_max_identifier_bytes) {
        return {status_code::unauthorized, "missing or excessive rendered scope declaration"};
    }
    const auto revision = _context.rendered_scopes_.front().policy_revision_;
    for (const auto& scope : _context.rendered_scopes_) {
        if (revision == 0 || scope.policy_revision_ != revision ||
            scope.source_id_ != _context.expected_source_id_) {
            return {status_code::unauthorized, "rendered scope source/revision mismatch"};
        }
        const auto result = _gate.vqec_vision_ai_core_otgat_authorize(scope, _now_ns);
        if (result.code_ != status_code::ok) {
            return result;
        }
    }
    return {};
}

}  // namespace

status vqec_vision_ai_outpt_encdp_dispatch(
    const owned_h264_output& _output, const encoded_dispatch_context& _context,
    std::uint64_t _now_ns, output_gate& _gate, encoded_sink& _sink) {
    const auto view = _output.vqec_vision_ai_core_encot_borrow_view();
    const auto valid = vqec_vision_ai_core_pvctr_validate_access_unit(
        view, _context.expected_frame_, _context.expected_geometry_);
    if (valid.code_ != status_code::ok) {
        return valid;
    }
    if (_context.mapping_generation_ == 0 || _context.max_age_ns_ == 0 ||
        _now_ns == UINT64_MAX || _context.created_monotonic_ns_ > _now_ns) {
        return {status_code::invalid_argument, "invalid encoded dispatch generation/time"};
    }
    if (_now_ns - _context.created_monotonic_ns_ > _context.max_age_ns_) {
        return {status_code::timeout, "encoded output expired before dispatch"};
    }
    const auto authorized = vqec_vision_ai_outpt_encdp_authorize_scopes(_context, _now_ns, _gate);
    if (authorized.code_ != status_code::ok) {
        return authorized;
    }
    encoded_sink_demand demand;
    const auto queried = _sink.vqec_vision_ai_cntr_encsk_query_demand(demand);
    if (queried.code_ != status_code::ok) {
        return queried;
    }
    if (demand.mapping_generation_ != _context.mapping_generation_) {
        return {status_code::invalid_state, "encoded sink mapping changed"};
    }
    if (demand.active_consumers_ == 0) {
        return {status_code::pending, "no encoded preview consumer"};
    }
    const auto rechecked = vqec_vision_ai_outpt_encdp_authorize_scopes(_context, _now_ns, _gate);
    if (rechecked.code_ != status_code::ok) {
        return rechecked;
    }
    return _sink.vqec_vision_ai_cntr_encsk_write(view, _context.mapping_generation_);
}

}  // namespace vqec::vision::ai
