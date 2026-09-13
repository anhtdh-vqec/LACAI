// Device-free tests for the per-source recovery controller: exponential backoff, the retry
// budget, success reset, per-source isolation and overflow-safe capping. Time is injected.

#include <cstdint>
#include <iostream>

#include "vqec_vision_recovery_controller.hpp"

using namespace vqec::vision::ai;

int main() {
    unsigned failures = 0;
    const auto check = [&failures](bool _condition) {
        if (!_condition) {
            ++failures;
        }
    };

    recovery_policy policy;
    policy.max_retries_ = 3;
    policy.initial_backoff_ns_ = 100;
    policy.max_backoff_ns_ = 400;
    policy.multiplier_numerator_ = 2;
    policy.multiplier_denominator_ = 1;

    // Configuration validation.
    {
        source_recovery_controller controller;
        auto bad = policy;
        bad.max_retries_ = 0;
        check(controller.vqec_vision_ai_life_rcvr_configure(1, bad).code_ ==
              status_code::invalid_argument);
        bad = policy;
        bad.initial_backoff_ns_ = 0;
        check(controller.vqec_vision_ai_life_rcvr_configure(1, bad).code_ ==
              status_code::invalid_argument);
        bad = policy;
        bad.max_backoff_ns_ = 50;
        check(controller.vqec_vision_ai_life_rcvr_configure(1, bad).code_ ==
              status_code::invalid_argument);
        check(controller.vqec_vision_ai_life_rcvr_configure(2, policy).code_ ==
              status_code::ok);
        check(controller.vqec_vision_ai_life_rcvr_configure(2, policy).code_ ==
              status_code::invalid_state);
    }

    // Exponential backoff, then a bounded retry budget.
    {
        source_recovery_controller controller;
        check(controller.vqec_vision_ai_life_rcvr_configure(2, policy).code_ ==
              status_code::ok);
        recovery_decision decision = recovery_decision::retry;
        check(controller.vqec_vision_ai_life_rcvr_on_fault(0, 1000, decision).code_ ==
              status_code::ok);
        check(decision == recovery_decision::wait);
        recovery_snapshot snapshot;
        check(controller.vqec_vision_ai_life_rcvr_get_snapshot(0, snapshot).code_ ==
              status_code::ok);
        check(snapshot.attempts_ == 1 && snapshot.next_retry_ns_ == 1100 &&
              !snapshot.exhausted_);

        check(controller.vqec_vision_ai_life_rcvr_on_fault(0, 1100, decision).code_ ==
              status_code::ok);
        check(controller.vqec_vision_ai_life_rcvr_get_snapshot(0, snapshot).code_ ==
              status_code::ok);
        check(snapshot.attempts_ == 2 && snapshot.next_retry_ns_ == 1300);

        check(controller.vqec_vision_ai_life_rcvr_on_fault(0, 1300, decision).code_ ==
              status_code::ok);
        check(controller.vqec_vision_ai_life_rcvr_get_snapshot(0, snapshot).code_ ==
              status_code::ok);
        check(snapshot.attempts_ == 3 && snapshot.next_retry_ns_ == 1700);

        check(controller.vqec_vision_ai_life_rcvr_on_fault(0, 1700, decision).code_ ==
              status_code::ok);
        check(decision == recovery_decision::exhausted);
        check(controller.vqec_vision_ai_life_rcvr_get_snapshot(0, snapshot).code_ ==
              status_code::ok);
        check(snapshot.exhausted_ && snapshot.attempts_ == 3);

        // Another source is unaffected by source 0's failure budget.
        check(controller.vqec_vision_ai_life_rcvr_on_fault(1, 1700, decision).code_ ==
              status_code::ok);
        check(decision == recovery_decision::wait);
        check(controller.vqec_vision_ai_life_rcvr_get_snapshot(1, snapshot).code_ ==
              status_code::ok);
        check(snapshot.attempts_ == 1 && !snapshot.exhausted_);

        // A success resets the failing source.
        check(controller.vqec_vision_ai_life_rcvr_on_success(0).code_ == status_code::ok);
        check(controller.vqec_vision_ai_life_rcvr_get_snapshot(0, snapshot).code_ ==
              status_code::ok);
        check(snapshot.attempts_ == 0 && snapshot.next_retry_ns_ == 0 &&
              !snapshot.exhausted_);

        check(controller.vqec_vision_ai_life_rcvr_on_fault(2, 0, decision).code_ ==
              status_code::invalid_argument);
        check(controller.vqec_vision_ai_life_rcvr_get_snapshot(2, snapshot).code_ ==
              status_code::invalid_argument);
    }

    // Backoff caps at max_backoff without overflow even with a large multiplier.
    {
        recovery_policy capped;
        capped.max_retries_ = 64;
        capped.initial_backoff_ns_ = 1000000;
        capped.max_backoff_ns_ = 8000000;
        capped.multiplier_numerator_ = 1000000000;
        capped.multiplier_denominator_ = 1;
        source_recovery_controller controller;
        check(controller.vqec_vision_ai_life_rcvr_configure(1, capped).code_ ==
              status_code::ok);
        recovery_decision decision = recovery_decision::retry;
        for (std::uint64_t now = 1; now <= 40; ++now) {
            check(controller.vqec_vision_ai_life_rcvr_on_fault(0, now, decision).code_ ==
                  status_code::ok);
            check(decision == recovery_decision::wait);
        }
        recovery_snapshot snapshot;
        check(controller.vqec_vision_ai_life_rcvr_get_snapshot(0, snapshot).code_ ==
              status_code::ok);
        check(snapshot.next_retry_ns_ <= 40 + capped.max_backoff_ns_);
    }

    std::cout << "recovery controller failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
