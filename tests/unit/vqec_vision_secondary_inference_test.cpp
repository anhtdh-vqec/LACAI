// Device-free tests for the secondary/ROI inference contract and bounded scheduler:
// validation, queue capacity, backend execution and correlation, deadline expiry, stale
// epoch and cancellation.

#include <cstdint>
#include <iostream>
#include <vector>

#include "vqec_vision_secondary_inference_scheduler.hpp"

using namespace vqec::vision::ai;

namespace {

class fake_secondary_backend final : public secondary_inference_backend {
public:
    [[nodiscard]] status vqec_vision_ai_cntr_secin_execute(
        const secondary_inference_request& _request,
        std::vector<std::uint8_t>& _payload) override {
        ++calls_;
        if (_request.model_slot_ == fail_slot_) {
            return {status_code::io_error, "injected secondary failure"};
        }
        _payload = {static_cast<std::uint8_t>(_request.model_slot_),
            static_cast<std::uint8_t>(_request.has_roi_ ? 1U : 0U)};
        return {};
    }

    unsigned calls_{0};
    std::uint16_t fail_slot_{UINT16_MAX};
};

secondary_inference_request make_request(std::uint64_t _task, std::uint16_t _model,
    std::uint64_t _epoch) {
    secondary_inference_request request;
    request.task_id_ = _task;
    request.source_slot_ = 0;
    request.model_slot_ = _model;
    request.source_epoch_ = _epoch;
    request.source_frame_id_ = _task;
    request.source_pts_ns_ = _task * 1000;
    request.has_roi_ = true;
    request.roi_ = {0.0F, 0.0F, 32.0F, 32.0F};
    request.track_id_ = 5;
    return request;
}

}  // namespace

int main() {
    unsigned failures = 0;
    const auto check = [&failures](bool _condition) {
        if (!_condition) {
            ++failures;
        }
    };

    // Validation and configuration.
    {
        fake_secondary_backend backend;
        secondary_inference_scheduler scheduler;
        check(scheduler.vqec_vision_ai_sched_secsd_configure(2, 1, backend).code_ ==
              status_code::ok);
        check(scheduler.vqec_vision_ai_sched_secsd_configure(2, 1, backend).code_ ==
              status_code::invalid_state);
        secondary_inference_request invalid;
        check(scheduler.vqec_vision_ai_sched_secsd_submit(invalid).code_ ==
              status_code::invalid_argument);
        auto out_of_range = make_request(1, 0, 1);
        out_of_range.source_slot_ = 3;
        check(scheduler.vqec_vision_ai_sched_secsd_submit(out_of_range).code_ ==
              status_code::invalid_argument);
        auto no_roi = make_request(2, 0, 1);
        no_roi.has_roi_ = false;
        check(scheduler.vqec_vision_ai_sched_secsd_submit(no_roi).code_ == status_code::ok);
        auto bad_roi = make_request(3, 0, 1);
        bad_roi.roi_.width_ = 0.0F;
        check(scheduler.vqec_vision_ai_sched_secsd_submit(bad_roi).code_ ==
              status_code::invalid_argument);
    }

    // Execution, correlation and queue capacity.
    {
        fake_secondary_backend backend;
        secondary_inference_scheduler scheduler;
        check(scheduler.vqec_vision_ai_sched_secsd_configure(2, 2, backend).code_ ==
              status_code::ok);
        check(scheduler.vqec_vision_ai_sched_secsd_submit(make_request(1, 1, 1)).code_ ==
              status_code::ok);
        check(scheduler.vqec_vision_ai_sched_secsd_submit(make_request(2, 0, 1)).code_ ==
              status_code::ok);
        check(scheduler.vqec_vision_ai_sched_secsd_submit(make_request(3, 0, 1)).code_ ==
              status_code::resource_exhausted);

        secondary_inference_result result;
        check(scheduler.vqec_vision_ai_sched_secsd_step(10, result).code_ == status_code::ok);
        check(result.result_.code_ == status_code::ok && result.request_.task_id_ == 1 &&
              result.payload_.size() == 2 && result.payload_[0] == 1 && !result.stale_epoch_);
        check(scheduler.vqec_vision_ai_sched_secsd_step(11, result).code_ == status_code::ok);
        check(result.request_.task_id_ == 2);
        check(scheduler.vqec_vision_ai_sched_secsd_step(12, result).code_ ==
              status_code::pending);

        const auto snapshot = scheduler.vqec_vision_ai_sched_secsd_get_snapshot();
        check(snapshot.submitted_total_ == 2 && snapshot.completed_total_ == 2 &&
              snapshot.rejected_total_ == 1 && snapshot.failed_total_ == 0 &&
              snapshot.queue_depth_ == 0 && backend.calls_ == 2);
    }

    // Backend failure is reported, and a deadline-expired task is not executed.
    {
        fake_secondary_backend backend;
        backend.fail_slot_ = 0;
        secondary_inference_scheduler scheduler;
        check(scheduler.vqec_vision_ai_sched_secsd_configure(2, 1, backend).code_ ==
              status_code::ok);
        check(scheduler.vqec_vision_ai_sched_secsd_submit(make_request(1, 0, 1)).code_ ==
              status_code::ok);
        secondary_inference_result result;
        check(scheduler.vqec_vision_ai_sched_secsd_step(1, result).code_ == status_code::ok);
        check(result.result_.code_ == status_code::io_error);
        check(scheduler.vqec_vision_ai_sched_secsd_get_snapshot().failed_total_ == 1);

        auto expiring = make_request(2, 1, 1);
        expiring.deadline_ns_ = 100;
        check(scheduler.vqec_vision_ai_sched_secsd_submit(expiring).code_ == status_code::ok);
        check(scheduler.vqec_vision_ai_sched_secsd_step(200, result).code_ == status_code::ok);
        check(result.result_.code_ == status_code::timeout &&
              scheduler.vqec_vision_ai_sched_secsd_get_snapshot().expired_total_ == 1);
        check(backend.calls_ == 1);
    }

    // Epoch correlation and cancellation.
    {
        fake_secondary_backend backend;
        secondary_inference_scheduler scheduler;
        check(scheduler.vqec_vision_ai_sched_secsd_configure(4, 1, backend).code_ ==
              status_code::ok);
        check(scheduler.vqec_vision_ai_sched_secsd_set_source_epoch(0, 5).code_ ==
              status_code::ok);
        check(scheduler.vqec_vision_ai_sched_secsd_set_source_epoch(0, 4).code_ ==
              status_code::invalid_argument);
        check(scheduler.vqec_vision_ai_sched_secsd_submit(make_request(1, 0, 4)).code_ ==
              status_code::ok);
        secondary_inference_result result;
        check(scheduler.vqec_vision_ai_sched_secsd_step(1, result).code_ == status_code::ok);
        check(result.stale_epoch_);

        check(scheduler.vqec_vision_ai_sched_secsd_submit(make_request(2, 0, 1)).code_ ==
              status_code::ok);
        check(scheduler.vqec_vision_ai_sched_secsd_submit(make_request(3, 0, 1)).code_ ==
              status_code::ok);
        scheduler.vqec_vision_ai_sched_secsd_cancel_all();
        check(scheduler.vqec_vision_ai_sched_secsd_step(2, result).code_ == status_code::ok);
        check(result.cancelled_ && result.request_.task_id_ == 2);
        check(scheduler.vqec_vision_ai_sched_secsd_step(3, result).code_ == status_code::ok);
        check(result.cancelled_ && result.request_.task_id_ == 3);
        check(scheduler.vqec_vision_ai_sched_secsd_get_snapshot().cancelled_total_ == 2);
        check(scheduler.vqec_vision_ai_sched_secsd_step(4, result).code_ ==
              status_code::pending);
    }

    std::cout << "secondary inference failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
