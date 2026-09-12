// Proves the pump selects neutral preprocessing + tensor submission when a binding carries
// an image processor, and keeps raw-frame submission otherwise.

#include <array>
#include <cassert>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "vqec_vision_multi_model_pump.hpp"

using namespace vqec::vision::ai;

namespace {

class fake_source final : public raw_source_port {
public:
    [[nodiscard]] status vqec_vision_ai_ports_rawsr_start(int) override {
        state_ = raw_source_state::running;
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_rawsr_receive(
        raw_frame& _frame, int) override {
        frame_descriptor descriptor;
        descriptor.buffer_id_ = ++buffer_id_;
        descriptor.session_epoch_ = 1;
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

    raw_source_state state_{raw_source_state::idle};
    std::uint64_t buffer_id_{0};
};

class fake_graph final : public inference_graph_port {
public:
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
        ++frame_submissions_;
        return {status_code::unsupported, "fake graph uses tensor submission"};
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
        ++tensor_submissions_;
        _ticket = {{7, ++next_job_id_}, 1, _frame_id, 1, 1};
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
        return 0;
    }
    [[nodiscard]] submission_ticket
    vqec_vision_ai_ports_infgr_get_pending_ticket() const noexcept override {
        return {};
    }

    unsigned frame_submissions_{0};
    unsigned tensor_submissions_{0};
    std::uint64_t next_job_id_{0};
};

class fake_processor final : public image_processor_port {
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
        blob.bytes_.assign(static_cast<std::size_t>(8U * 8U * 3U * 4U), 0U);
        _outputs = {std::move(blob)};
        return {};
    }

    unsigned calls_{0};
};

}  // namespace

int main() {
    fake_source source;
    fake_graph graph;
    fake_processor processor;
    assert(source.vqec_vision_ai_ports_rawsr_start(1000).code_ == status_code::ok);

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

    model_cadence_config cadence;
    cadence.source_fps_numerator_ = 25;
    cadence.source_fps_denominator_ = 1;
    cadence.model_count_ = 1;
    cadence.model_fps_numerators_[0] = 25;
    cadence.model_fps_denominators_[0] = 1;

    std::array<multi_model_graph_binding, deployment_limits::g_max_models_per_source>
        bindings{};
    bindings[0].graph_ = &graph;
    bindings[0].cycle_id_ = 7;
    bindings[0].job_timeout_ns_ = 1000000000;
    bindings[0].processor_ = &processor;
    bindings[0].plan_ = &plan;

    multi_model_pump pump(source);
    assert(pump.vqec_vision_ai_appl_mmump_configure(cadence, bindings, 1).code_ ==
           status_code::ok);
    tensor_result result;
    multi_model_pump_report report;
    for (std::uint64_t now = 1; now < 20 && graph.tensor_submissions_ == 0; ++now) {
        (void)pump.vqec_vision_ai_appl_mmump_pump_step(now, result, report);
    }
    assert(graph.tensor_submissions_ == 1);
    assert(graph.frame_submissions_ == 0);
    assert(processor.calls_ == 1);
    return 0;
}
