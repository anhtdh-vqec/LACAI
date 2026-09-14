// Board-side LACAI camera service. It captures NV12 frames from the QMMF camera
// (qtiqmmfsrc), runs one approved model package through the neutral path without
// tracking/feature/ring, draws the decoded boxes back onto the NV12 frame and serves an
// H.264 RTSP stream of the annotated output.
//
//   qtiqmmfsrc -> (NV12) -> reference_image_processor -> owned QNN engine
//              -> yolov8_decoder -> overlay on NV12 -> v4l2h264enc -> qtirtspbin
//
// It is a board integration tool, not a registered test. It proves wiring and end-to-end
// execution; it is not a zero-copy, performance, accuracy or product-acceptance claim.

#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <gst/gst.h>
#include <gst/video/video.h>
#include <nlohmann/json.hpp>

#include "vqec_vision_qnn_engine.hpp"
#include "vqec_vision_reference_processor.hpp"
#include "vqec_vision_yolov8_decoder.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_preprocess_spec.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_tensor_contract.hpp"

using namespace vqec::vision::ai;
using nlohmann::json;

namespace {

namespace camera_service_limits {
inline constexpr std::uint32_t g_default_width = 1280;
inline constexpr std::uint32_t g_default_height = 720;
inline constexpr std::uint32_t g_default_fps = 30;
inline constexpr std::uint32_t g_default_bitrate_bps = 6000000;
inline constexpr std::uint32_t g_default_keyframe_interval = 30;
inline constexpr std::uint32_t g_default_rtsp_port = 8900;
inline constexpr std::uint32_t g_pull_timeout_ns = 2000000000U;
inline constexpr int g_border_thickness = 3;
inline constexpr std::uint8_t g_box_luma = 180;
inline constexpr std::uint8_t g_box_chroma_u = 42;
inline constexpr std::uint8_t g_box_chroma_v = 21;
inline constexpr std::uint32_t g_nv12_planes = 2;
}  // namespace camera_service_limits

volatile std::sig_atomic_t g_stop_requested = 0;

void vqec_vision_ai_tools_camds_on_signal(int) {
    g_stop_requested = 1;
}

struct camera_service_options {
    std::string package_dir;
    std::string model_library;
    std::string backend_library;
    std::string system_library;
    std::uint32_t camera_id{0};
    std::uint32_t width{camera_service_limits::g_default_width};
    std::uint32_t height{camera_service_limits::g_default_height};
    std::uint32_t fps{camera_service_limits::g_default_fps};
    std::uint32_t bitrate{camera_service_limits::g_default_bitrate_bps};
    std::uint32_t rtsp_port{camera_service_limits::g_default_rtsp_port};
    std::string rtsp_mount{"/live"};
    std::uint64_t iterations{0};
    bool serve_rtsp{true};
};

bool vqec_vision_ai_tools_camds_parse(
    int _argc, char** _argv, camera_service_options& _options) {
    for (int index = 1; index < _argc; ++index) {
        const std::string option = _argv[index];
        if (option == "--no-rtsp") {
            _options.serve_rtsp = false;
            continue;
        }
        const bool has_value = index + 1 < _argc;
        if (!has_value) {
            return false;
        }
        const std::string value = _argv[++index];
        if (option == "--package") {
            _options.package_dir = value;
        } else if (option == "--model") {
            _options.model_library = value;
        } else if (option == "--backend") {
            _options.backend_library = value;
        } else if (option == "--system") {
            _options.system_library = value;
        } else if (option == "--camera") {
            _options.camera_id =
                static_cast<std::uint32_t>(std::strtoul(value.c_str(), nullptr, 10));
        } else if (option == "--width") {
            _options.width =
                static_cast<std::uint32_t>(std::strtoul(value.c_str(), nullptr, 10));
        } else if (option == "--height") {
            _options.height =
                static_cast<std::uint32_t>(std::strtoul(value.c_str(), nullptr, 10));
        } else if (option == "--fps") {
            _options.fps =
                static_cast<std::uint32_t>(std::strtoul(value.c_str(), nullptr, 10));
        } else if (option == "--bitrate") {
            _options.bitrate =
                static_cast<std::uint32_t>(std::strtoul(value.c_str(), nullptr, 10));
        } else if (option == "--rtsp-port") {
            _options.rtsp_port =
                static_cast<std::uint32_t>(std::strtoul(value.c_str(), nullptr, 10));
        } else if (option == "--rtsp-mount") {
            _options.rtsp_mount = value;
        } else if (option == "--iterations") {
            _options.iterations = std::strtoull(value.c_str(), nullptr, 10);
        } else {
            return false;
        }
    }
    return !_options.package_dir.empty() && !_options.model_library.empty() &&
        !_options.backend_library.empty() && !_options.system_library.empty() &&
        _options.width != 0 && _options.height != 0 && _options.fps != 0;
}

json vqec_vision_ai_tools_camds_load(const std::string& _path) {
    std::ifstream stream(_path);
    if (!stream.is_open()) {
        throw std::runtime_error("cannot open package file: " + _path);
    }
    return json::parse(stream);
}

tensor_element_type vqec_vision_ai_tools_camds_dtype(const std::string& _name) {
    if (_name == "uint16") return tensor_element_type::uint16;
    if (_name == "int8") return tensor_element_type::int8;
    if (_name == "uint8") return tensor_element_type::uint8;
    if (_name == "int16") return tensor_element_type::int16;
    if (_name == "int32") return tensor_element_type::int32;
    if (_name == "float32") return tensor_element_type::float32;
    return tensor_element_type::unknown;
}

preprocess_spec vqec_vision_ai_tools_camds_preprocess(const json& _root) {
    preprocess_spec spec;
    const auto source = _root.value("source_format", std::string{"nv12"});
    spec.source_format_ = source == "nv12" ? source_pixel_format::nv12 :
        (source == "rgb888" ? source_pixel_format::rgb888 : source_pixel_format::unknown);
    const auto matrix = _root.value("color_matrix", std::string{});
    spec.matrix_ = matrix == "bt601" ? color_matrix::bt601 :
        (matrix == "bt709" ? color_matrix::bt709 :
            (matrix == "bt2020" ? color_matrix::bt2020 : color_matrix::unspecified));
    const auto range = _root.value("color_range", std::string{});
    spec.range_ = range == "limited" ? color_range::limited :
        (range == "full" ? color_range::full : color_range::unspecified);
    const auto resize = _root.value("resize", std::string{"letterbox"});
    spec.resize_ = resize == "stretch" ? resize_mode::stretch :
        (resize == "crop" ? resize_mode::crop : resize_mode::letterbox);
    const auto interpolation = _root.value("interpolation", std::string{"bilinear"});
    spec.interpolation_ = interpolation == "nearest" ? interpolation_mode::nearest :
        (interpolation == "bilinear" ? interpolation_mode::bilinear : interpolation_mode::area);
    spec.placement_ = image_placement::centre;
    spec.pad_value_ = {0.0F, 0.0F, 0.0F};
    if (_root.contains("pad_value")) {
        const auto pad = _root["pad_value"].get<std::vector<float>>();
        for (std::size_t index = 0; index < 3U && index < pad.size(); ++index) {
            spec.pad_value_[index] = pad[index];
        }
    }
    spec.channels_ = _root.value("channel_order", std::string{"rgb"}) == "bgr" ?
        channel_order::bgr : channel_order::rgb;
    if (_root.contains("normalization")) {
        const auto& normalization = _root["normalization"];
        const auto formula = normalization.value("formula", std::string{"offset_scale"});
        spec.normalization_ = formula == "mean_std" ? normalization_formula::mean_std :
            (formula == "none" ? normalization_formula::none :
                normalization_formula::offset_scale);
        spec.offset_ = {0.0F, 0.0F, 0.0F};
        spec.scale_ = {1.0F, 1.0F, 1.0F};
        if (normalization.contains("offset")) {
            const auto offset = normalization["offset"].get<std::vector<float>>();
            for (std::size_t index = 0; index < 3U && index < offset.size(); ++index) {
                spec.offset_[index] = offset[index];
            }
        }
        if (normalization.contains("scale")) {
            const auto scale = normalization["scale"].get<std::vector<float>>();
            for (std::size_t index = 0; index < 3U && index < scale.size(); ++index) {
                spec.scale_[index] = scale[index];
            }
        }
    }
    spec.coordinates_ = coordinate_convention::tensor_pixels_xywh;
    return spec;
}

void vqec_vision_ai_tools_camds_fill_chroma(std::uint8_t* _uv, std::int32_t _stride_uv,
    int _px, int _py, std::uint8_t _u, std::uint8_t _v) {
    std::uint8_t* cell = _uv + static_cast<std::size_t>(_py / 2) * _stride_uv +
        static_cast<std::size_t>(_px / 2) * camera_service_limits::g_nv12_planes;
    cell[0] = _u;
    cell[1] = _v;
}

// Draws one axis-aligned box border directly on the NV12 planes. Luma carries the edge;
// chroma is set to a saturated colour so the box survives conversion to RGB on the client.
void vqec_vision_ai_tools_camds_draw_box(std::uint8_t* _y, std::int32_t _stride_y,
    std::uint8_t* _uv, std::int32_t _stride_uv, std::int32_t _width, std::int32_t _height,
    float _x, float _y0, float _w, float _h) {
    int x0 = static_cast<int>(_x);
    int y0 = static_cast<int>(_y0);
    int x1 = static_cast<int>(_x + _w);
    int y1 = static_cast<int>(_y0 + _h);
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > _width - 1) x1 = _width - 1;
    if (y1 > _height - 1) y1 = _height - 1;
    if (x1 < x0 || y1 < y0) {
        return;
    }
    const int thickness = camera_service_limits::g_border_thickness;
    for (int x = x0; x <= x1; ++x) {
        for (int t = 0; t < thickness; ++t) {
            const int top = y0 + t;
            const int bottom = y1 - t;
            if (top <= y1) {
                _y[static_cast<std::size_t>(top) * _stride_y + x] = camera_service_limits::g_box_luma;
                vqec_vision_ai_tools_camds_fill_chroma(_uv, _stride_uv, x, top,
                    camera_service_limits::g_box_chroma_u, camera_service_limits::g_box_chroma_v);
            }
            if (bottom >= y0) {
                _y[static_cast<std::size_t>(bottom) * _stride_y + x] =
                    camera_service_limits::g_box_luma;
                vqec_vision_ai_tools_camds_fill_chroma(_uv, _stride_uv, x, bottom,
                    camera_service_limits::g_box_chroma_u, camera_service_limits::g_box_chroma_v);
            }
        }
    }
    for (int y = y0; y <= y1; ++y) {
        for (int t = 0; t < thickness; ++t) {
            const int left = x0 + t;
            const int right = x1 - t;
            if (left <= x1) {
                _y[static_cast<std::size_t>(y) * _stride_y + left] = camera_service_limits::g_box_luma;
                vqec_vision_ai_tools_camds_fill_chroma(_uv, _stride_uv, left, y,
                    camera_service_limits::g_box_chroma_u, camera_service_limits::g_box_chroma_v);
            }
            if (right >= x0) {
                _y[static_cast<std::size_t>(y) * _stride_y + right] = camera_service_limits::g_box_luma;
                vqec_vision_ai_tools_camds_fill_chroma(_uv, _stride_uv, right, y,
                    camera_service_limits::g_box_chroma_u, camera_service_limits::g_box_chroma_v);
            }
        }
    }
}

std::string vqec_vision_ai_tools_camds_capture_description(
    const camera_service_options& _options) {
    return "qtiqmmfsrc name=camsrc camera=" + std::to_string(_options.camera_id) +
        " ! video/x-raw,format=NV12,width=" + std::to_string(_options.width) +
        ",height=" + std::to_string(_options.height) +
        ",framerate=" + std::to_string(_options.fps) + "/1 ! videoconvert"
        " ! video/x-raw,format=NV12"
        " ! appsink name=cap max-buffers=2 drop=true sync=false";
}

std::string vqec_vision_ai_tools_camds_output_description(
    const camera_service_options& _options) {
    std::string description = "appsrc name=out is-live=true format=time"
        " ! queue max-size-buffers=2 leaky=downstream"
        " ! videoconvert ! video/x-raw,format=NV12"
        " ! v4l2h264enc extra-controls=\"controls,video_bitrate=" +
        std::to_string(_options.bitrate) + ",h264_i_frame_period=" +
        std::to_string(camera_service_limits::g_default_keyframe_interval) + "\""
        " ! h264parse config-interval=1";
    if (_options.serve_rtsp) {
        description += " ! qtirtspbin address=0.0.0.0 port=" +
            std::to_string(_options.rtsp_port) + " mpoint=" + _options.rtsp_mount;
    } else {
        description += " ! fakesink sync=false";
    }
    return description;
}

}  // namespace

int main(int _argc, char** _argv) {
    camera_service_options options;
    if (!vqec_vision_ai_tools_camds_parse(_argc, _argv, options)) {
        std::fprintf(stderr,
            "usage: vqec_vision_camera_service --package DIR --model MODEL.so --backend "
            "libQnnHtp.so --system libQnnSystem.so [--camera N] [--width W] [--height H] "
            "[--fps F] [--bitrate BPS] [--rtsp-port P] [--rtsp-mount /live] "
            "[--iterations N] [--no-rtsp]\n");
        return 2;
    }
    std::signal(SIGINT, vqec_vision_ai_tools_camds_on_signal);
    std::signal(SIGTERM, vqec_vision_ai_tools_camds_on_signal);

    GstElement* capture = nullptr;
    GstElement* output = nullptr;
    GstElement* sink = nullptr;
    GstElement* source = nullptr;
    GstBus* output_bus = nullptr;
    qnn_engine engine;
    int mem_fd = -1;
    int result = 1;
    try {
        const json io_manifest =
            vqec_vision_ai_tools_camds_load(options.package_dir + "/io_manifest.json");
        const json preprocess_json =
            vqec_vision_ai_tools_camds_load(options.package_dir + "/preprocess.json");
        const json decoder_json =
            vqec_vision_ai_tools_camds_load(options.package_dir + "/decoder.json");
        const preprocess_spec preprocess = vqec_vision_ai_tools_camds_preprocess(preprocess_json);
        if (vqec_vision_ai_core_ppspc_validate(preprocess).code_ != status_code::ok) {
            std::fprintf(stderr, "package preprocess spec is invalid\n");
            return 1;
        }

        const auto opened = engine.vqec_vision_ai_qcom_qneng_open(
            options.backend_library, options.system_library, inference_execution_policy{});
        if (opened.code_ != status_code::ok) {
            std::fprintf(stderr, "engine open failed: %s\n", opened.message_.c_str());
            return 1;
        }
        const auto prepared = engine.vqec_vision_ai_qcom_qneng_prepare(options.model_library);
        if (prepared.code_ != status_code::ok) {
            std::fprintf(stderr, "engine prepare failed: %s\n", prepared.message_.c_str());
            return 1;
        }
        std::vector<tensor_spec> actual_inputs;
        std::vector<tensor_spec> actual_outputs;
        if (engine.vqec_vision_ai_qcom_qneng_get_tensors(actual_inputs, actual_outputs).code_ !=
                status_code::ok ||
            actual_inputs.size() != 1) {
            std::fprintf(stderr, "engine tensor metadata is unavailable\n");
            return 1;
        }
        if (io_manifest.contains("inputs") && io_manifest["inputs"].is_array() &&
            !io_manifest["inputs"].empty()) {
            const auto& declared = io_manifest["inputs"][0];
            if (declared.value("name", std::string{}) != actual_inputs[0].name_ ||
                vqec_vision_ai_tools_camds_dtype(declared.value("dtype", std::string{})) !=
                    actual_inputs[0].dtype_) {
                std::fprintf(stderr, "declared input identity differs from the graph\n");
                return 1;
            }
        }

        inference_plan plan;
        plan.source_width_ = options.width;
        plan.source_height_ = options.height;
        plan.fps_numerator_ = options.fps;
        plan.fps_denominator_ = 1;
        plan.tensor_width_ = actual_inputs[0].dimensions_.size() == 4 ?
            actual_inputs[0].dimensions_[2] : 0;
        plan.tensor_height_ = actual_inputs[0].dimensions_.size() == 4 ?
            actual_inputs[0].dimensions_[1] : 0;
        plan.input_type_ = actual_inputs[0].dtype_;
        plan.channel_order_ = preprocess.channels_;
        plan.placement_ = preprocess.placement_;
        plan.preprocess_ = preprocess;
        plan.model_path_ = options.model_library;
        plan.backend_path_ = options.backend_library;
        plan.system_path_ = options.system_library;
        plan.input_queue_bytes_ = 8U * 1024U * 1024U;
        plan.output_queue_buffers_ = 2;

        yolov8_decoder_config decoder_config;
        decoder_config.source_width_ = options.width;
        decoder_config.source_height_ = options.height;
        decoder_config.tensor_width_ = plan.tensor_width_;
        decoder_config.tensor_height_ = plan.tensor_height_;
        decoder_config.placement_ = preprocess.placement_;
        decoder_config.box_tensor_ = decoder_json.value("box_tensor", std::string{"boxes_out"});
        decoder_config.score_tensor_ = decoder_json.value("score_tensor", std::string{"conf_out"});
        decoder_config.class_count_ = decoder_json.value("class_count", std::size_t{1});
        decoder_config.confidence_threshold_ =
            decoder_json.value("confidence_threshold", 0.25F);
        decoder_config.iou_threshold_ = decoder_json.value("iou_threshold", 0.45F);
        if (decoder_json.contains("labels")) {
            decoder_config.class_names_ = decoder_json["labels"].get<std::vector<std::string>>();
        }
        yolov8_decoder decoder(decoder_config);
        reference_image_processor processor;

        mem_fd = static_cast<int>(::syscall(SYS_memfd_create, "lacai_frame", 0U));
        if (mem_fd < 0) {
            std::fprintf(stderr, "cannot create frame memfd\n");
            return 1;
        }

        gst_init(&_argc, &_argv);
        GError* error = nullptr;
        const std::string capture_description =
            vqec_vision_ai_tools_camds_capture_description(options);
        const std::string output_description =
            vqec_vision_ai_tools_camds_output_description(options);
        capture = gst_parse_launch(capture_description.c_str(), &error);
        if (capture == nullptr || error != nullptr) {
            std::fprintf(stderr, "capture pipeline error: %s\n",
                error != nullptr ? error->message : "unknown");
            if (error != nullptr) g_error_free(error);
            return 1;
        }
        error = nullptr;
        output = gst_parse_launch(output_description.c_str(), &error);
        if (output == nullptr || error != nullptr) {
            std::fprintf(stderr, "output pipeline error: %s\n",
                error != nullptr ? error->message : "unknown");
            if (error != nullptr) g_error_free(error);
            return 1;
        }
        sink = gst_bin_get_by_name(GST_BIN(capture), "cap");
        source = gst_bin_get_by_name(GST_BIN(output), "out");
        if (sink == nullptr || source == nullptr) {
            std::fprintf(stderr, "pipelines are missing the appsink/appsrc\n");
            return 1;
        }
        GstBus* output_bus_handle = gst_element_get_bus(output);
        output_bus = output_bus_handle;
        const std::string appsrc_caps = "video/x-raw,format=NV12,width=" +
            std::to_string(options.width) + ",height=" + std::to_string(options.height) +
            ",framerate=" + std::to_string(options.fps) + "/1";
        GstCaps* caps = gst_caps_from_string(appsrc_caps.c_str());
        gst_app_src_set_caps(GST_APP_SRC(source), caps);
        gst_caps_unref(caps);
        gst_element_set_state(output, GST_STATE_PLAYING);
        gst_element_set_state(capture, GST_STATE_PLAYING);
        GstState output_state = GST_STATE_VOID_PENDING;
        gst_element_get_state(output, &output_state, nullptr, 3U * GST_SECOND);
        std::fprintf(stderr, "output pipeline state=%d (4=PLAYING)\n",
            static_cast<int>(output_state));
        const std::string rtsp_url = options.serve_rtsp ?
            "rtsp://0.0.0.0:" + std::to_string(options.rtsp_port) + options.rtsp_mount :
            std::string{"disabled"};
        std::printf("camera service: %ux%u@%u camera=%u rtsp=%s\n", options.width,
            options.height, options.fps, options.camera_id, rtsp_url.c_str());

        const std::uint64_t frame_duration_ns =
            1000000000ULL / static_cast<std::uint64_t>(options.fps);
        const std::size_t frame_bytes =
            static_cast<std::size_t>(options.width) * options.height * 3U / 2U;
        std::uint64_t frames = 0;
        std::uint64_t detections = 0;
        std::uint64_t push_errors = 0;
        while (!g_stop_requested &&
               (options.iterations == 0 || frames < options.iterations)) {
            // qtirtspbin hands its RTSP server source to the default GLib main context;
            // unlike gst-launch we have no main loop, so pump it each iteration.
            for (int pump = 0; pump < 8; ++pump) {
                (void)g_main_context_iteration(nullptr, FALSE);
            }
            if (output_bus != nullptr) {
                GstMessage* message = gst_bus_pop_filtered(output_bus,
                    static_cast<GstMessageType>(GST_MESSAGE_ERROR | GST_MESSAGE_WARNING));
                if (message != nullptr) {
                    if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_ERROR) {
                        GError* error = nullptr;
                        gchar* debug = nullptr;
                        gst_message_parse_error(message, &error, &debug);
                        std::fprintf(stderr, "output pipeline error: %s (%s)\n",
                            error != nullptr ? error->message : "unknown",
                            debug != nullptr ? debug : "");
                        if (error != nullptr) g_error_free(error);
                        if (debug != nullptr) g_free(debug);
                    }
                    gst_message_unref(message);
                }
            }
            GstSample* sample = gst_app_sink_try_pull_sample(
                GST_APP_SINK(sink), camera_service_limits::g_pull_timeout_ns);
            if (sample == nullptr) {
                if (gst_app_sink_is_eos(GST_APP_SINK(sink))) {
                    std::fprintf(stderr, "capture pipeline reached EOS\n");
                    break;
                }
                continue;
            }
            GstBuffer* buffer = gst_sample_get_buffer(sample);
            GstMapInfo map {};
            if (buffer == nullptr || !gst_buffer_map(buffer, &map, GST_MAP_READ)) {
                gst_sample_unref(sample);
                continue;
            }
            std::int32_t stride_y = static_cast<std::int32_t>(options.width);
            std::int32_t stride_uv = static_cast<std::int32_t>(options.width);
            std::int32_t offset_uv = static_cast<std::int32_t>(options.width * options.height);
            const GstVideoMeta* video_meta = gst_buffer_get_video_meta(buffer);
            if (video_meta != nullptr && video_meta->n_planes >= 2 &&
                video_meta->format == GST_VIDEO_FORMAT_NV12) {
                stride_y = video_meta->stride[0];
                stride_uv = video_meta->stride[1];
                offset_uv = static_cast<std::int32_t>(video_meta->offset[1]);
            }
            if (map.size < frame_bytes || ::ftruncate(mem_fd, static_cast<off_t>(map.size)) != 0 ||
                ::pwrite(mem_fd, map.data, map.size, 0) != static_cast<ssize_t>(map.size)) {
                gst_buffer_unmap(buffer, &map);
                gst_sample_unref(sample);
                continue;
            }

            raw_frame frame;
            frame.descriptor_.width_ = options.width;
            frame.descriptor_.height_ = options.height;
            frame.descriptor_.strides_ = {stride_y, stride_uv};
            frame.descriptor_.offsets_ = {0, static_cast<std::uint32_t>(offset_uv)};
            frame.descriptor_.view_size_bytes_ = frame_bytes;
            frame.descriptor_.allocation_size_bytes_ = map.size;
            frame.descriptor_.buffer_id_ = frames + 1;
            frame.descriptor_.session_epoch_ = 1;
            frame.descriptor_.pts_ns_ = frames * frame_duration_ns;
            frame.native_handle_ = mem_fd;
            frame.owner_ = std::make_shared<int>(0);

            std::vector<tensor_blob> input_blobs;
            observation_batch observations;
            const auto preprocessed = processor.vqec_vision_ai_ports_imgpr_preprocess(
                frame, plan, actual_inputs[0], input_blobs);
            if (preprocessed.code_ == status_code::ok) {
                std::vector<tensor_blob> output_blobs;
                if (engine.vqec_vision_ai_qcom_qneng_execute(input_blobs, output_blobs).code_ ==
                    status_code::ok) {
                    tensor_result decoded_result;
                    decoded_result.tensors_ = output_blobs;
                    decoded_result.pipeline_pts_ns_ = frame.descriptor_.pts_ns_;
                    (void)decoder.vqec_vision_ai_cntr_mddec_decode(decoded_result,
                        preview_frame_key{1, 0, 1, frames + 1, frame.descriptor_.pts_ns_},
                        observations);
                }
            }

            GstBuffer* annotated = gst_buffer_new_allocate(nullptr, frame_bytes, nullptr);
            GstMapInfo out_map {};
            if (annotated != nullptr && gst_buffer_map(annotated, &out_map, GST_MAP_WRITE)) {
                std::memcpy(out_map.data, map.data, frame_bytes);
                std::uint8_t* y_plane = static_cast<std::uint8_t*>(out_map.data);
                std::uint8_t* uv_plane = y_plane + offset_uv;
                for (const auto& observation : observations.observations_) {
                    vqec_vision_ai_tools_camds_draw_box(y_plane, stride_y, uv_plane, stride_uv,
                        static_cast<std::int32_t>(options.width),
                        static_cast<std::int32_t>(options.height), observation.box_.x_,
                        observation.box_.y_, observation.box_.width_, observation.box_.height_);
                }
                gst_buffer_unmap(annotated, &out_map);
                GST_BUFFER_PTS(annotated) = frames * frame_duration_ns;
                GST_BUFFER_DURATION(annotated) = frame_duration_ns;
                const GstFlowReturn flow = gst_app_src_push_buffer(GST_APP_SRC(source), annotated);
                if (flow != GST_FLOW_OK) {
                    ++push_errors;
                }
                detections += observations.observations_.size();
            } else if (annotated != nullptr) {
                gst_buffer_unref(annotated);
            }

            gst_buffer_unmap(buffer, &map);
            gst_sample_unref(sample);
            ++frames;
            if (frames % 30U == 0U) {
                std::printf("frames=%llu detections=%llu push_errors=%llu\n",
                    static_cast<unsigned long long>(frames),
                    static_cast<unsigned long long>(detections),
                    static_cast<unsigned long long>(push_errors));
                std::fflush(stdout);
            }
        }

        std::printf("camera service stopping: frames=%llu detections=%llu\n",
            static_cast<unsigned long long>(frames),
            static_cast<unsigned long long>(detections));
        gst_app_src_end_of_stream(GST_APP_SRC(source));
        result = 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "camera service failed: %s\n", error.what());
        result = 1;
    }

    if (capture != nullptr) {
        gst_element_set_state(capture, GST_STATE_NULL);
    }
    if (output != nullptr) {
        gst_element_set_state(output, GST_STATE_NULL);
    }
    if (sink != nullptr) {
        gst_object_unref(sink);
    }
    if (source != nullptr) {
        gst_object_unref(source);
    }
    if (output_bus != nullptr) {
        gst_object_unref(output_bus);
    }
    if (capture != nullptr) {
        gst_object_unref(capture);
    }
    if (output != nullptr) {
        gst_object_unref(output);
    }
    if (mem_fd >= 0) {
        ::close(mem_fd);
    }
    engine.vqec_vision_ai_qcom_qneng_close();
    return result;
}
