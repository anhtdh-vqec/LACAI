// Device-free test for the pump's latest_wins/replace_pending mailbox: a busy graph parks
// the newest due preprocessed input and submits it once the graph frees up, while a free
// sibling graph keeps the source progressing.

#include <array>
#include <cassert>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "vqec_vision_multi_model_pump.hpp"

using namespace vqec::vision::ai;

namespace {

class controllable_source final : public raw_source_port {
public:
    [[nodiscard]] status vqec_vision_ai_ports_rawsr_start(int) override {
        state_ = raw_source_state::running;
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_rawsr_receive(
        raw_frame& _frame, int) override {
        frame_descriptor descriptor;
        descriptor.buffer_id_ = ++buffer_id_;
        descriptor.session_epoch_ = epoch_;
        descriptor.width_ = 640;
        descriptor.height_ = 480;
        descriptor.pts_ns_ = buffer_id_ * 40000000ULL;
        descriptor.allocation_size_bytes_ = 640 * 480 * 3 / 2;
        _frame.descriptor_ = descriptor;
        _frame.native_handle_ = 0;
        _frame.owner_ = std::make_shared<int>(0);
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_rawsr_stop(int) override {
        state_ = raw_source_state::stopped;
        return {};
    }
    [[nodiscard]] raw_source_state
    vqec_vision_ai_ports_rawsr_get_state() const noexcept override {
        return state_;
    }
    [[nodiscard]] raw_source_profile
    vqec_vision_ai_ports_rawsr_get_profile() const noexcept override {
        return {640, 480, 25, 1};
    }
    [[nodiscard]] unsigned
    vqec_vision_ai_ports_rawsr_get_outstanding() const noexcept override {
        return 0;
    }

    void set_epoch(std::uint64_t _epoch) noexcept { epoch_ = _epoch; }

    raw_source_state state_{raw_source_state::idle};
    std::uint64_t buffer_id_{0};
    std::uint64_t epoch_{1};
};

class controllable_graph final : public inference_graph_port {
public:
    explicit controllable_graph(bool _hold) : hold_(_hold) {}

    [[nodiscard]] status vqec_vision_ai_ports_infgr_validate_activation() const override {
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_infgr_configure(const inference_plan&) override {
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_infgr_load() override { return {}; }
    [[nodiscard]] status vqec_vision_ai_ports_infgr_poll_state() override { return {}; }
    [[nodiscard]] status vqec_vision_ai_ports_infgr_bind_source(const source_binding&) override {
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_infgr_start(
        const std::vector<tensor_spec>&, std::uint64_t) override {
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_infgr_arm(std::uint64_t, std::uint64_t, std::uint64_t)
        override {
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_infgr_submit_frame(
        const raw_frame&, std::uint64_t, submission_ticket&) override {
        return {status_code::unsupported, "controllable graph uses tensor submission"};
    }
    [[nodiscard]] status vqec_vision_ai_ports_infgr_get_input_specs(
        std::vector<tensor_spec>& _inputs) const override {
        tensor_spec spec;
        spec.name_ = "input";
        spec.dimensions_ = {1, 8, 8, 3};
        spec.dtype_ = tensor_element_type::float32;
        _inputs = {spec};
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_infgr_submit_tensors(
        std::uint64_t, std::uint64_t _frame_id, std::uint64_t, const std::vector<tensor_blob>&,
        std::uint64_t, submission_ticket& _ticket) override {
        last_submitted_frame_id_ = _frame_id;
        submission_count_ = submission_count_ + 1U;
        outstanding_ = hold_;
        _ticket = {{7, ++next_job_id_}, 1, _frame_id, _frame_id, _frame_id};
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_infgr_poll_result(
        std::uint64_t, tensor_result&) override {
        return {status_code::pending, "no result"};
    }
    [[nodiscard]] status vqec_vision_ai_ports_infgr_request_drain() override { return {}; }
    [[nodiscard]] status vqec_vision_ai_ports_infgr_unload() override { return {}; }
    [[nodiscard]] inference_graph_state
    vqec_vision_ai_ports_infgr_get_state() const noexcept override {
        return inference_graph_state::running;
    }
    [[nodiscard]] unsigned
    vqec_vision_ai_ports_infgr_get_outstanding() const noexcept override {
        return outstanding_ ? 1U : 0U;
    }
    [[nodiscard]] submission_ticket
    vqec_vision_ai_ports_infgr_get_pending_ticket() const noexcept override {
        return {};
    }

    void release() noexcept { outstanding_ = false; }

    bool hold_{false};
    bool outstanding_{false};
    std::uint64_t last_submitted_frame_id_{0};
    std::uint64_t submission_count_{0};
    std::uint64_t next_job_id_{0};
};

class counting_processor final : public image_processor_port {
public:
    [[nodiscard]] status vqec_vision_ai_ports_imgpr_validate(
        const raw_frame&, const inference_plan&, const tensor_spec&) const override {
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_imgpr_preprocess(
        const raw_frame&, const inference_plan&, const tensor_spec& _target,
        std::vector<tensor_blob>& _outputs) override {
        ++calls_;
        tensor_blob blob;
        blob.spec_ = _target;
        blob.bytes_.assign(8U * 8U * 3U * 4U, 0U);
        _outputs = {std::move(blob)};
        return {};
    }

    unsigned calls_{0};
};

inference_plan make_plan() {
    inference_plan plan;
    plan.source_width_ = 640;
    plan.source_height_ = 480;
    plan.fps_numerator_ = 25;
    plan.fps_denominator_ = 1;
    plan.tensor_width_ = 8;
    plan.tensor_height_ = 8;
    plan.input_type_ = tensor_element_type::float32;
    plan.channel_order_ = channel_order::rgb;
    plan.placement_ = image_placement::centre;
    plan.mean_ = {0.0, 0.0, 0.0};
    plan.sigma_ = {1.0, 1.0, 1.0};
    plan.model_path_ = "/opt/vqec/models/m.bin";
    plan.backend_path_ = "/usr/lib/libQnnHtp.so";
    plan.system_path_ = "/usr/lib/libQnnSystem.so";
    plan.input_queue_bytes_ = 1048576;
    plan.output_queue_buffers_ = 1;
    return plan;
}

}  // namespace

int main() {
    controllable_source source;
    controllable_graph busy_graph(true);
    controllable_graph free_graph(false);
    counting_processor busy_processor;
    counting_processor free_processor;
    assert(source.vqec_vision_ai_ports_rawsr_start(1000).code_ == status_code::ok);

    const auto plan = make_plan();
    model_cadence_config cadence;
    cadence.source_fps_numerator_ = 25;
    cadence.source_fps_denominator_ = 1;
    cadence.model_count_ = 2;
    cadence.model_fps_numerators_[0] = 5;
    cadence.model_fps_denominators_[0] = 1;
    cadence.model_fps_numerators_[1] = 5;
    cadence.model_fps_denominators_[1] = 1;
    cadence.dispatch_policies_[0] = model_dispatch_policy::replace_pending;
    cadence.dispatch_policies_[1] = model_dispatch_policy::drop_if_busy;

    std::array<multi_model_graph_binding, deployment_limits::g_max_models_per_source> bindings{};
    bindings[0].graph_ = &busy_graph;
    bindings[0].cycle_id_ = 7;
    bindings[0].job_timeout_ns_ = 1000000000;
    bindings[0].processor_ = &busy_processor;
    bindings[0].plan_ = &plan;
    bindings[1].graph_ = &free_graph;
    bindings[1].cycle_id_ = 8;
    bindings[1].job_timeout_ns_ = 1000000000;
    bindings[1].processor_ = &free_processor;
    bindings[1].plan_ = &plan;

    multi_model_pump pump(source);
    assert(pump.vqec_vision_ai_appl_mmump_configure(cadence, bindings, 2).code_ ==
           status_code::ok);
    assert(pump.vqec_vision_ai_appl_mmump_resolve_targets().code_ == status_code::ok);

    tensor_result result;
    multi_model_pump_report report;

    // Frame 1 is due for both slots; the busy graph holds its job.
    (void)pump.vqec_vision_ai_appl_mmump_pump_step(1, result, report);
    assert(report.submitted_model_mask_ == 3 && busy_graph.outstanding_ &&
           busy_graph.last_submitted_frame_id_ == 1);
    assert(busy_processor.calls_ == 1 && free_processor.calls_ == 1);

    // Frames 2..5 are not due for the 5 fps models. The free graph keeps receive alive.
    std::uint16_t pending_mask = 0;
    for (std::uint64_t now = 2; now <= 5; ++now) {
        (void)pump.vqec_vision_ai_appl_mmump_pump_step(now, result, report);
    }
    // Frame 6 is due while the busy graph still holds frame 1: park the newest input.
    (void)pump.vqec_vision_ai_appl_mmump_pump_step(6, result, report);
    pending_mask = report.pending_model_mask_;
    assert((pending_mask & 1U) != 0);
    assert((report.submitted_model_mask_ & 1U) == 0);  // slot 0 not submitted fresh
    assert(busy_processor.calls_ == 2);                // parked preprocess happened
    assert(busy_graph.last_submitted_frame_id_ == 1);  // not replaced in the graph yet

    // Free the busy graph; the parked frame 6 is submitted on the next step.
    busy_graph.release();
    (void)pump.vqec_vision_ai_appl_mmump_pump_step(7, result, report);
    assert(busy_graph.last_submitted_frame_id_ == 6);
    assert(busy_graph.submission_count_ == 2 && busy_processor.calls_ == 2);

    // Section 23: a parked input must not cross a source epoch boundary.
    {
        controllable_source epoch_source;
        controllable_graph epoch_busy(true);
        controllable_graph epoch_free(false);
        counting_processor epoch_busy_processor;
        counting_processor epoch_free_processor;
        assert(epoch_source.vqec_vision_ai_ports_rawsr_start(1000).code_ == status_code::ok);
        std::array<multi_model_graph_binding, deployment_limits::g_max_models_per_source>
            epoch_bindings{};
        epoch_bindings[0].graph_ = &epoch_busy;
        epoch_bindings[0].cycle_id_ = 7;
        epoch_bindings[0].job_timeout_ns_ = 1000000000;
        epoch_bindings[0].processor_ = &epoch_busy_processor;
        epoch_bindings[0].plan_ = &plan;
        epoch_bindings[1].graph_ = &epoch_free;
        epoch_bindings[1].cycle_id_ = 8;
        epoch_bindings[1].job_timeout_ns_ = 1000000000;
        epoch_bindings[1].processor_ = &epoch_free_processor;
        epoch_bindings[1].plan_ = &plan;
        multi_model_pump epoch_pump(epoch_source);
        assert(epoch_pump.vqec_vision_ai_appl_mmump_configure(
                   cadence, epoch_bindings, 2).code_ == status_code::ok);
        assert(epoch_pump.vqec_vision_ai_appl_mmump_resolve_targets().code_ ==
               status_code::ok);
        multi_model_pump_report epoch_report;
        (void)epoch_pump.vqec_vision_ai_appl_mmump_pump_step(1, result, epoch_report);
        for (std::uint64_t now = 2; now <= 5; ++now) {
            (void)epoch_pump.vqec_vision_ai_appl_mmump_pump_step(now, result, epoch_report);
        }
        (void)epoch_pump.vqec_vision_ai_appl_mmump_pump_step(6, result, epoch_report);
        assert((epoch_report.pending_model_mask_ & 1U) != 0);
        // Source restarts on a new epoch before the parked input is flushed.
        epoch_source.set_epoch(2);
        epoch_busy.release();
        (void)epoch_pump.vqec_vision_ai_appl_mmump_pump_step(7, result, epoch_report);
        assert(epoch_busy.submission_count_ == 1 &&  // only frame 1, parked frame 6 dropped
               epoch_busy.last_submitted_frame_id_ == 1);
    }

    return 0;
}
