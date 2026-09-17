#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "vqec_vision_dsp_session.hpp"
#include "vqec_vision_dsp_decoder.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_tensor_result.hpp"

namespace {

int vqec_vision_ai_unit_dspdt_test_dsp_session_describe() {
    const std::string desc_zero = vqec::vision::ai::dsp_session::vqec_vision_ai_qcom_dspsn_describe(0);
    if (desc_zero.find("0x00000000") == std::string::npos) {
        std::cerr << "Expected 0x00000000 in description\n";
        return 1;
    }
    const std::string desc_noslot = vqec::vision::ai::dsp_session::vqec_vision_ai_qcom_dspsn_describe(static_cast<int>(0x80000601));
    if (desc_noslot.find("VQEC_DSP_E_NOSLOT") == std::string::npos) {
        std::cerr << "Expected VQEC_DSP_E_NOSLOT in description\n";
        return 1;
    }
    return 0;
}

int vqec_vision_ai_unit_dspdt_test_yolov8_decode() {
    using namespace vqec::vision::ai;

    dsp_decoder_config config;
    config.kind_ = dsp_decoder_kind::yolov8;
    config.source_width_ = 1920;
    config.source_height_ = 1080;
    config.tensor_width_ = 640;
    config.tensor_height_ = 640;
    config.confidence_threshold_ = 0.25F;
    config.iou_threshold_ = 0.45F;
    config.class_id_ = "person";

    dsp_decoder decoder(config);

    // Build synthetic tensor result
    tensor_result result;
    constexpr std::size_t kAnchors = 8400;
    constexpr float kBoxScale = 0.01038312166929245F;
    constexpr float kConfScale = 1.52587890625e-05F;

    tensor_blob box_blob;
    box_blob.spec_.name_ = "boxes_out";
    box_blob.spec_.dtype_ = tensor_element_type::uint16;
    box_blob.spec_.dimensions_ = {1, 4, static_cast<std::uint32_t>(kAnchors)};
    box_blob.spec_.quantization_ = {true, kBoxScale, 0};
    box_blob.bytes_.assign(4 * kAnchors * sizeof(std::uint16_t), 0);

    tensor_blob conf_blob;
    conf_blob.spec_.name_ = "conf_out";
    conf_blob.spec_.dtype_ = tensor_element_type::uint16;
    conf_blob.spec_.dimensions_ = {1, 1, static_cast<std::uint32_t>(kAnchors)};
    conf_blob.spec_.quantization_ = {true, kConfScale, 0};
    conf_blob.bytes_.assign(1 * kAnchors * sizeof(std::uint16_t), 0);

    // Set one detection at anchor 100
    // Center (320, 320), size (100, 200) in 640x640 letterbox tensor space
    auto* boxes_u16 = reinterpret_cast<std::uint16_t*>(box_blob.bytes_.data());
    auto* conf_u16 = reinterpret_cast<std::uint16_t*>(conf_blob.bytes_.data());

    const std::size_t idx = 100;
    boxes_u16[0 * kAnchors + idx] = static_cast<std::uint16_t>(std::round(320.0F / kBoxScale)); // cx
    boxes_u16[1 * kAnchors + idx] = static_cast<std::uint16_t>(std::round(320.0F / kBoxScale)); // cy
    boxes_u16[2 * kAnchors + idx] = static_cast<std::uint16_t>(std::round(100.0F / kBoxScale)); // w
    boxes_u16[3 * kAnchors + idx] = static_cast<std::uint16_t>(std::round(200.0F / kBoxScale)); // h
    conf_u16[idx] = static_cast<std::uint16_t>(std::round(0.85F / kConfScale)); // conf 0.85

    result.tensors_.push_back(std::move(box_blob));
    result.tensors_.push_back(std::move(conf_blob));

    preview_frame_key frame_key;
    frame_key.frame_id_ = 42;
    observation_batch batch;

    const auto decode_status = decoder.vqec_vision_ai_cntr_mddec_decode(result, frame_key, batch);
    if (decode_status.code_ != status_code::ok) {
        std::cerr << "YOLOv8 decode failed: " << decode_status.message_ << "\n";
        return 1;
    }

    if (batch.observations_.size() != 1) {
        std::cerr << "Expected 1 observation, got " << batch.observations_.size() << "\n";
        return 1;
    }

    const auto& obs = batch.observations_[0];
    if (obs.class_id_ != "person") {
        std::cerr << "Expected class person, got " << obs.class_id_ << "\n";
        return 1;
    }
    if (std::abs(obs.confidence_ - 0.85F) > 0.05F) {
        std::cerr << "Confidence mismatch: " << obs.confidence_ << "\n";
        return 1;
    }
    if (obs.box_.width_ <= 0.0F || obs.box_.height_ <= 0.0F) {
        std::cerr << "Invalid box dimensions: " << obs.box_.width_ << "x" << obs.box_.height_ << "\n";
        return 1;
    }

    return 0;
}

int vqec_vision_ai_unit_dspdt_test_scrfd_decode() {
    using namespace vqec::vision::ai;

    dsp_decoder_config config;
    config.kind_ = dsp_decoder_kind::scrfd;
    config.source_width_ = 1920;
    config.source_height_ = 1080;
    config.tensor_width_ = 640;
    config.tensor_height_ = 640;
    config.confidence_threshold_ = 0.5F;
    config.iou_threshold_ = 0.4F;
    config.class_id_ = "face";

    dsp_decoder decoder(config);

    // Build 9 SCRFD tensors with stride 8, 16, 32
    tensor_result result;
    static const char* const g_names[9] = {
        "score_8", "score_16", "score_32",
        "bbox_8", "bbox_16", "bbox_32",
        "kps_8", "kps_16", "kps_32"
    };
    const std::size_t counts[9] = {
        12800, 3200, 800,
        12800 * 4, 3200 * 4, 800 * 4,
        12800 * 10, 3200 * 10, 800 * 10
    };

    for (std::size_t i = 0; i < 9; ++i) {
        tensor_blob blob;
        blob.spec_.name_ = g_names[i];
        blob.spec_.dtype_ = tensor_element_type::uint16;
        blob.spec_.quantization_ = {true, 0.01F, 0};
        blob.bytes_.assign(counts[i] * sizeof(std::uint16_t), 0);
        result.tensors_.push_back(std::move(blob));
    }

    // Set detection at stage 0 (stride 8), anchor 50
    constexpr float kScoreScale = 0.003921568627F; // 1/255
    result.tensors_[0].spec_.quantization_ = {true, kScoreScale, 0};
    auto* score8 = reinterpret_cast<std::uint16_t*>(result.tensors_[0].bytes_.data());
    score8[50] = static_cast<std::uint16_t>(std::round(0.9F / kScoreScale));

    auto* bbox8 = reinterpret_cast<std::uint16_t*>(result.tensors_[3].bytes_.data());
    bbox8[50 * 4 + 0] = 500; // l
    bbox8[50 * 4 + 1] = 500; // t
    bbox8[50 * 4 + 2] = 500; // r
    bbox8[50 * 4 + 3] = 500; // b

    auto* kps8 = reinterpret_cast<std::uint16_t*>(result.tensors_[6].bytes_.data());
    for (std::size_t k = 0; k < 10; ++k) {
        kps8[50 * 10 + k] = 100;
    }

    preview_frame_key frame_key;
    frame_key.frame_id_ = 101;
    observation_batch batch;

    const auto decode_status = decoder.vqec_vision_ai_cntr_mddec_decode(result, frame_key, batch);
    if (decode_status.code_ != status_code::ok) {
        std::cerr << "SCRFD decode failed: " << decode_status.message_ << "\n";
        return 1;
    }

    if (batch.observations_.size() != 1) {
        std::cerr << "Expected 1 face observation, got " << batch.observations_.size() << "\n";
        return 1;
    }

    const auto& obs = batch.observations_[0];
    if (obs.class_id_ != "face") {
        std::cerr << "Expected class face, got " << obs.class_id_ << "\n";
        return 1;
    }
    if (obs.landmarks_.points_.size() != 5) {
        std::cerr << "Expected 5 landmarks, got " << obs.landmarks_.points_.size() << "\n";
        return 1;
    }

    return 0;
}

}  // namespace

int main() {
    {
        using namespace vqec::vision::ai;
        dsp_session session;
        dsp_session_config config;
        config.skel_dir_ = "/opt/lacai/dsp";
        (void)session.vqec_vision_ai_qcom_dspsn_open(config);
    }
    if (vqec_vision_ai_unit_dspdt_test_dsp_session_describe() != 0) {
        return 1;
    }
    if (vqec_vision_ai_unit_dspdt_test_yolov8_decode() != 0) {
        return 1;
    }
    if (vqec_vision_ai_unit_dspdt_test_scrfd_decode() != 0) {
        return 1;
    }
    std::cout << "All dsp_decoder tests passed.\n";
    return 0;
}
