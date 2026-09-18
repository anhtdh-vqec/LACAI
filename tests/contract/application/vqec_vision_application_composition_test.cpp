#include "vqec_vision_application_composition.hpp"

#include <cassert>
#include <cstdint>

using namespace vqec::vision::ai;

namespace {

class fake_source_session final : public source_session_port {
public:
    [[nodiscard]] status vqec_vision_ai_appl_srcsn_step(
        std::uint64_t _steady_now_ns, tensor_result& _result,
        source_session_progress& _progress) override {
        (void)_steady_now_ns;
        ++steps_;
        _progress = {};
        if (health_.phase_ == source_session_phase::draining) {
            health_.phase_ = source_session_phase::stopped;
            return {};
        }
        health_.phase_ = source_session_phase::running;
        if (emits_result_) {
            _result.pipeline_pts_ns_ = 123;
            _progress.has_result_ = true;
            _progress.model_slot_ = 0;
            _progress.ticket_.source_frame_id_ = 42;
            return {};
        }
        return {status_code::pending, "fixture has no result"};
    }
    [[nodiscard]] status vqec_vision_ai_appl_srcsn_request_stop(
        std::uint64_t _steady_now_ns) override {
        (void)_steady_now_ns;
        health_.phase_ = delays_stop_ ? source_session_phase::draining : source_session_phase::stopped;
        return {};
    }
    [[nodiscard]] source_session_health
    vqec_vision_ai_appl_srcsn_get_health() const noexcept override {
        return health_;
    }

    bool emits_result_{false};
    bool delays_stop_{false};
    unsigned steps_{0};

private:
    source_session_health health_;
};

}  // namespace

int main() {
    application_composition invalid(0, 1, 1);
    assert(invalid.vqec_vision_ai_cntr_acomp_validate().code_ ==
           status_code::invalid_argument);

    application_composition composition(7, 11, 1);
    fake_source_session session;
    assert(composition.vqec_vision_ai_appl_acomp_bind_session(0, session).code_ ==
           status_code::ok);
    assert(composition.vqec_vision_ai_cntr_acomp_validate().code_ == status_code::ok);
    assert(composition.vqec_vision_ai_cntr_acomp_activate().code_ == status_code::ok);
    assert(composition.vqec_vision_ai_cntr_acomp_step(10).code_ ==
           status_code::pending);
    assert(composition.vqec_vision_ai_cntr_acomp_step(9).code_ ==
           status_code::invalid_argument);
    assert(composition.vqec_vision_ai_cntr_acomp_request_stop(10).code_ ==
           status_code::ok);
    const auto snapshot = composition.vqec_vision_ai_cntr_acomp_get_snapshot();
    assert(snapshot.deployment_revision_ == 7);
    assert(snapshot.catalog_revision_ == 11);
    assert(!snapshot.is_recovery_required_ &&
           snapshot.state_ == application_composition_state::stopped);
    // Pending delivery must preserve correlation and prevent another source step.
    fake_source_session producer;
    producer.emits_result_ = true;
    producer.delays_stop_ = true;
    application_composition delivery(1, 1, 1);
    assert(delivery.vqec_vision_ai_appl_acomp_bind_session(0, producer).code_ == status_code::ok);
    assert(delivery.vqec_vision_ai_cntr_acomp_validate().code_ == status_code::ok);
    assert(delivery.vqec_vision_ai_cntr_acomp_activate().code_ == status_code::ok);
    assert(delivery.vqec_vision_ai_cntr_acomp_step(1).code_ == status_code::ok);
    assert(delivery.vqec_vision_ai_cntr_acomp_step(2).code_ == status_code::pending);
    assert(producer.steps_ == 1);
    assert(delivery.vqec_vision_ai_cntr_acomp_validate().code_ == status_code::invalid_state);
    assert(delivery.vqec_vision_ai_cntr_acomp_request_stop(UINT64_MAX).code_ == status_code::invalid_argument);
    assert(delivery.vqec_vision_ai_cntr_acomp_request_stop(2).code_ == status_code::pending);
    assert(delivery.vqec_vision_ai_cntr_acomp_request_stop(2).code_ == status_code::pending);
    assert(!delivery.vqec_vision_ai_cntr_acomp_get_snapshot().is_recovery_required_);
    tensor_result result;
    multi_source_progress_report report;
    assert(delivery.vqec_vision_ai_appl_acomp_take_result(result, report).code_ == status_code::ok);
    assert(result.pipeline_pts_ns_ == 123 && report.source_index_ == 0 &&
           report.source_progress_.ticket_.source_frame_id_ == 42);
    assert(delivery.vqec_vision_ai_appl_acomp_take_result(result, report).code_ == status_code::pending);
    assert(result.pipeline_pts_ns_ == 123);
    assert(delivery.vqec_vision_ai_cntr_acomp_step(3).code_ == status_code::ok);
    assert(delivery.vqec_vision_ai_cntr_acomp_get_snapshot().state_ == application_composition_state::stopped);

    fake_source_session idle;
    application_composition early_stop(1, 1, 1);
    assert(early_stop.vqec_vision_ai_appl_acomp_bind_session(0, idle).code_ == status_code::ok);
    assert(early_stop.vqec_vision_ai_cntr_acomp_validate().code_ == status_code::ok);
    assert(early_stop.vqec_vision_ai_cntr_acomp_activate().code_ == status_code::ok);
    assert(early_stop.vqec_vision_ai_cntr_acomp_request_stop(0).code_ == status_code::ok);
    return 0;
}
