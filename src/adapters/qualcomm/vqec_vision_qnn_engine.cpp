#include "vqec_vision_qnn_engine.hpp"

#include <cstdint>
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
// locally instead of including the restricted SDK example header.
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

using model_error_t = std::int32_t;
using compose_graphs_fn = model_error_t (*)(
    Qnn_BackendHandle_t, QNN_INTERFACE_VER_TYPE, Qnn_ContextHandle_t,
    const qnn_model_graph_config_info**, std::uint32_t,
    qnn_model_graph_info***, std::uint32_t*, bool, QnnLog_Callback_t, QnnLog_Level_t);
using free_graphs_fn = model_error_t (*)(qnn_model_graph_info***, std::uint32_t);

constexpr model_error_t g_model_no_error = 0;

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
    const QnnHtpDevice_PerfInfrastructure_t* perf_{nullptr};
    std::uint32_t power_client_id_{0};
    bool has_power_client_{false};
    bool supports_low_latency_{false};
    bool is_open_{false};
    bool is_prepared_{false};
};

qnn_engine::qnn_engine() : implementation_(std::make_unique<implementation>()) {}

qnn_engine::~qnn_engine() noexcept {
    vqec_vision_ai_qcom_qneng_close();
}

status qnn_engine::vqec_vision_ai_qcom_qneng_open(
    const std::string& _backend_library, const std::string& _system_library,
    const inference_execution_policy& _policy) {
    if (implementation_ == nullptr) {
        return {status_code::invalid_state, "QNN engine implementation is unavailable"};
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
    capabilities.max_shared_registrations_ = 0;
    capabilities.supports_async_ = false;
    capabilities.supports_native_output_ = true;
    capabilities.supports_shared_memory_ = false;
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
    for (std::uint32_t index = 0; index < graph->numInputTensors; ++index) {
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
    std::vector<tensor_blob> outputs;
    outputs.reserve(impl.output_specs_.size());
    for (const auto& spec : impl.output_specs_) {
        tensor_blob blob;
        blob.spec_ = spec;
        const auto bytes = vqec_vision_ai_core_tnctr_shape_bytes(spec);
        if (bytes == 0) {
            return {status_code::unsupported, "model output tensor metadata is invalid"};
        }
        blob.bytes_.resize(static_cast<std::size_t>(bytes));
        outputs.push_back(std::move(blob));
    }
    for (std::uint32_t index = 0; index < graph->numInputTensors; ++index) {
        auto& tensor = graph->inputTensors[index];
        tensor.v2.memType = QNN_TENSORMEMTYPE_RAW;
        tensor.v2.clientBuf.data = const_cast<std::uint8_t*>(_inputs[index].bytes_.data());
        tensor.v2.clientBuf.dataSize = _inputs[index].bytes_.size();
    }
    for (std::uint32_t index = 0; index < graph->numOutputTensors; ++index) {
        auto& tensor = graph->outputTensors[index];
        tensor.v2.memType = QNN_TENSORMEMTYPE_RAW;
        tensor.v2.clientBuf.data = outputs[index].bytes_.data();
        tensor.v2.clientBuf.dataSize = outputs[index].bytes_.size();
    }
    const auto executed = impl.qnn_->graphExecute(
        graph->graph, graph->inputTensors, graph->numInputTensors,
        graph->outputTensors, graph->numOutputTensors, nullptr, nullptr);
    if (executed != QNN_SUCCESS) {
        return {status_code::io_error, "QNN graph execution failed"};
    }
    _outputs = std::move(outputs);
    return {};
}

void qnn_engine::vqec_vision_ai_qcom_qneng_close() noexcept {
    if (implementation_ == nullptr) {
        return;
    }
    auto& impl = *implementation_;
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
}

}  // namespace vqec::vision::ai
