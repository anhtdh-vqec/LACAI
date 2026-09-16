#include "vqec_vision_face_enrollment_image_source.hpp"

#include <algorithm>
#include <limits>
#include <mutex>
#include <utility>

#include <gst/app/gstappsink.h>
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
constexpr unsigned g_nv12_plane_count = 2;
constexpr unsigned g_single_image_buffer_count = 1;

void vqec_vision_ai_qcom_feimg_initialize_gst() {
    gst_init(nullptr, nullptr);
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
        _request.session_epoch_ == 0 || config_.max_image_bytes_ == 0 ||
        config_.timeout_ms_ == 0 || config_.jpeg_decoder_factory_.empty() ||
        config_.converter_factory_.empty() || config_.scaler_factory_.empty()) {
        return {status_code::invalid_argument, "invalid image source request"};
    }
    const auto pixels = static_cast<std::uint64_t>(_request.width_) * _request.height_;
    if (pixels > std::numeric_limits<std::uint32_t>::max() ||
        pixels + pixels / 2 > std::numeric_limits<std::size_t>::max() ||
        pixels > config_.max_image_bytes_ || pixels + pixels / 2 > config_.max_image_bytes_) {
        return {status_code::resource_exhausted, "decoded image exceeds configured bound"};
    }
    // Allocate before constructing the graph: allocation failure cannot strand Gst owners.
    auto bytes = std::make_shared<std::vector<std::uint8_t>>(pixels + pixels / 2);
    std::call_once(g_gst_initialized, vqec_vision_ai_qcom_feimg_initialize_gst);
    GstElement* pipeline = gst_pipeline_new(nullptr);
    GstElement* source = gst_element_factory_make(g_file_source_factory, nullptr);
    GstElement* decoder = gst_element_factory_make(config_.jpeg_decoder_factory_.c_str(), nullptr);
    GstElement* converter = gst_element_factory_make(config_.converter_factory_.c_str(), nullptr);
    GstElement* scaler = gst_element_factory_make(config_.scaler_factory_.c_str(), nullptr);
    GstElement* caps_filter = gst_element_factory_make(g_caps_filter_factory, nullptr);
    GstElement* sink = gst_element_factory_make(g_app_sink_factory, nullptr);
    if (pipeline == nullptr || source == nullptr || decoder == nullptr || converter == nullptr || scaler == nullptr ||
        caps_filter == nullptr || sink == nullptr) {
        for (auto* element : {source, decoder, converter, scaler, caps_filter, sink}) {
            if (element != nullptr) gst_object_unref(element);
        }
        if (pipeline != nullptr) gst_object_unref(pipeline);
        return {status_code::missing_plugin, "JPEG image decode elements are unavailable"};
    }
    g_object_set(source, g_location_property, _request.image_path_.c_str(), nullptr);
    GstCaps* caps = gst_caps_new_simple(g_raw_video_media, g_format_field, G_TYPE_STRING, g_nv12_format,
                                        g_width_field, G_TYPE_INT, static_cast<int>(_request.width_),
                                        g_height_field, G_TYPE_INT, static_cast<int>(_request.height_), nullptr);
    g_object_set(caps_filter, g_caps_property, caps, nullptr);
    gst_caps_unref(caps);
    g_object_set(sink, g_emit_signals_property, FALSE, g_sync_property, FALSE,
                 g_max_buffers_property, g_single_image_buffer_count, g_drop_property, TRUE, nullptr);
    gst_bin_add_many(GST_BIN(pipeline), source, decoder, scaler, converter, caps_filter, sink, nullptr);
    if (!gst_element_link_many(source, decoder, scaler, converter, caps_filter, sink, nullptr)) {
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
    GstVideoFrame mapped_frame{};
    const bool mapped = buffer != nullptr &&
        gst_video_info_from_caps(&info, gst_sample_get_caps(sample)) &&
        GST_VIDEO_INFO_FORMAT(&info) == GST_VIDEO_FORMAT_NV12 &&
        GST_VIDEO_INFO_WIDTH(&info) == static_cast<int>(_request.width_) &&
        GST_VIDEO_INFO_HEIGHT(&info) == static_cast<int>(_request.height_) &&
        gst_video_frame_map(&mapped_frame, &info, buffer, GST_MAP_READ);
    const auto expected = pixels + pixels / 2;
    if (!mapped || expected > config_.max_image_bytes_ ||
        GST_VIDEO_FRAME_PLANE_STRIDE(&mapped_frame, 0) < static_cast<int>(_request.width_) ||
        GST_VIDEO_FRAME_PLANE_STRIDE(&mapped_frame, 1) < static_cast<int>(_request.width_)) {
        if (mapped) gst_video_frame_unmap(&mapped_frame);
        gst_sample_unref(sample);
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);
        return {status_code::protocol_error, "decoded image is not a complete NV12 frame"};
    }
    for (unsigned plane = 0; plane < g_nv12_plane_count; ++plane) {
        const auto rows = plane == 0 ? _request.height_ : _request.height_ / 2;
        const auto* data = static_cast<const std::uint8_t*>(GST_VIDEO_FRAME_PLANE_DATA(&mapped_frame, plane));
        const auto stride = GST_VIDEO_FRAME_PLANE_STRIDE(&mapped_frame, plane);
        auto* destination = bytes->data() + (plane == 0 ? 0 : pixels);
        for (std::uint32_t row = 0; row < rows; ++row) {
            std::copy_n(data + static_cast<std::ptrdiff_t>(row) * stride,
                        _request.width_, destination + static_cast<std::size_t>(row) * _request.width_);
        }
    }
    gst_video_frame_unmap(&mapped_frame);
    gst_sample_unref(sample);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    face_enrollment_image candidate;
    candidate.descriptor_.buffer_id_ = _request.buffer_id_;
    candidate.descriptor_.session_epoch_ = _request.session_epoch_;
    candidate.descriptor_.width_ = _request.width_;
    candidate.descriptor_.height_ = _request.height_;
    candidate.descriptor_.offsets_ = {0U, _request.width_ * _request.height_};
    candidate.descriptor_.strides_ = {static_cast<std::int32_t>(_request.width_),
                                      static_cast<std::int32_t>(_request.width_)};
    candidate.descriptor_.view_size_bytes_ = expected;
    candidate.descriptor_.allocation_size_bytes_ = expected;
    candidate.nv12_ = std::move(bytes);
    _image = std::move(candidate);
    return {};
}
}  // namespace vqec::vision::ai
