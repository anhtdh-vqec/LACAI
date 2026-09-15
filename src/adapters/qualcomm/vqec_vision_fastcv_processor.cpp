#include "vqec_vision_fastcv_processor.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <new>
#include <utility>

#if defined(__aarch64__)
#include <arm_neon.h>
#endif

#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <gst/gst.h>

#include "vqec_vision_dmabuf_bridge.hpp"
#include "vqec_vision_tensor_output.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_color.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_tensor_contract.hpp"

namespace vqec::vision::ai {
namespace {

inline constexpr char g_source_factory[] = "appsrc";
inline constexpr char g_transform_factory[] = "qtivtransform";
inline constexpr char g_converter_factory[] = "qtimlvconverter";
inline constexpr char g_filter_factory[] = "capsfilter";
inline constexpr char g_sink_factory[] = "appsink";
inline constexpr char g_source_name[] = "input";
inline constexpr char g_transform_name[] = "letterbox";
inline constexpr char g_video_filter_name[] = "letterbox_caps";
inline constexpr char g_converter_name[] = "tensor_converter";
inline constexpr char g_tensor_filter_name[] = "tensor_caps";
inline constexpr char g_sink_name[] = "output";
inline constexpr char g_engine_property[] = "engine";
inline constexpr char g_fastcv_engine[] = "fcv";
inline constexpr char g_mode_property[] = "mode";
inline constexpr char g_single_image_mode[] = "image-batch-non-cumulative";
inline constexpr char g_disposition_property[] = "image-disposition";
inline constexpr char g_stretch_disposition[] = "stretch";
inline constexpr char g_layout_property[] = "subpixel-layout";
inline constexpr char g_regular_layout[] = "regular";
inline constexpr char g_reverse_layout[] = "reverse";
inline constexpr char g_caps_property[] = "caps";
inline constexpr char g_format_property[] = "format";
inline constexpr char g_is_live_property[] = "is-live";
inline constexpr char g_timestamp_property[] = "do-timestamp";
inline constexpr char g_block_property[] = "block";
inline constexpr char g_max_bytes_property[] = "max-bytes";
inline constexpr char g_background_property[] = "background";
inline constexpr char g_destination_property[] = "destination";
inline constexpr char g_max_buffers_property[] = "max-buffers";
inline constexpr char g_drop_property[] = "drop";
inline constexpr char g_sync_property[] = "sync";
inline constexpr char g_async_property[] = "async";
inline constexpr char g_last_sample_property[] = "enable-last-sample";
inline constexpr char g_nv12_format[] = "NV12";
inline constexpr char g_video_caps_name[] = "video/x-raw";
inline constexpr char g_tensor_caps_name[] = "neural-network/tensors";
inline constexpr char g_bt601_colorimetry[] = "bt601";
inline constexpr char g_bt709_colorimetry[] = "bt709";
inline constexpr char g_mpeg2_chroma_site[] = "mpeg2";
inline constexpr std::uint32_t g_tensor_batch = 1;
inline constexpr std::uint32_t g_tensor_channels = 3;
inline constexpr float g_uint8_input_max = 255.0F;
inline constexpr float g_float_comparison_tolerance = 0.0001F;
#if defined(__aarch64__)
inline constexpr std::size_t g_neon_u8_lane_count = 16U;
#endif

struct gst_object_deleter {
    void operator()(GstObject* _object) const noexcept {
        if (_object != nullptr) {
            gst_object_unref(_object);
        }
    }
};

struct gst_caps_deleter {
    void operator()(GstCaps* _caps) const noexcept {
        if (_caps != nullptr) {
            gst_caps_unref(_caps);
        }
    }
};

struct gst_sample_deleter {
    void operator()(GstSample* _sample) const noexcept {
        if (_sample != nullptr) {
            gst_sample_unref(_sample);
        }
    }
};

using gst_object_owner = std::unique_ptr<GstObject, gst_object_deleter>;
using gst_caps_owner = std::unique_ptr<GstCaps, gst_caps_deleter>;
using gst_sample_owner = std::unique_ptr<GstSample, gst_sample_deleter>;

bool vqec_vision_ai_qcom_fcprc_same_target(
    const tensor_spec& _left, const tensor_spec& _right) noexcept {
    return _left.name_ == _right.name_ && _left.dimensions_ == _right.dimensions_ &&
        _left.dtype_ == _right.dtype_ &&
        _left.quantization_.is_quantized_ == _right.quantization_.is_quantized_ &&
        _left.quantization_.zero_point_ == _right.quantization_.zero_point_ &&
        std::fabs(_left.quantization_.scale_ - _right.quantization_.scale_) <=
            g_float_comparison_tolerance;
}

status vqec_vision_ai_qcom_fcprc_set_enum(
    GObject* _object, const char* _property, const char* _nick) {
    const auto* specification =
        g_object_class_find_property(G_OBJECT_GET_CLASS(_object), _property);
    if (specification == nullptr || !G_IS_PARAM_SPEC_ENUM(specification) ||
        (specification->flags & G_PARAM_WRITABLE) == 0) {
        return {status_code::incompatible_plugin, "required Qualcomm enum property is absent"};
    }
    auto* values = static_cast<GEnumClass*>(
        g_type_class_ref(G_PARAM_SPEC_VALUE_TYPE(specification)));
    if (values == nullptr) {
        return {status_code::incompatible_plugin, "cannot inspect Qualcomm enum property"};
    }
    const auto* selected = g_enum_get_value_by_nick(values, _nick);
    if (selected == nullptr) {
        g_type_class_unref(values);
        return {status_code::incompatible_plugin, "required Qualcomm enum value is absent"};
    }
    const int value = selected->value;
    g_type_class_unref(values);
    g_object_set(_object, _property, value, nullptr);
    return {};
}

status vqec_vision_ai_qcom_fcprc_add_element(
    GstElement* _pipeline, const char* _factory, const char* _name,
    GstElement*& _element) {
    auto* element = gst_element_factory_make(_factory, _name);
    if (element == nullptr) {
        return {status_code::missing_plugin, "required Qualcomm preprocessing plugin is absent"};
    }
    if (!gst_bin_add(GST_BIN(_pipeline), element)) {
        gst_object_unref(element);
        return {status_code::incompatible_plugin,
            "cannot add Qualcomm preprocessing element"};
    }
    _element = element;
    return {};
}

gst_caps_owner vqec_vision_ai_qcom_fcprc_make_tensor_caps(
    const tensor_spec& _target) {
    const char* type_name = vqec_vision_ai_qcom_tnout_ml_type_name(_target.dtype_);
    if (type_name == nullptr) {
        return {};
    }
    gst_caps_owner caps(gst_caps_new_simple(
        g_tensor_caps_name, "type", G_TYPE_STRING, type_name, nullptr));
    if (!caps) {
        return {};
    }
    GValue dimensions = G_VALUE_INIT;
    GValue tensor_dimensions = G_VALUE_INIT;
    g_value_init(&dimensions, GST_TYPE_ARRAY);
    g_value_init(&tensor_dimensions, GST_TYPE_ARRAY);
    for (const auto dimension : _target.dimensions_) {
        GValue value = G_VALUE_INIT;
        g_value_init(&value, G_TYPE_INT);
        g_value_set_int(&value, static_cast<int>(dimension));
        gst_value_array_append_value(&tensor_dimensions, &value);
        g_value_unset(&value);
    }
    gst_value_array_append_value(&dimensions, &tensor_dimensions);
    gst_caps_set_value(caps.get(), "dimensions", &dimensions);
    g_value_unset(&tensor_dimensions);
    g_value_unset(&dimensions);
    return caps;
}

std::uint32_t vqec_vision_ai_qcom_fcprc_background_rgba(
    const preprocess_spec& _spec) noexcept {
    const auto channel = [](float _value) {
        return static_cast<std::uint32_t>(std::clamp(_value, 0.0F, g_uint8_input_max));
    };
    const auto red = channel(_spec.pad_value_[0]);
    const auto green = channel(_spec.pad_value_[1]);
    const auto blue = channel(_spec.pad_value_[2]);
    return (red << 24U) | (green << 16U) | (blue << 8U) | 0xffU;
}

status vqec_vision_ai_qcom_fcprc_read_bus(GstElement* _pipeline) {
    gst_object_owner bus_owner(GST_OBJECT(gst_element_get_bus(_pipeline)));
    auto* bus = GST_BUS(bus_owner.get());
    if (bus == nullptr) {
        return {status_code::incompatible_plugin, "Qualcomm preprocessing bus is absent"};
    }
    GstMessage* message = gst_bus_pop_filtered(
        bus, static_cast<GstMessageType>(GST_MESSAGE_ERROR));
    if (message == nullptr) {
        return {};
    }
    GError* error = nullptr;
    gchar* debug = nullptr;
    gst_message_parse_error(message, &error, &debug);
    const std::string text = error != nullptr ? error->message :
        "Qualcomm preprocessing pipeline failed";
    if (error != nullptr) {
        g_error_free(error);
    }
    g_free(debug);
    gst_message_unref(message);
    return {status_code::io_error, text};
}

void vqec_vision_ai_qcom_fcprc_widen_u8(
    const std::uint8_t* _source, std::size_t _elements,
    std::uint8_t* _destination) noexcept {
    std::size_t index = 0;
#if defined(__aarch64__)
    for (; index + g_neon_u8_lane_count <= _elements;
         index += g_neon_u8_lane_count) {
        const uint8x16_t input = vld1q_u8(_source + index);
        vst1q_u8(_destination + index * 2U, vzip1q_u8(input, input));
        vst1q_u8(_destination + index * 2U + g_neon_u8_lane_count,
            vzip2q_u8(input, input));
    }
#endif
    for (; index < _elements; ++index) {
        _destination[index * 2U] = _source[index];
        _destination[index * 2U + 1U] = _source[index];
    }
}

}  // namespace

struct fastcv_processor::implementation {
    explicit implementation(fastcv_processor_config _config) : config_(std::move(_config)) {}
    ~implementation() noexcept {
        if (pipeline_ != nullptr) {
            gst_element_set_state(pipeline_, GST_STATE_NULL);
            gst_object_unref(pipeline_);
        }
    }

    fastcv_processor_config config_;
    GstElement* pipeline_{nullptr};
    GstElement* source_{nullptr};
    GstElement* sink_{nullptr};
    inference_plan plan_;
    tensor_spec target_;
    dmabuf_bridge_profile bridge_;
    dmabuf_allocator_context allocator_;
    bool is_ready_{false};
};

fastcv_processor::fastcv_processor(fastcv_processor_config _config)
    : implementation_(std::make_unique<implementation>(std::move(_config))) {}

fastcv_processor::~fastcv_processor() noexcept = default;

status fastcv_processor::vqec_vision_ai_ports_imgpr_validate(
    const raw_frame& _frame, const inference_plan& _plan,
    const tensor_spec& _target) const {
    if (implementation_ == nullptr ||
        implementation_->config_.max_frame_allocation_bytes_ == 0 ||
        implementation_->config_.output_timeout_ns_ == 0 ||
        implementation_->config_.output_timeout_ns_ == UINT64_MAX) {
        return {status_code::invalid_state, "FastCV processor configuration is invalid"};
    }
    const auto plan_status = vqec_vision_ai_core_infpl_validate_plan(_plan);
    if (plan_status.code_ != status_code::ok) {
        return plan_status;
    }
    const auto preprocess_status = vqec_vision_ai_core_ppspc_validate(_plan.preprocess_);
    if (preprocess_status.code_ != status_code::ok) {
        return preprocess_status;
    }
    const auto& descriptor = _frame.descriptor_;
    if (!_frame.owner_ || _frame.native_handle_ < 0 || descriptor.width_ != _plan.source_width_ ||
        descriptor.height_ != _plan.source_height_ || descriptor.width_ % 2U != 0 ||
        descriptor.height_ % 2U != 0 || descriptor.allocation_size_bytes_ == 0 ||
        descriptor.allocation_size_bytes_ >
            implementation_->config_.max_frame_allocation_bytes_) {
        return {status_code::invalid_argument, "invalid RAW frame for FastCV preprocessing"};
    }
    if (_target.dimensions_.size() != 4U || _target.dimensions_[0] != g_tensor_batch ||
        _target.dimensions_[1] != _plan.tensor_height_ ||
        _target.dimensions_[2] != _plan.tensor_width_ ||
        _target.dimensions_[3] != g_tensor_channels ||
        (_target.dtype_ != tensor_element_type::uint8 &&
            _target.dtype_ != tensor_element_type::uint16) ||
        !_target.quantization_.is_quantized_) {
        return {status_code::unsupported,
            "FastCV adapter requires a quantized NHWC RGB uint8/uint16 tensor"};
    }
    if (_plan.preprocess_.source_format_ != source_pixel_format::nv12 ||
        _plan.preprocess_.resize_ == resize_mode::crop ||
        _plan.preprocess_.interpolation_ != interpolation_mode::bilinear ||
        (_plan.preprocess_.matrix_ != color_matrix::bt601 &&
            _plan.preprocess_.matrix_ != color_matrix::bt709) ||
        _plan.preprocess_.range_ != color_range::limited ||
        _plan.preprocess_.normalization_ != normalization_formula::offset_scale) {
        return {status_code::unsupported,
            "FastCV adapter does not implement the requested preprocess semantics"};
    }
    const auto direct_mapping =
        vqec_vision_ai_core_color_validate_direct_integer_mapping(
            _plan.preprocess_, _target);
    if (direct_mapping.code_ != status_code::ok) {
        return direct_mapping;
    }
    return {};
}

status fastcv_processor::vqec_vision_ai_ports_imgpr_preprocess(
    const raw_frame& _frame, const inference_plan& _plan,
    const tensor_spec& _target, std::vector<tensor_blob>& _outputs) {
    const auto valid = vqec_vision_ai_ports_imgpr_validate(_frame, _plan, _target);
    if (valid.code_ != status_code::ok) {
        return valid;
    }
    try {
        _outputs.reserve(1U);
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted,
            "cannot reserve Qualcomm preprocessing output"};
    }
    auto& impl = *implementation_;
    if (!impl.is_ready_) {
        GError* init_error = nullptr;
        if (!gst_init_check(nullptr, nullptr, &init_error)) {
            const std::string message = init_error != nullptr ? init_error->message :
                "GStreamer initialization failed";
            if (init_error != nullptr) {
                g_error_free(init_error);
            }
            return {status_code::incompatible_plugin, message};
        }
        auto* pipeline = gst_pipeline_new("vqec_vision_ai_fastcv_preprocess");
        if (pipeline == nullptr) {
            return {status_code::resource_exhausted,
                "cannot allocate Qualcomm preprocessing pipeline"};
        }
        GstElement* source = nullptr;
        GstElement* transform = nullptr;
        GstElement* video_filter = nullptr;
        GstElement* converter = nullptr;
        GstElement* tensor_filter = nullptr;
        GstElement* sink = nullptr;
        const std::array<status, 6> created{
            vqec_vision_ai_qcom_fcprc_add_element(
                pipeline, g_source_factory, g_source_name, source),
            vqec_vision_ai_qcom_fcprc_add_element(
                pipeline, g_transform_factory, g_transform_name, transform),
            vqec_vision_ai_qcom_fcprc_add_element(
                pipeline, g_filter_factory, g_video_filter_name, video_filter),
            vqec_vision_ai_qcom_fcprc_add_element(
                pipeline, g_converter_factory, g_converter_name, converter),
            vqec_vision_ai_qcom_fcprc_add_element(
                pipeline, g_filter_factory, g_tensor_filter_name, tensor_filter),
            vqec_vision_ai_qcom_fcprc_add_element(
                pipeline, g_sink_factory, g_sink_name, sink)};
        for (const auto& result : created) {
            if (result.code_ != status_code::ok) {
                gst_object_unref(pipeline);
                return result;
            }
        }
        gst_caps_owner source_caps(gst_caps_new_simple(g_video_caps_name,
            "format", G_TYPE_STRING, g_nv12_format,
            "width", G_TYPE_INT, static_cast<int>(_plan.source_width_),
            "height", G_TYPE_INT, static_cast<int>(_plan.source_height_),
            "framerate", GST_TYPE_FRACTION, static_cast<int>(_plan.fps_numerator_),
            static_cast<int>(_plan.fps_denominator_),
            "colorimetry", G_TYPE_STRING,
            _plan.preprocess_.matrix_ == color_matrix::bt601 ?
                g_bt601_colorimetry : g_bt709_colorimetry,
            "chroma-site", G_TYPE_STRING, g_mpeg2_chroma_site, nullptr));
        gst_caps_owner transformed_caps(gst_caps_new_simple(g_video_caps_name,
            "format", G_TYPE_STRING, g_nv12_format,
            "width", G_TYPE_INT, static_cast<int>(_plan.tensor_width_),
            "height", G_TYPE_INT, static_cast<int>(_plan.tensor_height_), nullptr));
        // qtimlvconverter normalizes integer widths above UINT8 in a generic CPU loop.
        // Keep resize/color conversion on FastCV in UINT8 and widen into the model's
        // quantized tensor below, where the packed loop can be vectorized.
        tensor_spec converter_target = _target;
        converter_target.dtype_ = tensor_element_type::uint8;
        converter_target.quantization_ = {};
        auto tensor_caps = vqec_vision_ai_qcom_fcprc_make_tensor_caps(converter_target);
        if (!source_caps || !transformed_caps || !tensor_caps) {
            gst_object_unref(pipeline);
            return {status_code::resource_exhausted,
                "cannot allocate Qualcomm preprocessing caps"};
        }
        const float scale = std::min(
            static_cast<float>(_plan.tensor_width_) / _plan.source_width_,
            static_cast<float>(_plan.tensor_height_) / _plan.source_height_);
        const auto destination_width = _plan.preprocess_.resize_ == resize_mode::stretch ?
            _plan.tensor_width_ : static_cast<std::uint32_t>(_plan.source_width_ * scale);
        const auto destination_height = _plan.preprocess_.resize_ == resize_mode::stretch ?
            _plan.tensor_height_ : static_cast<std::uint32_t>(_plan.source_height_ * scale);
        const auto destination_x = _plan.preprocess_.placement_ == image_placement::centre ?
            (_plan.tensor_width_ - destination_width) / 2U : 0U;
        const auto destination_y = _plan.preprocess_.placement_ == image_placement::centre ?
            (_plan.tensor_height_ - destination_height) / 2U : 0U;
        GValue destination = G_VALUE_INIT;
        g_value_init(&destination, GST_TYPE_ARRAY);
        for (const auto coordinate :
             {destination_x, destination_y, destination_width, destination_height}) {
            GValue value = G_VALUE_INIT;
            g_value_init(&value, G_TYPE_INT);
            g_value_set_int(&value, static_cast<int>(coordinate));
            gst_value_array_append_value(&destination, &value);
            g_value_unset(&value);
        }
        g_object_set(source,
            g_caps_property, source_caps.get(), g_format_property, GST_FORMAT_TIME,
            g_is_live_property, TRUE, g_timestamp_property, FALSE,
            g_block_property, TRUE, g_max_bytes_property, _plan.input_queue_bytes_, nullptr);
        g_object_set(transform,
            g_background_property,
            vqec_vision_ai_qcom_fcprc_background_rgba(_plan.preprocess_), nullptr);
        g_object_set_property(G_OBJECT(transform), g_destination_property, &destination);
        g_value_unset(&destination);
        g_object_set(video_filter, g_caps_property, transformed_caps.get(), nullptr);
        g_object_set(tensor_filter, g_caps_property, tensor_caps.get(), nullptr);
        g_object_set(sink, g_max_buffers_property, 1U, g_drop_property, FALSE,
            g_sync_property, FALSE, g_async_property, FALSE,
            g_last_sample_property, FALSE, nullptr);
        const std::array<status, 4> configured{
            vqec_vision_ai_qcom_fcprc_set_enum(
                G_OBJECT(transform), g_engine_property, g_fastcv_engine),
            vqec_vision_ai_qcom_fcprc_set_enum(
                G_OBJECT(converter), g_engine_property, g_fastcv_engine),
            vqec_vision_ai_qcom_fcprc_set_enum(
                G_OBJECT(converter), g_mode_property, g_single_image_mode),
            vqec_vision_ai_qcom_fcprc_set_enum(G_OBJECT(converter),
                g_layout_property, _plan.preprocess_.channels_ == channel_order::rgb ?
                    g_regular_layout : g_reverse_layout)};
        for (const auto& result : configured) {
            if (result.code_ != status_code::ok) {
                gst_object_unref(pipeline);
                return result;
            }
        }
        const auto disposition = vqec_vision_ai_qcom_fcprc_set_enum(
            G_OBJECT(converter), g_disposition_property, g_stretch_disposition);
        if (disposition.code_ != status_code::ok ||
            !gst_element_link_many(source, transform, video_filter, converter,
                tensor_filter, sink, nullptr)) {
            gst_object_unref(pipeline);
            return disposition.code_ != status_code::ok ? disposition :
                status{status_code::graph_link_failed,
                    "cannot link Qualcomm preprocessing pipeline"};
        }
        const auto transition = gst_element_set_state(pipeline, GST_STATE_PLAYING);
        if (transition == GST_STATE_CHANGE_FAILURE) {
            gst_element_set_state(pipeline, GST_STATE_NULL);
            gst_object_unref(pipeline);
            return {status_code::io_error,
                "cannot start Qualcomm preprocessing pipeline"};
        }
        impl.pipeline_ = pipeline;
        impl.source_ = source;
        impl.sink_ = sink;
        impl.plan_ = _plan;
        impl.target_ = _target;
        impl.bridge_ = {_plan.source_width_, _plan.source_height_,
            impl.config_.max_frame_allocation_bytes_};
        impl.is_ready_ = true;
    } else if (impl.plan_.source_width_ != _plan.source_width_ ||
        impl.plan_.source_height_ != _plan.source_height_ ||
        impl.plan_.tensor_width_ != _plan.tensor_width_ ||
        impl.plan_.tensor_height_ != _plan.tensor_height_ ||
        !vqec_vision_ai_qcom_fcprc_same_target(impl.target_, _target)) {
        return {status_code::invalid_state,
            "FastCV processor cannot change an active preprocess contract"};
    }

    GstBuffer* input = nullptr;
    const auto wrapped = vqec_vision_ai_qcom_dmbrg_wrap_frame(
        _frame.descriptor_, static_cast<int>(_frame.native_handle_), _frame.owner_,
        impl.bridge_, impl.allocator_, input);
    if (wrapped.code_ != status_code::ok) {
        return wrapped;
    }
    const auto pushed = gst_app_src_push_buffer(GST_APP_SRC(impl.source_), input);
    if (pushed != GST_FLOW_OK) {
        return {status_code::io_error,
            "Qualcomm preprocessing appsrc rejected the frame"};
    }
    gst_sample_owner sample(gst_app_sink_try_pull_sample(GST_APP_SINK(impl.sink_),
        static_cast<GstClockTime>(impl.config_.output_timeout_ns_)));
    if (!sample) {
        const auto pipeline = vqec_vision_ai_qcom_fcprc_read_bus(impl.pipeline_);
        return pipeline.code_ == status_code::ok ?
            status{status_code::timeout, "Qualcomm preprocessing output timed out"} : pipeline;
    }
    auto* output = gst_sample_get_buffer(sample.get());
    if (output == nullptr || GST_BUFFER_PTS(output) != _frame.descriptor_.pts_ns_) {
        return {status_code::protocol_error,
            "Qualcomm preprocessing output identity is invalid"};
    }
    GstMapInfo mapped{};
    if (!gst_buffer_map(output, &mapped, GST_MAP_READ)) {
        return {status_code::io_error,
            "cannot map Qualcomm preprocessing output"};
    }
    const auto expected_bytes = vqec_vision_ai_core_tnctr_shape_bytes(_target);
    const auto target_element_bytes = vqec_vision_ai_core_tnctr_element_size(_target.dtype_);
    const auto expected_elements = target_element_bytes == 0 ? 0 :
        expected_bytes / target_element_bytes;
    if (expected_bytes == 0 || expected_elements == 0 ||
        mapped.size != expected_elements) {
        gst_buffer_unmap(output, &mapped);
        return {status_code::protocol_error,
            "Qualcomm preprocessing output size differs from the tensor contract"};
    }
    tensor_blob candidate;
    const bool can_reuse = _outputs.size() == 1U &&
        vqec_vision_ai_qcom_fcprc_same_target(_outputs[0].spec_, _target) &&
        _outputs[0].bytes_.size() == expected_bytes;
    try {
        if (can_reuse) {
            candidate = std::move(_outputs[0]);
        } else {
            candidate.spec_ = _target;
            candidate.bytes_.resize(expected_bytes);
        }
    } catch (const std::bad_alloc&) {
        gst_buffer_unmap(output, &mapped);
        return {status_code::resource_exhausted,
            "cannot allocate Qualcomm preprocessing tensor"};
    }
    if (_target.dtype_ == tensor_element_type::uint16) {
        vqec_vision_ai_qcom_fcprc_widen_u8(
            mapped.data, expected_elements, candidate.bytes_.data());
    } else {
        std::memcpy(candidate.bytes_.data(), mapped.data, mapped.size);
    }
    gst_buffer_unmap(output, &mapped);
    if (can_reuse) {
        _outputs[0] = std::move(candidate);
    } else {
        _outputs.clear();
        _outputs.push_back(std::move(candidate));
    }
    return {};
}

}  // namespace vqec::vision::ai
