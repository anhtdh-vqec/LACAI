// libFuzzer harness for the dense anchor-free decoder. Host clang only; the decoder is built
// into the always-on detection target. The graph layout is fixed so the fuzzer drives the
// tensor values, NMS and geometry code with arbitrary bits (including NaN/Inf).
//
// Build: -DVQEC_VISION_AI_BUILD_FUZZERS=ON.

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "vqec_vision_dense_decoder.hpp"

using namespace vqec::vision::ai;

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* _data, std::size_t _size) {
    if (_data == nullptr) {
        return 0;
    }
    dense_decoder_config config;
    config.source_width_ = 64;
    config.source_height_ = 64;
    config.tensor_width_ = 64;
    config.tensor_height_ = 64;
    config.class_count_ = 2;
    config.confidence_threshold_ = 0.5F;
    config.iou_threshold_ = 0.5F;
    dense_decoder_stage stage;
    stage.box_tensor_ = "boxes";
    stage.score_tensor_ = "scores";
    stage.grid_width_ = 2;
    stage.grid_height_ = 2;
    config.stages_.push_back(stage);
    dense_decoder decoder(config);

    tensor_blob boxes;
    boxes.spec_.name_ = "boxes";
    boxes.spec_.dimensions_ = {1, 2, 2, 4};
    boxes.spec_.dtype_ = tensor_element_type::float32;
    boxes.bytes_.assign(16 * sizeof(float), 0);
    if (_size > 0) {
        std::memcpy(boxes.bytes_.data(), _data,
            _size < boxes.bytes_.size() ? _size : boxes.bytes_.size());
    }
    tensor_blob scores;
    scores.spec_.name_ = "scores";
    scores.spec_.dimensions_ = {1, 2, 2, 2};
    scores.spec_.dtype_ = tensor_element_type::float32;
    scores.bytes_.assign(8 * sizeof(float), 0);
    if (_size > boxes.bytes_.size()) {
        const std::size_t remaining = _size - boxes.bytes_.size();
        std::memcpy(scores.bytes_.data(), _data + boxes.bytes_.size(),
            remaining < scores.bytes_.size() ? remaining : scores.bytes_.size());
    }

    tensor_result result;
    result.tensors_.push_back(std::move(boxes));
    result.tensors_.push_back(std::move(scores));
    observation_batch observations;
    const preview_frame_key frame{1, 0, 1, 1, 1000};
    (void)decoder.vqec_vision_ai_cntr_mddec_decode(result, frame, observations);
    return 0;
}
