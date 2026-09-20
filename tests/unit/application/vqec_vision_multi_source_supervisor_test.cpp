#include <cstdint>
#include <iostream>

#include "vqec_vision_multi_source_supervisor.hpp"

namespace {

class fake_source_session final : public vqec::vision::ai::source_session_port {
public:
    explicit fake_source_session(bool _fails, bool _repeats_fault = false) :
        fails_(_fails), repeats_fault_(_repeats_fault) {}

    vqec::vision::ai::status vqec_vision_ai_appl_srcsn_step(
        std::uint64_t _steady_now_ns, vqec::vision::ai::tensor_result& _result,
        vqec::vision::ai::source_session_progress& _progress) override {
        (void)_steady_now_ns;
        (void)_result;
        _progress = {};
        ++steps_;
        if (fails_ && (steps_ == 1 || repeats_fault_)) {
            health_.phase_ = vqec::vision::ai::source_session_phase::draining;
            health_.first_error_code_ = vqec::vision::ai::status_code::source_lost;
            return {vqec::vision::ai::status_code::source_lost, "injected source loss"};
        }
        if (health_.phase_ == vqec::vision::ai::source_session_phase::draining) {
            health_.phase_ = vqec::vision::ai::source_session_phase::stopped;
            return {};
        }
        health_.phase_ = vqec::vision::ai::source_session_phase::running;
        health_.model_graph_count_ = 2;
        health_.running_graph_count_ = 2;
        return {vqec::vision::ai::status_code::pending, {}};
    }

    vqec::vision::ai::status vqec_vision_ai_appl_srcsn_request_stop(
        std::uint64_t _steady_now_ns) override {
        (void)_steady_now_ns;
        ++stop_requests_;
        health_.phase_ = vqec::vision::ai::source_session_phase::stopped;
        return {};
    }

    vqec::vision::ai::source_session_health
    vqec_vision_ai_appl_srcsn_get_health() const noexcept override {
        return health_;
    }

    unsigned steps_{0};
    unsigned stop_requests_{0};

private:
    bool fails_{false};
    bool repeats_fault_{false};
    vqec::vision::ai::source_session_health health_;
};

}  // namespace

int main() {
    using namespace vqec::vision::ai;
    unsigned failures = 0;
    const auto check = [&failures](bool _condition) {
        if (!_condition) {
            ++failures;
        }
    };

    fake_source_session healthy(false);
    fake_source_session faulted(true);
    multi_source_supervisor supervisor({11, 22, 2});
    check(supervisor.vqec_vision_ai_appl_mssup_bind_session(0, healthy).code_ ==
          status_code::ok);
    check(supervisor.vqec_vision_ai_appl_mssup_activate().code_ ==
          status_code::invalid_argument);
    check(supervisor.vqec_vision_ai_appl_mssup_bind_session(1, faulted).code_ ==
          status_code::ok);
    check(supervisor.vqec_vision_ai_appl_mssup_bind_session(1, healthy).code_ ==
          status_code::invalid_state);
    check(supervisor.vqec_vision_ai_appl_mssup_activate().code_ == status_code::ok);

    tensor_result result;
    multi_source_progress_report report;
    check(supervisor.vqec_vision_ai_appl_mssup_step(1, result, report).code_ ==
          status_code::pending);
    check(report.source_index_ == 0 && healthy.steps_ == 1);
    check(supervisor.vqec_vision_ai_appl_mssup_step(2, result, report).code_ ==
          status_code::pending);
    check(report.source_index_ == 1 &&
          report.source_status_.code_ == status_code::source_lost);
    check(supervisor.vqec_vision_ai_appl_mssup_step(3, result, report).code_ ==
          status_code::pending);
    check(report.source_index_ == 0 && healthy.steps_ == 2);
    check(supervisor.vqec_vision_ai_appl_mssup_step(4, result, report).code_ ==
          status_code::pending);
    check(report.source_index_ == 1 && faulted.steps_ == 2);
    check(supervisor.vqec_vision_ai_appl_mssup_step(5, result, report).code_ ==
          status_code::pending);
    check(report.source_index_ == 0 && healthy.steps_ == 3);

    const auto running = supervisor.vqec_vision_ai_appl_mssup_get_snapshot();
    check(running.running_sources_ == 1 && running.stopped_sources_ == 1);
    check(running.first_error_code_ == status_code::source_lost);
    // Section 12: the isolated source error is visible on its own channel and counters,
    // not only as a swallowed pending return.
    check(running.fault_event_total_ == 1 && running.faulted_sources_ == 1 &&
          running.source_fault_codes_[1] == status_code::source_lost &&
          running.source_fault_codes_[0] == status_code::ok);
    multi_source_fault_event fault;
    check(supervisor.vqec_vision_ai_appl_mssup_take_fault(fault).code_ == status_code::ok);
    check(fault.source_index_ == 1 && fault.code_ == status_code::source_lost);
    check(supervisor.vqec_vision_ai_appl_mssup_take_fault(fault).code_ ==
          status_code::pending);
    check(supervisor.vqec_vision_ai_appl_mssup_request_stop(6).code_ == status_code::ok);
    check(healthy.stop_requests_ == 1 && faulted.stop_requests_ == 1);
    check(supervisor.vqec_vision_ai_appl_mssup_get_state() ==
          multi_source_supervisor_state::stopped);

    // A fail-closed session may report the same drain timeout on every progress call
    // until an external DMA owner returns. The supervisor retains one fault state/event
    // instead of flooding its bounded event channel and process log.
    fake_source_session repeated_fault(true, true);
    multi_source_supervisor repeated_supervisor({33, 44, 1});
    check(repeated_supervisor.vqec_vision_ai_appl_mssup_bind_session(
              0, repeated_fault).code_ == status_code::ok);
    check(repeated_supervisor.vqec_vision_ai_appl_mssup_activate().code_ ==
          status_code::ok);
    check(repeated_supervisor.vqec_vision_ai_appl_mssup_step(
              10, result, report).code_ == status_code::pending);
    check(repeated_supervisor.vqec_vision_ai_appl_mssup_step(
              11, result, report).code_ == status_code::pending);
    check(repeated_supervisor.vqec_vision_ai_appl_mssup_step(
              12, result, report).code_ == status_code::pending);
    const auto repeated_snapshot =
        repeated_supervisor.vqec_vision_ai_appl_mssup_get_snapshot();
    check(repeated_snapshot.fault_event_total_ == 1 &&
          repeated_snapshot.faulted_sources_ == 1 &&
          repeated_snapshot.source_fault_codes_[0] == status_code::source_lost);
    check(repeated_supervisor.vqec_vision_ai_appl_mssup_take_fault(fault).code_ ==
          status_code::ok);
    check(repeated_supervisor.vqec_vision_ai_appl_mssup_take_fault(fault).code_ ==
          status_code::pending);

    std::cout << "multi-source supervisor failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
