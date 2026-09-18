#ifndef VQEC_VISION_AI_QUALCOMM_DSP_SESSION_HPP
#define VQEC_VISION_AI_QUALCOMM_DSP_SESSION_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "vqec/vision/ai/contracts/vqec_vision_status.hpp"

namespace vqec::vision::ai {

struct dsp_session_config {
    std::string skel_dir_{"/opt/lacai/dsp"};
    std::int32_t clock_corner_{7};
    std::int32_t latency_us_{100};
    bool enable_unsigned_pd_{true};
};

struct dsp_post_result {
    std::vector<float> boxes_;
    std::vector<float> kps_;
    std::int32_t count_{0};
    std::int32_t truncated_{0};
    std::uint32_t time_us_{0};
};

class dsp_session final {
public:
    dsp_session();
    ~dsp_session();

    dsp_session(const dsp_session&) = delete;
    dsp_session& operator=(const dsp_session&) = delete;
    dsp_session(dsp_session&&) noexcept;
    dsp_session& operator=(dsp_session&&) noexcept;

    [[nodiscard]] status vqec_vision_ai_qcom_dspsn_open(
        const dsp_session_config& _config);

    void vqec_vision_ai_qcom_dspsn_close();

    [[nodiscard]] bool vqec_vision_ai_qcom_dspsn_is_open() const noexcept;

    [[nodiscard]] std::uint64_t vqec_vision_ai_qcom_dspsn_handle() const noexcept;

    [[nodiscard]] const dsp_session_config& vqec_vision_ai_qcom_dspsn_config() const noexcept;

    [[nodiscard]] static std::string vqec_vision_ai_qcom_dspsn_describe(int _rc);

    [[nodiscard]] static std::string vqec_vision_ai_qcom_dspsn_search_path(
        const std::string& _skel_dir);

    [[nodiscard]] status vqec_vision_ai_qcom_dspsn_postprocess_person_yolov8n(
        const std::uint16_t* _boxes_t, int _boxes_len,
        const std::uint16_t* _conf_t, int _conf_len,
        const float* _quant, int _quant_len,
        const float* _params, int _params_len,
        dsp_post_result& _out);

    [[nodiscard]] status vqec_vision_ai_qcom_dspsn_postprocess_face_scrfd(
        const std::uint16_t* const _tensors[9], const int _lens[9],
        const float* _quant, int _quant_len,
        const float* _params, int _params_len,
        dsp_post_result& _out);

    [[nodiscard]] status vqec_vision_ai_qcom_dspsn_preprocess_person_yolov8n(
        const std::uint8_t* _frame, int _frame_len,
        const std::int32_t* _geom, int _geom_len,
        std::uint16_t* _tensor, int _tensor_len,
        std::uint32_t* _time_us = nullptr);

    [[nodiscard]] status vqec_vision_ai_qcom_dspsn_preprocess_face_scrfd(
        const std::uint8_t* _frame, int _frame_len,
        const std::int32_t* _geom, int _geom_len,
        std::uint16_t* _tensor, int _tensor_len,
        std::uint32_t* _time_us = nullptr);

private:
    class impl;
    std::unique_ptr<impl> impl_;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_QUALCOMM_DSP_SESSION_HPP
