#include "vqec_vision_dsp_session.hpp"

#include <cstdlib>
#include <cmath>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <vqec_vision_dsp_legacy.h>
#include <vqec_vision_dsp_legacy_codes.h>
#include <vqec_vision_dsp_legacy_types.h>
extern "C" {
#include <vqec_vision_dsp_legacy_post_common.h>
#include <vqec_vision_dsp_legacy_post_person.h>
#include <vqec_vision_dsp_legacy_post_face.h>
#include <vqec_vision_dsp_legacy_pre.h>
}

#if defined(__has_include)
#if __has_include(<remote.h>)
#include <remote.h>
#define VQEC_VISION_AI_HAVE_CDSP 1
#endif
#endif

namespace vqec::vision::ai {

namespace {

constexpr int g_aee_success = 0;
constexpr unsigned int g_aee_efailed = 0x80000401U;
constexpr unsigned int g_aee_eunabletoload = 0x80000406U;
constexpr unsigned int g_aee_ebadstate = 0x8000040DU;
constexpr unsigned int g_aee_ebadparm = 0x8000040EU;
constexpr unsigned int g_aee_eunsupported = 0x80000414U;
constexpr unsigned int g_aee_econnreset = 104U;

int vqec_vision_ai_qcom_dspsn_enable_unsigned_pd() noexcept {
#if defined(VQEC_VISION_AI_HAVE_CDSP)
    remote_rpc_control_unsigned_module req{};
    req.domain = CDSP_DOMAIN_ID;
    req.enable = 1;
    return remote_session_control(
        DSPRPC_CONTROL_UNSIGNED_MODULE,
        reinterpret_cast<void*>(&req),
        sizeof(req));
#else
    return 0;
#endif
}

status vqec_vision_ai_qcom_dspsn_validate_post_parameters(
    const float* _quant, int _quant_len, const float* _params) {
    for (int pair = 0; pair < _quant_len / VQEC_QUANT_FLOATS; ++pair) {
        if (!std::isfinite(_quant[pair * VQEC_QUANT_FLOATS]) ||
            _quant[pair * VQEC_QUANT_FLOATS] <= 0 ||
            !std::isfinite(_quant[pair * VQEC_QUANT_FLOATS + 1])) {
            return {status_code::invalid_argument, "invalid legacy DSP quantization"};
        }
    }
    for (int field = 0; field < VQEC_POST_PARAM_FLOATS; ++field) {
        if (!std::isfinite(_params[field])) {
            return {status_code::invalid_argument, "nonfinite legacy DSP parameters"};
        }
    }
    if (_params[VQEC_PP_CONF] <= 0 || _params[VQEC_PP_CONF] > 1 ||
        _params[VQEC_PP_NMS] <= 0 || _params[VQEC_PP_NMS] > 1 ||
        _params[VQEC_PP_SCALE] <= 0 || _params[VQEC_PP_SRC_W] <= 0 ||
        _params[VQEC_PP_SRC_H] <= 0 || _params[VQEC_PP_SRC_W] > VQEC_MAX_FRAME_SIDE ||
        _params[VQEC_PP_SRC_H] > VQEC_MAX_FRAME_SIDE) {
        return {status_code::invalid_argument, "legacy DSP threshold/geometry mismatch"};
    }
    return {};
}

}  // namespace

class dsp_session::impl final {
public:
    dsp_session_config config_;
    std::atomic<std::uint64_t> handle_{0};
    std::atomic<bool> is_open_{false};
    std::mutex rpc_mutex_;
    dsp_execution_mode mode_{dsp_execution_mode::accelerator_required};

    // Explicit reference-only scratch. Production never initializes this arena.
    std::unique_ptr<CandList> host_scratch_;
    PreScratch host_pre_scratch_{};

    impl() = default;

    ~impl() {
        vqec_vision_ai_qcom_dspsn_impl_close();
        pre_scratch_free(&host_pre_scratch_);
    }

    void vqec_vision_ai_qcom_dspsn_impl_close() {
        std::lock_guard<std::mutex> lock(rpc_mutex_);
        if (is_open_ && handle_ != 0) {
            vqec_dsp_close(static_cast<remote_handle64>(handle_));
        }
        handle_ = 0;
        is_open_ = false;
    }
};

std::string dsp_session::vqec_vision_ai_qcom_dspsn_search_path(
    const std::string& _skel_dir) {
    const char* qairt = std::getenv("ADSP_LIBRARY_PATH");
    std::string search_path = _skel_dir + ";/usr/lib/dsp/cdsp/cv/v68/KODIAK;/usr/lib/rfsa/adsp;/dsp";
    if (qairt != nullptr && *qairt != '\0' && search_path.find(qairt) == std::string::npos) {
        search_path += ";";
        search_path += qairt;
    }
    return search_path;
}

std::string dsp_session::vqec_vision_ai_qcom_dspsn_describe(int _rc) {
    char hex[16];
    std::snprintf(hex, sizeof(hex), "0x%08x", static_cast<unsigned int>(_rc));
    const char* name = "unknown";
    switch (static_cast<unsigned int>(_rc)) {
        case VQEC_DSP_E_NOSLOT: name = "VQEC_DSP_E_NOSLOT"; break;
        case VQEC_DSP_E_TOOMANY: name = "VQEC_DSP_E_TOOMANY"; break;
        case VQEC_DSP_E_GEOM: name = "VQEC_DSP_E_GEOM"; break;
        case VQEC_DSP_E_SIZE: name = "VQEC_DSP_E_SIZE"; break;
        case VQEC_DSP_E_DLOPEN: name = "VQEC_DSP_E_DLOPEN"; break;
        case g_aee_efailed: name = "AEE_EFAILED"; break;
        case g_aee_eunabletoload: name = "AEE_EUNABLETOLOAD"; break;
        case g_aee_ebadstate: name = "AEE_EBADSTATE"; break;
        case g_aee_ebadparm: name = "AEE_EBADPARM"; break;
        case g_aee_eunsupported: name = "AEE_EUNSUPPORTED"; break;
        case g_aee_econnreset: name = "AEE_ECONNRESET (DSP process died)"; break;
        default:
            if (static_cast<unsigned int>(_rc) >= VQEC_DSP_E_DLSYM_BASE &&
                static_cast<unsigned int>(_rc) < VQEC_DSP_E_DLSYM_BASE + 16) {
                name = "VQEC_DSP_E_DLSYM (missing FastCV symbol)";
            }
            break;
    }
    return std::string(hex) + " (" + name + ")";
}

dsp_session::dsp_session(dsp_execution_mode _mode) : impl_(std::make_unique<impl>()) {
    impl_->mode_ = _mode;
    if (_mode == dsp_execution_mode::reference_cpu) {
        impl_->host_scratch_ = std::make_unique<CandList>();
    }
}

dsp_session::~dsp_session() = default;

dsp_session::dsp_session(dsp_session&&) noexcept = default;

dsp_session& dsp_session::operator=(dsp_session&&) noexcept = default;

status dsp_session::vqec_vision_ai_qcom_dspsn_open(
    const dsp_session_config& _config) {
    if (impl_ == nullptr) {
        return {status_code::protocol_error, "DSP session implementation is null"};
    }
    std::lock_guard<std::mutex> lock(impl_->rpc_mutex_);
    if (impl_->mode_ != dsp_execution_mode::accelerator_required) {
        return {status_code::unsupported, "reference DSP session cannot open hardware"};
    }
    if (impl_->is_open_) {
        return {status_code::ok, ""};
    }
    impl_->config_ = _config;

    const std::string library_path =
        vqec_vision_ai_qcom_dspsn_search_path(_config.skel_dir_);
    setenv("ADSP_LIBRARY_PATH", library_path.c_str(), 1);
    setenv("DSP_LIBRARY_PATH", library_path.c_str(), 1);

    if (_config.enable_unsigned_pd_) {
        (void)vqec_vision_ai_qcom_dspsn_enable_unsigned_pd();
    }

    remote_handle64 h = 0;
    const int rc = vqec_dsp_open(vqec_dsp_URI "&_dom=cdsp", &h);
    if (rc != g_aee_success) {
        // Fail closed. Hardware opening is not a request to select CPU reference.
        return {status_code::io_error,
            "Cannot open libvqec_dsp_skel.so on cDSP: " + vqec_vision_ai_qcom_dspsn_describe(rc) +
            " (DSP_LIBRARY_PATH=" + library_path + ")"};
    }

    const int clocks = vqec_dsp_set_clocks(h, _config.clock_corner_, _config.latency_us_);
    if (clocks != g_aee_success) {
        vqec_dsp_close(h);
        return {status_code::io_error,
            "cDSP clock vote failed: " + vqec_vision_ai_qcom_dspsn_describe(clocks)};
    }

    impl_->handle_ = static_cast<std::uint64_t>(h);
    impl_->is_open_ = true;
    return {status_code::ok, ""};
}

void dsp_session::vqec_vision_ai_qcom_dspsn_close() {
    if (impl_ != nullptr) {
        impl_->vqec_vision_ai_qcom_dspsn_impl_close();
    }
}

bool dsp_session::vqec_vision_ai_qcom_dspsn_is_open() const noexcept {
    return impl_ != nullptr && impl_->is_open_;
}

std::uint64_t dsp_session::vqec_vision_ai_qcom_dspsn_handle() const noexcept {
    return impl_ != nullptr ? impl_->handle_.load() : 0;
}

const dsp_session_config& dsp_session::vqec_vision_ai_qcom_dspsn_config() const noexcept {
    static const dsp_session_config g_default_config{};
    return impl_ != nullptr ? impl_->config_ : g_default_config;
}

status dsp_session::vqec_vision_ai_qcom_dspsn_postprocess_person_yolov8n(
    const std::uint16_t* _boxes_t, int _boxes_len,
    const std::uint16_t* _conf_t, int _conf_len,
    const float* _quant, int _quant_len,
    const float* _params, int _params_len,
    dsp_post_result& _out) {
    if (_boxes_t == nullptr || _conf_t == nullptr || _quant == nullptr || _params == nullptr) {
        return {status_code::invalid_argument, "Null tensor or parameter pointers"};
    }
    if (impl_ == nullptr || _boxes_len != YOLOV8N_PERSON_PREDICTIONS * 4 ||
        _conf_len != YOLOV8N_PERSON_PREDICTIONS || _quant_len != 4 ||
        _params_len != VQEC_POST_PARAM_FLOATS) {
        return {status_code::invalid_argument, "Invalid tensor lengths or parameters"};
    }
    const auto parameters = vqec_vision_ai_qcom_dspsn_validate_post_parameters(_quant, _quant_len, _params);
    if (parameters.code_ != status_code::ok) {
        return parameters;
    }

    _out.boxes_.assign(static_cast<std::size_t>(VQEC_MAX_BOXES) * VQEC_BOX_FLOATS, 0.0F);
    _out.kps_.clear();
    std::int32_t count = 0;
    std::int32_t truncated = 0;
    std::uint32_t time_us = 0;

    std::lock_guard<std::mutex> lock(impl_->rpc_mutex_);
    if (!impl_->is_open_ && impl_->mode_ != dsp_execution_mode::reference_cpu) {
        return {status_code::invalid_state, "DSP accelerator session is not open"};
    }
    if (impl_->is_open_ && impl_->handle_ != 0) {
        const int rc = vqec_dsp_postprocess_person_yolov8n(
            static_cast<remote_handle64>(impl_->handle_),
            _boxes_t, _boxes_len, _conf_t, _conf_len,
            _quant, _quant_len, _params, _params_len,
            _out.boxes_.data(), static_cast<int>(_out.boxes_.size()),
            &count, &truncated, &time_us);
        if (rc != g_aee_success) {
            return {status_code::io_error,
                "cDSP postprocess_person_yolov8n failed: " + vqec_vision_ai_qcom_dspsn_describe(rc)};
        }
    } else {
        // Host C reference execution for test environments or emulation
        count = post_person_yolov8n(
            impl_->host_scratch_.get(), _boxes_t, _conf_t, _quant, _params,
            _out.boxes_.data(), VQEC_MAX_BOXES);
        if (count < 0) {
            return {status_code::protocol_error, "Host post_person_yolov8n execution failed"};
        }
        truncated = impl_->host_scratch_->truncated;
        time_us = 0;
    }

    if (count < 0 || count > VQEC_MAX_BOXES) {
        return {status_code::protocol_error,
            "Invalid box count returned: " + std::to_string(count)};
    }
    _out.boxes_.resize(static_cast<std::size_t>(count) * VQEC_BOX_FLOATS);
    _out.count_ = count;
    _out.truncated_ = truncated;
    _out.time_us_ = time_us;
    return {status_code::ok, ""};
}

status dsp_session::vqec_vision_ai_qcom_dspsn_postprocess_face_scrfd(
    const std::uint16_t* const _tensors[9], const int _lens[9],
    const float* _quant, int _quant_len,
    const float* _params, int _params_len,
    dsp_post_result& _out) {
    if (_tensors == nullptr || _lens == nullptr || _quant == nullptr || _params == nullptr) {
        return {status_code::invalid_argument, "Null tensor or parameter pointers"};
    }
    if (impl_ == nullptr) {
        return {status_code::invalid_state, "DSP session implementation is absent"};
    }
    for (std::size_t i = 0; i < 9; ++i) {
        const int stride = 8 << (i % 3);
        const int cells = (SCRFD_INPUT / stride) * (SCRFD_INPUT / stride) * SCRFD_ANCHORS_PER_CELL;
        const int channels = i < 3 ? 1 : (i < 6 ? 4 : VQEC_KPS_FLOATS);
        if (_tensors[i] == nullptr || _lens[i] != cells * channels) {
            return {status_code::invalid_argument, "Invalid tensor pointer or length at index " + std::to_string(i)};
        }
    }
    if (_quant_len != 18 || _params_len != VQEC_POST_PARAM_FLOATS) {
        return {status_code::invalid_argument, "Invalid quant or params length for SCRFD"};
    }
    const auto parameters = vqec_vision_ai_qcom_dspsn_validate_post_parameters(_quant, _quant_len, _params);
    if (parameters.code_ != status_code::ok) {
        return parameters;
    }

    _out.boxes_.assign(static_cast<std::size_t>(VQEC_MAX_BOXES) * VQEC_BOX_FLOATS, 0.0F);
    _out.kps_.assign(static_cast<std::size_t>(VQEC_MAX_BOXES) * VQEC_KPS_FLOATS, 0.0F);
    std::int32_t count = 0;
    std::int32_t truncated = 0;
    std::uint32_t time_us = 0;

    std::lock_guard<std::mutex> lock(impl_->rpc_mutex_);
    if (!impl_->is_open_ && impl_->mode_ != dsp_execution_mode::reference_cpu) {
        return {status_code::invalid_state, "DSP accelerator session is not open"};
    }
    if (impl_->is_open_ && impl_->handle_ != 0) {
        const int rc = vqec_dsp_postprocess_face_scrfd(
            static_cast<remote_handle64>(impl_->handle_),
            _tensors[0], _lens[0], _tensors[1], _lens[1], _tensors[2], _lens[2],
            _tensors[3], _lens[3], _tensors[4], _lens[4], _tensors[5], _lens[5],
            _tensors[6], _lens[6], _tensors[7], _lens[7], _tensors[8], _lens[8],
            _quant, _quant_len, _params, _params_len,
            _out.boxes_.data(), static_cast<int>(_out.boxes_.size()),
            _out.kps_.data(), static_cast<int>(_out.kps_.size()),
            &count, &truncated, &time_us);
        if (rc != g_aee_success) {
            return {status_code::io_error,
                "cDSP postprocess_face_scrfd failed: " + vqec_vision_ai_qcom_dspsn_describe(rc)};
        }
    } else {
        // Host C reference execution for test environments or emulation
        count = post_face_scrfd(
            impl_->host_scratch_.get(), _tensors, _quant, _params,
            _out.boxes_.data(), _out.kps_.data(), VQEC_MAX_BOXES);
        if (count < 0) {
            return {status_code::protocol_error, "Host post_face_scrfd execution failed"};
        }
        truncated = impl_->host_scratch_->truncated;
        time_us = 0;
    }

    if (count < 0 || count > VQEC_MAX_BOXES) {
        return {status_code::protocol_error,
            "Invalid box count returned: " + std::to_string(count)};
    }
    _out.boxes_.resize(static_cast<std::size_t>(count) * VQEC_BOX_FLOATS);
    _out.kps_.resize(static_cast<std::size_t>(count) * VQEC_KPS_FLOATS);
    _out.count_ = count;
    _out.truncated_ = truncated;
    _out.time_us_ = time_us;
    return {status_code::ok, ""};
}

status dsp_session::vqec_vision_ai_qcom_dspsn_preprocess_person_yolov8n(
    const std::uint8_t* _frame, int _frame_len,
    const std::int32_t* _geom, int _geom_len,
    std::uint16_t* _tensor, int _tensor_len,
    std::uint32_t* _time_us) {
    if (_frame == nullptr || _geom == nullptr || _tensor == nullptr) {
        return {status_code::invalid_argument, "Null pointer in preprocess arguments"};
    }
    if (impl_ == nullptr || _frame_len <= 0 || _tensor_len <= 0 || _geom_len != VQEC_GEOM_INTS) {
        return {status_code::invalid_argument, "geom_len must be VQEC_GEOM_INTS"};
    }
    std::uint32_t time_us = 0;
    std::lock_guard<std::mutex> lock(impl_->rpc_mutex_);
    if (!impl_->is_open_ && impl_->mode_ != dsp_execution_mode::reference_cpu) {
        return {status_code::invalid_state, "DSP accelerator session is not open"};
    }
    if (impl_->is_open_ && impl_->handle_ != 0) {
        const int rc = vqec_dsp_preprocess_person_yolov8n(
            static_cast<remote_handle64>(impl_->handle_),
            _frame, _frame_len, _geom, _geom_len,
            _tensor, _tensor_len, &time_us);
        if (rc != g_aee_success) {
            return {status_code::io_error,
                "cDSP preprocess_person_yolov8n failed: " + vqec_vision_ai_qcom_dspsn_describe(rc)};
        }
    } else {
        const int rc = pre_letterbox_rgb_u16(
            &impl_->host_pre_scratch_, _frame, _frame_len, _geom, _tensor, _tensor_len);
        if (rc != 0) {
            return {status_code::protocol_error, "Host pre_letterbox_rgb_u16 failed: " + std::to_string(rc)};
        }
        time_us = 0;
    }
    if (_time_us != nullptr) {
        *_time_us = time_us;
    }
    return {status_code::ok, ""};
}

status dsp_session::vqec_vision_ai_qcom_dspsn_preprocess_face_scrfd(
    const std::uint8_t* _frame, int _frame_len,
    const std::int32_t* _geom, int _geom_len,
    std::uint16_t* _tensor, int _tensor_len,
    std::uint32_t* _time_us) {
    if (_frame == nullptr || _geom == nullptr || _tensor == nullptr) {
        return {status_code::invalid_argument, "Null pointer in preprocess arguments"};
    }
    if (impl_ == nullptr || _frame_len <= 0 || _tensor_len <= 0 || _geom_len != VQEC_GEOM_INTS) {
        return {status_code::invalid_argument, "geom_len must be VQEC_GEOM_INTS"};
    }
    std::uint32_t time_us = 0;
    std::lock_guard<std::mutex> lock(impl_->rpc_mutex_);
    if (!impl_->is_open_ && impl_->mode_ != dsp_execution_mode::reference_cpu) {
        return {status_code::invalid_state, "DSP accelerator session is not open"};
    }
    if (impl_->is_open_ && impl_->handle_ != 0) {
        const int rc = vqec_dsp_preprocess_face_scrfd(
            static_cast<remote_handle64>(impl_->handle_),
            _frame, _frame_len, _geom, _geom_len,
            _tensor, _tensor_len, &time_us);
        if (rc != g_aee_success) {
            return {status_code::io_error,
                "cDSP preprocess_face_scrfd failed: " + vqec_vision_ai_qcom_dspsn_describe(rc)};
        }
    } else {
        const int rc = pre_letterbox_rgb_u16(
            &impl_->host_pre_scratch_, _frame, _frame_len, _geom, _tensor, _tensor_len);
        if (rc != 0) {
            return {status_code::protocol_error, "Host pre_letterbox_rgb_u16 failed: " + std::to_string(rc)};
        }
        time_us = 0;
    }
    if (_time_us != nullptr) {
        *_time_us = time_us;
    }
    return {status_code::ok, ""};
}

}  // namespace vqec::vision::ai
