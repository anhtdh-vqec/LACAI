// Device-free tests for the pump's cascade-root retention: retain before submit, rollback
// when the graph does not accept the job, drop when the borrowed store is full, fail closed
// when no store is bound, and no retention for non-cascade bindings.

#include <array>
#include <cstdint>
#include <iostream>
#include <memory>
#include <utility>

#include "vqec_vision_multi_model_pump.hpp"

namespace {

using namespace vqec::vision::ai;

class fake_source final : public raw_source_port {
public:
    [[nodiscard]] status vqec_vision_ai_ports_rawsr_start(int) override { return {}; }
    [[nodiscard]] status vqec_vision_ai_ports_rawsr_receive(
        raw_frame& _frame, int) override {
        ++receive_calls_;
        if (!has_frame_) {
            return {status_code::timeout, "no frame"};
        }
        _frame = std::move(frame_);
        has_frame_ = false;
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_rawsr_stop(int) override { return {}; }
    [[nodiscard]] raw_source_state vqec_vision_ai_ports_rawsr_get_state() const noexcept
        override {
        return raw_source_state::running;
    }
    [[nodiscard]] raw_source_profile vqec_vision_ai_ports_rawsr_get_profile() const noexcept
        override {
        return {1920, 1080, 30, 1};
    }
    [[nodiscard]] unsigned vqec_vision_ai_ports_rawsr_get_outstanding() const noexcept
        override {
        return has_frame_ ? 1U : 0U;
    }

    void supply(std::uint64_t _buffer_id, std::uint64_t _allocation_bytes,
        std::shared_ptr<const void> _owner) {
        frame_ = {};
        frame_.descriptor_.buffer_id_ = _buffer_id;
        frame_.descriptor_.session_epoch_ = 7;
        frame_.descriptor_.pts_ns_ = _buffer_id * 1000;
        frame_.descriptor_.allocation_size_bytes_ = _allocation_bytes;
        frame_.native_handle_ = 31;
        frame_.owner_ = std::move(_owner);
        has_frame_ = true;
    }

    raw_frame frame_;
    unsigned receive_calls_{0};
    bool has_frame_{false};
};

class fake_graph final : public inference_graph_port {
public:
    [[nodiscard]] status vqec_vision_ai_ports_infgr_validate_activation() const override {
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_infgr_configure(
        const inference_plan&) override {
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_infgr_load() override { return {}; }
    [[nodiscard]] status vqec_vision_ai_ports_infgr_poll_state() override { return {}; }
    [[nodiscard]] status vqec_vision_ai_ports_infgr_bind_source(
        const source_binding&) override {
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_infgr_start(
        const std::vector<tensor_spec>&, std::uint64_t) override {
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_infgr_arm(
        std::uint64_t _cycle_id, std::uint64_t, std::uint64_t) override {
        cycle_id_ = _cycle_id;
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_infgr_submit_frame(
        const raw_frame& _frame, std::uint64_t, submission_ticket& _ticket) override {
        if (!accept_submit_) {
            return {status_code::pending, "fixture graph is busy"};
        }
        ++submit_calls_;
        ticket_ = {};
        ticket_.token_.cycle_id_ = cycle_id_;
        ticket_.token_.job_id_ = _frame.descriptor_.buffer_id_;
        ticket_.source_epoch_ = _frame.descriptor_.session_epoch_;
        ticket_.source_frame_id_ = _frame.descriptor_.buffer_id_;
        ticket_.source_pts_ns_ = _frame.descriptor_.pts_ns_;
        _ticket = ticket_;
        is_outstanding_ = true;
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_infgr_poll_result(
        std::uint64_t, tensor_result&) override {
        if (!is_outstanding_ || !is_result_ready_) {
            return {status_code::pending, "fixture has no result"};
        }
        is_result_ready_ = false;
        is_outstanding_ = false;
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_infgr_request_drain() override {
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_infgr_unload() override { return {}; }
    [[nodiscard]] inference_graph_state vqec_vision_ai_ports_infgr_get_state() const noexcept
        override {
        return inference_graph_state::running;
    }
    [[nodiscard]] unsigned vqec_vision_ai_ports_infgr_get_outstanding() const noexcept
        override {
        return is_outstanding_ ? 1U : 0U;
    }
    [[nodiscard]] submission_ticket vqec_vision_ai_ports_infgr_get_pending_ticket()
        const noexcept override {
        return ticket_;
    }

    void complete_result() noexcept { is_result_ready_ = true; }

    submission_ticket ticket_;
    std::uint64_t cycle_id_{0};
    unsigned submit_calls_{0};
    bool accept_submit_{true};
    bool is_outstanding_{false};
    bool is_result_ready_{false};
};

model_cadence_config make_cadence(std::uint16_t _count) {
    model_cadence_config cadence;
    cadence.source_fps_numerator_ = 30;
    cadence.source_fps_denominator_ = 1;
    cadence.model_count_ = _count;
    for (std::uint16_t slot = 0; slot < _count; ++slot) {
        cadence.model_fps_numerators_[slot] = 30;
        cadence.model_fps_denominators_[slot] = 1;
    }
    return cadence;
}

using binding_array =
    std::array<multi_model_graph_binding, deployment_limits::g_max_models_per_source>;

}  // namespace

int main() {
    unsigned failures = 0;
    unsigned check_index = 0;
    const auto check = [&failures, &check_index](bool _condition) {
        ++check_index;
        if (!_condition) {
            ++failures;
            std::cerr << "failed check " << check_index << "\n";
        }
    };

    // Retain succeeds for a cascade-root binding and charges the store.
    {
        fake_source source;
        fake_graph graph;
        multi_model_pump pump(source);
        binding_array bindings{};
        bindings[0] = {&graph, 11, 1000000, nullptr, nullptr, true};
        cascade_frame_store store(1, 1, 16);
        check(pump.vqec_vision_ai_appl_mmump_configure(make_cadence(1), bindings, 1).code_ ==
            status_code::ok);
        check(pump.vqec_vision_ai_appl_mmump_bind_cascade_store(store, 0, 0).code_ ==
            status_code::ok);
        source.supply(1, 16, std::make_shared<int>(1));
        tensor_result result;
        multi_model_pump_report report;
        check(pump.vqec_vision_ai_appl_mmump_pump_step(100, result, report).code_ ==
            status_code::ok);
        check(report.cascade_retained_model_mask_ == 1 &&
            report.cascade_dropped_model_mask_ == 0 && store.
            vqec_vision_ai_sched_cfstr_bytes() == 16);
    }

    // A rejected submit rolls the retention back: no frame stays charged.
    {
        fake_source source;
        fake_graph graph;
        graph.accept_submit_ = false;
        multi_model_pump pump(source);
        binding_array bindings{};
        bindings[0] = {&graph, 11, 1000000, nullptr, nullptr, true};
        cascade_frame_store store(1, 1, 16);
        check(pump.vqec_vision_ai_appl_mmump_configure(make_cadence(1), bindings, 1).code_ ==
            status_code::ok);
        check(pump.vqec_vision_ai_appl_mmump_bind_cascade_store(store, 0, 0).code_ ==
            status_code::ok);
        source.supply(1, 16, std::make_shared<int>(1));
        tensor_result result;
        multi_model_pump_report report;
        check(pump.vqec_vision_ai_appl_mmump_pump_step(100, result, report).code_ ==
            status_code::pending);
        check(report.cascade_retained_model_mask_ == 0 &&
            report.busy_model_mask_ == 1 && store.vqec_vision_ai_sched_cfstr_bytes() == 0);
    }

    // A full store drops every cascade-root model for that frame, not just one.
    {
        fake_source source;
        fake_graph graph;
        multi_model_pump pump(source);
        binding_array bindings{};
        bindings[0] = {&graph, 11, 1000000, nullptr, nullptr, true};
        cascade_frame_store store(1, 1, 16);
        check(pump.vqec_vision_ai_appl_mmump_configure(make_cadence(1), bindings, 1).code_ ==
            status_code::ok);
        check(pump.vqec_vision_ai_appl_mmump_bind_cascade_store(store, 0, 0).code_ ==
            status_code::ok);
        source.supply(1, 16, std::make_shared<int>(1));
        tensor_result result;
        multi_model_pump_report report;
        check(pump.vqec_vision_ai_appl_mmump_pump_step(100, result, report).code_ ==
            status_code::ok);
        check(report.cascade_retained_model_mask_ == 1 && store.
            vqec_vision_ai_sched_cfstr_bytes() == 16);
        graph.complete_result();
        check(pump.vqec_vision_ai_appl_mmump_pump_step(101, result, report).code_ ==
            status_code::ok);
        check(store.vqec_vision_ai_sched_cfstr_bytes() == 16);
        source.supply(2, 16, std::make_shared<int>(2));
        check(pump.vqec_vision_ai_appl_mmump_pump_step(102, result, report).code_ ==
            status_code::pending);
        check(report.cascade_retained_model_mask_ == 0 &&
            report.cascade_dropped_model_mask_ == 1 && report.submitted_model_mask_ == 0 &&
            store.vqec_vision_ai_sched_cfstr_bytes() == 16);
    }

    // A cascade-root binding with no bound store fails closed before receiving.
    {
        fake_source source;
        fake_graph graph;
        multi_model_pump pump(source);
        binding_array bindings{};
        bindings[0] = {&graph, 11, 1000000, nullptr, nullptr, true};
        check(pump.vqec_vision_ai_appl_mmump_configure(make_cadence(1), bindings, 1).code_ ==
            status_code::ok);
        source.supply(1, 16, std::make_shared<int>(1));
        tensor_result result;
        multi_model_pump_report report;
        check(pump.vqec_vision_ai_appl_mmump_pump_step(100, result, report).code_ ==
            status_code::invalid_state);
        check(source.receive_calls_ == 0);
    }

    // A non-cascade binding never charges the store.
    {
        fake_source source;
        fake_graph graph;
        multi_model_pump pump(source);
        binding_array bindings{};
        bindings[0] = {&graph, 11, 1000000};
        cascade_frame_store store(1, 1, 16);
        check(pump.vqec_vision_ai_appl_mmump_configure(make_cadence(1), bindings, 1).code_ ==
            status_code::ok);
        check(pump.vqec_vision_ai_appl_mmump_bind_cascade_store(store, 0, 0).code_ ==
            status_code::ok);
        source.supply(1, 16, std::make_shared<int>(1));
        tensor_result result;
        multi_model_pump_report report;
        check(pump.vqec_vision_ai_appl_mmump_pump_step(100, result, report).code_ ==
            status_code::ok);
        check(report.cascade_retained_model_mask_ == 0 &&
            store.vqec_vision_ai_sched_cfstr_bytes() == 0);
    }

    std::cout << "multi-model pump cascade failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
