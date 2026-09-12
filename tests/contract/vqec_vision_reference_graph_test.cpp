// Device-free contract for the reference graph's dtype-generic output path. It proves the
// neutral graph port can carry per-tensor element types and exact packed byte sizes; it is
// not a model, accuracy or hardware claim.

#include <array>
#include <cassert>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "vqec_vision_reference_graph.hpp"

using namespace vqec::vision::ai;

namespace {

inference_plan vqec_vision_ai_ctest_rfgpt_plan() {
    inference_plan plan;
    plan.source_width_ = 640;
    plan.source_height_ = 480;
    plan.fps_numerator_ = 25;
    plan.fps_denominator_ = 1;
    plan.tensor_width_ = 64;
    plan.tensor_height_ = 64;
    plan.input_type_ = tensor_element_type::uint8;
    plan.channel_order_ = channel_order::rgb;
    plan.placement_ = image_placement::centre;
    plan.mean_ = {0.0, 0.0, 0.0};
    plan.sigma_ = {1.0, 1.0, 1.0};
    plan.model_path_ = "/opt/vqec/models/detector.bin";
    plan.backend_path_ = "/usr/lib/libQnnHtp.so";
    plan.system_path_ = "/usr/lib/libQnnSystem.so";
    plan.input_queue_bytes_ = 1048576;
    plan.output_queue_buffers_ = 2;
    return plan;
}

tensor_spec vqec_vision_ai_ctest_rfgpt_spec(
    const char* _name, std::vector<std::uint32_t> _dims, tensor_element_type _dtype) {
    tensor_spec spec;
    spec.name_ = _name;
    spec.dimensions_ = std::move(_dims);
    spec.dtype_ = _dtype;
    return spec;
}

}  // namespace

int main() {
    reference_inference_graph graph;
    assert(graph.vqec_vision_ai_ports_infgr_validate_activation().code_ == status_code::ok);
    assert(graph.vqec_vision_ai_ports_infgr_configure(vqec_vision_ai_ctest_rfgpt_plan()).code_ ==
           status_code::ok);
    assert(graph.vqec_vision_ai_ports_infgr_load().code_ == status_code::ok);
    std::vector<tensor_spec> outputs{
        vqec_vision_ai_ctest_rfgpt_spec("quantized", {1, 4}, tensor_element_type::int8),
        vqec_vision_ai_ctest_rfgpt_spec("half", {1, 2}, tensor_element_type::float16)};
    assert(graph.vqec_vision_ai_ports_infgr_start(outputs, 64).code_ == status_code::ok);
    assert(graph.vqec_vision_ai_ports_infgr_arm(1, 1, 1000000000).code_ == status_code::ok);

    raw_frame frame;
    frame.descriptor_.buffer_id_ = 7;
    frame.descriptor_.session_epoch_ = 1;
    frame.descriptor_.pts_ns_ = 100;
    frame.owner_ = std::make_shared<int>(0);
    submission_ticket ticket;
    assert(graph.vqec_vision_ai_ports_infgr_submit_frame(frame, 5, ticket).code_ ==
           status_code::ok);
    assert(ticket.token_.job_id_ != 0 && graph.vqec_vision_ai_ports_infgr_get_outstanding() == 1);

    tensor_result result;
    assert(graph.vqec_vision_ai_ports_infgr_poll_result(6, result).code_ == status_code::ok);
    assert(result.pipeline_pts_ns_ == ticket.pipeline_pts_ns_);
    assert(result.tensors_.size() == 2);
    assert(result.tensors_[0].spec_.dtype_ == tensor_element_type::int8 &&
           result.tensors_[0].bytes_.size() == 4);
    assert(result.tensors_[1].spec_.dtype_ == tensor_element_type::float16 &&
           result.tensors_[1].bytes_.size() == 4);
    for (const auto& tensor : result.tensors_) {
        for (const auto byte : tensor.bytes_) {
            assert(byte == 0);
        }
    }
    assert(graph.vqec_vision_ai_ports_infgr_get_outstanding() == 0);
    assert(graph.vqec_vision_ai_ports_infgr_request_drain().code_ == status_code::ok);
    assert(graph.vqec_vision_ai_ports_infgr_unload().code_ == status_code::ok);
    return 0;
}
