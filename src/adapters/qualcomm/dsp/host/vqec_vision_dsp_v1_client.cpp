#include "vqec_vision_dsp_v1_client.hpp"

#include <array>
#include <climits>
#include <cstdlib>
#include <mutex>
#include <string>

#include <vqec_vision_dsp_v1.h>

#if defined(VQEC_VISION_AI_HAVE_CDSP)
#include <remote.h>
#endif

namespace vqec::vision::ai {

namespace {

constexpr int g_aee_success = 0;
constexpr const char* g_cdsp_domain_query = "&_dom=cdsp";

int vqec_vision_ai_qcom_d1cli_system_open(const char* _uri, std::uint64_t* _handle) {
    if (_handle == nullptr) {
        return -1;
    }
    remote_handle64 handle = 0;
    const int result = vqec_vision_dsp_v1_open(_uri, &handle);
    if (result == g_aee_success) {
        *_handle = static_cast<std::uint64_t>(handle);
    }
    return result;
}

int vqec_vision_ai_qcom_d1cli_system_close(std::uint64_t _handle) {
    return vqec_vision_dsp_v1_close(static_cast<remote_handle64>(_handle));
}

int vqec_vision_ai_qcom_d1cli_system_query(std::uint64_t _handle, std::uint8_t* _response,
                                           int _response_bytes) {
    return vqec_vision_dsp_v1_query_capabilities(static_cast<remote_handle64>(_handle), _response,
                                                  _response_bytes);
}

int vqec_vision_ai_qcom_d1cli_system_execute(
    std::uint64_t _handle, const std::uint8_t* _descriptor, int _descriptor_bytes,
    const std::uint8_t* _input, int _input_bytes, std::uint8_t* _output,
    int _output_capacity_bytes, std::uint8_t* _response, int _response_bytes) {
    return vqec_vision_dsp_v1_execute(
        static_cast<remote_handle64>(_handle), _descriptor, _descriptor_bytes, _input,
        _input_bytes, _output, _output_capacity_bytes, _response, _response_bytes);
}

int vqec_vision_ai_qcom_d1cli_system_prepare_domain() {
#if defined(VQEC_VISION_AI_HAVE_CDSP)
    remote_rpc_control_unsigned_module request{};
    request.domain = CDSP_DOMAIN_ID;
    request.enable = 1;
    return remote_session_control(DSPRPC_CONTROL_UNSIGNED_MODULE,
                                  reinterpret_cast<void*>(&request), sizeof(request));
#else
    return g_aee_success;
#endif
}

bool vqec_vision_ai_qcom_d1cli_api_valid(const dsp_v1_rpc_api& _api) noexcept {
    return _api.open_ != nullptr && _api.close_ != nullptr && _api.query_ != nullptr &&
           _api.execute_ != nullptr;
}

std::string vqec_vision_ai_qcom_d1cli_search_path(const std::string& _skel_dir) {
    const char* existing = std::getenv("ADSP_LIBRARY_PATH");
    if (existing == nullptr || *existing == '\0') {
        return _skel_dir;
    }
    const std::string current(existing);
    if (current == _skel_dir || current.find(_skel_dir + ";") == 0 ||
        current.find(";" + _skel_dir + ";") != std::string::npos ||
        (current.size() > _skel_dir.size() &&
         current.compare(current.size() - _skel_dir.size(), _skel_dir.size(), _skel_dir) == 0 &&
         current[current.size() - _skel_dir.size() - 1] == ';')) {
        return current;
    }
    return _skel_dir + ";" + current;
}

status vqec_vision_ai_qcom_d1cli_wire_failure(vqec_vision_ai_dsp_v1_wire_status _wire_status,
                                               const char* _context) {
    status_code code = status_code::protocol_error;
    switch (_wire_status) {
        case vqec_vision_ai_dsp_v1_wire_incompatible:
        case vqec_vision_ai_dsp_v1_wire_unsupported:
            code = status_code::unsupported;
            break;
        case vqec_vision_ai_dsp_v1_wire_stale_generation:
            code = status_code::invalid_state;
            break;
        case vqec_vision_ai_dsp_v1_wire_out_of_range:
            code = status_code::invalid_argument;
            break;
        case vqec_vision_ai_dsp_v1_wire_malformed:
        case vqec_vision_ai_dsp_v1_wire_ok:
            break;
    }
    return {code, std::string(_context) + " (wire status " +
                      std::to_string(static_cast<unsigned int>(_wire_status)) + ")"};
}

}  // namespace

class dsp_v1_client::impl final {
public:
    explicit impl(dsp_v1_rpc_api _api) : api_(_api) {}

    ~impl() {
        vqec_vision_ai_qcom_d1cli_impl_close();
    }

    void vqec_vision_ai_qcom_d1cli_impl_close() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (handle_ != 0 && api_.close_ != nullptr) {
            (void)api_.close_(handle_);
        }
        handle_ = 0;
        opened_ = false;
        faulted_ = false;
        capabilities_ = {};
    }

    dsp_v1_rpc_api api_{};
    mutable std::mutex mutex_;
    std::uint64_t handle_{0};
    dsp_v1_client_config config_;
    bool configured_{false};
    bool opened_{false};
    bool faulted_{false};
    vqec_vision_ai_dsp_v1_capabilities capabilities_{};
};

dsp_v1_client::dsp_v1_client()
    : impl_(std::make_unique<impl>(vqec_vision_ai_qcom_d1cli_system_rpc_api())) {}

dsp_v1_client::dsp_v1_client(dsp_v1_rpc_api _api) : impl_(std::make_unique<impl>(_api)) {}

dsp_v1_client::~dsp_v1_client() = default;

dsp_v1_rpc_api dsp_v1_client::vqec_vision_ai_qcom_d1cli_system_rpc_api() noexcept {
    return {vqec_vision_ai_qcom_d1cli_system_open, vqec_vision_ai_qcom_d1cli_system_close,
            vqec_vision_ai_qcom_d1cli_system_query, vqec_vision_ai_qcom_d1cli_system_execute,
            vqec_vision_ai_qcom_d1cli_system_prepare_domain};
}

status dsp_v1_client::vqec_vision_ai_qcom_d1cli_open(const dsp_v1_client_config& _config) {
    const auto configured = vqec_vision_ai_qcom_d1cli_configure(_config);
    if (configured.code_ != status_code::ok) {
        return configured;
    }
    return vqec_vision_ai_qcom_d1cli_ensure_open();
}

status dsp_v1_client::vqec_vision_ai_qcom_d1cli_configure(
    const dsp_v1_client_config& _config) {
    if (impl_ == nullptr || !vqec_vision_ai_qcom_d1cli_api_valid(impl_->api_)) {
        return {status_code::invalid_state, "DSP v1 RPC API is incomplete"};
    }
    if (_config.skel_dir_.empty()) {
        return {status_code::invalid_argument, "DSP v1 skeleton directory is empty"};
    }

    std::lock_guard<std::mutex> lock(impl_->mutex_);
    if (impl_->opened_ && !impl_->faulted_) {
        return impl_->config_.skel_dir_ == _config.skel_dir_ &&
                impl_->config_.enable_unsigned_pd_ == _config.enable_unsigned_pd_ ?
            status{} : status{status_code::invalid_state,
                "DSP v1 client cannot change configuration while open"};
    }
    if (impl_->handle_ != 0) {
        return {status_code::invalid_state,
                "DSP v1 client is faulted; close it before opening a new domain"};
    }

    impl_->config_ = _config;
    impl_->configured_ = true;
    return {};
}

status dsp_v1_client::vqec_vision_ai_qcom_d1cli_ensure_open() {
    if (impl_ == nullptr || !vqec_vision_ai_qcom_d1cli_api_valid(impl_->api_)) {
        return {status_code::invalid_state, "DSP v1 RPC API is incomplete"};
    }
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    if (impl_->opened_ && !impl_->faulted_) {
        return {};
    }
    if (!impl_->configured_) {
        return {status_code::invalid_state, "DSP v1 client is not configured"};
    }
    if (impl_->handle_ != 0) {
        return {status_code::invalid_state,
                "DSP v1 client is faulted; close it before opening a new domain"};
    }

    const auto& config = impl_->config_;
    const std::string library_path = vqec_vision_ai_qcom_d1cli_search_path(config.skel_dir_);
    if (setenv("ADSP_LIBRARY_PATH", library_path.c_str(), 1) != 0 ||
        setenv("DSP_LIBRARY_PATH", library_path.c_str(), 1) != 0) {
        return {status_code::io_error, "Cannot configure DSP v1 library search path"};
    }
    if (config.enable_unsigned_pd_ && impl_->api_.prepare_domain_ != nullptr) {
        const int prepare_result = impl_->api_.prepare_domain_();
        if (prepare_result != g_aee_success) {
            return {status_code::io_error,
                    "Cannot prepare the DSP v1 process domain (transport " +
                        std::to_string(prepare_result) + ")"};
        }
    }

    const std::string uri = std::string(vqec_vision_dsp_v1_URI) + g_cdsp_domain_query;
    std::uint64_t handle = 0;
    const int open_result = impl_->api_.open_(uri.c_str(), &handle);
    if (open_result != g_aee_success || handle == 0) {
        if (handle != 0) {
            (void)impl_->api_.close_(handle);
        }
        return {status_code::io_error,
                "Cannot open the negotiated DSP v1 skeleton (transport " +
                    std::to_string(open_result) + ")"};
    }

    std::array<std::uint8_t, VQEC_VISION_AI_DSP_V1_ENVELOPE_BYTES> response{};
    const int query_result =
        impl_->api_.query_(handle, response.data(), static_cast<int>(response.size()));
    if (query_result != g_aee_success) {
        (void)impl_->api_.close_(handle);
        return {status_code::io_error,
                "DSP v1 capability query failed (transport " +
                    std::to_string(query_result) + ")"};
    }

    vqec_vision_ai_dsp_v1_capabilities capabilities{};
    const auto decoded = vqec_vision_ai_qcom_dvwir_decode_capabilities(
        response.data(), response.size(), &capabilities);
    if (decoded != vqec_vision_ai_dsp_v1_wire_ok) {
        (void)impl_->api_.close_(handle);
        return vqec_vision_ai_qcom_d1cli_wire_failure(decoded,
                                                       "DSP v1 capability response rejected");
    }

    impl_->handle_ = handle;
    impl_->opened_ = true;
    impl_->faulted_ = false;
    impl_->capabilities_ = capabilities;
    return {};
}

void dsp_v1_client::vqec_vision_ai_qcom_d1cli_close() {
    if (impl_ != nullptr) {
        impl_->vqec_vision_ai_qcom_d1cli_impl_close();
    }
}

bool dsp_v1_client::vqec_vision_ai_qcom_d1cli_is_open() const noexcept {
    if (impl_ == nullptr) {
        return false;
    }
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    return impl_->opened_ && !impl_->faulted_ && impl_->handle_ != 0;
}

bool dsp_v1_client::vqec_vision_ai_qcom_d1cli_is_configured() const noexcept {
    if (impl_ == nullptr) {
        return false;
    }
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    return impl_->configured_;
}

vqec_vision_ai_dsp_v1_capabilities
dsp_v1_client::vqec_vision_ai_qcom_d1cli_capabilities() const noexcept {
    if (impl_ == nullptr) {
        return {};
    }
    std::lock_guard<std::mutex> lock(impl_->mutex_);
    return impl_->capabilities_;
}

dsp_v1_call_result dsp_v1_client::vqec_vision_ai_qcom_d1cli_execute(
    const std::uint8_t* _descriptor, std::size_t _descriptor_bytes,
    const std::uint8_t* _input, std::size_t _input_bytes, std::uint8_t* _output,
    std::size_t _output_capacity_bytes) {
    dsp_v1_call_result result{};
    if (impl_ == nullptr || _descriptor == nullptr || _input == nullptr || _output == nullptr ||
        _descriptor_bytes == 0 || _input_bytes == 0 || _output_capacity_bytes == 0 ||
        _descriptor_bytes > static_cast<std::size_t>(INT_MAX) ||
        _input_bytes > static_cast<std::size_t>(INT_MAX) ||
        _output_capacity_bytes > static_cast<std::size_t>(INT_MAX)) {
        result.status_ = {status_code::invalid_argument, "Invalid DSP v1 operation buffers"};
        return result;
    }

    std::lock_guard<std::mutex> lock(impl_->mutex_);
    if (!impl_->opened_ || impl_->faulted_ || impl_->handle_ == 0) {
        result.status_ = {status_code::invalid_state, "DSP v1 client is not runnable"};
        return result;
    }

    vqec_vision_ai_dsp_v1_request request{};
    const auto validated = vqec_vision_ai_qcom_dvwir_validate_request(
        &impl_->capabilities_, _descriptor, _descriptor_bytes, _input_bytes,
        _output_capacity_bytes, &request);
    if (validated != vqec_vision_ai_dsp_v1_wire_ok) {
        result.status_ =
            vqec_vision_ai_qcom_d1cli_wire_failure(validated, "DSP v1 request rejected on host");
        return result;
    }

    std::array<std::uint8_t, VQEC_VISION_AI_DSP_V1_RESPONSE_BYTES> response{};
    const int execute_result = impl_->api_.execute_(
        impl_->handle_, _descriptor, static_cast<int>(_descriptor_bytes), _input,
        static_cast<int>(_input_bytes), _output, static_cast<int>(_output_capacity_bytes),
        response.data(), static_cast<int>(response.size()));
    if (execute_result != g_aee_success) {
        impl_->faulted_ = true;
        result.status_ = {status_code::io_error,
                          "DSP v1 execution completion is uncertain (transport " +
                              std::to_string(execute_result) + ")"};
        result.completion_ = dsp_v1_completion::uncertain;
        return result;
    }
    result.completion_ = dsp_v1_completion::completed;

    vqec_vision_ai_dsp_v1_operation_response operation_response{};
    const auto decoded = vqec_vision_ai_qcom_dvwir_decode_operation_response(
        response.data(), response.size(), &operation_response);
    if (decoded != vqec_vision_ai_dsp_v1_wire_ok) {
        result.status_ =
            vqec_vision_ai_qcom_d1cli_wire_failure(decoded, "DSP v1 operation response rejected");
        return result;
    }
    if (operation_response.status == vqec_vision_ai_dsp_v1_wire_ok &&
        (operation_response.operation != request.operation ||
         operation_response.output_bytes > _output_capacity_bytes)) {
        result.status_ = {status_code::protocol_error,
                          "DSP v1 response operation or output length mismatch"};
        return result;
    }
    if (operation_response.status != vqec_vision_ai_dsp_v1_wire_ok) {
        result.status_ = vqec_vision_ai_qcom_d1cli_wire_failure(
            operation_response.status, "DSP v1 operation failed");
        result.detail_ = operation_response.detail;
        return result;
    }

    result.status_ = {};
    result.output_bytes_ = operation_response.output_bytes;
    result.detail_ = operation_response.detail;
    return result;
}

}  // namespace vqec::vision::ai
