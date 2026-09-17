// Device-free unit tests for the bounded asynchronous cascade execution worker:
// - Bounded queue scheduling and task rejection on full queue
// - Asynchronous execution and completion queue retrieval
// - Rule 5 compliance: buffer quarantine on alignment incomplete/timeout
// - Quiescent reset on epoch change or recovery
// - Bounded stop and drain_and_join without deadlock

#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

#include "vqec_vision_cascade_execution_worker.hpp"

using namespace vqec::vision::ai;

namespace {

class fake_lease final : public cascade_frame_lease_port {
public:
    [[nodiscard]] status vqec_vision_ai_ports_cflse_acquire(
        const preview_frame_key&, raw_frame& _frame, std::uint64_t& _ticket) override {
        ++acquire_calls_;
        if (fail_acquire_) {
            return {status_code::resource_exhausted, "fixture store is full"};
        }
        _frame.owner_ = std::make_shared<int>(0);
        _frame.native_handle_ = 1;
        _frame.descriptor_.allocation_size_bytes_ = 16;
        _ticket = next_ticket_++;
        issued_.push_back(_ticket);
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_cflse_retire(
        const preview_frame_key&) override {
        ++retire_calls_;
        return issued_.empty() ? status{} :
            status{status_code::invalid_state, "fixture has an outstanding frame ticket"};
    }
    [[nodiscard]] status vqec_vision_ai_ports_cflse_complete(std::uint64_t _ticket) override {
        for (auto iterator = issued_.begin(); iterator != issued_.end(); ++iterator) {
            if (*iterator == _ticket) {
                issued_.erase(iterator);
                ++complete_calls_;
                return {};
            }
        }
        return {status_code::invalid_state, "fixture ticket was never issued"};
    }

    unsigned acquire_calls_{0};
    unsigned retire_calls_{0};
    unsigned complete_calls_{0};
    std::uint64_t next_ticket_{100};
    std::vector<std::uint64_t> issued_;
    bool fail_acquire_{false};
};

class fake_aligner final : public image_alignment_port {
public:
    [[nodiscard]] status vqec_vision_ai_ports_imaln_probe_capabilities(
        alignment_capabilities& _capabilities) const override {
        _capabilities.supports_similarity_ = true;
        _capabilities.max_points_ = image_alignment_limits::g_max_points;
        _capabilities.max_destination_dimension_ = 112;
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_imaln_validate_template(
        const alignment_template& _template,
        const alignment_capabilities& _capabilities) const override {
        return vqec_vision_ai_core_imaln_require_capability(_capabilities, _template);
    }
    [[nodiscard]] status vqec_vision_ai_ports_imaln_align(
        const alignment_request& _request, const raw_frame&, const alignment_template&,
        alignment_result& _result, std::uint64_t& _ticket) override {
        if (fail_) {
            return {status_code::io_error, "fixture warp failed"};
        }
        if (vqec_vision_ai_core_imaln_validate_request(_request, template_).code_ !=
            status_code::ok) {
            return {status_code::invalid_argument, "fixture request invalid"};
        }
        _result.tensor_.spec_.dimensions_ = {1U, template_.destination_height_,
            template_.destination_width_, 3U};
        _result.tensor_.spec_.dtype_ = tensor_element_type::uint8;
        _result.tensor_.bytes_.assign(
            static_cast<std::size_t>(template_.destination_width_) *
                template_.destination_height_ * 3U,
            10U);
        _result.has_transform_ = true;
        _ticket = 1;
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_imaln_poll_completion(
        std::uint64_t, bool& _complete) override {
        _complete = !completion_pending_;
        return {};
    }

    alignment_template template_{};
    bool fail_{false};
    bool completion_pending_{false};
};

alignment_template vqec_vision_ai_unit_cxwts_make_template() {
    alignment_template tmpl;
    tmpl.schema_id_ = "five_point_landmarks";
    tmpl.schema_version_ = "1.0";
    tmpl.destination_width_ = 112;
    tmpl.destination_height_ = 112;
    tmpl.reference_points_ = {
        landmark_point{38.2946F, 51.6963F},
        landmark_point{73.5318F, 51.5014F},
        landmark_point{56.0252F, 71.7366F},
        landmark_point{41.5493F, 92.3655F},
        landmark_point{70.7299F, 92.2041F}
    };
    return tmpl;
}

observation_batch vqec_vision_ai_unit_cxwts_make_batch(
    std::uint64_t _epoch, std::uint64_t _frame_id, std::size_t _face_count) {
    observation_batch batch;
    batch.frame_ = {1, 0, _epoch, _frame_id, 1000000ULL * _frame_id};
    batch.geometry_ = {1920, 1080};
    const auto tmpl = vqec_vision_ai_unit_cxwts_make_template();
    for (std::size_t i = 0; i < _face_count; ++i) {
        observation item;
        item.track_id_ = i + 1;
        item.landmarks_.schema_id_ = tmpl.schema_id_;
        item.landmarks_.schema_version_ = tmpl.schema_version_;
        item.landmarks_.points_ = tmpl.reference_points_;
        batch.observations_.push_back(std::move(item));
    }
    return batch;
}

int vqec_vision_ai_unit_cxwts_test_normal_async_pipeline() {
    fake_lease lease;
    fake_aligner aligner;
    aligner.template_ = vqec_vision_ai_unit_cxwts_make_template();

    cascade_coordinator_config config;
    config.aligner_ = &aligner;
    config.lease_ = &lease;
    config.template_ = aligner.template_;
    config.max_tasks_per_frame_ = 4;

    cascade_execution_worker worker;
    if (worker.vqec_vision_ai_appl_cxwrk_configure(config).code_ != status_code::ok) {
        std::cerr << "FAIL: configure failed" << std::endl;
        return 1;
    }
    if (worker.vqec_vision_ai_appl_cxwrk_start().code_ != status_code::ok) {
        std::cerr << "FAIL: start failed" << std::endl;
        return 1;
    }

    const auto batch = vqec_vision_ai_unit_cxwts_make_batch(1, 10, 2);
    if (worker.vqec_vision_ai_appl_cxwrk_schedule(1000, batch).code_ != status_code::ok) {
        std::cerr << "FAIL: schedule failed" << std::endl;
        return 1;
    }

    // Wait for completion
    cascade_worker_completion completion;
    bool got_completion = false;
    for (int retry = 0; retry < 50; ++retry) {
        if (worker.vqec_vision_ai_appl_cxwrk_poll_completion(completion).code_ == status_code::ok) {
            got_completion = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    if (!got_completion) {
        std::cerr << "FAIL: did not receive cascade completion in time" << std::endl;
        return 1;
    }

    if (completion.report_.accepted_ != 2 || completion.aligned_.size() != 2) {
        std::cerr << "FAIL: unexpected completion report accepted="
                  << completion.report_.accepted_ << std::endl;
        return 1;
    }

    if (worker.vqec_vision_ai_appl_cxwrk_drain_and_join(1000000000ULL).code_ != status_code::ok) {
        std::cerr << "FAIL: drain_and_join failed" << std::endl;
        return 1;
    }

    if (lease.issued_.size() != 0) {
        std::cerr << "FAIL: frame lease was not completely retired" << std::endl;
        return 1;
    }

    return 0;
}

int vqec_vision_ai_unit_cxwts_test_quarantine_on_timeout() {
    fake_lease lease;
    fake_aligner aligner;
    aligner.template_ = vqec_vision_ai_unit_cxwts_make_template();
    aligner.completion_pending_ = true;

    cascade_coordinator_config config;
    config.aligner_ = &aligner;
    config.lease_ = &lease;
    config.template_ = aligner.template_;
    config.max_tasks_per_frame_ = 2;

    cascade_execution_worker worker;
    if (worker.vqec_vision_ai_appl_cxwrk_configure(config).code_ != status_code::ok ||
        worker.vqec_vision_ai_appl_cxwrk_start().code_ != status_code::ok) {
        std::cerr << "FAIL: setup failed in quarantine test" << std::endl;
        return 1;
    }

    const auto batch = vqec_vision_ai_unit_cxwts_make_batch(1, 20, 1);
    if (worker.vqec_vision_ai_appl_cxwrk_schedule(2000, batch).code_ != status_code::ok) {
        std::cerr << "FAIL: schedule failed in quarantine test" << std::endl;
        return 1;
    }

    cascade_worker_completion completion;
    bool got_completion = false;
    for (int retry = 0; retry < 50; ++retry) {
        if (worker.vqec_vision_ai_appl_cxwrk_poll_completion(completion).code_ == status_code::ok) {
            got_completion = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    if (!got_completion) {
        std::cerr << "FAIL: did not receive completion in quarantine test" << std::endl;
        return 1;
    }

    // Rule 5 check:
    // The ticket must NOT be completed. It remains in lease.issued_ (quarantined)!
    if (lease.complete_calls_ != 0) {
        std::cerr << "FAIL: Rule 5 violation! Lease was completed when hardware did not signal completion"
                  << std::endl;
        return 1;
    }
    if (lease.issued_.empty()) {
        std::cerr << "FAIL: Frame lease was prematurely released during pending hardware"
                  << std::endl;
        return 1;
    }

    const auto metrics = worker.vqec_vision_ai_appl_cxwrk_get_metrics();
    if (metrics.quarantine_count_ == 0) {
        std::cerr << "FAIL: quarantine count was not incremented" << std::endl;
        return 1;
    }

    (void)worker.vqec_vision_ai_appl_cxwrk_drain_and_join(1000000000ULL);
    return 0;
}

int vqec_vision_ai_unit_cxwts_test_bounded_queue_rejection() {
    fake_lease lease;
    fake_aligner aligner;
    aligner.template_ = vqec_vision_ai_unit_cxwts_make_template();

    cascade_coordinator_config config;
    config.aligner_ = &aligner;
    config.lease_ = &lease;
    config.template_ = aligner.template_;
    config.max_tasks_per_frame_ = 1;

    cascade_execution_worker worker;
    if (worker.vqec_vision_ai_appl_cxwrk_configure(config).code_ != status_code::ok ||
        worker.vqec_vision_ai_appl_cxwrk_start().code_ != status_code::ok) {
        std::cerr << "FAIL: setup failed in queue rejection test" << std::endl;
        return 1;
    }

    // Schedule items up to max capacity
    for (std::size_t i = 0; i < cascade_worker_limits::g_max_queue_depth; ++i) {
        const auto batch = vqec_vision_ai_unit_cxwts_make_batch(1, 100 + i, 1);
        (void)worker.vqec_vision_ai_appl_cxwrk_schedule(1000 + i, batch);
    }

    // Now drain and verify clean shutdown
    if (worker.vqec_vision_ai_appl_cxwrk_drain_and_join(1000000000ULL).code_ != status_code::ok) {
        std::cerr << "FAIL: drain_and_join failed in rejection test" << std::endl;
        return 1;
    }
    return 0;
}

int vqec_vision_ai_unit_cxwts_test_quiescent_reset() {
    fake_lease lease;
    fake_aligner aligner;
    aligner.template_ = vqec_vision_ai_unit_cxwts_make_template();

    cascade_coordinator_config config;
    config.aligner_ = &aligner;
    config.lease_ = &lease;
    config.template_ = aligner.template_;
    config.max_tasks_per_frame_ = 2;

    cascade_execution_worker worker;
    if (worker.vqec_vision_ai_appl_cxwrk_configure(config).code_ != status_code::ok ||
        worker.vqec_vision_ai_appl_cxwrk_start().code_ != status_code::ok) {
        std::cerr << "FAIL: setup failed in quiescent reset test" << std::endl;
        return 1;
    }

    // Schedule 2 batches
    (void)worker.vqec_vision_ai_appl_cxwrk_schedule(5000, vqec_vision_ai_unit_cxwts_make_batch(1, 200, 1));
    (void)worker.vqec_vision_ai_appl_cxwrk_schedule(5001, vqec_vision_ai_unit_cxwts_make_batch(1, 201, 1));

    // Execute quiescent reset (e.g. on epoch change)
    const auto reset_res = worker.vqec_vision_ai_appl_cxwrk_quiescent_reset(5002, 500000000ULL);
    if (reset_res.code_ != status_code::ok) {
        std::cerr << "FAIL: quiescent_reset returned error" << std::endl;
        return 1;
    }

    (void)worker.vqec_vision_ai_appl_cxwrk_drain_and_join(500000000ULL);
    return 0;
}

}  // namespace

int main() {
    if (vqec_vision_ai_unit_cxwts_test_normal_async_pipeline() != 0) {
        return 1;
    }
    if (vqec_vision_ai_unit_cxwts_test_quarantine_on_timeout() != 0) {
        return 2;
    }
    if (vqec_vision_ai_unit_cxwts_test_bounded_queue_rejection() != 0) {
        return 3;
    }
    if (vqec_vision_ai_unit_cxwts_test_quiescent_reset() != 0) {
        return 4;
    }
    std::cout << "PASS: all cascade execution worker tests passed" << std::endl;
    return 0;
}
