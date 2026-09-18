#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>

#include "vqec_vision_dsp_v1_client.hpp"

extern "C" {
#include "vqec_vision_dsp_v1_dense.h"
#include "vqec_vision_dsp_v1_service.h"
}

namespace {

constexpr std::uint32_t g_fake_domain_generation = 17U;
constexpr std::uint64_t g_fake_handle = 7U;
constexpr int g_fake_transport_reset = 104;
constexpr std::uint32_t g_fixture_source_side = 100U;
constexpr std::uint16_t g_fixture_box_center = 20U;
constexpr std::uint16_t g_fixture_box_side = 10U;
constexpr std::uint16_t g_fixture_score = 1U;
constexpr float g_fixture_threshold = 0.5F;
constexpr const char* g_fixture_skeleton_dir = "/tmp/vqec-dsp-v1-client-test";

struct fake_rpc_state {
    vqec_vision_ai_dsp_v1_service service_{};
    int open_calls_{0};
    int close_calls_{0};
    int query_calls_{0};
    int execute_calls_{0};
    int prepare_calls_{0};
    bool fail_execute_{false};
    bool fail_prepare_{false};
    bool corrupt_capabilities_{false};
};

fake_rpc_state g_fake_rpc_state;

bool vqec_vision_ai_unit_d1clt_check(bool _condition, const char* _message) {
    if (!_condition) {
        std::cerr << _message << '\n';
    }
    return _condition;
}

void vqec_vision_ai_unit_d1clt_reset_fake() {
    g_fake_rpc_state = {};
    (void)vqec_vision_ai_qcom_d1svc_initialize(&g_fake_rpc_state.service_,
                                               g_fake_domain_generation);
}

int vqec_vision_ai_unit_d1clt_fake_open(const char* _uri, std::uint64_t* _handle) {
    ++g_fake_rpc_state.open_calls_;
    if (_uri == nullptr || _handle == nullptr) {
        return -1;
    }
    *_handle = g_fake_handle;
    return 0;
}

int vqec_vision_ai_unit_d1clt_fake_close(std::uint64_t _handle) {
    ++g_fake_rpc_state.close_calls_;
    return _handle == g_fake_handle ? 0 : -1;
}

int vqec_vision_ai_unit_d1clt_fake_query(std::uint64_t _handle, std::uint8_t* _response,
                                         int _response_bytes) {
    ++g_fake_rpc_state.query_calls_;
    if (_handle != g_fake_handle || _response_bytes < 0) {
        return -1;
    }
    const auto result = vqec_vision_ai_qcom_d1svc_query_capabilities(
        &g_fake_rpc_state.service_, _response, static_cast<std::size_t>(_response_bytes));
    if (result != vqec_vision_ai_dsp_v1_wire_ok) {
        return -1;
    }
    if (g_fake_rpc_state.corrupt_capabilities_) {
        _response[0] = 0U;
    }
    return 0;
}

int vqec_vision_ai_unit_d1clt_fake_execute(
    std::uint64_t _handle, const std::uint8_t* _descriptor, int _descriptor_bytes,
    const std::uint8_t* _input, int _input_bytes, std::uint8_t* _output,
    int _output_capacity_bytes, std::uint8_t* _response, int _response_bytes) {
    ++g_fake_rpc_state.execute_calls_;
    if (_handle != g_fake_handle || g_fake_rpc_state.fail_execute_ || _descriptor_bytes < 0 ||
        _input_bytes < 0 || _output_capacity_bytes < 0 || _response_bytes < 0) {
        return g_fake_transport_reset;
    }
    const auto result = vqec_vision_ai_qcom_d1svc_execute(
        &g_fake_rpc_state.service_, _descriptor, static_cast<std::size_t>(_descriptor_bytes),
        _input, static_cast<std::size_t>(_input_bytes), _output,
        static_cast<std::size_t>(_output_capacity_bytes), _response,
        static_cast<std::size_t>(_response_bytes));
    return result == vqec_vision_ai_dsp_v1_wire_ok ? 0 : -1;
}

int vqec_vision_ai_unit_d1clt_fake_prepare_domain() {
    ++g_fake_rpc_state.prepare_calls_;
    return g_fake_rpc_state.fail_prepare_ ? -1 : 0;
}

vqec::vision::ai::dsp_v1_rpc_api vqec_vision_ai_unit_d1clt_fake_api() {
    return {vqec_vision_ai_unit_d1clt_fake_open, vqec_vision_ai_unit_d1clt_fake_close,
            vqec_vision_ai_unit_d1clt_fake_query, vqec_vision_ai_unit_d1clt_fake_execute,
            vqec_vision_ai_unit_d1clt_fake_prepare_domain};
}

void vqec_vision_ai_unit_d1clt_write_u16(std::uint8_t* _output, std::uint16_t _value) {
    _output[0] = static_cast<std::uint8_t>(_value);
    _output[1] = static_cast<std::uint8_t>(_value >> 8U);
}

bool vqec_vision_ai_unit_d1clt_open_and_execute() {
    using namespace vqec::vision::ai;
    vqec_vision_ai_unit_d1clt_reset_fake();
    dsp_v1_client client(vqec_vision_ai_unit_d1clt_fake_api());
    dsp_v1_client_config client_config{};
    client_config.skel_dir_ = g_fixture_skeleton_dir;
    client_config.enable_unsigned_pd_ = true;
    const auto opened = client.vqec_vision_ai_qcom_d1cli_open(client_config);
    bool ok = vqec_vision_ai_unit_d1clt_check(opened.code_ == status_code::ok,
                                              "client open failed") &&
              vqec_vision_ai_unit_d1clt_check(
                  client.vqec_vision_ai_qcom_d1cli_is_open(), "client did not remain open") &&
              vqec_vision_ai_unit_d1clt_check(g_fake_rpc_state.open_calls_ == 1 &&
                                                   g_fake_rpc_state.query_calls_ == 1 &&
                                                   g_fake_rpc_state.prepare_calls_ == 1,
                                               "open did not query one capability generation");

    const auto capabilities = client.vqec_vision_ai_qcom_d1cli_capabilities();
    constexpr std::size_t g_fixture_box_bytes = 4U * sizeof(std::uint16_t);
    constexpr std::size_t g_fixture_input_bytes = g_fixture_box_bytes + sizeof(std::uint16_t);
    std::array<std::uint8_t, g_fixture_input_bytes> input{};
    vqec_vision_ai_unit_d1clt_write_u16(input.data() + 0U, g_fixture_box_center);
    vqec_vision_ai_unit_d1clt_write_u16(input.data() + 2U, g_fixture_box_center);
    vqec_vision_ai_unit_d1clt_write_u16(input.data() + 4U, g_fixture_box_side);
    vqec_vision_ai_unit_d1clt_write_u16(input.data() + 6U, g_fixture_box_side);
    vqec_vision_ai_unit_d1clt_write_u16(input.data() + g_fixture_box_bytes, g_fixture_score);

    vqec_vision_ai_dsp_v1_dense_config dense_config{};
    dense_config.flags = VQEC_VISION_AI_DSP_V1_DENSE_CLASS_AWARE_NMS;
    dense_config.prediction_count = 1U;
    dense_config.class_count = 1U;
    dense_config.box_offset = 0U;
    dense_config.score_offset = static_cast<std::uint32_t>(g_fixture_box_bytes);
    dense_config.source_width = g_fixture_source_side;
    dense_config.source_height = g_fixture_source_side;
    dense_config.candidate_capacity = 1U;
    dense_config.output_capacity = 1U;
    dense_config.box_scale = 1.0F;
    dense_config.score_scale = 1.0F;
    dense_config.confidence_threshold = g_fixture_threshold;
    dense_config.iou_threshold = g_fixture_threshold;
    dense_config.scale_x = 1.0F;
    dense_config.scale_y = 1.0F;

    std::array<std::uint8_t, VQEC_VISION_AI_DSP_V1_DENSE_DESCRIPTOR_BYTES> descriptor{};
    std::array<std::uint8_t, VQEC_VISION_AI_DSP_V1_DENSE_RECORD_BYTES> output{};
    ok = vqec_vision_ai_unit_d1clt_check(
             vqec_vision_ai_qcom_d1dns_encode_descriptor(
                 &dense_config, capabilities.domain_generation, input.size(), output.size(),
                 descriptor.data(), descriptor.size()) == vqec_vision_ai_dsp_v1_wire_ok,
             "dense descriptor encode failed") &&
         ok;
    const auto executed = client.vqec_vision_ai_qcom_d1cli_execute(
        descriptor.data(), descriptor.size(), input.data(), input.size(), output.data(),
        output.size());
    ok = vqec_vision_ai_unit_d1clt_check(executed.status_.code_ == status_code::ok,
                                         "valid generic operation failed") &&
         vqec_vision_ai_unit_d1clt_check(
             executed.completion_ == dsp_v1_completion::completed,
             "successful transport did not prove synchronous completion") &&
         vqec_vision_ai_unit_d1clt_check(
             executed.output_bytes_ == VQEC_VISION_AI_DSP_V1_DENSE_RECORD_BYTES,
             "generic operation output length mismatch") &&
         vqec_vision_ai_unit_d1clt_check(g_fake_rpc_state.execute_calls_ == 1,
                                         "generic operation was not dispatched once") &&
         ok;

    std::array<std::uint8_t, VQEC_VISION_AI_DSP_V1_DENSE_DESCRIPTOR_BYTES> stale{};
    (void)vqec_vision_ai_qcom_d1dns_encode_descriptor(
        &dense_config, capabilities.domain_generation + 1U, input.size(), output.size(),
        stale.data(), stale.size());
    const auto stale_result = client.vqec_vision_ai_qcom_d1cli_execute(
        stale.data(), stale.size(), input.data(), input.size(), output.data(), output.size());
    ok = vqec_vision_ai_unit_d1clt_check(
             stale_result.status_.code_ == status_code::invalid_state &&
                 stale_result.completion_ == dsp_v1_completion::not_submitted &&
                 g_fake_rpc_state.execute_calls_ == 1,
             "stale generation reached transport") &&
         ok;

    std::array<std::uint8_t, VQEC_VISION_AI_DSP_V1_ENVELOPE_BYTES> unsupported{};
    const vqec_vision_ai_dsp_v1_request unsupported_request{
        VQEC_VISION_AI_DSP_V1_IMAGE_TRANSFORM,
        VQEC_VISION_AI_DSP_V1_ENVELOPE_BYTES,
        static_cast<std::uint32_t>(input.size()),
        static_cast<std::uint32_t>(output.size()),
        capabilities.domain_generation};
    (void)vqec_vision_ai_qcom_dvwir_encode_request(
        &unsupported_request, unsupported.data(), unsupported.size());
    const auto unsupported_result = client.vqec_vision_ai_qcom_d1cli_execute(
        unsupported.data(), unsupported.size(), input.data(), input.size(), output.data(),
        output.size());
    ok = vqec_vision_ai_unit_d1clt_check(
             unsupported_result.status_.code_ == status_code::unsupported &&
                 unsupported_result.completion_ == dsp_v1_completion::not_submitted &&
                 g_fake_rpc_state.execute_calls_ == 1,
             "unsupported operation reached transport") &&
         ok;

    g_fake_rpc_state.fail_execute_ = true;
    const auto uncertain = client.vqec_vision_ai_qcom_d1cli_execute(
        descriptor.data(), descriptor.size(), input.data(), input.size(), output.data(),
        output.size());
    ok = vqec_vision_ai_unit_d1clt_check(
             uncertain.status_.code_ == status_code::io_error &&
                 uncertain.completion_ == dsp_v1_completion::uncertain &&
                 !client.vqec_vision_ai_qcom_d1cli_is_open(),
             "transport failure did not fault the session with uncertain completion") &&
         ok;
    client.vqec_vision_ai_qcom_d1cli_close();
    ok = vqec_vision_ai_unit_d1clt_check(g_fake_rpc_state.close_calls_ == 1,
                                         "faulted handle was not closed") &&
         ok;
    return ok;
}

bool vqec_vision_ai_unit_d1clt_reject_malformed_capability() {
    using namespace vqec::vision::ai;
    vqec_vision_ai_unit_d1clt_reset_fake();
    g_fake_rpc_state.corrupt_capabilities_ = true;
    dsp_v1_client client(vqec_vision_ai_unit_d1clt_fake_api());
    dsp_v1_client_config config{};
    config.skel_dir_ = g_fixture_skeleton_dir;
    config.enable_unsigned_pd_ = true;
    const auto opened = client.vqec_vision_ai_qcom_d1cli_open(config);
    return vqec_vision_ai_unit_d1clt_check(
        opened.code_ == status_code::protocol_error &&
            !client.vqec_vision_ai_qcom_d1cli_is_open() && g_fake_rpc_state.close_calls_ == 1,
        "malformed capability reply was accepted or leaked its handle");
}

bool vqec_vision_ai_unit_d1clt_reject_domain_prepare_failure() {
    using namespace vqec::vision::ai;
    vqec_vision_ai_unit_d1clt_reset_fake();
    g_fake_rpc_state.fail_prepare_ = true;
    dsp_v1_client client(vqec_vision_ai_unit_d1clt_fake_api());
    dsp_v1_client_config config{};
    config.skel_dir_ = g_fixture_skeleton_dir;
    config.enable_unsigned_pd_ = true;
    const auto opened = client.vqec_vision_ai_qcom_d1cli_open(config);
    return vqec_vision_ai_unit_d1clt_check(
        opened.code_ == status_code::io_error && g_fake_rpc_state.prepare_calls_ == 1 &&
            g_fake_rpc_state.open_calls_ == 0,
        "failed domain preparation did not stop before skeleton open");
}

}  // namespace

int main() {
    return vqec_vision_ai_unit_d1clt_open_and_execute() &&
                   vqec_vision_ai_unit_d1clt_reject_malformed_capability() &&
                   vqec_vision_ai_unit_d1clt_reject_domain_prepare_failure()
               ? 0
               : 1;
}
