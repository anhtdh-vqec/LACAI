#include <iostream>

#include "vqec/vision/ai/contracts/vqec_vision_encoder_window.hpp"

int main() {
    using namespace vqec::vision::ai;
    unsigned failures = 0;
    const auto check = [&](bool _condition) {
        if (!_condition) {
            ++failures;
        }
    };
    encoder_window_config config;
    config.submission_ = {1, 1, 0, 100, 2};
    config.geometry_ = {2, 2};
    config.max_input_bytes_ = 6;  // Memory budget limits to one job despite two slots.
    encoder_window window;
    check(window.vqec_vision_ai_core_encwn_configure(config).code_ == status_code::ok);
    check(window.vqec_vision_ai_core_encwn_configure(config).code_ == status_code::invalid_state);
    preview_frame_key frame{0, 0, 1, 42, 10};
    submission_ticket ticket{{99, 99}, 99, 99, 99, 99};
    check(window.vqec_vision_ai_core_encwn_reserve(frame, false, 0, ticket).code_ ==
        status_code::pending);
    check(ticket.token_.job_id_ == 99 && window.vqec_vision_ai_core_encwn_reserved_bytes() == 0);
    check(window.vqec_vision_ai_core_encwn_reserve(frame, true, 0, ticket).code_ == status_code::ok);
    check(window.vqec_vision_ai_core_encwn_reserved_bytes() == 6);
    check(window.vqec_vision_ai_core_encwn_complete_input(ticket.token_).code_ ==
        status_code::invalid_state);
    check(window.vqec_vision_ai_core_encwn_commit(ticket.token_).code_ == status_code::ok);
    check(window.vqec_vision_ai_core_encwn_cancel_reserved(ticket.token_).code_ ==
        status_code::invalid_state);
    submission_ticket next;
    auto newer = frame;
    newer.frame_id_++;
    newer.source_pts_ns_++;
    check(window.vqec_vision_ai_core_encwn_reserve(newer, true, 1, next).code_ ==
        status_code::resource_exhausted);
    const std::uint8_t payload[]{0, 0, 1, 0x65};
    h264_access_unit_view unit{newer, {2, 2}, true, {payload, sizeof(payload)}, {}, {}};
    check(window.vqec_vision_ai_core_encwn_complete_result(ticket.token_, unit).code_ ==
        status_code::invalid_state);
    unit.frame_ = frame;
    check(window.vqec_vision_ai_core_encwn_complete_result(ticket.token_, unit).code_ ==
        status_code::ok);
    check(window.vqec_vision_ai_core_encwn_reserved_bytes() == 6);
    check(window.vqec_vision_ai_core_encwn_complete_result(ticket.token_, unit).code_ ==
        status_code::invalid_state);
    check(window.vqec_vision_ai_core_encwn_check_deadlines(100).code_ == status_code::timeout);
    check(window.vqec_vision_ai_core_encwn_outstanding() == 1);
    check(window.vqec_vision_ai_core_encwn_reserved_bytes() == 6);
    check(window.vqec_vision_ai_core_encwn_check_deadlines(99).code_ == status_code::invalid_argument);
    check(window.vqec_vision_ai_core_encwn_complete_input({2, ticket.token_.job_id_}).code_ ==
        status_code::invalid_state);
    check(window.vqec_vision_ai_core_encwn_complete_input(ticket.token_).code_ == status_code::ok);
    check(window.vqec_vision_ai_core_encwn_reserved_bytes() == 0);
    check(window.vqec_vision_ai_core_encwn_reserve(newer, true, 101, next).code_ ==
        status_code::invalid_state);  // Timeout is latched, not cleared by late completion.

    encoder_window second;
    config.submission_.cycle_id_ = 2;
    config.max_input_bytes_ = 12;
    check(second.vqec_vision_ai_core_encwn_configure(config).code_ == status_code::ok);
    check(second.vqec_vision_ai_core_encwn_reserve(frame, true, 0, ticket).code_ == status_code::ok);
    check(second.vqec_vision_ai_core_encwn_cancel_reserved(ticket.token_).code_ == status_code::ok);
    check(second.vqec_vision_ai_core_encwn_reserved_bytes() == 0);
    check(second.vqec_vision_ai_core_encwn_reserve(frame, true, 0, next).code_ ==
        status_code::invalid_argument);  // Cancelled PTS is consumed.
    check(second.vqec_vision_ai_core_encwn_reserve(newer, true, 0, next).code_ == status_code::ok);
    check(second.vqec_vision_ai_core_encwn_commit(next.token_).code_ == status_code::ok);
    check(second.vqec_vision_ai_core_encwn_complete_input(next.token_).code_ == status_code::ok);
    check(second.vqec_vision_ai_core_encwn_reserved_bytes() == 6);
    second.vqec_vision_ai_core_encwn_begin_drain();
    check(second.vqec_vision_ai_core_encwn_reserve(newer, false, 1, ticket).code_ ==
        status_code::invalid_state);
    check(second.vqec_vision_ai_core_encwn_complete_dropped_result(next.token_).code_ ==
        status_code::ok);
    check(second.vqec_vision_ai_core_encwn_reserved_bytes() == 0 &&
        second.vqec_vision_ai_core_encwn_outstanding() == 0);
    encoder_window invalid;
    config.max_input_bytes_ = 5;
    check(invalid.vqec_vision_ai_core_encwn_configure(config).code_ == status_code::resource_exhausted);
    config.max_input_bytes_ = 12;
    config.submission_.capacity_ = 5;
    check(invalid.vqec_vision_ai_core_encwn_configure(config).code_ == status_code::invalid_argument);
    config.submission_.capacity_ = 2;
    check(invalid.vqec_vision_ai_core_encwn_configure(config).code_ == status_code::ok);
    encoder_window events;
    check(events.vqec_vision_ai_core_encwn_configure(config).code_ == status_code::ok);
    check(events.vqec_vision_ai_core_encwn_reserve(frame, true, 0, ticket).code_ == status_code::ok);
    check(events.vqec_vision_ai_core_encwn_commit(ticket.token_).code_ == status_code::ok);
    encoder_event completion{encoder_event_kind::input_complete, ticket.token_, {}, {}};
    check(events.vqec_vision_ai_core_encwn_validate_event(completion).code_ == status_code::ok);
    check(events.vqec_vision_ai_core_encwn_outstanding() == 1);
    check(events.vqec_vision_ai_core_encwn_apply_event(completion).code_ == status_code::ok);
    check(events.vqec_vision_ai_core_encwn_validate_event(completion).code_ ==
        status_code::invalid_state);
    check(events.vqec_vision_ai_core_encwn_outstanding() == 1);
    check(events.vqec_vision_ai_core_encwn_apply_event(completion).code_ == status_code::invalid_state);
    check(events.vqec_vision_ai_core_encwn_outstanding() == 1);
    check(events.vqec_vision_ai_core_encwn_reserve(newer, true, 1, next).code_ ==
        status_code::invalid_state);
    completion.kind_ = encoder_event_kind::output_dropped;
    check(events.vqec_vision_ai_core_encwn_apply_event(completion).code_ == status_code::ok);
    check(events.vqec_vision_ai_core_encwn_outstanding() == 0);

    encoder_window fault_window;
    check(fault_window.vqec_vision_ai_core_encwn_configure(config).code_ == status_code::ok);
    check(fault_window.vqec_vision_ai_core_encwn_reserve(frame, true, 0, ticket).code_ ==
        status_code::ok);
    check(fault_window.vqec_vision_ai_core_encwn_commit(ticket.token_).code_ == status_code::ok);
    encoder_event fault{encoder_event_kind::fault, {}, {}, {status_code::io_error, "test fault"}};
    check(fault_window.vqec_vision_ai_core_encwn_apply_event(fault).code_ == status_code::io_error);
    check(fault_window.vqec_vision_ai_core_encwn_outstanding() == 1 &&
        fault_window.vqec_vision_ai_core_encwn_reserved_bytes() == 6);
    completion.token_ = ticket.token_;
    check(fault_window.vqec_vision_ai_core_encwn_apply_event(completion).code_ == status_code::ok);
    check(fault_window.vqec_vision_ai_core_encwn_outstanding() == 1);
    completion.kind_ = encoder_event_kind::input_complete;
    check(fault_window.vqec_vision_ai_core_encwn_apply_event(completion).code_ == status_code::ok);
    check(fault_window.vqec_vision_ai_core_encwn_reserved_bytes() == 0);
    encoder_window attempts;
    check(attempts.vqec_vision_ai_core_encwn_configure(config).code_ == status_code::ok);
    check(attempts.vqec_vision_ai_core_encwn_reserve(frame, true, 0, ticket).code_ == status_code::ok);
    check(attempts.vqec_vision_ai_core_encwn_begin_submission(ticket.token_).code_ ==
        status_code::invalid_state);
    check(attempts.vqec_vision_ai_core_encwn_commit(ticket.token_).code_ == status_code::ok);
    check(attempts.vqec_vision_ai_core_encwn_begin_submission(ticket.token_).code_ == status_code::ok);
    check(attempts.vqec_vision_ai_core_encwn_begin_submission(ticket.token_).code_ ==
        status_code::invalid_state);
    check(attempts.vqec_vision_ai_core_encwn_outstanding() == 1 &&
        attempts.vqec_vision_ai_core_encwn_reserved_bytes() == 6);
    check(attempts.vqec_vision_ai_core_encwn_complete_input(ticket.token_).code_ == status_code::ok);
    check(attempts.vqec_vision_ai_core_encwn_complete_dropped_result(ticket.token_).code_ ==
        status_code::ok);
    check(attempts.vqec_vision_ai_core_encwn_begin_submission(ticket.token_).code_ ==
        status_code::invalid_state);
    encoder_window input_window;
    check(input_window.vqec_vision_ai_core_encwn_configure(config).code_ == status_code::ok);
    check(input_window.vqec_vision_ai_core_encwn_reserve(frame, true, 0, ticket).code_ == status_code::ok);
    check(input_window.vqec_vision_ai_core_encwn_commit(ticket.token_).code_ == status_code::ok);
    encoder_input input{frame, config.geometry_, ticket, 7,
        std::make_shared<const std::vector<std::uint8_t>>(6)};
    ++input.ticket_.pipeline_pts_ns_;
    check(input_window.vqec_vision_ai_core_encwn_begin_input(input, 7).code_ ==
        status_code::invalid_state);
    input.ticket_ = ticket;
    check(input_window.vqec_vision_ai_core_encwn_begin_input(input, 8).code_ ==
        status_code::invalid_state);
    check(input_window.vqec_vision_ai_core_encwn_begin_input(input, 7).code_ == status_code::ok);
    check(input_window.vqec_vision_ai_core_encwn_begin_input(input, 7).code_ ==
        status_code::invalid_state);
    check(input_window.vqec_vision_ai_core_encwn_outstanding() == 1);
    check(input_window.vqec_vision_ai_core_encwn_complete_input(ticket.token_).code_ == status_code::ok);
    check(input_window.vqec_vision_ai_core_encwn_complete_dropped_result(ticket.token_).code_ ==
        status_code::ok);
    std::cout << "encoder window failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
