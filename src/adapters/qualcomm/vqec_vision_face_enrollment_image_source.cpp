#include "vqec_vision_face_enrollment_image_source.hpp"

#include <algorithm>
#include <limits>
#include <mutex>
#include <utility>

#include <gst/app/gstappsink.h>
#include <gst/allocators/gstdmabuf.h>
#include <gst/video/video.h>

namespace vqec::vision::ai {
namespace {
std::once_flag g_gst_initialized;
// GStreamer 1.x ABI keys and the fixed portable NV12 plane contract.
constexpr auto g_file_source_factory = "filesrc";
constexpr auto g_caps_filter_factory = "capsfilter";
constexpr auto g_app_sink_factory = "appsink";
constexpr auto g_location_property = "location";
constexpr auto g_caps_property = "caps";
constexpr auto g_raw_video_media = "video/x-raw";
constexpr auto g_format_field = "format";
constexpr auto g_nv12_format = "NV12";
constexpr auto g_width_field = "width";
constexpr auto g_height_field = "height";
constexpr auto g_emit_signals_property = "emit-signals";
constexpr auto g_sync_property = "sync";
constexpr auto g_max_buffers_property = "max-buffers";
constexpr auto g_drop_property = "drop";
constexpr auto g_engine_property = "engine";
constexpr unsigned g_nv12_plane_count = 2;
constexpr unsigned g_single_image_buffer_count = 1;

struct sample_owner {
    explicit sample_owner(GstSample* _sample) noexcept : sample_(_sample) {}
    ~sample_owner() noexcept {
        if (sample_ != nullptr) gst_sample_unref(sample_);
    }
    GstSample* sample_{nullptr};
};

void vqec_vision_ai_qcom_feimg_initialize_gst() {
    gst_init(nullptr, nullptr);
}

status vqec_vision_ai_qcom_feimg_set_enum(
    GObject* _object, const char* _property, const std::string& _nick) {
    const auto* specification = g_object_class_find_property(
        G_OBJECT_GET_CLASS(_object), _property);
    if (specification == nullptr || !G_IS_PARAM_SPEC_ENUM(specification) ||
        (specification->flags & G_PARAM_WRITABLE) == 0) {
        return {status_code::incompatible_plugin, "image transform enum property is absent"};
    }
    auto* values = static_cast<GEnumClass*>(
        g_type_class_ref(G_PARAM_SPEC_VALUE_TYPE(specification)));
    const auto* selected = values == nullptr ? nullptr :
        g_enum_get_value_by_nick(values, _nick.c_str());
    if (selected == nullptr) {
        if (values != nullptr) g_type_class_unref(values);
        return {status_code::incompatible_plugin, "image transform enum value is absent"};
    }
    const int value = selected->value;
    g_type_class_unref(values);
    g_object_set(_object, _property, value, nullptr);
    return {};
}
}

qcom_face_enrollment_image_source::qcom_face_enrollment_image_source(
    qcom_face_enrollment_image_source_config _config) noexcept : config_(std::move(_config)) {}

status qcom_face_enrollment_image_source::vqec_vision_ai_ports_feimg_load(
    const face_enrollment_image_request& _request, face_enrollment_image& _image) {
    if (_request.image_path_.empty() || _request.image_path_.find('\0') != std::string::npos ||
        _request.width_ == 0 || _request.height_ == 0 || _request.buffer_id_ == 0 ||
        _request.width_ % 2 != 0 || _request.height_ % 2 != 0 ||
        _request.width_ > G_MAXINT || _request.height_ > G_MAXINT ||
        _request.session_epoch_ == 0 ||
        _request.source_pts_ns_ == std::numeric_limits<std::uint64_t>::max() ||
        config_.max_image_bytes_ == 0 ||
        config_.timeout_ms_ == 0 || config_.jpeg_decoder_factory_.empty() ||
        config_.converter_factory_.empty() || config_.scaler_factory_.empty() ||
        (config_.require_dmabuf_ && (config_.output_transform_factory_.empty() ||
            config_.output_transform_engine_.empty()))) {
        return {status_code::invalid_argument, "invalid image source request"};
    }
    const auto pixels = static_cast<std::uint64_t>(_request.width_) * _request.height_;
    if (pixels > std::numeric_limits<std::uint32_t>::max() ||
        pixels + pixels / 2 > std::numeric_limits<std::size_t>::max() ||
        pixels > config_.max_image_bytes_ || pixels + pixels / 2 > config_.max_image_bytes_) {
        return {status_code::resource_exhausted, "decoded image exceeds configured bound"};
    }
    std::shared_ptr<std::vector<std::uint8_t>> bytes;
    if (!config_.require_dmabuf_) {
        // Allocate before constructing the graph: allocation failure cannot strand Gst owners.
        bytes = std::make_shared<std::vector<std::uint8_t>>(pixels + pixels / 2);
    }
    std::call_once(g_gst_initialized, vqec_vision_ai_qcom_feimg_initialize_gst);
    GstElement* pipeline = gst_pipeline_new(nullptr);
    GstElement* source = gst_element_factory_make(g_file_source_factory, nullptr);
    GstElement* decoder = gst_element_factory_make(config_.jpeg_decoder_factory_.c_str(), nullptr);
    GstElement* converter = gst_element_factory_make(config_.converter_factory_.c_str(), nullptr);
    GstElement* scaler = gst_element_factory_make(config_.scaler_factory_.c_str(), nullptr);
    GstElement* output_transform = config_.output_transform_factory_.empty() ? nullptr :
        gst_element_factory_make(config_.output_transform_factory_.c_str(), nullptr);
    GstElement* caps_filter = gst_element_factory_make(g_caps_filter_factory, nullptr);
    GstElement* sink = gst_element_factory_make(g_app_sink_factory, nullptr);
    if (pipeline == nullptr || source == nullptr || decoder == nullptr || converter == nullptr || scaler == nullptr ||
        caps_filter == nullptr || sink == nullptr ||
        (!config_.output_transform_factory_.empty() && output_transform == nullptr)) {
        for (auto* element : {source, decoder, converter, scaler, output_transform, caps_filter, sink}) {
            if (element != nullptr) gst_object_unref(element);
        }
        if (pipeline != nullptr) gst_object_unref(pipeline);
        return {status_code::missing_plugin, "JPEG image decode elements are unavailable"};
    }
    g_object_set(source, g_location_property, _request.image_path_.c_str(), nullptr);
    if (output_transform != nullptr && !config_.output_transform_engine_.empty()) {
        const auto selected = vqec_vision_ai_qcom_feimg_set_enum(
            G_OBJECT(output_transform), g_engine_property, config_.output_transform_engine_);
        if (selected.code_ != status_code::ok) {
            for (auto* element : {source, decoder, converter, scaler, output_transform,
                                  caps_filter, sink}) gst_object_unref(element);
            gst_object_unref(pipeline);
            return selected;
        }
    }
    GstCaps* caps = gst_caps_new_simple(g_raw_video_media, g_format_field, G_TYPE_STRING, g_nv12_format,
                                        g_width_field, G_TYPE_INT, static_cast<int>(_request.width_),
                                        g_height_field, G_TYPE_INT, static_cast<int>(_request.height_), nullptr);
    g_object_set(caps_filter, g_caps_property, caps, nullptr);
    gst_caps_unref(caps);
    g_object_set(sink, g_emit_signals_property, FALSE, g_sync_property, FALSE,
                 g_max_buffers_property, g_single_image_buffer_count, g_drop_property, TRUE, nullptr);
    if (output_transform == nullptr) {
        gst_bin_add_many(GST_BIN(pipeline), source, decoder, scaler, converter,
                         caps_filter, sink, nullptr);
    } else {
        gst_bin_add_many(GST_BIN(pipeline), source, decoder, scaler, converter,
                         output_transform, caps_filter, sink, nullptr);
    }
    const bool linked = output_transform == nullptr ?
        gst_element_link_many(source, decoder, scaler, converter, caps_filter, sink, nullptr) :
        gst_element_link_many(source, decoder, scaler, converter, output_transform,
                              caps_filter, sink, nullptr);
    if (!linked) {
        gst_object_unref(pipeline);
        return {status_code::graph_link_failed, "JPEG image decode graph cannot be linked"};
    }
    if (gst_element_set_state(pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);
        return {status_code::io_error, "JPEG image decode graph cannot start"};
    }
    GstSample* sample = gst_app_sink_try_pull_sample(
        GST_APP_SINK(sink), static_cast<GstClockTime>(config_.timeout_ms_) * GST_MSECOND);
    if (sample == nullptr) {
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);
        return {status_code::timeout, "JPEG image decode timed out"};
    }
    GstBuffer* buffer = gst_sample_get_buffer(sample);
    GstVideoInfo info{};
    const bool valid_video = buffer != nullptr &&
        gst_video_info_from_caps(&info, gst_sample_get_caps(sample)) &&
        GST_VIDEO_INFO_FORMAT(&info) == GST_VIDEO_FORMAT_NV12 &&
        GST_VIDEO_INFO_WIDTH(&info) == static_cast<int>(_request.width_) &&
        GST_VIDEO_INFO_HEIGHT(&info) == static_cast<int>(_request.height_);
    const auto expected = pixels + pixels / 2;
    if (!valid_video || expected > config_.max_image_bytes_ ||
        GST_VIDEO_INFO_PLANE_STRIDE(&info, 0) < static_cast<int>(_request.width_) ||
        GST_VIDEO_INFO_PLANE_STRIDE(&info, 1) < static_cast<int>(_request.width_)) {
        gst_sample_unref(sample);
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);
        return {status_code::protocol_error, "decoded image is not a complete NV12 frame"};
    }
    face_enrollment_image candidate;
    auto& descriptor = candidate.frame_.descriptor_;
    descriptor.buffer_id_ = _request.buffer_id_;
    descriptor.session_epoch_ = _request.session_epoch_;
    descriptor.width_ = _request.width_;
    descriptor.height_ = _request.height_;
    descriptor.pts_ns_ = _request.source_pts_ns_;
    if (config_.require_dmabuf_) {
        if (gst_buffer_n_memory(buffer) != 1) {
            gst_sample_unref(sample);
            gst_element_set_state(pipeline, GST_STATE_NULL);
            gst_object_unref(pipeline);
            return {status_code::protocol_error, "decoded GBM image has multiple memories"};
        }
        GstMemory* memory = gst_buffer_peek_memory(buffer, 0);
        if (memory == nullptr || !gst_is_dmabuf_memory(memory)) {
            gst_sample_unref(sample);
            gst_element_set_state(pipeline, GST_STATE_NULL);
            gst_object_unref(pipeline);
            return {status_code::incompatible_plugin, "decoded image is not DMA-BUF backed"};
        }
        gsize memory_offset = 0;
        gsize allocation_size = 0;
        const gsize view_size = gst_memory_get_sizes(memory, &memory_offset, &allocation_size);
        const auto y_offset = GST_VIDEO_INFO_PLANE_OFFSET(&info, 0);
        const auto uv_offset = GST_VIDEO_INFO_PLANE_OFFSET(&info, 1);
        const auto y_end = y_offset + static_cast<gsize>(GST_VIDEO_INFO_PLANE_STRIDE(&info, 0)) *
            _request.height_;
        const auto uv_end = uv_offset + static_cast<gsize>(GST_VIDEO_INFO_PLANE_STRIDE(&info, 1)) *
            (_request.height_ / 2);
        const int native_fd = gst_dmabuf_memory_get_fd(memory);
        if (native_fd < 0 || allocation_size == 0 ||
            allocation_size > config_.max_image_bytes_ || memory_offset > allocation_size ||
            view_size > allocation_size - memory_offset || y_end > view_size || uv_end > view_size ||
            y_offset > std::numeric_limits<std::uint32_t>::max() ||
            uv_offset > std::numeric_limits<std::uint32_t>::max()) {
            gst_sample_unref(sample);
            gst_element_set_state(pipeline, GST_STATE_NULL);
            gst_object_unref(pipeline);
            return {status_code::protocol_error, "decoded DMA-BUF layout exceeds its bounds"};
        }
        descriptor.offsets_ = {
            static_cast<std::uint32_t>(y_offset), static_cast<std::uint32_t>(uv_offset)};
        descriptor.strides_ = {GST_VIDEO_INFO_PLANE_STRIDE(&info, 0),
                               GST_VIDEO_INFO_PLANE_STRIDE(&info, 1)};
        descriptor.view_size_bytes_ = view_size;
        descriptor.memory_offset_bytes_ = memory_offset;
        descriptor.allocation_size_bytes_ = allocation_size;
        candidate.frame_.native_handle_ = native_fd;
        candidate.frame_.owner_ = std::make_shared<sample_owner>(sample);
    } else {
        GstVideoFrame mapped_frame{};
        if (!gst_video_frame_map(&mapped_frame, &info, buffer, GST_MAP_READ)) {
            gst_sample_unref(sample);
            gst_element_set_state(pipeline, GST_STATE_NULL);
            gst_object_unref(pipeline);
            return {status_code::protocol_error, "decoded NV12 image cannot be mapped"};
        }
        for (unsigned plane = 0; plane < g_nv12_plane_count; ++plane) {
            const auto rows = plane == 0 ? _request.height_ : _request.height_ / 2;
            const auto* data = static_cast<const std::uint8_t*>(
                GST_VIDEO_FRAME_PLANE_DATA(&mapped_frame, plane));
            const auto stride = GST_VIDEO_FRAME_PLANE_STRIDE(&mapped_frame, plane);
            auto* destination = bytes->data() + (plane == 0 ? 0 : pixels);
            for (std::uint32_t row = 0; row < rows; ++row) {
                std::copy_n(data + static_cast<std::ptrdiff_t>(row) * stride,
                            _request.width_, destination +
                                static_cast<std::size_t>(row) * _request.width_);
            }
        }
        gst_video_frame_unmap(&mapped_frame);
        gst_sample_unref(sample);
        descriptor.offsets_ = {0U, static_cast<std::uint32_t>(pixels)};
        descriptor.strides_ = {static_cast<std::int32_t>(_request.width_),
                               static_cast<std::int32_t>(_request.width_)};
        descriptor.view_size_bytes_ = expected;
        descriptor.allocation_size_bytes_ = expected;
        candidate.frame_.owner_ = bytes;
        candidate.nv12_ = std::move(bytes);
    }
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    _image = std::move(candidate);
    return {};
}
}  // namespace vqec::vision::ai
