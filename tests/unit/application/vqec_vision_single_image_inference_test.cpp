#include <cassert>
#include <memory>

#include "vqec_vision_single_image_inference.hpp"

using namespace vqec::vision::ai;

namespace {
class test_processor final : public image_processor_port {
public:
    status vqec_vision_ai_ports_imgpr_validate(
        const raw_frame&, const inference_plan&, const tensor_spec&) const override { return {}; }
    status vqec_vision_ai_ports_imgpr_preprocess(
        const raw_frame&, const inference_plan&, const tensor_spec& _target,
        std::vector<tensor_blob>& _outputs) override {
        _outputs = {{_target, std::vector<std::uint8_t>(12, 1)}};
        return {};
    }
};

class test_graph final : public inference_graph_port {
public:
    status vqec_vision_ai_ports_infgr_validate_activation() const override { return {}; }
    status vqec_vision_ai_ports_infgr_configure(const inference_plan&) override { return {}; }
    status vqec_vision_ai_ports_infgr_load() override { return {}; }
    status vqec_vision_ai_ports_infgr_poll_state() override { return {}; }
    status vqec_vision_ai_ports_infgr_bind_source(const source_binding&) override { return {}; }
    status vqec_vision_ai_ports_infgr_start(const std::vector<tensor_spec>&, std::uint64_t) override {
        return {};
    }
    status vqec_vision_ai_ports_infgr_arm(
        std::uint64_t, std::uint64_t _epoch, std::uint64_t,
        submission_sequence_policy) override {
        epoch_ = _epoch;
        return {};
    }
    status vqec_vision_ai_ports_infgr_submit_frame(
        const raw_frame&, std::uint64_t, submission_ticket&) override {
        return {status_code::unsupported, "raw submission disabled"};
    }
    status vqec_vision_ai_ports_infgr_get_input_specs(
        std::vector<tensor_spec>& _inputs) const override {
        _inputs = {input_};
        return {};
    }
    status vqec_vision_ai_ports_infgr_submit_tensors(
        std::uint64_t _epoch, std::uint64_t _frame_id, std::uint64_t,
        const std::vector<tensor_blob>& _inputs, std::uint64_t,
        submission_ticket&) override {
        if (_epoch != epoch_ || _frame_id == 0 || _inputs.size() != 1 || outstanding_ != 0) {
            return {status_code::invalid_argument, "bad synthetic submission"};
        }
        outstanding_ = 1;
        return {};
    }
    status vqec_vision_ai_ports_infgr_poll_result(
        std::uint64_t, tensor_result& _result) override {
        if (outstanding_ == 0) return {status_code::pending, "no result"};
        tensor_blob output;
        output.spec_ = input_;
        output.bytes_.assign(12, 0);
        _result.tensors_ = {std::move(output)};
        outstanding_ = 0;
        return {};
    }
    status vqec_vision_ai_ports_infgr_request_drain() override { return {}; }
    status vqec_vision_ai_ports_infgr_unload() override { return {}; }
    inference_graph_state vqec_vision_ai_ports_infgr_get_state() const noexcept override {
        return inference_graph_state::running;
    }
    unsigned vqec_vision_ai_ports_infgr_get_outstanding() const noexcept override {
        return outstanding_;
    }
    submission_ticket vqec_vision_ai_ports_infgr_get_pending_ticket() const noexcept override {
        return {};
    }
    tensor_spec input_{"input", {1, 2, 2, 3}, tensor_element_type::uint8, {}};
    std::uint64_t epoch_{0};
    unsigned outstanding_{0};
};

class test_decoder final : public model_decoder_port {
public:
    status vqec_vision_ai_cntr_mddec_validate(const model_outputs&) const override { return {}; }
    status vqec_vision_ai_cntr_mddec_decode(
        const tensor_result&, const preview_frame_key& _key,
        observation_batch& _observations) override {
        _observations.frame_ = _key;
        _observations.geometry_ = {64, 48};
        _observations.observations_.push_back(
            {_key, 0, "face", {1, 2, 10, 12, 0xffffffffU, "face"}, 0.9F,
             observation_quality::high, {}, {}});
        return {};
    }
};
}

int main() {
    test_processor processor;
    test_graph graph;
    test_decoder decoder;
    inference_plan plan;
    single_image_inference runner;
    const single_image_inference_config config{
        &processor, &graph, &decoder, &plan, {64, 48}, 2, 3, 1, 1000};
    assert(runner.vqec_vision_ai_appl_siinf_configure(config).code_ == status_code::ok);
    raw_frame frame;
    frame.descriptor_.buffer_id_ = 7;
    frame.descriptor_.session_epoch_ = 9;
    frame.descriptor_.width_ = 64;
    frame.descriptor_.height_ = 48;
    frame.descriptor_.pts_ns_ = 11;
    frame.owner_ = std::make_shared<int>(1);
    observation_batch observations;
    assert(runner.vqec_vision_ai_appl_siinf_run(frame, 12, observations).code_ == status_code::ok);
    assert(observations.frame_.camera_id_ == 2 && observations.observations_.size() == 1);
    const auto retained = observations.frame_;
    frame.descriptor_.width_ = 63;
    assert(runner.vqec_vision_ai_appl_siinf_run(frame, 13, observations).code_ ==
        status_code::invalid_argument);
    assert(observations.frame_.frame_id_ == retained.frame_id_);
    return 0;
}
