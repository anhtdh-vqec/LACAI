#ifndef VQEC_VISION_AI_QUALCOMM_DSP_V1_CLIENT_HPP
#define VQEC_VISION_AI_QUALCOMM_DSP_V1_CLIENT_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "vqec/vision/ai/contracts/base/vqec_vision_status.hpp"

extern "C" {
#include "vqec_vision_dsp_v1_wire.h"
}

namespace vqec::vision::ai {

struct dsp_v1_client_config {
    std::string skel_dir_;
    bool enable_unsigned_pd_{false};
};

using dsp_v1_open_function = int (*)(const char*, std::uint64_t*);
using dsp_v1_close_function = int (*)(std::uint64_t);
using dsp_v1_query_function = int (*)(std::uint64_t, std::uint8_t*, int);
using dsp_v1_execute_function = int (*)(std::uint64_t, const std::uint8_t*, int,
                                        const std::uint8_t*, int, std::uint8_t*, int,
                                        std::uint8_t*, int);
using dsp_v1_prepare_domain_function = int (*)();

struct dsp_v1_rpc_api {
    dsp_v1_open_function open_{nullptr};
    dsp_v1_close_function close_{nullptr};
    dsp_v1_query_function query_{nullptr};
    dsp_v1_execute_function execute_{nullptr};
    dsp_v1_prepare_domain_function prepare_domain_{nullptr};
};

enum class dsp_v1_completion {
    not_submitted,
    completed,
    uncertain,
};

struct dsp_v1_call_result {
    status status_{};
    dsp_v1_completion completion_{dsp_v1_completion::not_submitted};
    std::uint32_t output_bytes_{0};
    std::uint32_t detail_{0};
};

class dsp_v1_client final {
public:
    dsp_v1_client();
    explicit dsp_v1_client(dsp_v1_rpc_api _api);
    ~dsp_v1_client();

    dsp_v1_client(const dsp_v1_client&) = delete;
    dsp_v1_client& operator=(const dsp_v1_client&) = delete;
    dsp_v1_client(dsp_v1_client&&) = delete;
    dsp_v1_client& operator=(dsp_v1_client&&) = delete;

    // Stores a validated activation recipe without touching FastRPC or a DSP domain.
    [[nodiscard]] status vqec_vision_ai_qcom_d1cli_configure(
        const dsp_v1_client_config& _config);
    // Idempotently opens the configured domain. Call only after live media is observed.
    [[nodiscard]] status vqec_vision_ai_qcom_d1cli_ensure_open();
    [[nodiscard]] status vqec_vision_ai_qcom_d1cli_open(const dsp_v1_client_config& _config);

    void vqec_vision_ai_qcom_d1cli_close();

    [[nodiscard]] bool vqec_vision_ai_qcom_d1cli_is_open() const noexcept;
    [[nodiscard]] bool vqec_vision_ai_qcom_d1cli_is_configured() const noexcept;

    [[nodiscard]] vqec_vision_ai_dsp_v1_capabilities
    vqec_vision_ai_qcom_d1cli_capabilities() const noexcept;

    [[nodiscard]] dsp_v1_call_result vqec_vision_ai_qcom_d1cli_execute(
        const std::uint8_t* _descriptor, std::size_t _descriptor_bytes,
        const std::uint8_t* _input, std::size_t _input_bytes, std::uint8_t* _output,
        std::size_t _output_capacity_bytes);

    [[nodiscard]] static dsp_v1_rpc_api
    vqec_vision_ai_qcom_d1cli_system_rpc_api() noexcept;

private:
    class impl;
    std::unique_ptr<impl> impl_;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_QUALCOMM_DSP_V1_CLIENT_HPP
