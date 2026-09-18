#include <cstdint>
#include <iostream>
#include <memory>
#include <string>

#include "vqec_vision_dsp_v1_dense_decoder.hpp"

extern "C" {
#include "vqec_vision_dsp_v1_service.h"
}

namespace {

constexpr std::uint64_t g_fake_handle = 9U;
constexpr std::uint32_t g_domain_generation = 23U;
constexpr const char* g_skeleton_dir = "/tmp/vqec-dsp-v1-dense-decoder-test";

vqec_vision_ai_dsp_v1_service g_service{};

int vqec_vision_ai_unit_d1ddt_open(const char* _uri, std::uint64_t* _handle) {
    if (_uri == nullptr || _handle == nullptr) {
        return -1;
    }
    *_handle = g_fake_handle;
    return 0;
}

int vqec_vision_ai_unit_d1ddt_close(std::uint64_t _handle) {
    return _handle == g_fake_handle ? 0 : -1;
}

int vqec_vision_ai_unit_d1ddt_query(
    std::uint64_t _handle, std::uint8_t* _response, int _response_bytes) {
    if (_handle != g_fake_handle || _response_bytes < 0) {
        return -1;
    }
    return vqec_vision_ai_qcom_d1svc_query_capabilities(
               &g_service, _response, static_cast<std::size_t>(_response_bytes)) ==
            vqec_vision_ai_dsp_v1_wire_ok ? 0 : -1;
}

int vqec_vision_ai_unit_d1ddt_execute(
    std::uint64_t _handle, const std::uint8_t* _descriptor, int _descriptor_bytes,
    const std::uint8_t* _input, int _input_bytes, std::uint8_t* _output,
    int _output_capacity_bytes, std::uint8_t* _response, int _response_bytes) {
    if (_handle != g_fake_handle || _descriptor_bytes < 0 || _input_bytes < 0 ||
        _output_capacity_bytes < 0 || _response_bytes < 0) {
        return -1;
    }
    return vqec_vision_ai_qcom_d1svc_execute(
               &g_service, _descriptor, static_cast<std::size_t>(_descriptor_bytes),
               _input, static_cast<std::size_t>(_input_bytes), _output,
               static_cast<std::size_t>(_output_capacity_bytes), _response,
               static_cast<std::size_t>(_response_bytes)) ==
            vqec_vision_ai_dsp_v1_wire_ok ? 0 : -1;
}

int vqec_vision_ai_unit_d1ddt_prepare() {
    return 0;
}

void vqec_vision_ai_unit_d1ddt_write_u16(
    std::vector<std::uint8_t>& _bytes, std::size_t _offset, std::uint16_t _value) {
    _bytes[_offset] = static_cast<std::uint8_t>(_value);
    _bytes[_offset + 1U] = static_cast<std::uint8_t>(_value >> 8U);
}

vqec::vision::ai::tensor_spec vqec_vision_ai_unit_d1ddt_tensor(
    const std::string& _name, std::uint32_t _channels) {
    using namespace vqec::vision::ai;
    tensor_spec spec;
    spec.name_ = _name;
    spec.dimensions_ = {1U, _channels, 1U};
    spec.dtype_ = tensor_element_type::uint16;
    spec.layout_ = tensor_layout::flat;
    spec.quantization_.is_quantized_ = true;
    spec.quantization_.scale_ = 1.0F;
    spec.quantization_.zero_point_ = 0;
    return spec;
}

bool vqec_vision_ai_unit_d1ddt_run() {
    using namespace vqec::vision::ai;
    (void)vqec_vision_ai_qcom_d1svc_initialize(&g_service, g_domain_generation);
    const dsp_v1_rpc_api api{vqec_vision_ai_unit_d1ddt_open,
        vqec_vision_ai_unit_d1ddt_close, vqec_vision_ai_unit_d1ddt_query,
        vqec_vision_ai_unit_d1ddt_execute, vqec_vision_ai_unit_d1ddt_prepare};
    auto client = std::make_shared<dsp_v1_client>(api);
    dsp_v1_client_config client_config;
    client_config.skel_dir_ = g_skeleton_dir;
    client_config.enable_unsigned_pd_ = true;
    if (client->vqec_vision_ai_qcom_d1cli_open(client_config).code_ != status_code::ok) {
        return false;
    }

    dsp_v1_dense_decoder_config config;
    config.source_width_ = 100U;
    config.source_height_ = 100U;
    config.tensor_width_ = 100U;
    config.tensor_height_ = 100U;
    config.placement_ = image_placement::stretch;
    config.box_tensor_ = "boxes";
    config.score_tensor_ = "scores";
    config.prediction_count_ = 1U;
    config.class_count_ = 2U;
    config.class_names_ = {"smoke", "fire"};
    config.confidence_threshold_ = 0.5F;
    config.iou_threshold_ = 0.5F;
    config.candidate_capacity_ = 2U;
    config.output_capacity_ = 2U;
    config.client_ = client;
    dsp_v1_dense_decoder decoder(config);

    model_outputs outputs;
    outputs.outputs_ = {vqec_vision_ai_unit_d1ddt_tensor("boxes", 4U),
        vqec_vision_ai_unit_d1ddt_tensor("scores", 2U)};
    if (decoder.vqec_vision_ai_cntr_mddec_validate(outputs).code_ != status_code::ok) {
        return false;
    }

    tensor_blob boxes;
    boxes.spec_ = outputs.outputs_[0];
    boxes.bytes_.resize(4U * sizeof(std::uint16_t));
    vqec_vision_ai_unit_d1ddt_write_u16(boxes.bytes_, 0U, 20U);
    vqec_vision_ai_unit_d1ddt_write_u16(boxes.bytes_, 2U, 20U);
    vqec_vision_ai_unit_d1ddt_write_u16(boxes.bytes_, 4U, 10U);
    vqec_vision_ai_unit_d1ddt_write_u16(boxes.bytes_, 6U, 10U);
    tensor_blob scores;
    scores.spec_ = outputs.outputs_[1];
    scores.bytes_.resize(2U * sizeof(std::uint16_t));
    vqec_vision_ai_unit_d1ddt_write_u16(scores.bytes_, 0U, 0U);
    vqec_vision_ai_unit_d1ddt_write_u16(scores.bytes_, 2U, 1U);
    tensor_result result;
    result.tensors_.push_back(std::move(boxes));
    result.tensors_.push_back(std::move(scores));
    preview_frame_key frame;
    observation_batch observations;
    if (decoder.vqec_vision_ai_cntr_mddec_decode(result, frame, observations).code_ !=
        status_code::ok) {
        return false;
    }
    if (observations.observations_.size() != 1U ||
        observations.observations_[0].class_id_ != "fire" ||
        observations.observations_[0].box_.x_ != 15.0F ||
        observations.observations_[0].box_.y_ != 15.0F ||
        observations.observations_[0].box_.width_ != 10.0F ||
        observations.observations_[0].box_.height_ != 10.0F) {
        return false;
    }

    outputs.outputs_[1].dimensions_[1] = 1U;
    return decoder.vqec_vision_ai_cntr_mddec_validate(outputs).code_ ==
        status_code::unsupported;
}

}  // namespace

int main() {
    const bool passed = vqec_vision_ai_unit_d1ddt_run();
    std::cout << "DSP v1 dense decoder adapter: " << (passed ? "passed" : "failed") << '\n';
    return passed ? 0 : 1;
}
