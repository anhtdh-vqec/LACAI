// Device-free conformance tests for the model-independent dense-decode v1 payload.

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <vector>

#include "vqec_vision_dsp_v1_dense.h"

namespace {

constexpr std::uint32_t g_domain_generation = 7U;
constexpr std::size_t g_record_float_count = 5U;

struct test_fixture {
    vqec_vision_ai_dsp_v1_capabilities capabilities{1U << (VQEC_VISION_AI_DSP_V1_DENSE_DECODE - 1U),
                                                    VQEC_VISION_AI_DSP_V1_DENSE_DESCRIPTOR_BYTES,
                                                    1U << 20U, 1U << 16U, g_domain_generation};
    vqec_vision_ai_dsp_v1_dense_config config{};
    std::array<std::uint8_t, VQEC_VISION_AI_DSP_V1_DENSE_DESCRIPTOR_BYTES> descriptor{};
    std::vector<std::uint8_t> input;
    std::vector<std::uint8_t> output;
    vqec_vision_ai_dsp_v1_dense_scratch scratch{};
    vqec_vision_ai_dsp_v1_dense_result result{};
};

void vqec_vision_ai_unit_d1dst_write_u16(std::vector<std::uint8_t>& _bytes, std::size_t _offset,
                                         std::uint16_t _value) {
    _bytes[_offset] = static_cast<std::uint8_t>(_value);
    _bytes[_offset + 1U] = static_cast<std::uint8_t>(_value >> 8U);
}

float vqec_vision_ai_unit_d1dst_read_f32(const std::vector<std::uint8_t>& _bytes,
                                         std::size_t _offset) {
    const std::uint32_t bits = static_cast<std::uint32_t>(_bytes[_offset]) |
                               (static_cast<std::uint32_t>(_bytes[_offset + 1U]) << 8U) |
                               (static_cast<std::uint32_t>(_bytes[_offset + 2U]) << 16U) |
                               (static_cast<std::uint32_t>(_bytes[_offset + 3U]) << 24U);
    float value = 0.0F;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

std::uint32_t vqec_vision_ai_unit_d1dst_read_u32(const std::vector<std::uint8_t>& _bytes,
                                                 std::size_t _offset) {
    return static_cast<std::uint32_t>(_bytes[_offset]) |
           (static_cast<std::uint32_t>(_bytes[_offset + 1U]) << 8U) |
           (static_cast<std::uint32_t>(_bytes[_offset + 2U]) << 16U) |
           (static_cast<std::uint32_t>(_bytes[_offset + 3U]) << 24U);
}

test_fixture vqec_vision_ai_unit_d1dst_make_fixture() {
    test_fixture fixture;
    fixture.config.flags = VQEC_VISION_AI_DSP_V1_DENSE_CLASS_AWARE_NMS;
    fixture.config.prediction_count = 3U;
    fixture.config.class_count = 2U;
    fixture.config.box_offset = 0U;
    fixture.config.score_offset = 24U;
    fixture.config.source_width = 64U;
    fixture.config.source_height = 64U;
    fixture.config.candidate_capacity = 8U;
    fixture.config.output_capacity = 4U;
    fixture.config.box_scale = 0.5F;
    fixture.config.box_zero_point = -10;
    fixture.config.score_scale = 0.001F;
    fixture.config.score_zero_point = 0;
    fixture.config.confidence_threshold = 0.5F;
    fixture.config.iou_threshold = 0.5F;
    fixture.config.scale_x = 1.0F;
    fixture.config.scale_y = 1.0F;
    fixture.config.pad_x = 0.0F;
    fixture.config.pad_y = 0.0F;
    fixture.input.resize(36U);
    fixture.output.resize(fixture.config.output_capacity *
                          VQEC_VISION_AI_DSP_V1_DENSE_RECORD_BYTES);

    // Channel-major boxes: centre_x, centre_y, width, height.
    const std::array<std::uint16_t, 12U> boxes{22U, 26U, 86U, 22U, 26U, 86U,
                                               22U, 22U, 22U, 22U, 22U, 22U};
    for (std::size_t index = 0; index < boxes.size(); ++index) {
        vqec_vision_ai_unit_d1dst_write_u16(fixture.input, index * 2U, boxes[index]);
    }
    // Class-major scores. Prediction 1 overlaps prediction 0 in class 0.
    const std::array<std::uint16_t, 6U> scores{900U, 800U, 50U, 100U, 100U, 950U};
    for (std::size_t index = 0; index < scores.size(); ++index) {
        vqec_vision_ai_unit_d1dst_write_u16(fixture.input, fixture.config.score_offset + index * 2U,
                                            scores[index]);
    }
    return fixture;
}

bool vqec_vision_ai_unit_d1dst_encode(test_fixture& _fixture) {
    return vqec_vision_ai_qcom_d1dns_encode_descriptor(
               &_fixture.config, g_domain_generation, _fixture.input.size(), _fixture.output.size(),
               _fixture.descriptor.data(),
               _fixture.descriptor.size()) == vqec_vision_ai_dsp_v1_wire_ok;
}

vqec_vision_ai_dsp_v1_wire_status vqec_vision_ai_unit_d1dst_execute(test_fixture& _fixture) {
    return vqec_vision_ai_qcom_d1dns_execute(
        &_fixture.capabilities, _fixture.descriptor.data(), _fixture.descriptor.size(),
        _fixture.input.data(), _fixture.input.size(), _fixture.output.data(),
        _fixture.output.size(), &_fixture.scratch, &_fixture.result);
}

} // namespace

int main() {
    unsigned failures = 0U;
    const auto check = [&failures](bool _condition) {
        if (!_condition) {
            ++failures;
        }
    };

    {
        auto fixture = vqec_vision_ai_unit_d1dst_make_fixture();
        check(vqec_vision_ai_unit_d1dst_encode(fixture));
        check(vqec_vision_ai_unit_d1dst_execute(fixture) == vqec_vision_ai_dsp_v1_wire_ok);
        check(fixture.result.output_count == 2U);
        check(fixture.result.output_bytes == 2U * VQEC_VISION_AI_DSP_V1_DENSE_RECORD_BYTES);
        check(fixture.result.truncated_candidates == 0U);
        check(std::fabs(vqec_vision_ai_unit_d1dst_read_f32(fixture.output, 4U * sizeof(float)) -
                        0.95F) < 0.0001F);
        check(vqec_vision_ai_unit_d1dst_read_u32(fixture.output,
                                                 g_record_float_count * sizeof(float)) == 1U);
        const std::size_t second = VQEC_VISION_AI_DSP_V1_DENSE_RECORD_BYTES;
        check(std::fabs(vqec_vision_ai_unit_d1dst_read_f32(fixture.output, second) - 8.0F) <
              0.0001F);
        check(std::fabs(
                  vqec_vision_ai_unit_d1dst_read_f32(fixture.output, second + 2U * sizeof(float)) -
                  24.0F) < 0.0001F);
        check(vqec_vision_ai_unit_d1dst_read_u32(fixture.output, second + g_record_float_count *
                                                                              sizeof(float)) == 0U);
    }

    // Candidate truncation keeps the strongest item and reports every overflow decision.
    {
        auto fixture = vqec_vision_ai_unit_d1dst_make_fixture();
        fixture.config.candidate_capacity = 1U;
        fixture.config.output_capacity = 1U;
        fixture.output.resize(VQEC_VISION_AI_DSP_V1_DENSE_RECORD_BYTES);
        check(vqec_vision_ai_unit_d1dst_encode(fixture));
        check(vqec_vision_ai_unit_d1dst_execute(fixture) == vqec_vision_ai_dsp_v1_wire_ok);
        check(fixture.result.output_count == 1U);
        check(fixture.result.truncated_candidates == 2U);
        check(vqec_vision_ai_unit_d1dst_read_u32(fixture.output,
                                                 g_record_float_count * sizeof(float)) == 1U);
    }

    // Equal scores use class then prediction order, independent of the sort implementation.
    {
        auto fixture = vqec_vision_ai_unit_d1dst_make_fixture();
        vqec_vision_ai_unit_d1dst_write_u16(
            fixture.input, fixture.config.score_offset + 5U * sizeof(std::uint16_t), 900U);
        check(vqec_vision_ai_unit_d1dst_encode(fixture));
        check(vqec_vision_ai_unit_d1dst_execute(fixture) == vqec_vision_ai_dsp_v1_wire_ok);
        check(vqec_vision_ai_unit_d1dst_read_u32(fixture.output,
                                                 g_record_float_count * sizeof(float)) == 0U);
    }

    // Real model shapes are descriptor data, not kernel dispatch branches.
    for (const auto profile : std::array<std::array<std::uint32_t, 3U>, 2U>{
             std::array<std::uint32_t, 3U>{8400U, 1U, 640U},
             std::array<std::uint32_t, 3U>{2100U, 2U, 320U}}) {
        auto fixture = vqec_vision_ai_unit_d1dst_make_fixture();
        fixture.config.prediction_count = profile[0];
        fixture.config.class_count = profile[1];
        fixture.config.source_width = profile[2];
        fixture.config.source_height = profile[2];
        const std::size_t box_bytes =
            static_cast<std::size_t>(profile[0]) * 4U * sizeof(std::uint16_t);
        const std::size_t score_bytes =
            static_cast<std::size_t>(profile[0]) * profile[1] * sizeof(std::uint16_t);
        fixture.config.box_offset = 0U;
        fixture.config.score_offset = static_cast<std::uint32_t>(box_bytes);
        fixture.input.assign(box_bytes + score_bytes, 0U);
        check(vqec_vision_ai_unit_d1dst_encode(fixture));
    }

    // Invalid packing, non-finite policy and wrong payload revision fail before reads.
    {
        auto fixture = vqec_vision_ai_unit_d1dst_make_fixture();
        fixture.config.score_offset += 2U;
        check(!vqec_vision_ai_unit_d1dst_encode(fixture));
        fixture = vqec_vision_ai_unit_d1dst_make_fixture();
        fixture.config.score_scale = std::numeric_limits<float>::infinity();
        check(!vqec_vision_ai_unit_d1dst_encode(fixture));
        fixture = vqec_vision_ai_unit_d1dst_make_fixture();
        fixture.config.score_scale = 0.01F;
        check(vqec_vision_ai_unit_d1dst_encode(fixture));
        check(vqec_vision_ai_unit_d1dst_execute(fixture) ==
              vqec_vision_ai_dsp_v1_wire_out_of_range);
        fixture = vqec_vision_ai_unit_d1dst_make_fixture();
        check(vqec_vision_ai_unit_d1dst_encode(fixture));
        fixture.descriptor[VQEC_VISION_AI_DSP_V1_DENSE_PAYLOAD_VERSION_OFFSET] = 2U;
        check(vqec_vision_ai_unit_d1dst_execute(fixture) == vqec_vision_ai_dsp_v1_wire_malformed);
    }

    // Domain reset and capability mismatch are distinct transport failures.
    {
        auto fixture = vqec_vision_ai_unit_d1dst_make_fixture();
        check(vqec_vision_ai_unit_d1dst_encode(fixture));
        ++fixture.capabilities.domain_generation;
        check(vqec_vision_ai_unit_d1dst_execute(fixture) ==
              vqec_vision_ai_dsp_v1_wire_stale_generation);
        fixture.capabilities.domain_generation = g_domain_generation;
        fixture.capabilities.operations_mask = 1U << (VQEC_VISION_AI_DSP_V1_IMAGE_TRANSFORM - 1U);
        check(vqec_vision_ai_unit_d1dst_execute(fixture) == vqec_vision_ai_dsp_v1_wire_unsupported);
    }

    std::cout << "dsp v1 dense failures: " << failures << '\n';
    return failures == 0U ? 0 : 1;
}
