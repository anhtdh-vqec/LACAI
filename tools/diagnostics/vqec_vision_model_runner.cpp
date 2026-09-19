// Board-side model integration runner. It drives one approved model package through the
// neutral path without camera/FW/ring/tracking/feature/encoder:
//
//   NV12 fixture -> reference_image_processor -> owned QNN engine -> yolov8_decoder -> JSON
//
// It reads the package metadata (io_manifest/preprocess/decoder JSON) and takes the runtime
// paths (model .so, backend/system libraries) plus the fixture and source geometry from the
// command line. It is built only when the QNN engine and reference targets exist; it is not a
// registered test.

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <nlohmann/json.hpp>

#include "vqec_vision_backend_factory.hpp"
#include "vqec_vision_reference_processor.hpp"
#include "vqec_vision_yolov8_decoder.hpp"
#include "vqec/vision/ai/contracts/inference/vqec_vision_preprocess_spec.hpp"
#include "vqec/vision/ai/contracts/inference/vqec_vision_tensor_contract.hpp"

using namespace vqec::vision::ai;
using nlohmann::json;

namespace {

struct runner_options {
    std::string package_dir;
    std::string model_library;
    std::string backend_library;
    std::string system_library;
    std::string input_nv12;
    std::string output_json;
    std::string dump_dir;
    std::uint32_t source_width{0};
    std::uint32_t source_height{0};
};

bool vqec_vision_ai_tools_mdlrun_parse(int _argc, char** _argv, runner_options& _options) {
    for (int index = 1; index < _argc; ++index) {
        const std::string option = _argv[index];
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
        } else if (option == "--input") {
            _options.input_nv12 = value;
        } else if (option == "--output") {
            _options.output_json = value;
        } else if (option == "--dump-dir") {
            _options.dump_dir = value;
        } else if (option == "--source-width") {
            _options.source_width = static_cast<std::uint32_t>(std::strtoul(value.c_str(), nullptr, 10));
        } else if (option == "--source-height") {
            _options.source_height = static_cast<std::uint32_t>(std::strtoul(value.c_str(), nullptr, 10));
        } else {
            return false;
        }
    }
    return !_options.package_dir.empty() && !_options.model_library.empty() &&
        !_options.backend_library.empty() && !_options.system_library.empty() &&
        !_options.input_nv12.empty() && _options.source_width != 0 && _options.source_height != 0;
}

json vqec_vision_ai_tools_mdlrun_load(const std::string& _path) {
    std::ifstream stream(_path);
    if (!stream.is_open()) {
        throw std::runtime_error("cannot open package file: " + _path);
    }
    return json::parse(stream);
}

tensor_element_type vqec_vision_ai_tools_mdlrun_dtype(const std::string& _name) {
    if (_name == "uint16") {
        return tensor_element_type::uint16;
    }
    if (_name == "int8") {
        return tensor_element_type::int8;
    }
    if (_name == "uint8") {
        return tensor_element_type::uint8;
    }
    if (_name == "int16") {
        return tensor_element_type::int16;
    }
    if (_name == "int32") {
        return tensor_element_type::int32;
    }
    if (_name == "float32") {
        return tensor_element_type::float32;
    }
    return tensor_element_type::unknown;
}

preprocess_spec vqec_vision_ai_tools_mdlrun_preprocess(const json& _root) {
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
    spec.coordinates_ = _root.value("coordinates", std::string{"tensor_pixels_xywh"}) ==
            "tensor_pixels_xywh" ? coordinate_convention::tensor_pixels_xywh :
        coordinate_convention::tensor_pixels_xywh;
    return spec;
}

std::string vqec_vision_ai_tools_mdlrun_dtype_name(tensor_element_type _type) {
    switch (_type) {
        case tensor_element_type::uint16: return "uint16";
        case tensor_element_type::int8: return "int8";
        case tensor_element_type::uint8: return "uint8";
        case tensor_element_type::int16: return "int16";
        case tensor_element_type::int32: return "int32";
        case tensor_element_type::float32: return "float32";
        default: return "unknown";
    }
}

}  // namespace

int main(int _argc, char** _argv) {
    runner_options options;
    if (!vqec_vision_ai_tools_mdlrun_parse(_argc, _argv, options)) {
        std::fprintf(stderr,
            "usage: vqec_vision_model_runner --package DIR --model MODEL.so --backend libQnnHtp.so "
            "--system libQnnSystem.so --input frame.nv12 --source-width W --source-height H "
            "[--output detections.json]\n");
        return 2;
    }
    try {
        const json io_manifest =
            vqec_vision_ai_tools_mdlrun_load(options.package_dir + "/io_manifest.json");
        const json preprocess_json =
            vqec_vision_ai_tools_mdlrun_load(options.package_dir + "/preprocess.json");
        const json decoder_json =
            vqec_vision_ai_tools_mdlrun_load(options.package_dir + "/decoder.json");
        const preprocess_spec preprocess = vqec_vision_ai_tools_mdlrun_preprocess(preprocess_json);
        if (vqec_vision_ai_core_ppspc_validate(preprocess).code_ != status_code::ok) {
            std::fprintf(stderr, "package preprocess spec is invalid\n");
            return 1;
        }

        // Exercise the same capability-checked backend selection production uses: the
        // factory opens the engine, probes adapter capabilities and validates the default
        // execution policy before returning the engine/graph bundle.
        resolved_model_paths paths;
        paths.model_id_ = io_manifest.value("model_id", std::string{"model"});
        paths.model_path_ = options.model_library;
        paths.backend_path_ = options.backend_library;
        paths.system_path_ = options.system_library;
        std::unique_ptr<qnn_backend_bundle> backend;
        const auto created =
            vqec_vision_ai_qcom_bfact_create(paths, inference_execution_policy{}, backend);
        if (created.code_ != status_code::ok) {
            std::fprintf(stderr, "backend factory failed: %s\n", created.message_.c_str());
            return 1;
        }
        auto* engine = backend->vqec_vision_ai_qcom_bfact_get_engine();
        const auto prepared = engine->vqec_vision_ai_qcom_qneng_prepare(options.model_library);
        if (prepared.code_ != status_code::ok) {
            std::fprintf(stderr, "engine prepare failed: %s\n", prepared.message_.c_str());
            return 1;
        }
        std::vector<tensor_spec> actual_inputs;
        std::vector<tensor_spec> actual_outputs;
        if (engine->vqec_vision_ai_qcom_qneng_get_tensors(actual_inputs, actual_outputs).code_ !=
                status_code::ok ||
            actual_inputs.size() != 1) {
            std::fprintf(stderr, "engine tensor metadata is unavailable\n");
            return 1;
        }
        std::printf("qnn input name=%s dtype=%s bytes=%llu\n", actual_inputs[0].name_.c_str(),
            vqec_vision_ai_tools_mdlrun_dtype_name(actual_inputs[0].dtype_).c_str(),
            static_cast<unsigned long long>(
                vqec_vision_ai_core_tnctr_shape_bytes(actual_inputs[0])));
        // Activation check: the package-declared input identity must match the graph.
        if (io_manifest.contains("inputs") && io_manifest["inputs"].is_array() &&
            !io_manifest["inputs"].empty()) {
            const auto& declared = io_manifest["inputs"][0];
            if (declared.value("name", std::string{}) != actual_inputs[0].name_ ||
                vqec_vision_ai_tools_mdlrun_dtype(declared.value("dtype", std::string{})) !=
                    actual_inputs[0].dtype_) {
                std::fprintf(stderr, "declared input identity differs from the graph\n");
                return 1;
            }
        }

        inference_plan plan;
        plan.source_width_ = options.source_width;
        plan.source_height_ = options.source_height;
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

        const int fd = ::open(options.input_nv12.c_str(), O_RDONLY | O_CLOEXEC);
        if (fd < 0) {
            std::fprintf(stderr, "cannot open NV12 fixture: %s\n", options.input_nv12.c_str());
            return 1;
        }
        struct stat file_info {};
        if (::fstat(fd, &file_info) != 0) {
            ::close(fd);
            return 1;
        }
        raw_frame frame;
        frame.descriptor_.width_ = options.source_width;
        frame.descriptor_.height_ = options.source_height;
        frame.descriptor_.strides_ = {static_cast<std::int32_t>(options.source_width),
            static_cast<std::int32_t>(options.source_width)};
        frame.descriptor_.offsets_ = {0,
            options.source_width * options.source_height};
        frame.descriptor_.view_size_bytes_ =
            static_cast<std::uint64_t>(options.source_width) * options.source_height * 3U / 2U;
        frame.descriptor_.allocation_size_bytes_ = static_cast<std::uint64_t>(file_info.st_size);
        frame.descriptor_.buffer_id_ = 1;
        frame.descriptor_.session_epoch_ = 1;
        frame.descriptor_.pts_ns_ = 1000000;
        frame.native_handle_ = fd;
        frame.owner_ = std::make_shared<int>(0);

        reference_image_processor processor;
        std::vector<tensor_blob> input_blobs;
        const auto preprocessed = processor.vqec_vision_ai_ports_imgpr_preprocess(
            frame, plan, actual_inputs[0], input_blobs);
        ::close(fd);
        if (preprocessed.code_ != status_code::ok) {
            std::fprintf(stderr, "preprocess failed: %s\n", preprocessed.message_.c_str());
            return 1;
        }
        std::printf("preprocess PASS tensors=%zu bytes=%zu\n", input_blobs.size(),
            input_blobs.empty() ? 0U : input_blobs[0].bytes_.size());
        if (!options.dump_dir.empty() && !input_blobs.empty()) {
            std::ofstream out(options.dump_dir + "/input_tensor.raw", std::ios::binary | std::ios::trunc);
            out.write(reinterpret_cast<const char*>(input_blobs[0].bytes_.data()),
                static_cast<std::streamsize>(input_blobs[0].bytes_.size()));
        }

        std::vector<tensor_blob> output_blobs;
        const auto executed = engine->vqec_vision_ai_qcom_qneng_execute(input_blobs, output_blobs);
        if (executed.code_ != status_code::ok) {
            std::fprintf(stderr, "QNN execute failed: %s\n", executed.message_.c_str());
            return 1;
        }
        std::printf("qnn PASS outputs=%zu\n", output_blobs.size());
        if (!options.dump_dir.empty()) {
            for (const auto& blob : output_blobs) {
                std::ofstream out(options.dump_dir + "/" + blob.spec_.name_ + ".raw",
                    std::ios::binary | std::ios::trunc);
                out.write(reinterpret_cast<const char*>(blob.bytes_.data()),
                    static_cast<std::streamsize>(blob.bytes_.size()));
            }
        }

        yolov8_decoder_config decoder_config;
        decoder_config.source_width_ = options.source_width;
        decoder_config.source_height_ = options.source_height;
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
        tensor_result result;
        result.tensors_ = output_blobs;
        result.pipeline_pts_ns_ = frame.descriptor_.pts_ns_;
        observation_batch observations;
        const auto decoded = decoder.vqec_vision_ai_cntr_mddec_decode(
            result, preview_frame_key{1, 0, 1, 1, frame.descriptor_.pts_ns_}, observations);
        if (decoded.code_ != status_code::ok) {
            std::fprintf(stderr, "decoder failed: %s\n", decoded.message_.c_str());
            return 1;
        }

        json report;
        report["model_id"] = io_manifest.value("model_id", std::string{});
        report["preprocess"]["status"] = "PASS";
        report["qnn"]["status"] = "PASS";
        report["detections"] = json::array();
        for (const auto& observation : observations.observations_) {
            json detection;
            detection["class_id"] = observation.class_id_;
            detection["confidence"] = observation.confidence_;
            detection["x"] = observation.box_.x_;
            detection["y"] = observation.box_.y_;
            detection["width"] = observation.box_.width_;
            detection["height"] = observation.box_.height_;
            report["detections"].push_back(std::move(detection));
        }
        const std::string text = report.dump(2);
        if (!options.output_json.empty()) {
            std::ofstream out(options.output_json, std::ios::trunc);
            out << text << '\n';
        }
        std::printf("%s\n", text.c_str());
        std::printf("decoder PASS detections=%zu\n", observations.observations_.size());
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "model runner failed: %s\n", error.what());
        return 1;
    }
}
