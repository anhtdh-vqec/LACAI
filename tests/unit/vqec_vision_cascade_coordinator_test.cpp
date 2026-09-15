// Device-free tests for the cascade coordinator: bounded per-frame task admission, exact
// acquire/complete/retire ordering, and per-task fault isolation.

#include <cstdint>
#include <iostream>
#include <memory>
#include <vector>

#include "vqec_vision_cascade_coordinator.hpp"

using namespace vqec::vision::ai;

namespace {

class fake_lease final : public cascade_frame_lease_port {
public:
    [[nodiscard]] status vqec_vision_ai_ports_cflse_acquire(
        const preview_frame_key&, raw_frame& _frame, std::uint64_t& _ticket) override {
        if (fail_acquire_) {
            ++acquire_calls_;
            return {status_code::resource_exhausted, "fixture store is full"};
        }
        _frame.owner_ = std::make_shared<int>(0);
        _frame.native_handle_ = 1;
        _frame.descriptor_.allocation_size_bytes_ = 16;
        _ticket = next_ticket_++;
        ++acquire_calls_;
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_cflse_retire(
        const preview_frame_key&) override {
        ++retire_calls_;
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_cflse_complete(std::uint64_t) override {
        ++complete_calls_;
        return {};
    }

    unsigned acquire_calls_{0};
    unsigned retire_calls_{0};
    unsigned complete_calls_{0};
    std::uint64_t next_ticket_{1};
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
        _result.tensor_.bytes_ = std::vector<std::uint8_t>(64U, 0U);
        _result.has_transform_ = true;
        _ticket = 1;
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_imaln_poll_completion(
        std::uint64_t, bool& _complete) override {
        _complete = true;
        return {};
    }

    alignment_template template_;
    bool fail_{false};
};

alignment_template make_template() {
    alignment_template value;
    value.schema_id_ = "face.5pt";
    value.schema_version_ = "1";
    value.destination_width_ = 112;
    value.destination_height_ = 112;
    value.reference_points_ = {{0.0F, 0.0F}, {112.0F, 0.0F}, {112.0F, 112.0F},
        {0.0F, 112.0F}};
    return value;
}

observation_batch make_batch(unsigned _count) {
    observation_batch batch;
    batch.frame_ = {0U, 0U, 1U, 1U, 1000U};
    for (unsigned index = 0; index < _count; ++index) {
        observation item;
        item.frame_ = batch.frame_;
        item.landmarks_.schema_id_ = "face.5pt";
        item.landmarks_.schema_version_ = "1";
        item.landmarks_.points_ = {{0.0F, 0.0F}, {112.0F, 0.0F}, {112.0F, 112.0F},
            {0.0F, 112.0F}};
        batch.observations_.push_back(std::move(item));
    }
    return batch;
}

cascade_coordinator_config make_config(
    fake_aligner& _aligner, fake_lease& _lease, std::size_t _max_tasks) {
    cascade_coordinator_config config;
    config.aligner_ = &_aligner;
    config.lease_ = &_lease;
    config.template_ = make_template();
    config.max_tasks_per_frame_ = _max_tasks;
    _aligner.template_ = config.template_;
    return config;
}

}  // namespace

int main() {
    unsigned failures = 0;
    const auto check = [&failures](bool _condition) {
        if (!_condition) {
            ++failures;
        }
    };

    // Two of three faces are admitted; each acquires, completes, and admission is retired.
    {
        fake_aligner aligner;
        fake_lease lease;
        cascade_coordinator coordinator;
        check(coordinator.vqec_vision_ai_appl_cscrd_configure(
                  make_config(aligner, lease, 2)).code_ == status_code::ok);
        std::vector<alignment_result> aligned;
        cascade_coordinator_report report;
        const auto batch = make_batch(3);
        check(coordinator.vqec_vision_ai_appl_cscrd_process(batch, aligned, report).code_ ==
            status_code::ok);
        check(report.accepted_ == 2 && report.skipped_ == 1 && report.failed_ == 0);
        check(aligned.size() == 2 && lease.acquire_calls_ == 2 &&
            lease.complete_calls_ == 2 && lease.retire_calls_ == 1);
    }

    // An unconfigured coordinator fails closed.
    {
        cascade_coordinator coordinator;
        std::vector<alignment_result> aligned;
        cascade_coordinator_report report;
        check(coordinator.vqec_vision_ai_appl_cscrd_process(
                  make_batch(1), aligned, report).code_ == status_code::invalid_state);
    }

    // Alignment failure is isolated, the ticket is still completed and admission retired.
    {
        fake_aligner aligner;
        aligner.fail_ = true;
        fake_lease lease;
        cascade_coordinator coordinator;
        check(coordinator.vqec_vision_ai_appl_cscrd_configure(
                  make_config(aligner, lease, 2)).code_ == status_code::ok);
        std::vector<alignment_result> aligned;
        cascade_coordinator_report report;
        check(coordinator.vqec_vision_ai_appl_cscrd_process(
                  make_batch(2), aligned, report).code_ == status_code::ok);
        check(report.accepted_ == 0 && report.failed_ == 2 && aligned.empty() &&
            lease.acquire_calls_ == 2 && lease.complete_calls_ == 2 &&
            lease.retire_calls_ == 1);
    }

    // Acquire failure is counted and no ticket is completed; admission is still retired.
    {
        fake_aligner aligner;
        fake_lease lease;
        lease.fail_acquire_ = true;
        cascade_coordinator coordinator;
        check(coordinator.vqec_vision_ai_appl_cscrd_configure(
                  make_config(aligner, lease, 2)).code_ == status_code::ok);
        std::vector<alignment_result> aligned;
        cascade_coordinator_report report;
        check(coordinator.vqec_vision_ai_appl_cscrd_process(
                  make_batch(2), aligned, report).code_ == status_code::ok);
        check(report.accepted_ == 0 && report.failed_ == 2 && aligned.empty() &&
            lease.complete_calls_ == 0 && lease.retire_calls_ == 1);
    }

    // A zero task budget is rejected at configuration.
    {
        fake_aligner aligner;
        fake_lease lease;
        cascade_coordinator coordinator;
        check(coordinator.vqec_vision_ai_appl_cscrd_configure(
                  make_config(aligner, lease, 0)).code_ == status_code::invalid_argument);
    }

    std::cout << "cascade coordinator failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
