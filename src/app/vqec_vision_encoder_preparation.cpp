#include "vqec_vision_encoder_preparation.hpp"

#include <utility>

namespace vqec::vision::ai {

status vqec_vision_ai_appl_enprp_drain_backend(
    encoder_window& _window, encoder_backend& _backend) {
    _window.vqec_vision_ai_core_encwn_begin_drain();
    const auto drained = _backend.vqec_vision_ai_cntr_encbk_begin_drain();
    if (drained.code_ != status_code::ok) {
        return drained;
    }
    if (_window.vqec_vision_ai_core_encwn_outstanding() != 0) {
        return {status_code::pending, "backend drained; encoder ledger still requires reconciliation"};
    }
    return {};
}

status vqec_vision_ai_appl_enprp_submit_backend(
    encoder_window& _window, encoder_backend& _backend, const encoder_input& _input,
    std::uint64_t _expected_generation) {
    const auto admitted = _window.vqec_vision_ai_core_encwn_begin_input(_input, _expected_generation);
    if (admitted.code_ != status_code::ok) {
        return admitted; // Never reconcile a duplicate attempt as a new backend rejection.
    }
    try {
        const auto submitted = _backend.vqec_vision_ai_cntr_encbk_submit(_input);
        if (submitted.code_ == status_code::ok) {
            return submitted;
        }
        // Port guarantees this rejected attempt retained/accessed no input and emits no events.
        const auto input_done = _window.vqec_vision_ai_core_encwn_complete_input(_input.ticket_.token_);
        if (input_done.code_ != status_code::ok) {
            _window.vqec_vision_ai_core_encwn_begin_drain();
            return input_done;
        }
        const auto result_done = _window.vqec_vision_ai_core_encwn_complete_dropped_result(
            _input.ticket_.token_);
        if (result_done.code_ != status_code::ok) {
            _window.vqec_vision_ai_core_encwn_begin_drain();
            return result_done;
        }
        return submitted;
    } catch (...) {
        // Acceptance may be ambiguous: retain caller/backend owners and ledger for recovery.
        _window.vqec_vision_ai_core_encwn_begin_drain();
        throw;
    }
}

encoder_preparation::encoder_preparation(encoder_window& _window) noexcept : window_(_window) {}

status encoder_preparation::vqec_vision_ai_appl_enprp_prepare(
    preview_surface_pool& _pool, const preview_frame_key& _frame,
    bool _has_demand, std::uint64_t _now_ns) {
    const auto result = vqec_vision_ai_appl_enprp_prepare_input(
        window_, _pool, _frame, _has_demand, _now_ns, ticket_, writer_);
    if (result.code_ == status_code::ok) {
        frame_ = _frame;
        dispatch_generation_ = 0;
    }
    return result;
}

status encoder_preparation::vqec_vision_ai_appl_enprp_prepare_backend(
    preview_surface_pool& _pool, const preview_frame_key& _frame,
    bool _has_demand, std::uint64_t _now_ns, std::uint64_t _dispatch_generation) {
    if (_dispatch_generation == 0) {
        return {status_code::invalid_argument, "encoder preparation requires binding identity"};
    }
    const auto result = vqec_vision_ai_appl_enprp_prepare(_pool, _frame, _has_demand, _now_ns);
    if (result.code_ == status_code::ok) {
        dispatch_generation_ = _dispatch_generation;
    }
    return result;
}

status encoder_preparation::vqec_vision_ai_appl_enprp_commit_backend(encoder_input& _input) {
    if (_input.pixels_ || dispatch_generation_ == 0) {
        return {status_code::invalid_state, "encoder handoff destination occupied or binding absent"};
    }
    encoder_input candidate;
    candidate.frame_ = frame_;
    candidate.geometry_ = window_.vqec_vision_ai_core_encwn_geometry();
    candidate.dispatch_generation_ = dispatch_generation_;
    const auto result = vqec_vision_ai_appl_enprp_commit_input(candidate.ticket_, candidate.pixels_);
    if (result.code_ == status_code::ok) {
        _input = std::move(candidate);
    }
    return result;
}

std::uint8_t* encoder_preparation::vqec_vision_ai_appl_enprp_borrow_data() noexcept {
    return writer_.vqec_vision_ai_core_pvsrf_borrow_data();
}

status encoder_preparation::vqec_vision_ai_appl_enprp_cancel() {
    return vqec_vision_ai_appl_enprp_cancel_input(window_, ticket_.token_, writer_);
}

status encoder_preparation::vqec_vision_ai_appl_enprp_commit_input(
    submission_ticket& _ticket, std::shared_ptr<const std::vector<std::uint8_t>>& _input) {
    if (_input || writer_.vqec_vision_ai_core_pvsrf_size_bytes() == 0) {
        return {status_code::invalid_state, "encoder handoff requires writer and empty destination"};
    }
    const auto committed = window_.vqec_vision_ai_core_encwn_commit(ticket_.token_);
    if (committed.code_ != status_code::ok) {
        return committed;
    }
    _input = writer_.vqec_vision_ai_core_pvsrf_seal();
    _ticket = ticket_;
    return {};
}

status vqec_vision_ai_appl_enprp_prepare_input(
    encoder_window& _window, preview_surface_pool& _pool, const preview_frame_key& _frame,
    bool _has_demand, std::uint64_t _now_ns, submission_ticket& _ticket,
    writable_preview_surface& _writer) {
    if (_writer.vqec_vision_ai_core_pvsrf_size_bytes() != 0) {
        return {status_code::invalid_state, "encoder preparation writer is occupied"};
    }
    const auto pool_geometry = _pool.vqec_vision_ai_core_pvpol_geometry();
    const auto window_geometry = _window.vqec_vision_ai_core_encwn_geometry();
    if (pool_geometry.width_ == 0 || window_geometry.width_ == 0 ||
        pool_geometry.width_ != window_geometry.width_ ||
        pool_geometry.height_ != window_geometry.height_) {
        return {status_code::invalid_state, "encoder pool and ledger geometry mismatch"};
    }
    const auto reserved = _window.vqec_vision_ai_core_encwn_reserve(
        _frame, _has_demand, _now_ns, _ticket);
    if (reserved.code_ != status_code::ok) {
        return reserved;
    }
    const auto acquired = _pool.vqec_vision_ai_core_pvpol_acquire(_writer);
    if (acquired.code_ != status_code::ok) {
        const auto cancelled = _window.vqec_vision_ai_core_encwn_cancel_reserved(_ticket.token_);
        if (cancelled.code_ != status_code::ok) {
            return cancelled;  // Preserve the published token for explicit reconciliation.
        }
    }
    return acquired;
}

status vqec_vision_ai_appl_enprp_cancel_input(
    encoder_window& _window, submission_token _token, writable_preview_surface& _writer) {
    if (_writer.vqec_vision_ai_core_pvsrf_size_bytes() == 0) {
        return {status_code::invalid_state, "encoder cancellation requires an owned writer"};
    }
    const auto cancelled = _window.vqec_vision_ai_core_encwn_cancel_reserved(_token);
    if (cancelled.code_ == status_code::ok) {
        _writer = writable_preview_surface{};
    }
    return cancelled;
}

}  // namespace vqec::vision::ai
