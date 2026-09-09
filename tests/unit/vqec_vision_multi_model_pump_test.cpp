#include <array>
#include <cstdint>
#include <iostream>
#include <memory>
#include <utility>

#include "vqec_vision_multi_model_pump.hpp"

namespace {

class fake_raw_source final : public vqec::vision::ai::raw_source_port {
public:
    [[nodiscard]] vqec::vision::ai::status vqec_vision_ai_ports_rawsr_start(
        int _timeout_ms) override {
        (void)_timeout_ms;
        state_ = vqec::vision::ai::raw_source_state::running;
        return {};
    }

    [[nodiscard]] vqec::vision::ai::status vqec_vision_ai_ports_rawsr_receive(
        vqec::vision::ai::raw_frame& _frame, int _timeout_ms) override {
        (void)_timeout_ms;
        ++receive_calls_;
        if (!has_frame_) {
            return {vqec::vision::ai::status_code::timeout, "fixture has no frame"};
        }
        _frame = std::move(frame_);
        has_frame_ = false;
        return {};
    }

    [[nodiscard]] vqec::vision::ai::status vqec_vision_ai_ports_rawsr_stop(
        int _timeout_ms) override {
        (void)_timeout_ms;
        state_ = vqec::vision::ai::raw_source_state::stopped;
        return {};
    }

    [[nodiscard]] vqec::vision::ai::raw_source_state
    vqec_vision_ai_ports_rawsr_get_state() const noexcept override {
        return state_;
    }

    [[nodiscard]] vqec::vision::ai::raw_source_profile
    vqec_vision_ai_ports_rawsr_get_profile() const noexcept override {
        return {1920, 1080, 30, 1};
    }

    [[nodiscard]] unsigned
    vqec_vision_ai_ports_rawsr_get_outstanding() const noexcept override {
        return has_frame_ ? 1U : 0U;
    }

    void vqec_vision_ai_unit_mmpst_supply_frame(
        std::uint64_t _buffer_id, std::shared_ptr<const void> _owner) {
        frame_ = {};
        frame_.descriptor_.buffer_id_ = _buffer_id;
        frame_.descriptor_.session_epoch_ = 7;
        frame_.descriptor_.pts_ns_ = _buffer_id * 1000;
        frame_.native_handle_ = 31;
        frame_.owner_ = std::move(_owner);
        has_frame_ = true;
    }

    vqec::vision::ai::raw_source_state state_{
        vqec::vision::ai::raw_source_state::running};
    vqec::vision::ai::raw_frame frame_;
    unsigned receive_calls_{0};
    bool has_frame_{false};
};

class fake_inference_graph final : public vqec::vision::ai::inference_graph_port {
public:
    [[nodiscard]] vqec::vision::ai::status
    vqec_vision_ai_ports_infgr_validate_activation() const override {
        return {};
    }

    [[nodiscard]] vqec::vision::ai::status vqec_vision_ai_ports_infgr_configure(
        const vqec::vision::ai::inference_plan& _plan) override {
        (void)_plan;
        return {};
    }

    [[nodiscard]] vqec::vision::ai::status
    vqec_vision_ai_ports_infgr_load() override {
        return {};
    }

    [[nodiscard]] vqec::vision::ai::status
    vqec_vision_ai_ports_infgr_poll_state() override {
        return {};
    }

    [[nodiscard]] vqec::vision::ai::status vqec_vision_ai_ports_infgr_bind_source(
        const vqec::vision::ai::source_binding& _binding) override {
        (void)_binding;
        return {};
    }

    [[nodiscard]] vqec::vision::ai::status vqec_vision_ai_ports_infgr_start(
        const std::vector<vqec::vision::ai::float_tensor_spec>& _outputs,
        std::uint64_t _max_output_bytes) override {
        (void)_outputs;
        (void)_max_output_bytes;
        return {};
    }

    [[nodiscard]] vqec::vision::ai::status vqec_vision_ai_ports_infgr_arm(
        std::uint64_t _cycle_id, std::uint64_t _source_epoch,
        std::uint64_t _job_timeout_ns) override {
        (void)_source_epoch;
        (void)_job_timeout_ns;
        cycle_id_ = _cycle_id;
        ++arm_calls_;
        return {};
    }

    [[nodiscard]] vqec::vision::ai::status vqec_vision_ai_ports_infgr_submit_frame(
        const vqec::vision::ai::raw_frame& _frame, std::uint64_t _steady_now_ns,
        vqec::vision::ai::submission_ticket& _ticket) override {
        (void)_steady_now_ns;
        if (is_outstanding_) {
            return {vqec::vision::ai::status_code::pending, "fixture graph is busy"};
        }
        ++submit_calls_;
        owner_ = _frame.owner_;
        ticket_.token_.cycle_id_ = cycle_id_;
        ticket_.token_.job_id_ = _frame.descriptor_.buffer_id_;
        ticket_.source_pts_ns_ = _frame.descriptor_.pts_ns_;
        ticket_.pipeline_pts_ns_ = _frame.descriptor_.pts_ns_;
        _ticket = ticket_;
        is_outstanding_ = true;
        return {};
    }

    [[nodiscard]] vqec::vision::ai::status vqec_vision_ai_ports_infgr_poll_result(
        std::uint64_t _steady_now_ns,
        vqec::vision::ai::tensor_result& _result) override {
        (void)_steady_now_ns;
        if (!is_outstanding_ || !is_result_ready_) {
            return {vqec::vision::ai::status_code::pending, "fixture result is pending"};
        }
        _result.pipeline_pts_ns_ = ticket_.pipeline_pts_ns_;
        is_result_ready_ = false;
        is_outstanding_ = false;
        owner_.reset();
        return {};
    }

    [[nodiscard]] vqec::vision::ai::status
    vqec_vision_ai_ports_infgr_request_drain() override {
        return {};
    }

    [[nodiscard]] vqec::vision::ai::status
    vqec_vision_ai_ports_infgr_unload() override {
        return {};
    }

    [[nodiscard]] vqec::vision::ai::inference_graph_state
    vqec_vision_ai_ports_infgr_get_state() const noexcept override {
        return vqec::vision::ai::inference_graph_state::running;
    }

    [[nodiscard]] unsigned
    vqec_vision_ai_ports_infgr_get_outstanding() const noexcept override {
        return is_outstanding_ ? 1U : 0U;
    }

    [[nodiscard]] vqec::vision::ai::submission_ticket
    vqec_vision_ai_ports_infgr_get_pending_ticket() const noexcept override {
        return ticket_;
    }

    void vqec_vision_ai_unit_mmpst_complete_result() noexcept {
        is_result_ready_ = true;
    }

    void vqec_vision_ai_unit_mmpst_force_busy() noexcept {
        ticket_.token_.cycle_id_ = 99;
        ticket_.token_.job_id_ = 99;
        is_outstanding_ = true;
        is_result_ready_ = false;
    }

    [[nodiscard]] const void* vqec_vision_ai_unit_mmpst_get_owner() const noexcept {
        return owner_.get();
    }

    std::shared_ptr<const void> owner_;
    vqec::vision::ai::submission_ticket ticket_;
    std::uint64_t cycle_id_{0};
    unsigned arm_calls_{0};
    unsigned submit_calls_{0};
    bool is_outstanding_{false};
    bool is_result_ready_{false};
};

vqec::vision::ai::model_cadence_config
vqec_vision_ai_unit_mmpst_make_cadence() {
    vqec::vision::ai::model_cadence_config cadence;
    cadence.source_fps_numerator_ = 30;
    cadence.source_fps_denominator_ = 1;
    cadence.model_count_ = 2;
    cadence.model_fps_numerators_[0] = 30;
    cadence.model_fps_denominators_[0] = 1;
    cadence.model_fps_numerators_[1] = 30;
    cadence.model_fps_denominators_[1] = 1;
    return cadence;
}

std::array<vqec::vision::ai::multi_model_graph_binding,
    vqec::vision::ai::deployment_limits::g_max_models_per_source>
vqec_vision_ai_unit_mmpst_make_bindings(
    fake_inference_graph& _first, fake_inference_graph& _second) {
    std::array<vqec::vision::ai::multi_model_graph_binding,
        vqec::vision::ai::deployment_limits::g_max_models_per_source> bindings{};
    bindings[0] = {&_first, 11, 1000000};
    bindings[1] = {&_second, 12, 1000000};
    return bindings;
}

}  // namespace

int main() {
    using namespace vqec::vision::ai;
    unsigned failures = 0;
    const auto check = [&failures](bool _condition) {
        if (!_condition) {
            ++failures;
        }
    };

    fake_raw_source source;
    fake_inference_graph first;
    fake_inference_graph second;
    multi_model_pump pump(source);
    const auto cadence = vqec_vision_ai_unit_mmpst_make_cadence();
    const auto bindings = vqec_vision_ai_unit_mmpst_make_bindings(first, second);
    check(pump.vqec_vision_ai_appl_mmump_configure(cadence, bindings, 2).code_ ==
          status_code::ok);

    auto frame_owner = std::make_shared<int>(7);
    std::weak_ptr<int> weak_owner = frame_owner;
    source.vqec_vision_ai_unit_mmpst_supply_frame(1, frame_owner);
    frame_owner.reset();
    tensor_result result;
    multi_model_pump_report report;
    check(pump.vqec_vision_ai_appl_mmump_pump_step(100, result, report).code_ ==
          status_code::ok);
    check(source.receive_calls_ == 1 && report.due_model_mask_ == 3 &&
          report.submitted_model_mask_ == 3 && report.busy_model_mask_ == 0);
    check(first.arm_calls_ == 1 && second.arm_calls_ == 1 &&
          first.submit_calls_ == 1 && second.submit_calls_ == 1);
    check(first.vqec_vision_ai_unit_mmpst_get_owner() ==
          second.vqec_vision_ai_unit_mmpst_get_owner());
    check(!weak_owner.expired());

    first.vqec_vision_ai_unit_mmpst_complete_result();
    check(pump.vqec_vision_ai_appl_mmump_pump_step(101, result, report).code_ ==
          status_code::ok);
    check(report.has_result_ && report.result_model_slot_ == 0 &&
          !weak_owner.expired());
    second.vqec_vision_ai_unit_mmpst_complete_result();
    check(pump.vqec_vision_ai_appl_mmump_pump_step(102, result, report).code_ ==
          status_code::ok);
    check(report.has_result_ && report.result_model_slot_ == 1 && weak_owner.expired());

    first.vqec_vision_ai_unit_mmpst_force_busy();
    second.vqec_vision_ai_unit_mmpst_force_busy();
    source.vqec_vision_ai_unit_mmpst_supply_frame(2, std::make_shared<int>(8));
    check(pump.vqec_vision_ai_appl_mmump_pump_step(103, result, report).code_ ==
          status_code::pending);
    check(source.receive_calls_ == 1);

    fake_raw_source partial_source;
    fake_inference_graph busy_graph;
    fake_inference_graph idle_graph;
    busy_graph.vqec_vision_ai_unit_mmpst_force_busy();
    multi_model_pump partial_pump(partial_source);
    const auto partial_bindings =
        vqec_vision_ai_unit_mmpst_make_bindings(busy_graph, idle_graph);
    check(partial_pump.vqec_vision_ai_appl_mmump_configure(
              cadence, partial_bindings, 2).code_ == status_code::ok);
    partial_source.vqec_vision_ai_unit_mmpst_supply_frame(
        1, std::make_shared<int>(9));
    check(partial_pump.vqec_vision_ai_appl_mmump_pump_step(
              200, result, report).code_ == status_code::ok);
    check(report.due_model_mask_ == 3 && report.busy_model_mask_ == 1 &&
          report.submitted_model_mask_ == 2 && partial_source.receive_calls_ == 1);

    fake_raw_source stopped_source;
    fake_inference_graph stop_first;
    fake_inference_graph stop_second;
    multi_model_pump stopped_pump(stopped_source);
    const auto stop_bindings =
        vqec_vision_ai_unit_mmpst_make_bindings(stop_first, stop_second);
    check(stopped_pump.vqec_vision_ai_appl_mmump_configure(
              cadence, stop_bindings, 2).code_ == status_code::ok);
    stopped_source.vqec_vision_ai_unit_mmpst_supply_frame(
        1, std::make_shared<int>(10));
    stopped_pump.vqec_vision_ai_appl_mmump_begin_stop();
    check(stopped_pump.vqec_vision_ai_appl_mmump_pump_step(
              300, result, report).code_ == status_code::pending);
    check(stopped_source.receive_calls_ == 0);

    fake_raw_source invalid_source;
    multi_model_pump invalid_pump(invalid_source);
    auto duplicate_bindings = bindings;
    duplicate_bindings[1].graph_ = duplicate_bindings[0].graph_;
    check(invalid_pump.vqec_vision_ai_appl_mmump_configure(
              cadence, duplicate_bindings, 2).code_ == status_code::invalid_argument);
    check(invalid_pump.vqec_vision_ai_appl_mmump_get_model_count() == 0);

    std::cout << "multi-model pump failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
