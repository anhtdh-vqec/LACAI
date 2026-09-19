#include "vqec_vision_qnn_engine.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <utility>

#include <dlfcn.h>

#include <HTP/QnnHtpCommon.h>  // private QAIRT SDK header, not a project include
#include <HTP/QnnHtpDevice.h>  // private QAIRT SDK header, not a project include
#include <QnnInterface.h>  // private QAIRT SDK header, not a project include

#include "vqec_vision_sdk_loader.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_tensor_contract.hpp"

namespace vqec::vision::ai {
namespace {

std::uint32_t vqec_vision_ai_qcom_qneng_dtype_bit(tensor_element_type _type) noexcept {
    return static_cast<std::uint32_t>(1U) << static_cast<std::uint32_t>(_type);
}

std::uint32_t vqec_vision_ai_qcom_qneng_supported_dtype_mask() noexcept {
    // QNN HTP supports these element types. This is an engine-level advertisement; a
    // specific model may support fewer, and the composed graph tensors are authoritative.
    return vqec_vision_ai_qcom_qneng_dtype_bit(tensor_element_type::int8) |
        vqec_vision_ai_qcom_qneng_dtype_bit(tensor_element_type::uint8) |
        vqec_vision_ai_qcom_qneng_dtype_bit(tensor_element_type::int16) |
        vqec_vision_ai_qcom_qneng_dtype_bit(tensor_element_type::uint16) |
        vqec_vision_ai_qcom_qneng_dtype_bit(tensor_element_type::int32) |
        vqec_vision_ai_qcom_qneng_dtype_bit(tensor_element_type::uint32) |
        vqec_vision_ai_qcom_qneng_dtype_bit(tensor_element_type::float16) |
        vqec_vision_ai_qcom_qneng_dtype_bit(tensor_element_type::float32);
}

tensor_element_type vqec_vision_ai_qcom_qneng_map_dtype(Qnn_DataType_t _type) noexcept {
    switch (_type) {
        case QNN_DATATYPE_INT_8:
        case QNN_DATATYPE_SFIXED_POINT_8:
            return tensor_element_type::int8;
        case QNN_DATATYPE_UINT_8:
        case QNN_DATATYPE_UFIXED_POINT_8:
            return tensor_element_type::uint8;
        case QNN_DATATYPE_INT_16:
        case QNN_DATATYPE_SFIXED_POINT_16:
            return tensor_element_type::int16;
        case QNN_DATATYPE_UINT_16:
        case QNN_DATATYPE_UFIXED_POINT_16:
            return tensor_element_type::uint16;
        case QNN_DATATYPE_INT_32:
        case QNN_DATATYPE_SFIXED_POINT_32:
            return tensor_element_type::int32;
        case QNN_DATATYPE_UINT_32:
        case QNN_DATATYPE_UFIXED_POINT_32:
            return tensor_element_type::uint32;
        case QNN_DATATYPE_INT_64:
            return tensor_element_type::int64;
        case QNN_DATATYPE_UINT_64:
            return tensor_element_type::uint64;
        case QNN_DATATYPE_FLOAT_16:
            return tensor_element_type::float16;
        case QNN_DATATYPE_FLOAT_32:
            return tensor_element_type::float32;
        default:
            break;
    }
    return tensor_element_type::unknown;
}

bool vqec_vision_ai_qcom_qneng_same_spec(
    const tensor_spec& _left, const tensor_spec& _right) noexcept {
    return _left.name_ == _right.name_ && _left.dimensions_ == _right.dimensions_ &&
        _left.dtype_ == _right.dtype_ &&
        _left.quantization_.is_quantized_ == _right.quantization_.is_quantized_ &&
        _left.quantization_.scale_ == _right.quantization_.scale_ &&
        _left.quantization_.zero_point_ == _right.quantization_.zero_point_;
}

tensor_spec vqec_vision_ai_qcom_qneng_make_spec(const Qnn_Tensor_t& _tensor) {
    tensor_spec spec;
    if (_tensor.version != QNN_TENSOR_VERSION_2) {
        return spec;
    }
    const auto& v2 = _tensor.v2;
    if (v2.name != nullptr) {
        spec.name_ = v2.name;
    }
    spec.dtype_ = vqec_vision_ai_qcom_qneng_map_dtype(v2.dataType);
    if (v2.dimensions != nullptr) {
        for (std::uint32_t axis = 0; axis < v2.rank; ++axis) {
            spec.dimensions_.push_back(v2.dimensions[axis]);
        }
        if (spec.dimensions_.size() == 4 && spec.dimensions_[0] == 1 && spec.dimensions_[3] == 3) {
            spec.layout_ = tensor_layout::nhwc;
        } else if (!spec.dimensions_.empty() && spec.dimensions_.size() <= 3) {
            // QNN client buffers are contiguous and carry no stride metadata. Rank-one
            // through rank-three graph tensors therefore satisfy the neutral packed-flat
            // contract; do not leave decoder-visible outputs ambiguously unknown.
            spec.layout_ = tensor_layout::flat;
        }
    }
    if (v2.quantizeParams.encodingDefinition == QNN_DEFINITION_DEFINED &&
        v2.quantizeParams.quantizationEncoding == QNN_QUANTIZATION_ENCODING_SCALE_OFFSET &&
        v2.quantizeParams.scaleOffsetEncoding.scale > 0.0F) {
        spec.quantization_.is_quantized_ = true;
        spec.quantization_.scale_ = v2.quantizeParams.scaleOffsetEncoding.scale;
        // QNN stores real = (stored + offset) * scale; the neutral convention is
        // real = (stored - zero_point) * scale.
        spec.quantization_.zero_point_ =
            -v2.quantizeParams.scaleOffsetEncoding.offset;
    }
    return spec;
}

// ABI mirror of the QNN sample-app wrapper structures used by generated model libraries.
// The layout must match the model library built by qnn-model-lib-generator; we declare it
// locally instead of including the restricted SDK example header. The static assertions
// below pin the field order/offsets against this translation unit's own layout so an
// accidental edit or compiler packing change cannot silently desync the ABI. A mismatch
// with the actual model library still needs a board compose/execute check.
struct qnn_model_graph_config_info {
    char* graphName;
    const QnnGraph_Config_t** graphConfigs;
};

struct qnn_model_graph_info {
    Qnn_GraphHandle_t graph;
    char* graphName;
    Qnn_Tensor_t* inputTensors;
    std::uint32_t numInputTensors;
    Qnn_Tensor_t* outputTensors;
    std::uint32_t numOutputTensors;
};

static_assert(offsetof(qnn_model_graph_info, graph) == 0,
    "QNN model graph ABI: graph must be the first field");
static_assert(offsetof(qnn_model_graph_info, graphName) == sizeof(void*),
    "QNN model graph ABI: graphName must follow the graph handle");
static_assert(offsetof(qnn_model_graph_info, inputTensors) == 2U * sizeof(void*),
    "QNN model graph ABI: inputTensors must follow graphName");
static_assert(offsetof(qnn_model_graph_info, numInputTensors) == 3U * sizeof(void*),
    "QNN model graph ABI: numInputTensors must follow inputTensors");
static_assert(offsetof(qnn_model_graph_info, outputTensors) > offsetof(
    qnn_model_graph_info, numInputTensors),
    "QNN model graph ABI: outputTensors must follow the input count");
static_assert(offsetof(qnn_model_graph_config_info, graphConfigs) == sizeof(void*),
    "QNN model graph config ABI: graphConfigs must follow graphName");

using model_error_t = std::int32_t;
using compose_graphs_fn = model_error_t (*)(
    Qnn_BackendHandle_t, QNN_INTERFACE_VER_TYPE, Qnn_ContextHandle_t,
    const qnn_model_graph_config_info**, std::uint32_t,
    qnn_model_graph_info***, std::uint32_t*, bool, QnnLog_Callback_t, QnnLog_Level_t);
using free_graphs_fn = model_error_t (*)(qnn_model_graph_info***, std::uint32_t);

constexpr model_error_t g_model_no_error = 0;
constexpr int g_rpcmem_heap_id_system = 25;
constexpr std::uint32_t g_rpcmem_default_flags = 1;
constexpr std::size_t g_rpcmem_page_alignment = 4096;
constexpr std::uint32_t g_max_shared_registrations = 16;

using rpcmem_alloc_fn = void* (*)(int, std::uint32_t, int);
using rpcmem_to_fd_fn = int (*)(void*);
using rpcmem_free_fn = void (*)(void*);

struct qnn_rpcmem_driver {
    void* lib_handle_{nullptr};
    rpcmem_alloc_fn alloc_{nullptr};
    rpcmem_to_fd_fn to_fd_{nullptr};
    rpcmem_free_fn free_{nullptr};

    [[nodiscard]] bool is_available() const noexcept {
        return alloc_ != nullptr && to_fd_ != nullptr && free_ != nullptr;
    }
};

struct qnn_registered_buffer {
    void* data_{nullptr};
    int fd_{-1};
    std::size_t size_{0};
    Qnn_MemHandle_t handle_{nullptr};
};

std::size_t vqec_vision_ai_qcom_qneng_round_up(
    std::size_t _size, std::size_t _alignment) noexcept {
    return (_size + (_alignment - 1U)) & ~(_alignment - 1U);
}

}  // namespace

struct qnn_engine::implementation {
    qnn_sdk_libraries libraries_;
    const QNN_INTERFACE_VER_TYPE* qnn_{nullptr};
    Qnn_BackendHandle_t backend_{nullptr};
    Qnn_DeviceHandle_t device_{nullptr};
    Qnn_ContextHandle_t context_{nullptr};
    void* model_handle_{nullptr};
    compose_graphs_fn compose_{nullptr};
    free_graphs_fn free_graphs_{nullptr};
    qnn_model_graph_info** graphs_{nullptr};
    std::uint32_t graph_count_{0};
    // Resolved once at prepare so execute reconstructs no tensor metadata on the hot path.
    std::vector<tensor_spec> input_specs_;
    std::vector<tensor_spec> output_specs_;
    // Pre-allocated output workspace to eliminate per-frame heap allocations.
    std::vector<tensor_blob> output_workspace_;
    // Registered zero-copy ION/rpcmem buffers for model outputs.
    std::vector<qnn_registered_buffer> registered_outputs_;
    qnn_rpcmem_driver rpcmem_;
    const QnnHtpDevice_PerfInfrastructure_t* perf_{nullptr};
    std::uint32_t power_client_id_{0};
    std::string backend_library_;
    std::string system_library_;
    inference_execution_policy policy_;
    bool is_configured_{false};
    bool has_power_client_{false};
    bool supports_low_latency_{false};
    bool supports_shared_memory_{false};
    bool has_memhandle_output_{false};
    bool is_open_{false};
    bool is_prepared_{false};
};

qnn_engine::qnn_engine() : implementation_(std::make_unique<implementation>()) {}

qnn_engine::~qnn_engine() noexcept {
    vqec_vision_ai_qcom_qneng_close();
}

status qnn_engine::vqec_vision_ai_qcom_qneng_configure(
    const std::string& _backend_library, const std::string& _system_library,
    const inference_execution_policy& _policy) {
    if (implementation_ == nullptr) {
        return {status_code::invalid_state, "QNN engine implementation is unavailable"};
    }
    if (_backend_library.empty() || _system_library.empty()) {
        return {status_code::invalid_argument, "QNN backend and system paths are required"};
    }
    const auto valid_policy = vqec_vision_ai_core_inexe_validate_policy(_policy);
    if (valid_policy.code_ != status_code::ok) {
        return valid_policy;
    }
    auto& impl = *implementation_;
    if (impl.is_open_) {
        return impl.backend_library_ == _backend_library &&
                impl.system_library_ == _system_library ? status{} :
            status{status_code::invalid_state,
                "QNN engine cannot change configuration while open"};
    }
    impl.backend_library_ = _backend_library;
    impl.system_library_ = _system_library;
    impl.policy_ = _policy;
    impl.is_configured_ = true;
    return {};
}

status qnn_engine::vqec_vision_ai_qcom_qneng_ensure_open() {
    if (implementation_ == nullptr || !implementation_->is_configured_) {
        return {status_code::invalid_state, "QNN engine is not configured"};
    }
    if (implementation_->is_open_) {
        return {};
    }
    return vqec_vision_ai_qcom_qneng_open(
        implementation_->backend_library_, implementation_->system_library_,
        implementation_->policy_);
}

status qnn_engine::vqec_vision_ai_qcom_qneng_open(
    const std::string& _backend_library, const std::string& _system_library,
    const inference_execution_policy& _policy) {
    if (implementation_ == nullptr) {
        return {status_code::invalid_state, "QNN engine implementation is unavailable"};
    }
    const auto configured = vqec_vision_ai_qcom_qneng_configure(
        _backend_library, _system_library, _policy);
    if (configured.code_ != status_code::ok) {
        return configured;
    }
    auto& impl = *implementation_;
    if (impl.is_open_) {
        return {status_code::invalid_state, "QNN engine is already open"};
    }
    const auto valid_policy = vqec_vision_ai_core_inexe_validate_policy(_policy);
    if (valid_policy.code_ != status_code::ok) {
        return valid_policy;
    }
    const auto loaded = impl.libraries_.vqec_vision_ai_qcom_sdkld_open(
        _backend_library, _system_library);
    if (loaded.code_ != status_code::ok) {
        return loaded;
    }
    const auto* provider = static_cast<const QnnInterface_t*>(
        impl.libraries_.vqec_vision_ai_qcom_sdkld_get_provider());
    if (provider == nullptr) {
        impl.libraries_.vqec_vision_ai_qcom_sdkld_close();
        return {status_code::unsupported, "QNN interface provider is unavailable"};
    }
    impl.qnn_ = &provider->QNN_INTERFACE_VER_NAME;
    if (impl.qnn_->backendCreate == nullptr) {
        vqec_vision_ai_qcom_qneng_close();
        return {status_code::unsupported, "QNN backend interface is incomplete"};
    }
    if (impl.qnn_->backendCreate(nullptr, nullptr, &impl.backend_) != QNN_SUCCESS ||
        impl.backend_ == nullptr) {
        vqec_vision_ai_qcom_qneng_close();
        return {status_code::io_error, "QNN backend creation failed"};
    }
    if (impl.qnn_->deviceCreate != nullptr) {
        // HTP needs a device for affinity and performance configuration. A failed device
        // creation is a fault for the admitted HTP path, not a silent CPU downgrade.
        if (impl.qnn_->deviceCreate(nullptr, nullptr, &impl.device_) != QNN_SUCCESS) {
            vqec_vision_ai_qcom_qneng_close();
            return {status_code::unsupported, "QNN device creation failed for the selected backend"};
        }
    }
    // Probe the selected backend, not its filename. HTP ABI is private to this adapter.
    if (provider->backendId == QNN_BACKEND_ID_HTP &&
        impl.qnn_->deviceGetInfrastructure != nullptr &&
        impl.qnn_->deviceGetPlatformInfo != nullptr &&
        impl.qnn_->deviceFreePlatformInfo != nullptr) {
        QnnDevice_Infrastructure_t infrastructure = nullptr;
        if (impl.qnn_->deviceGetInfrastructure(&infrastructure) == QNN_SUCCESS &&
            infrastructure != nullptr &&
            infrastructure->infraType == QNN_HTP_DEVICE_INFRASTRUCTURE_TYPE_PERF) {
            impl.perf_ = &infrastructure->perfInfra;
            impl.supports_low_latency_ = impl.perf_->createPowerConfigId != nullptr &&
                impl.perf_->destroyPowerConfigId != nullptr &&
                impl.perf_->setPowerConfig != nullptr;
        }
    }
    impl.rpcmem_.lib_handle_ = ::dlopen("libcdsprpc.so", RTLD_NOW | RTLD_LOCAL);
    if (impl.rpcmem_.lib_handle_ != nullptr) {
        impl.rpcmem_.alloc_ = reinterpret_cast<rpcmem_alloc_fn>(
            ::dlsym(impl.rpcmem_.lib_handle_, "rpcmem_alloc"));
        impl.rpcmem_.to_fd_ = reinterpret_cast<rpcmem_to_fd_fn>(
            ::dlsym(impl.rpcmem_.lib_handle_, "rpcmem_to_fd"));
        impl.rpcmem_.free_ = reinterpret_cast<rpcmem_free_fn>(
            ::dlsym(impl.rpcmem_.lib_handle_, "rpcmem_free"));
    }
    impl.supports_shared_memory_ = impl.rpcmem_.is_available() &&
        impl.qnn_->memRegister != nullptr && impl.qnn_->memDeRegister != nullptr;
    impl.is_open_ = true;
    inference_capabilities capabilities;
    const auto probed = vqec_vision_ai_qcom_qneng_probe_capabilities(capabilities);
    const auto supported = probed.code_ == status_code::ok
        ? vqec_vision_ai_core_inexe_policy_is_supported(_policy, capabilities) : probed;
    if (supported.code_ != status_code::ok) {
        vqec_vision_ai_qcom_qneng_close();
        return supported;
    }
    if (_policy.profile_ == inference_perf_profile::low_latency) {
        const QnnDevice_PlatformInfo_t* platform = nullptr;
        const auto discovered = impl.qnn_->deviceGetPlatformInfo(nullptr, &platform);
        bool found = false;
        std::uint32_t device_id = 0;
        std::uint32_t core_id = 0;
        if (discovered == QNN_SUCCESS && platform != nullptr &&
            platform->version == QNN_DEVICE_PLATFORM_INFO_VERSION_1 &&
            platform->v1.hwDevices != nullptr) {
            for (std::uint32_t index = 0; index < platform->v1.numHwDevices && !found; ++index) {
                const auto& device = platform->v1.hwDevices[index];
                if (device.version != QNN_DEVICE_HARDWARE_DEVICE_INFO_VERSION_1 ||
                    device.v1.cores == nullptr) {
                    continue;
                }
                for (std::uint32_t core = 0; core < device.v1.numCores; ++core) {
                    const auto& info = device.v1.cores[core];
                    if (info.version == QNN_DEVICE_CORE_INFO_VERSION_1 &&
                        info.v1.coreType == QNN_HTP_CORE_TYPE_NSP) {
                        device_id = device.v1.deviceId;
                        core_id = info.v1.coreId;
                        found = true;
                        break;
                    }
                }
            }
        }
        if (platform != nullptr) {
            (void)impl.qnn_->deviceFreePlatformInfo(nullptr, platform);
        }
        if (!found || impl.perf_->createPowerConfigId(device_id, core_id,
                &impl.power_client_id_) != QNN_SUCCESS) {
            vqec_vision_ai_qcom_qneng_close();
            return {status_code::unsupported, "HTP performance client creation failed"};
        }
        impl.has_power_client_ = true;
        // Client zero overrides other votes in this process; never use it.
        if (impl.power_client_id_ == 0) {
            vqec_vision_ai_qcom_qneng_close();
            return {status_code::unsupported, "HTP returned a global power client"};
        }
        QnnHtpPerfInfrastructure_PowerConfig_t power{};
        power.option = QNN_HTP_PERF_INFRASTRUCTURE_POWER_CONFIGOPTION_DCVS_V3;
        auto& dcvs = power.dcvsV3Config;
        dcvs.contextId = impl.power_client_id_;
        dcvs.setDcvsEnable = 1;
        dcvs.dcvsEnable = 0;
        dcvs.powerMode = QNN_HTP_PERF_INFRASTRUCTURE_POWERMODE_PERFORMANCE_MODE;
        dcvs.setBusParams = 1;
        dcvs.busVoltageCornerMin = DCVS_VOLTAGE_VCORNER_TURBO;
        dcvs.busVoltageCornerTarget = DCVS_VOLTAGE_VCORNER_TURBO;
        dcvs.busVoltageCornerMax = DCVS_VOLTAGE_VCORNER_TURBO;
        dcvs.setCoreParams = 1;
        dcvs.coreVoltageCornerMin = DCVS_VOLTAGE_VCORNER_TURBO;
        dcvs.coreVoltageCornerTarget = DCVS_VOLTAGE_VCORNER_TURBO;
        dcvs.coreVoltageCornerMax = DCVS_VOLTAGE_VCORNER_TURBO;
        const QnnHtpPerfInfrastructure_PowerConfig_t* configs[] = {&power, nullptr};
        if (impl.perf_->setPowerConfig(impl.power_client_id_, configs) != QNN_SUCCESS) {
            vqec_vision_ai_qcom_qneng_close();
            return {status_code::io_error, "HTP low-latency power vote failed"};
        }
    }
    return {};
}

bool qnn_engine::vqec_vision_ai_qcom_qneng_is_open() const noexcept {
    return implementation_ != nullptr && implementation_->is_open_;
}

bool qnn_engine::vqec_vision_ai_qcom_qneng_is_configured() const noexcept {
    return implementation_ != nullptr && implementation_->is_configured_;
}

status qnn_engine::vqec_vision_ai_qcom_qneng_get_declared_capabilities(
    inference_capabilities& _capabilities) const noexcept {
    if (!vqec_vision_ai_qcom_qneng_is_configured()) {
        return {status_code::invalid_state, "QNN engine is not configured"};
    }
    inference_capabilities capabilities;
    capabilities.supported_dtype_mask_ = vqec_vision_ai_qcom_qneng_supported_dtype_mask();
    capabilities.perf_profile_mask_ = static_cast<std::uint8_t>(
        1U << static_cast<unsigned>(inference_perf_profile::balanced));
    capabilities.graph_count_ = 1;
    capabilities.max_inflight_jobs_ = 1;
    capabilities.supports_native_output_ = true;
    const auto valid = vqec_vision_ai_core_inexe_validate_capabilities(capabilities);
    if (valid.code_ != status_code::ok) {
        return valid;
    }
    _capabilities = capabilities;
    return {};
}

status qnn_engine::vqec_vision_ai_qcom_qneng_probe_capabilities(
    inference_capabilities& _capabilities) const noexcept {
    if (!vqec_vision_ai_qcom_qneng_is_open()) {
        return {status_code::invalid_state, "QNN engine is not open"};
    }
    // S01/O01: advertise only operations this adapter actually implements. SDK symbols may
    // be present (graphExecuteAsync, memRegister, contextApplyBinarySection) without a
    // wired lifecycle path, so capability must not be inherited from symbol presence.
    // Effective capability is the intersection of adapter + model graph + backend + policy.
    //
    // Implemented here: synchronous client-buffer execute of one graph with graph-native
    // (typed) output. Not implemented yet: async execute, shared/registered buffers,
    // artifact/LoRA update and multi-model execution domains; those report unsupported so
    // a policy that needs them is rejected before load or submit. Perf profile and compute
    // affinity are not applied, so only the default balanced profile is offered and the
    // accelerator topology stays unadvertised (0).
    inference_capabilities capabilities;
    capabilities.supported_dtype_mask_ = vqec_vision_ai_qcom_qneng_supported_dtype_mask();
    capabilities.perf_profile_mask_ =
        static_cast<std::uint8_t>(1U << static_cast<unsigned>(inference_perf_profile::balanced));
    if (implementation_->supports_low_latency_) {
        capabilities.perf_profile_mask_ |= static_cast<std::uint8_t>(
            1U << static_cast<unsigned>(inference_perf_profile::low_latency));
    }
    capabilities.compute_unit_count_ = 0;
    capabilities.graph_count_ = 1;
    capabilities.max_inflight_jobs_ = 1;
    capabilities.max_shared_registrations_ = implementation_->supports_shared_memory_
        ? g_max_shared_registrations : 0;
    capabilities.supports_async_ = false;
    capabilities.supports_native_output_ = true;
    capabilities.supports_shared_memory_ = implementation_->supports_shared_memory_;
    capabilities.supports_artifact_update_ = false;
    capabilities.supports_multi_model_domain_ = false;
    const auto valid = vqec_vision_ai_core_inexe_validate_capabilities(capabilities);
    if (valid.code_ != status_code::ok) {
        return valid;
    }
    _capabilities = capabilities;
    return {};
}

status qnn_engine::vqec_vision_ai_qcom_qneng_prepare(const std::string& _model_library) {
    if (!vqec_vision_ai_qcom_qneng_is_open()) {
        return {status_code::invalid_state, "QNN engine is not open"};
    }
    if (_model_library.empty()) {
        return {status_code::invalid_argument, "model library path is required"};
    }
    auto& impl = *implementation_;
    // A prepared model must be released explicitly first. This keeps reload a two-step
    // contract (release_model then prepare) instead of silently discarding a live model.
    if (impl.is_prepared_) {
        return {status_code::invalid_state, "QNN engine already has a prepared model"};
    }
    if (impl.qnn_->contextCreate == nullptr ||
        impl.qnn_->contextCreate(impl.backend_, impl.device_, nullptr, &impl.context_) !=
            QNN_SUCCESS ||
        impl.context_ == nullptr) {
        return {status_code::io_error, "QNN context creation failed"};
    }
    impl.model_handle_ = ::dlopen(_model_library.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (impl.model_handle_ == nullptr) {
        vqec_vision_ai_qcom_qneng_close();
        return {status_code::io_error, "cannot load QNN model library"};
    }
    impl.compose_ = reinterpret_cast<compose_graphs_fn>(
        ::dlsym(impl.model_handle_, "QnnModel_composeGraphs"));
    impl.free_graphs_ = reinterpret_cast<free_graphs_fn>(
        ::dlsym(impl.model_handle_, "QnnModel_freeGraphsInfo"));
    if (impl.compose_ == nullptr || impl.free_graphs_ == nullptr) {
        vqec_vision_ai_qcom_qneng_close();
        return {status_code::unsupported, "model library does not expose QNN compose functions"};
    }
    const auto composed = impl.compose_(
        impl.backend_, *impl.qnn_, impl.context_, nullptr, 0,
        &impl.graphs_, &impl.graph_count_, false, nullptr, QNN_LOG_LEVEL_ERROR);
    if (composed != g_model_no_error || impl.graphs_ == nullptr || impl.graph_count_ == 0) {
        vqec_vision_ai_qcom_qneng_close();
        return {status_code::unsupported, "QNN model graph composition failed"};
    }
    if (impl.graph_count_ != 1) {
        vqec_vision_ai_qcom_qneng_close();
        return {status_code::unsupported, "only single-graph model libraries are supported"};
    }
    const auto* graph = impl.graphs_[0];
    if (graph == nullptr || graph->inputTensors == nullptr || graph->outputTensors == nullptr ||
        graph->numInputTensors == 0 || graph->numOutputTensors == 0) {
        vqec_vision_ai_qcom_qneng_close();
        return {status_code::unsupported, "composed graph tensor metadata is missing"};
    }
    // Generated model libraries compose a graph but do not finalize it; execute fails with
    // "graph was not finalized" until graphFinalize runs. Board-discovered on QCS6490.
    if (impl.qnn_->graphFinalize == nullptr ||
        impl.qnn_->graphFinalize(graph->graph, nullptr, nullptr) != QNN_SUCCESS) {
        vqec_vision_ai_qcom_qneng_close();
        return {status_code::unsupported, "QNN graph finalization failed"};
    }
    // Resolve and validate tensor identity once, off the execute hot path. An unsupported
    // dtype or a zero-byte shape is rejected here, before any submit, not per frame.
    impl.input_specs_.clear();
    impl.output_specs_.clear();
    impl.input_specs_.reserve(graph->numInputTensors);
    impl.output_specs_.reserve(graph->numOutputTensors);
    // Reject a non-v2 tensor description here so execute never reads tensor.v2 on a
    // structure the vendor did not populate at version 2.
    for (std::uint32_t index = 0; index < graph->numInputTensors; ++index) {
        if (graph->inputTensors[index].version != QNN_TENSOR_VERSION_2) {
            impl.input_specs_.clear();
            impl.output_specs_.clear();
            vqec_vision_ai_qcom_qneng_close();
            return {status_code::unsupported, "model input tensor version is not v2"};
        }
        const auto spec = vqec_vision_ai_qcom_qneng_make_spec(graph->inputTensors[index]);
        if (spec.dtype_ == tensor_element_type::unknown ||
            vqec_vision_ai_core_tnctr_shape_bytes(spec) == 0) {
            impl.input_specs_.clear();
            impl.output_specs_.clear();
            vqec_vision_ai_qcom_qneng_close();
            return {status_code::unsupported, "model input tensor identity is unsupported"};
        }
        impl.input_specs_.push_back(spec);
    }
    for (std::uint32_t index = 0; index < graph->numOutputTensors; ++index) {
        if (graph->outputTensors[index].version != QNN_TENSOR_VERSION_2) {
            impl.input_specs_.clear();
            impl.output_specs_.clear();
            vqec_vision_ai_qcom_qneng_close();
            return {status_code::unsupported, "model output tensor version is not v2"};
        }
        const auto spec = vqec_vision_ai_qcom_qneng_make_spec(graph->outputTensors[index]);
        if (spec.dtype_ == tensor_element_type::unknown ||
            vqec_vision_ai_core_tnctr_shape_bytes(spec) == 0) {
            impl.input_specs_.clear();
            impl.output_specs_.clear();
            vqec_vision_ai_qcom_qneng_close();
            return {status_code::unsupported, "model output tensor identity is unsupported"};
        }
        impl.output_specs_.push_back(spec);
    }
    // Select exactly one QNN output backing store. Registered output must be
    // attempted before heap fallback so startup does not allocate and touch two
    // complete copies of every model output.
    impl.output_workspace_.clear();
    impl.registered_outputs_.clear();
    impl.has_memhandle_output_ = false;
    if (impl.supports_shared_memory_ && impl.context_ != nullptr) {
        bool all_registered = true;
        std::vector<qnn_registered_buffer> registered;
        registered.reserve(graph->numOutputTensors);
        for (std::uint32_t index = 0; index < graph->numOutputTensors; ++index) {
            const auto bytes = vqec_vision_ai_core_tnctr_shape_bytes(impl.output_specs_[index]);
            if (bytes > static_cast<std::uint64_t>(std::numeric_limits<int>::max()) -
                    (g_rpcmem_page_alignment - 1U)) {
                all_registered = false;
                break;
            }
            const std::size_t aligned_size = vqec_vision_ai_qcom_qneng_round_up(
                static_cast<std::size_t>(bytes), g_rpcmem_page_alignment);
            void* ptr = impl.rpcmem_.alloc_(
                g_rpcmem_heap_id_system, g_rpcmem_default_flags, static_cast<int>(aligned_size));
            const int fd = ptr != nullptr ? impl.rpcmem_.to_fd_(ptr) : -1;
            if (ptr == nullptr || fd < 0) {
                if (ptr != nullptr) {
                    impl.rpcmem_.free_(ptr);
                }
                all_registered = false;
                break;
            }

            Qnn_MemDescriptor_t desc{};
            desc.memShape.numDim = graph->outputTensors[index].v2.rank;
            desc.memShape.dimSize = graph->outputTensors[index].v2.dimensions;
            desc.memShape.shapeConfig = nullptr;
            desc.dataType = graph->outputTensors[index].v2.dataType;
            desc.memType = QNN_MEM_TYPE_ION;
            desc.ionInfo.fd = fd;
            Qnn_MemHandle_t handle = nullptr;
            const auto reg_status = impl.qnn_->memRegister(
                impl.context_, &desc, 1U, &handle);
            if (reg_status != QNN_SUCCESS || handle == nullptr) {
                impl.rpcmem_.free_(ptr);
                all_registered = false;
                break;
            }
            registered.push_back({ptr, fd, aligned_size, handle});
        }
        if (all_registered && registered.size() == graph->numOutputTensors) {
            impl.registered_outputs_ = std::move(registered);
            impl.has_memhandle_output_ = true;
            for (std::uint32_t index = 0; index < graph->numOutputTensors; ++index) {
                auto& tensor = graph->outputTensors[index];
                tensor.v2.memType = QNN_TENSORMEMTYPE_MEMHANDLE;
                tensor.v2.memHandle = impl.registered_outputs_[index].handle_;
            }
        } else {
            for (auto& buf : registered) {
                if (buf.handle_ != nullptr && impl.qnn_->memDeRegister != nullptr) {
                    (void)impl.qnn_->memDeRegister(&buf.handle_, 1U);
                }
                if (buf.data_ != nullptr && impl.rpcmem_.free_ != nullptr) {
                    impl.rpcmem_.free_(buf.data_);
                }
            }
            registered.clear();
        }
    }
    if (!impl.has_memhandle_output_) {
        try {
            impl.output_workspace_.reserve(impl.output_specs_.size());
            for (const auto& spec : impl.output_specs_) {
                tensor_blob blob;
                blob.spec_ = spec;
                const auto bytes = vqec_vision_ai_core_tnctr_shape_bytes(spec);
                blob.bytes_.resize(static_cast<std::size_t>(bytes));
                impl.output_workspace_.push_back(std::move(blob));
            }
        } catch (const std::bad_alloc&) {
            vqec_vision_ai_qcom_qneng_close();
            return {status_code::resource_exhausted,
                "cannot allocate QNN output fallback workspace"};
        }
    }
    impl.is_prepared_ = true;
    return {};
}

status qnn_engine::vqec_vision_ai_qcom_qneng_get_tensors(
    std::vector<tensor_spec>& _inputs, std::vector<tensor_spec>& _outputs) const {
    if (implementation_ == nullptr || !implementation_->is_prepared_) {
        return {status_code::invalid_state, "QNN engine has no prepared model"};
    }
    _inputs = implementation_->input_specs_;
    _outputs = implementation_->output_specs_;
    return {};
}

status qnn_engine::vqec_vision_ai_qcom_qneng_execute(
    const std::vector<tensor_blob>& _inputs, std::vector<tensor_blob>& _outputs) {
    if (implementation_ == nullptr || !implementation_->is_prepared_) {
        return {status_code::invalid_state, "QNN engine has no prepared model"};
    }
    auto& impl = *implementation_;
    auto* graph = impl.graphs_[0];
    if (graph == nullptr) {
        return {status_code::invalid_state, "QNN model graph is unavailable"};
    }
    if (_inputs.size() != impl.input_specs_.size() ||
        impl.input_specs_.size() != graph->numInputTensors) {
        return {status_code::invalid_argument, "input tensor count differs from the model graph"};
    }
    // Full identity check against the cached graph contract: name, shape, dtype and
    // quantization must all match, not only the total byte count.
    for (std::size_t index = 0; index < impl.input_specs_.size(); ++index) {
        const auto& expected = impl.input_specs_[index];
        if (!vqec_vision_ai_qcom_qneng_same_spec(_inputs[index].spec_, expected) ||
            _inputs[index].bytes_.size() !=
                vqec_vision_ai_core_tnctr_shape_bytes(expected)) {
            return {status_code::invalid_argument,
                "input blob does not match the model graph tensor identity"};
        }
    }
    for (std::uint32_t index = 0; index < graph->numInputTensors; ++index) {
        auto& tensor = graph->inputTensors[index];
        // Defense in depth: prepare rejected non-v2 tensors, so this only fires if the graph
        // metadata changed after prepare. Never reinterpret a version-1 union as v2.
        if (tensor.version != QNN_TENSOR_VERSION_2) {
            return {status_code::unsupported, "model input tensor version changed to non-v2"};
        }
        tensor.v2.memType = QNN_TENSORMEMTYPE_RAW;
        tensor.v2.clientBuf.data = const_cast<std::uint8_t*>(_inputs[index].bytes_.data());
        tensor.v2.clientBuf.dataSize = _inputs[index].bytes_.size();
    }
    if (!impl.has_memhandle_output_) {
        for (std::uint32_t index = 0; index < graph->numOutputTensors; ++index) {
            auto& tensor = graph->outputTensors[index];
            if (tensor.version != QNN_TENSOR_VERSION_2) {
                return {status_code::unsupported,
                    "model output tensor version changed to non-v2"};
            }
            tensor.v2.memType = QNN_TENSORMEMTYPE_RAW;
            tensor.v2.clientBuf.data = impl.output_workspace_[index].bytes_.data();
            tensor.v2.clientBuf.dataSize = impl.output_workspace_[index].bytes_.size();
        }
    }
    const auto executed = impl.qnn_->graphExecute(
        graph->graph, graph->inputTensors, graph->numInputTensors,
        graph->outputTensors, graph->numOutputTensors, nullptr, nullptr);
    if (executed != QNN_SUCCESS) {
        return {status_code::io_error, "QNN graph execution failed"};
    }
    if (_outputs.size() != impl.output_specs_.size()) {
        _outputs.resize(impl.output_specs_.size());
    }
    for (std::size_t index = 0; index < impl.output_specs_.size(); ++index) {
        _outputs[index].spec_ = impl.output_specs_[index];
        const auto bytes = static_cast<std::size_t>(
            vqec_vision_ai_core_tnctr_shape_bytes(impl.output_specs_[index]));
        if (_outputs[index].bytes_.size() != bytes) {
            _outputs[index].bytes_.resize(bytes);
        }
        const void* source = impl.has_memhandle_output_
            ? impl.registered_outputs_[index].data_
            : impl.output_workspace_[index].bytes_.data();
        if (source == nullptr ||
            (impl.has_memhandle_output_ && impl.registered_outputs_[index].size_ < bytes)) {
            return {status_code::invalid_state, "QNN output backing store is invalid"};
        }
        std::memcpy(_outputs[index].bytes_.data(), source, bytes);
    }
    return {};
}

void qnn_engine::vqec_vision_ai_qcom_qneng_release_model() noexcept {
    if (implementation_ == nullptr) {
        return;
    }
    auto& impl = *implementation_;
    if (!impl.registered_outputs_.empty()) {
        std::vector<Qnn_MemHandle_t> handles;
        handles.reserve(impl.registered_outputs_.size());
        for (const auto& buf : impl.registered_outputs_) {
            if (buf.handle_ != nullptr) {
                handles.push_back(buf.handle_);
            }
        }
        if (!handles.empty() && impl.qnn_ != nullptr && impl.qnn_->memDeRegister != nullptr) {
            (void)impl.qnn_->memDeRegister(
                handles.data(), static_cast<std::uint32_t>(handles.size()));
        }
        for (auto& buf : impl.registered_outputs_) {
            if (buf.data_ != nullptr && impl.rpcmem_.free_ != nullptr) {
                impl.rpcmem_.free_(buf.data_);
            }
        }
        impl.registered_outputs_.clear();
    }
    impl.has_memhandle_output_ = false;
    impl.output_workspace_.clear();
    impl.input_specs_.clear();
    impl.output_specs_.clear();
    if (impl.graphs_ != nullptr && impl.free_graphs_ != nullptr) {
        (void)impl.free_graphs_(&impl.graphs_, impl.graph_count_);
    }
    impl.graphs_ = nullptr;
    impl.graph_count_ = 0;
    impl.compose_ = nullptr;
    impl.free_graphs_ = nullptr;
    if (impl.model_handle_ != nullptr) {
        ::dlclose(impl.model_handle_);
        impl.model_handle_ = nullptr;
    }
    if (impl.qnn_ != nullptr && impl.context_ != nullptr && impl.qnn_->contextFree != nullptr) {
        (void)impl.qnn_->contextFree(impl.context_, nullptr);
    }
    impl.context_ = nullptr;
    impl.is_prepared_ = false;
}

void qnn_engine::vqec_vision_ai_qcom_qneng_close() noexcept {
    if (implementation_ == nullptr) {
        return;
    }
    auto& impl = *implementation_;
    // Release the model-vscoped state first; the remaining teardown is backend/device.
    vqec_vision_ai_qcom_qneng_release_model();
    if (impl.has_power_client_ && impl.perf_ != nullptr) {
        (void)impl.perf_->destroyPowerConfigId(impl.power_client_id_);
    }
    impl.has_power_client_ = false;
    impl.power_client_id_ = 0;
    impl.perf_ = nullptr;
    impl.supports_low_latency_ = false;
    if (impl.qnn_ != nullptr && impl.device_ != nullptr && impl.qnn_->deviceFree != nullptr) {
        (void)impl.qnn_->deviceFree(impl.device_);
    }
    impl.device_ = nullptr;
    if (impl.qnn_ != nullptr && impl.backend_ != nullptr && impl.qnn_->backendFree != nullptr) {
        (void)impl.qnn_->backendFree(impl.backend_);
    }
    impl.backend_ = nullptr;
    impl.qnn_ = nullptr;
    impl.is_open_ = false;
    impl.libraries_.vqec_vision_ai_qcom_sdkld_close();
    if (impl.rpcmem_.lib_handle_ != nullptr) {
        ::dlclose(impl.rpcmem_.lib_handle_);
        impl.rpcmem_.lib_handle_ = nullptr;
        impl.rpcmem_.alloc_ = nullptr;
        impl.rpcmem_.to_fd_ = nullptr;
        impl.rpcmem_.free_ = nullptr;
    }
}

}  // namespace vqec::vision::ai
