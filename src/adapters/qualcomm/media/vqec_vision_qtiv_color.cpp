#include "vqec_vision_qtiv_color.hpp"

#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <gst/gst.h>
#include <gst/video/video.h>

#include <cstddef>
#include <cstring>
#include <string>
#include <utility>

namespace vqec::vision::ai {
namespace {

constexpr char g_pipeline_template[] =
    "appsrc name=vqecsrc is-live=true do-timestamp=true format=time ! "
    "video/x-raw,format=NV12,width=%u,height=%u,framerate=30/1,colorimetry=%s,"
    "chroma-site=mpeg2 ! qtivtransform engine=%s ! "
    "video/x-raw,format=RGB,width=%u,height=%u ! appsink name=vqecsink max-buffers=1 "
    "drop=false sync=false async=false";
constexpr char g_colorimetry_bt601[] = "bt601";
constexpr char g_colorimetry_bt709[] = "bt709";
constexpr char g_engine_fcv[] = "fcv";
constexpr char g_engine_gles[] = "gles";
constexpr char g_source_name[] = "vqecsrc";
constexpr char g_sink_name[] = "vqecsink";
constexpr char g_video_caps_name[] = "video/x-raw";
constexpr char g_nv12_format[] = "NV12";

void vqec_vision_ai_qcom_qtcol_drain_bus(GstElement* _pipeline, std::string& _error) {
    if (_pipeline == nullptr) {
        return;
    }
    GstBus* bus = gst_element_get_bus(_pipeline);
    if (bus == nullptr) {
        return;
    }
    GstMessage* message = gst_bus_pop_filtered(bus, GST_MESSAGE_ERROR);
    while (message != nullptr) {
        GError* error = nullptr;
        gchar* debug = nullptr;
        gst_message_parse_error(message, &error, &debug);
        if (error != nullptr) {
            _error = error->message;
            g_error_free(error);
        }
        if (debug != nullptr) {
            if (!_error.empty()) {
                _error += " | ";
            }
            _error += debug;
            g_free(debug);
        }
        gst_message_unref(message);
        message = gst_bus_pop_filtered(bus, GST_MESSAGE_ERROR);
    }
    gst_object_unref(bus);
}

}  // namespace

struct qtiv_color_converter::implementation {
    GstElement* pipeline_{nullptr};
    GstElement* source_{nullptr};
    GstElement* sink_{nullptr};
    std::uint32_t width_{0};
    std::uint32_t height_{0};
    std::uint64_t timeout_ns_{0};
    bool is_open_{false};
};

qtiv_color_converter::qtiv_color_converter()
    : implementation_(std::make_unique<implementation>()) {}

qtiv_color_converter::~qtiv_color_converter() noexcept {
    vqec_vision_ai_qcom_qtcol_close();
}

status qtiv_color_converter::vqec_vision_ai_qcom_qtcol_open(
    qtiv_color_engine _engine, std::uint32_t _width, std::uint32_t _height,
    color_matrix _matrix, std::uint64_t _timeout_ns) {
    if (implementation_ == nullptr) {
        return {status_code::invalid_state, "color converter is unavailable"};
    }
    if (implementation_->is_open_) {
        return {status_code::invalid_state, "color converter is already open"};
    }
    if (_timeout_ns == 0 || _width == 0 || _height == 0 || _width % 2 != 0 ||
        _height % 2 != 0 ||
        (_matrix != color_matrix::bt601 && _matrix != color_matrix::bt709)) {
        return {status_code::invalid_argument, "invalid color converter configuration"};
    }
    GError* init_error = nullptr;
    if (!gst_init_check(nullptr, nullptr, &init_error)) {
        const std::string message = init_error != nullptr ? init_error->message :
            "GStreamer initialization failed";
        if (init_error != nullptr) {
            g_error_free(init_error);
        }
        return {status_code::incompatible_plugin, message};
    }
    const char* engine = _engine == qtiv_color_engine::gles ? g_engine_gles : g_engine_fcv;
    const char* colorimetry =
        _matrix == color_matrix::bt601 ? g_colorimetry_bt601 : g_colorimetry_bt709;
    gchar* launch = g_strdup_printf(g_pipeline_template, _width, _height, colorimetry,
        engine, _width, _height);
    GError* parse_error = nullptr;
    GstElement* pipeline = gst_parse_launch(launch, &parse_error);
    g_free(launch);
    if (pipeline == nullptr || parse_error != nullptr) {
        if (parse_error != nullptr) {
            g_error_free(parse_error);
        }
        if (pipeline != nullptr) {
            gst_object_unref(pipeline);
        }
        return {status_code::incompatible_plugin,
            "cannot build the QtIV color conversion pipeline"};
    }
    GstElement* source = gst_bin_get_by_name(GST_BIN(pipeline), g_source_name);
    GstElement* sink = gst_bin_get_by_name(GST_BIN(pipeline), g_sink_name);
    if (source == nullptr || sink == nullptr) {
        if (source != nullptr) {
            gst_object_unref(source);
        }
        if (sink != nullptr) {
            gst_object_unref(sink);
        }
        gst_object_unref(pipeline);
        return {status_code::incompatible_plugin,
            "QtIV color conversion pipeline is missing its appsrc/appsink"};
    }
    GstCaps* caps = gst_caps_new_simple(g_video_caps_name,
        "format", G_TYPE_STRING, g_nv12_format,
        "width", G_TYPE_INT, static_cast<int>(_width),
        "height", G_TYPE_INT, static_cast<int>(_height), nullptr);
    if (caps == nullptr) {
        gst_object_unref(source);
        gst_object_unref(sink);
        gst_object_unref(pipeline);
        return {status_code::resource_exhausted, "cannot allocate the color conversion caps"};
    }
    gst_app_src_set_caps(GST_APP_SRC(source), caps);
    gst_caps_unref(caps);
    if (gst_element_set_state(pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
        gst_object_unref(source);
        gst_object_unref(sink);
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);
        return {status_code::io_error, "cannot start the QtIV color conversion pipeline"};
    }
    implementation_->pipeline_ = pipeline;
    implementation_->source_ = source;
    implementation_->sink_ = sink;
    implementation_->width_ = _width;
    implementation_->height_ = _height;
    implementation_->timeout_ns_ = _timeout_ns;
    implementation_->is_open_ = true;
    return {};
}

status qtiv_color_converter::vqec_vision_ai_qcom_qtcol_convert(
    const std::uint8_t* _nv12, std::vector<std::uint8_t>& _rgb) {
    if (implementation_ == nullptr || !implementation_->is_open_) {
        return {status_code::invalid_state, "color converter is not open"};
    }
    if (_nv12 == nullptr) {
        return {status_code::invalid_argument, "invalid NV12 color conversion input"};
    }
    auto& impl = *implementation_;
    const std::uint32_t width = impl.width_;
    const std::uint32_t height = impl.height_;
    const std::size_t nv12_bytes = static_cast<std::size_t>(width) * height * 3U / 2U;
    GstBuffer* buffer = gst_buffer_new_allocate(nullptr, nv12_bytes, nullptr);
    if (buffer == nullptr) {
        return {status_code::resource_exhausted, "cannot allocate the color conversion buffer"};
    }
    GstMapInfo map{};
    if (!gst_buffer_map(buffer, &map, GST_MAP_WRITE)) {
        gst_buffer_unref(buffer);
        return {status_code::io_error, "cannot map the color conversion buffer"};
    }
    std::memcpy(map.data, _nv12, nv12_bytes);
    gst_buffer_unmap(buffer, &map);
    // Let the live appsrc timestamp the buffer (do-timestamp=true).
    // The pipeline fixes the NV12/RGB caps before PLAYING; push the buffer directly.
    if (gst_app_src_push_buffer(GST_APP_SRC(impl.source_), buffer) != GST_FLOW_OK) {
        return {status_code::io_error, "appsrc rejected the color conversion frame"};
    }
    GstSample* sample =
        gst_app_sink_try_pull_sample(GST_APP_SINK(impl.sink_),
            static_cast<GstClockTime>(impl.timeout_ns_));
    if (sample == nullptr) {
        std::string error;
        vqec_vision_ai_qcom_qtcol_drain_bus(impl.pipeline_, error);
        return {status_code::timeout, error.empty() ? "color conversion output timed out" :
            ("color conversion output timed out: " + error)};
    }
    GstBuffer* output = gst_sample_get_buffer(sample);
    GstMapInfo out_map{};
    const std::size_t row_bytes = static_cast<std::size_t>(width) * 3U;
    const bool mapped =
        output != nullptr && gst_buffer_map(output, &out_map, GST_MAP_READ);
    if (!mapped) {
        gst_sample_unref(sample);
        return {status_code::protocol_error, "cannot map the color conversion output"};
    }
    // Honor the actual row stride (hardware RGB output can be padded).
    std::size_t stride = row_bytes;
    const GstVideoMeta* video_meta =
        output != nullptr ? gst_buffer_get_video_meta(output) : nullptr;
    if (video_meta != nullptr && video_meta->stride[0] > 0) {
        stride = static_cast<std::size_t>(video_meta->stride[0]);
    }
    if (stride < row_bytes || out_map.size < stride * height) {
        gst_buffer_unmap(output, &out_map);
        gst_sample_unref(sample);
        return {status_code::protocol_error, "color conversion output shape is invalid"};
    }
    _rgb.resize(row_bytes * height);
    const auto* data = static_cast<const std::uint8_t*>(out_map.data);
    for (std::uint32_t row = 0; row < height; ++row) {
        std::memcpy(_rgb.data() + static_cast<std::size_t>(row) * row_bytes,
            data + static_cast<std::size_t>(row) * stride, row_bytes);
    }
    gst_buffer_unmap(output, &out_map);
    gst_sample_unref(sample);
    return {};
}

void qtiv_color_converter::vqec_vision_ai_qcom_qtcol_close() noexcept {
    if (implementation_ == nullptr) {
        return;
    }
    auto& impl = *implementation_;
    if (impl.sink_ != nullptr) {
        gst_object_unref(impl.sink_);
        impl.sink_ = nullptr;
    }
    if (impl.source_ != nullptr) {
        gst_object_unref(impl.source_);
        impl.source_ = nullptr;
    }
    if (impl.pipeline_ != nullptr) {
        gst_element_set_state(impl.pipeline_, GST_STATE_NULL);
        gst_object_unref(impl.pipeline_);
        impl.pipeline_ = nullptr;
    }
    impl.is_open_ = false;
}

}  // namespace vqec::vision::ai
