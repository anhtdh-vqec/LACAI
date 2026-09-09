#include <iostream>

#include "vqec/vision/ai/contracts/vqec_vision_submission_window.hpp"

int main() {
    using vqec::vision::ai::status_code;
    using vqec::vision::ai::submission_config;
    using vqec::vision::ai::submission_ticket;
    using vqec::vision::ai::submission_window;
    unsigned failures = 0;
    const auto check = [&failures](bool _condition) {
        if (!_condition) {
            ++failures;
        }
    };
    submission_window window;
    submission_ticket first;
    check(window.vqec_vision_ai_core_subwn_reserve(1, 1, 100, 0, first).code_ ==
          status_code::invalid_state);
    submission_config config{42, 1, 1000, 100, 1};
    check(window.vqec_vision_ai_core_subwn_configure(config).code_ == status_code::ok);
    check(window.vqec_vision_ai_core_subwn_configure(config).code_ == status_code::invalid_state);
    check(window.vqec_vision_ai_core_subwn_reserve(2, 1, 100, 0, first).code_ ==
          status_code::invalid_argument);
    check(window.vqec_vision_ai_core_subwn_reserve(1, 1, UINT64_MAX, 0, first).code_ ==
          status_code::invalid_argument);
    check(window.vqec_vision_ai_core_subwn_reserve(1, 1, 100, 0, first).code_ ==
          status_code::ok);
    check(first.source_epoch_ == 1 && first.source_frame_id_ == 1 &&
          first.pipeline_pts_ns_ == 1000 && first.source_pts_ns_ == 100);
    submission_ticket second;
    check(window.vqec_vision_ai_core_subwn_reserve(1, 2, 120, 1, second).code_ ==
          status_code::resource_exhausted);
    check(second.token_.job_id_ == 0);
    check(window.vqec_vision_ai_core_subwn_complete_input(first.token_).code_ ==
          status_code::invalid_state);
    check(window.vqec_vision_ai_core_subwn_commit(first.token_).code_ == status_code::ok);
    check(window.vqec_vision_ai_core_subwn_cancel_reserved(first.token_).code_ ==
          status_code::invalid_state);
    check(window.vqec_vision_ai_core_subwn_complete_result(first.token_).code_ == status_code::ok);
    check(window.vqec_vision_ai_core_subwn_get_outstanding() == 1);
    check(window.vqec_vision_ai_core_subwn_complete_result(first.token_).code_ ==
          status_code::invalid_state);
    check(window.vqec_vision_ai_core_subwn_complete_input({41, first.token_.job_id_}).code_ ==
          status_code::invalid_state);
    check(window.vqec_vision_ai_core_subwn_complete_input(first.token_).code_ == status_code::ok);
    check(window.vqec_vision_ai_core_subwn_get_outstanding() == 0);
    check(window.vqec_vision_ai_core_subwn_reserve(1, 2, 100, 1, second).code_ ==
          status_code::invalid_argument);
    check(window.vqec_vision_ai_core_subwn_reserve(1, 2, 120, 1, second).code_ ==
          status_code::ok);
    check(second.source_epoch_ == 1 && second.source_frame_id_ == 2);
    check(second.pipeline_pts_ns_ == 1020 && second.token_.job_id_ != first.token_.job_id_);
    check(window.vqec_vision_ai_core_subwn_complete_input(first.token_).code_ ==
          status_code::invalid_state);
    check(window.vqec_vision_ai_core_subwn_commit(second.token_).code_ == status_code::ok);
    check(window.vqec_vision_ai_core_subwn_check_deadlines(100).code_ == status_code::ok);
    check(window.vqec_vision_ai_core_subwn_check_deadlines(101).code_ == status_code::timeout);
    check(!window.vqec_vision_ai_core_subwn_is_accepting() &&
          window.vqec_vision_ai_core_subwn_get_outstanding() == 1);
    check(window.vqec_vision_ai_core_subwn_complete_input(second.token_).code_ == status_code::ok);
    check(window.vqec_vision_ai_core_subwn_get_outstanding() == 1);
    check(window.vqec_vision_ai_core_subwn_complete_result(second.token_).code_ == status_code::ok);
    check(window.vqec_vision_ai_core_subwn_get_outstanding() == 0 &&
          !window.vqec_vision_ai_core_subwn_is_accepting());

    submission_window draining;
    config.cycle_id_ = 43;
    check(draining.vqec_vision_ai_core_subwn_configure(config).code_ == status_code::ok);
    check(draining.vqec_vision_ai_core_subwn_reserve(1, 1, 0, 0, first).code_ ==
          status_code::ok);
    draining.vqec_vision_ai_core_subwn_begin_drain();
    check(draining.vqec_vision_ai_core_subwn_commit(first.token_).code_ ==
          status_code::invalid_state);
    check(draining.vqec_vision_ai_core_subwn_get_outstanding() == 1);
    check(draining.vqec_vision_ai_core_subwn_cancel_reserved(first.token_).code_ ==
          status_code::ok);
    check(draining.vqec_vision_ai_core_subwn_get_outstanding() == 0);

    submission_window overflow;
    config.pipeline_anchor_ns_ = UINT64_MAX - 10;
    check(overflow.vqec_vision_ai_core_subwn_configure(config).code_ == status_code::ok);
    check(overflow.vqec_vision_ai_core_subwn_reserve(1, 1, 0, 0, first).code_ ==
          status_code::ok);
    check(overflow.vqec_vision_ai_core_subwn_cancel_reserved(first.token_).code_ ==
          status_code::ok);
    check(overflow.vqec_vision_ai_core_subwn_reserve(1, 2, 10, 0, second).code_ ==
          status_code::invalid_argument);
    check(overflow.vqec_vision_ai_core_subwn_reserve(1, 2, 1, UINT64_MAX, second).code_ ==
          status_code::invalid_argument);
    check(overflow.vqec_vision_ai_core_subwn_get_outstanding() == 0);
    submission_window invalid;
    config.capacity_ = 5;
    check(invalid.vqec_vision_ai_core_subwn_configure(config).code_ ==
          status_code::invalid_argument);
    std::cout << "submission window failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
