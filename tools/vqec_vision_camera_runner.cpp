// Board-side integration runner. Unlike vqec_vision_model_runner (file fixture), this one
// consumes live RAW frames through LACAI's own camera adapter:
//
//   mock/instrumented FW RAW socket -> frame_source (legacy wire) -> reference_image_processor
//     -> owned QNN engine -> yolov8_decoder -> detection JSON
//
// It exercises the released FW wire decoder and FD receiver against a real producer and is
// built only when the camera adapter, QNN engine and reference targets exist. It is a board
// tool, not a registered test.

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "vqec_vision_dbus_rpc.hpp"
#include "vqec_vision_frame_source.hpp"
#include "vqec_vision_source_lifecycle.hpp"
#include "vqec_vision_qnn_engine.hpp"
#include "vqec_vision_reference_processor.hpp"
#include "vqec_vision_yolov8_decoder.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_preprocess_spec.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_tensor_contract.hpp"

using namespace vqec::vision::ai;
using nlohmann::json;

namespace {

struct camera_runner_options {
    std::string socket_path;
    std::string package_dir;
    std::string model_library;
    std::string backend_library;
    std::string system_library;
    std::string output_json;
    std::uint32_t producer_uid{0};
    std::uint32_t nv12_format{0};
    std::uint32_t max_width{0};
    std::uint32_t max_height{0};
    std::uint64_t max_allocation{0};
    std::uint64_t iterations{0};
    bool use_control{false};
};

bool vqec_vision_ai_tools_camrun_parse(
    int _argc, char** _argv, camera_runner_options& _options) {
    for (int index = 1; index < _argc; ++index) {
        const std::string option = _argv[index];
        const bool has_value = index + 1 < _argc;
        if (!has_value) {
            return false;
        }
        const std::string value = _argv[++index];
        if (option == "--socket") {
            _options.socket_path = value;
        } else if (option == "--package") {
            _options.package_dir = value;
        } else if (option == "--model") {
            _options.model_library = value;
        } else if (option == "--backend") {
            _options.backend_library = value;
        } else if (option == "--system") {
            _options.system_library = value;
        } else if (option == "--output") {
            _options.output_json = value;
        } else if (option == "--producer-uid") {
            _options.producer_uid =
                static_cast<std::uint32_t>(std::strtoul(value.c_str(), nullptr, 10));
        } else if (option == "--nv12-format") {
            _options.nv12_format =
                static_cast<std::uint32_t>(std::strtoul(value.c_str(), nullptr, 10));
        } else if (option == "--max-width") {
            _options.max_width =
                static_cast<std::uint32_t>(std::strtoul(value.c_str(), nullptr, 10));
        } else if (option == "--max-height") {
            _options.max_height =
                static_cast<std::uint32_t>(std::strtoul(value.c_str(), nullptr, 10));
        } else if (option == "--max-allocation") {
            _options.max_allocation = std::strtoull(value.c_str(), nullptr, 10);
        } else if (option == "--iterations") {
            _options.iterations = std::strtoull(value.c_str(), nullptr, 10);
        } else if (option == "--use-control") {
            _options.use_control = value != "0";
        } else {
            return false;
        }
    }
    return !_options.socket_path.empty() && !_options.package_dir.empty() &&
        !_options.model_library.empty() && !_options.backend_library.empty() &&
        !_options.system_library.empty() && _options.nv12_format != 0 &&
        _options.max_width != 0 && _options.max_height != 0 &&
        _options.max_allocation != 0;
}

json vqec_vision_ai_tools_camrun_load(const std::string& _path) {
    std::ifstream stream(_path);
    if (!stream.is_open()) {
        throw std::runtime_error("cannot open package file: " + _path);
    }
    return json::parse(stream);
}

tensor_element_type vqec_vision_ai_tools_camrun_dtype(const std::string& _name) {
    if (_name == "uint16") return tensor_element_type::uint16;
    if (_name == "int8") return tensor_element_type::int8;
    if (_name == "uint8") return tensor_element_type::uint8;
    if (_name == "int16") return tensor_element_type::int16;
    if (_name == "int32") return tensor_element_type::int32;
    if (_name == "float32") return tensor_element_type::float32;
    return tensor_element_type::unknown;
}

preprocess_spec vqec_vision_ai_tools_camrun_preprocess(const json& _root) {
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

}  // namespace

int main(int _argc, char** _argv) {
    camera_runner_options options;
    if (!vqec_vision_ai_tools_camrun_parse(_argc, _argv, options)) {
        std::fprintf(stderr,
            "usage: vqec_vision_camera_runner --socket PATH --package DIR --model MODEL.so "
            "--backend libQnnHtp.so --system libQnnSystem.so --nv12-format 23 --max-width W "
            "--max-height H --max-allocation B [--producer-uid N] [--iterations N] "
            "[--output detections.json]\n");
        return 2;
    }
    try {
        const json io_manifest =
            vqec_vision_ai_tools_camrun_load(options.package_dir + "/io_manifest.json");
        const json preprocess_json =
            vqec_vision_ai_tools_camrun_load(options.package_dir + "/preprocess.json");
        const json decoder_json =
            vqec_vision_ai_tools_camrun_load(options.package_dir + "/decoder.json");
        const preprocess_spec preprocess = vqec_vision_ai_tools_camrun_preprocess(preprocess_json);
        if (vqec_vision_ai_core_ppspc_validate(preprocess).code_ != status_code::ok) {
            std::fprintf(stderr, "package preprocess spec is invalid\n");
            return 1;
        }

        qnn_engine engine;
        if (engine.vqec_vision_ai_qcom_qneng_open(options.backend_library,
                options.system_library, inference_execution_policy{}).code_ != status_code::ok) {
            std::fprintf(stderr, "engine open failed\n");
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
                vqec_vision_ai_tools_camrun_dtype(declared.value("dtype", std::string{})) !=
                    actual_inputs[0].dtype_) {
                std::fprintf(stderr, "declared input identity differs from the graph\n");
                return 1;
            }
        }

        inference_plan plan;
        plan.source_width_ = options.max_width;
        plan.source_height_ = options.max_height;
        plan.fps_numerator_ = 25;
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
        decoder_config.source_width_ = options.max_width;
        decoder_config.source_height_ = options.max_height;
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

        json detections_json = json::array();
        std::uint64_t frames = 0;
        std::uint64_t total_detections = 0;
        const auto process_frame = [&](raw_frame& _raw) -> bool {
            plan.source_width_ = _raw.descriptor_.width_;
            plan.source_height_ = _raw.descriptor_.height_;
            std::vector<tensor_blob> input_blobs;
            if (processor.vqec_vision_ai_ports_imgpr_preprocess(
                    _raw, plan, actual_inputs[0], input_blobs).code_ != status_code::ok) {
                std::fprintf(stderr, "preprocess failed\n");
                return false;
            }
            std::vector<tensor_blob> output_blobs;
            if (engine.vqec_vision_ai_qcom_qneng_execute(input_blobs, output_blobs).code_ !=
                status_code::ok) {
                std::fprintf(stderr, "QNN execute failed\n");
                return false;
            }
            tensor_result decoded_result;
            decoded_result.tensors_ = output_blobs;
            decoded_result.pipeline_pts_ns_ = _raw.descriptor_.pts_ns_;
            observation_batch observations;
            (void)decoder.vqec_vision_ai_cntr_mddec_decode(decoded_result,
                preview_frame_key{1, 0, 1, frames + 1, _raw.descriptor_.pts_ns_}, observations);
            if (frames == 0) {
                for (const auto& item : observations.observations_) {
                    json detection;
                    detection["class_id"] = item.class_id_;
                    detection["confidence"] = item.confidence_;
                    detection["x"] = item.box_.x_;
                    detection["y"] = item.box_.y_;
                    detection["width"] = item.box_.width_;
                    detection["height"] = item.box_.height_;
                    detections_json.push_back(std::move(detection));
                }
            }
            total_detections += observations.observations_.size();
            ++frames;
            if (frames % 30U == 0U) {
                std::printf("frames=%llu detections=%llu\n",
                    static_cast<unsigned long long>(frames),
                    static_cast<unsigned long long>(total_detections));
                std::fflush(stdout);
            }
            return true;
        };
        if (options.use_control) {
            auto rpc = std::make_shared<dbus_rpc>();
            const auto opened = rpc->vqec_vision_ai_camer_dbrpc_open(false);
            if (opened.code_ != status_code::ok) {
                std::fprintf(stderr, "camera D-Bus open failed: %s\n", opened.message_.c_str());
                return 1;
            }
            camera_lifecycle_config lifecycle_config;
            lifecycle_config.acquire_.camera_id_ = 0;
            lifecycle_config.acquire_.channel_id_ = 0;
            lifecycle_config.acquire_.consumer_id_ = "ai";
            lifecycle_config.acquire_.request_id_ = "camrun-start-1";
            lifecycle_config.media_.socket_path_ = options.socket_path;
            lifecycle_config.media_.producer_uid_ = options.producer_uid;
            lifecycle_config.media_.limits_.nv12_format_value_ = options.nv12_format;
            lifecycle_config.media_.limits_.max_width_ = options.max_width;
            lifecycle_config.media_.limits_.max_height_ = options.max_height;
            lifecycle_config.media_.limits_.max_allocation_bytes_ = options.max_allocation;
            lifecycle_config.stop_request_id_ = "camrun-stop-1";
            lifecycle_config.max_fps_ = 30;
            source_lifecycle lifecycle(rpc, lifecycle_config);
            const auto started = lifecycle.vqec_vision_ai_camer_srclc_start(3000);
            if (started.code_ != status_code::ok) {
                std::fprintf(stderr, "camera lease start failed: %s\n", started.message_.c_str());
                return 1;
            }
            std::printf("camera lease acquired via D-Bus\n");
            while (options.iterations == 0 || frames < options.iterations) {
                raw_frame raw;
                if (lifecycle.vqec_vision_ai_ports_rawsr_receive(raw, 3000).code_ !=
                    status_code::ok) {
                    std::fprintf(stderr, "camera receive failed\n");
                    break;
                }
                if (!process_frame(raw)) {
                    break;
                }
            }
            (void)lifecycle.vqec_vision_ai_camer_srclc_stop(3000);
        } else {
            camera_source_config source_config;
            source_config.socket_path_ = options.socket_path;
            source_config.producer_uid_ = options.producer_uid;
            source_config.limits_.nv12_format_value_ = options.nv12_format;
            source_config.limits_.max_width_ = options.max_width;
            source_config.limits_.max_height_ = options.max_height;
            source_config.limits_.max_allocation_bytes_ = options.max_allocation;
            frame_source source;
            const auto connected = source.vqec_vision_ai_camer_frsrc_connect(source_config);
            if (connected.code_ != status_code::ok) {
                std::fprintf(stderr, "camera connect failed: %s\n", connected.message_.c_str());
                return 1;
            }
            std::printf("camera connected to %s\n", options.socket_path.c_str());
            while (options.iterations == 0 || frames < options.iterations) {
                std::shared_ptr<const received_frame> frame;
                const auto received = source.vqec_vision_ai_camer_frsrc_receive(frame, 3000);
                if (received.code_ != status_code::ok || frame == nullptr) {
                    std::fprintf(stderr, "camera receive failed: %s\n", received.message_.c_str());
                    break;
                }
                raw_frame raw;
                raw.descriptor_ = frame->vqec_vision_ai_camer_frsrc_get_descriptor();
                raw.native_handle_ = frame->vqec_vision_ai_camer_frsrc_get_fd();
                raw.owner_ = frame;
                if (!process_frame(raw)) {
                    break;
                }
            }
            source.vqec_vision_ai_camer_frsrc_disconnect();
        }

        json report;
        report["model_id"] = io_manifest.value("model_id", std::string{});
        report["frames"] = frames;
        report["total_detections"] = total_detections;
        report["first_frame_detections"] = detections_json;
        const std::string text = report.dump(2);
        if (!options.output_json.empty()) {
            std::ofstream out(options.output_json, std::ios::trunc);
            out << text << '\n';
        }
        std::printf("%s\n", text.c_str());
        return frames == 0 ? 1 : 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "camera runner failed: %s\n", error.what());
        return 1;
    }
}
