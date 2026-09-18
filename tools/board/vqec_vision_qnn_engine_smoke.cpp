// Board-side smoke for the LACAI-owned QNN engine. It opens the backend/device, probes
// capabilities, composes one model library, prints the graph tensor identity, executes one
// zero-filled input and reports the output byte sizes. Read-only against the repository.
//
// This is evidence that the owned engine reaches HTP on the target; it is not accuracy,
// performance, async/shared-memory or BSP-recovery qualification. It does not create a
// test registration and is built only when the QNN engine target exists.

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "vqec_vision_qnn_engine.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_tensor_contract.hpp"

using namespace vqec::vision::ai;

namespace {

const char* vqec_vision_ai_tools_qnsmk_dtype_name(tensor_element_type _type) {
    switch (_type) {
        case tensor_element_type::int8: return "int8";
        case tensor_element_type::uint8: return "uint8";
        case tensor_element_type::int16: return "int16";
        case tensor_element_type::uint16: return "uint16";
        case tensor_element_type::int32: return "int32";
        case tensor_element_type::uint32: return "uint32";
        case tensor_element_type::int64: return "int64";
        case tensor_element_type::uint64: return "uint64";
        case tensor_element_type::float16: return "float16";
        case tensor_element_type::float32: return "float32";
        default: return "unknown";
    }
}

void vqec_vision_ai_tools_qnsmk_print_specs(
    const char* _label, const std::vector<tensor_spec>& _specs) {
    for (const auto& spec : _specs) {
        std::printf("%s name=%s dtype=%s dims=[", _label, spec.name_.c_str(),
            vqec_vision_ai_tools_qnsmk_dtype_name(spec.dtype_));
        for (std::size_t index = 0; index < spec.dimensions_.size(); ++index) {
            std::printf("%s%u", index == 0 ? "" : ",", spec.dimensions_[index]);
        }
        std::printf("] quantized=%d scale=%.9g zero_point=%d bytes=%llu\n",
            spec.quantization_.is_quantized_ ? 1 : 0,
            static_cast<double>(spec.quantization_.scale_),
            spec.quantization_.zero_point_,
            static_cast<unsigned long long>(vqec_vision_ai_core_tnctr_shape_bytes(spec)));
    }
}

}  // namespace

int main(int _argc, char** _argv) {
    std::string backend_library;
    std::string system_library;
    std::string model_library;
    std::string input_file;
    std::string output_dir;
    int iterations = 1;
    int reload_cycles = 0;
    for (int index = 1; index < _argc; ++index) {
        const std::string option = _argv[index];
        const bool has_value = index + 1 < _argc;
        if (option == "--backend" && has_value) {
            backend_library = _argv[++index];
        } else if (option == "--system" && has_value) {
            system_library = _argv[++index];
        } else if (option == "--model" && has_value) {
            model_library = _argv[++index];
        } else if (option == "--input-file" && has_value) {
            input_file = _argv[++index];
        } else if (option == "--output-dir" && has_value) {
            output_dir = _argv[++index];
        } else if (option == "--iterations" && has_value) {
            iterations = std::atoi(_argv[++index]);
        } else if (option == "--reload-cycles" && has_value) {
            reload_cycles = std::atoi(_argv[++index]);
        } else {
            std::fprintf(stderr, "unknown or incomplete argument: %s\n", option.c_str());
            return 2;
        }
    }
    if (backend_library.empty() || system_library.empty() || model_library.empty()) {
        std::fprintf(stderr,
            "usage: vqec_vision_qnn_engine_smoke --backend libQnnHtp.so "
            "--system libQnnSystem.so --model model.so\n");
        return 2;
    }

    qnn_engine engine;
    const inference_execution_policy policy;  // synchronous, copy, balanced
    const auto opened =
        engine.vqec_vision_ai_qcom_qneng_open(backend_library, system_library, policy);
    if (opened.code_ != status_code::ok) {
        std::fprintf(stderr, "engine open failed (%d): %s\n",
            static_cast<int>(opened.code_), opened.message_.c_str());
        return 1;
    }
    inference_capabilities capabilities;
    const auto probed = engine.vqec_vision_ai_qcom_qneng_probe_capabilities(capabilities);
    if (probed.code_ != status_code::ok) {
        std::fprintf(stderr, "capability probe failed (%d): %s\n",
            static_cast<int>(probed.code_), probed.message_.c_str());
        return 1;
    }
    std::printf("capabilities graph_count=%u inflight=%u async=%d shared=%d update=%d "
        "native_output=%d dtype_mask=0x%08x\n",
        capabilities.graph_count_, capabilities.max_inflight_jobs_,
        capabilities.supports_async_ ? 1 : 0, capabilities.supports_shared_memory_ ? 1 : 0,
        capabilities.supports_artifact_update_ ? 1 : 0,
        capabilities.supports_native_output_ ? 1 : 0, capabilities.supported_dtype_mask_);

    const auto prepared = engine.vqec_vision_ai_qcom_qneng_prepare(model_library);
    if (prepared.code_ != status_code::ok) {
        std::fprintf(stderr, "engine prepare failed (%d): %s\n",
            static_cast<int>(prepared.code_), prepared.message_.c_str());
        return 1;
    }
    std::vector<tensor_spec> inputs;
    std::vector<tensor_spec> outputs;
    const auto tensors = engine.vqec_vision_ai_qcom_qneng_get_tensors(inputs, outputs);
    if (tensors.code_ != status_code::ok) {
        std::fprintf(stderr, "engine tensor query failed (%d): %s\n",
            static_cast<int>(tensors.code_), tensors.message_.c_str());
        return 1;
    }
    vqec_vision_ai_tools_qnsmk_print_specs("input", inputs);
    vqec_vision_ai_tools_qnsmk_print_specs("output", outputs);

    std::vector<tensor_blob> input_blobs;
    for (const auto& spec : inputs) {
        tensor_blob blob;
        blob.spec_ = spec;
        blob.bytes_.assign(static_cast<std::size_t>(
            vqec_vision_ai_core_tnctr_shape_bytes(spec)), 0U);
        input_blobs.push_back(std::move(blob));
    }
    // A single-input model may take a raw file so the same input can be compared against
    // qnn-net-run output byte for byte.
    if (!input_file.empty()) {
        if (input_blobs.size() != 1) {
            std::fprintf(stderr, "--input-file requires a single-input model\n");
            return 2;
        }
        std::ifstream stream(input_file, std::ios::binary);
        if (!stream.is_open()) {
            std::fprintf(stderr, "cannot open input file: %s\n", input_file.c_str());
            return 1;
        }
        stream.read(reinterpret_cast<char*>(input_blobs[0].bytes_.data()),
            static_cast<std::streamsize>(input_blobs[0].bytes_.size()));
        if (stream.gcount() != static_cast<std::streamsize>(input_blobs[0].bytes_.size())) {
            std::fprintf(stderr, "input file size does not match the model input\n");
            return 1;
        }
    }
    if (iterations < 1) {
        iterations = 1;
    }
    std::vector<tensor_blob> output_blobs;
    std::uint64_t best_us = UINT64_MAX;
    std::uint64_t worst_us = 0;
    std::uint64_t total_us = 0;
    for (int iteration = 0; iteration < iterations; ++iteration) {
        const auto started = std::chrono::steady_clock::now();
        output_blobs.clear();
        const auto executed = engine.vqec_vision_ai_qcom_qneng_execute(input_blobs, output_blobs);
        if (executed.code_ != status_code::ok) {
            std::fprintf(stderr, "engine execute failed (%d): %s\n",
                static_cast<int>(executed.code_), executed.message_.c_str());
            return 1;
        }
        const auto elapsed_us = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - started).count());
        best_us = elapsed_us < best_us ? elapsed_us : best_us;
        worst_us = elapsed_us > worst_us ? elapsed_us : worst_us;
        total_us += elapsed_us;
    }
    std::printf("latency_us iterations=%d min=%llu avg=%llu max=%llu\n", iterations,
        static_cast<unsigned long long>(best_us),
        static_cast<unsigned long long>(total_us / static_cast<std::uint64_t>(iterations)),
        static_cast<unsigned long long>(worst_us));
    std::printf("execute ok outputs=%zu\n", output_blobs.size());
    // Reload evidence: release the prepared model and prepare the same library again on the
    // same open engine. This exercises the release_model path that keeps backend/device open.
    for (int cycle = 0; cycle < reload_cycles; ++cycle) {
        engine.vqec_vision_ai_qcom_qneng_release_model();
        const auto reprepared = engine.vqec_vision_ai_qcom_qneng_prepare(model_library);
        if (reprepared.code_ != status_code::ok) {
            std::fprintf(stderr, "reload prepare failed on cycle %d (%d): %s\n", cycle,
                static_cast<int>(reprepared.code_), reprepared.message_.c_str());
            return 1;
        }
        std::vector<tensor_spec> reload_inputs;
        std::vector<tensor_spec> reload_outputs;
        const auto reload_tensors =
            engine.vqec_vision_ai_qcom_qneng_get_tensors(reload_inputs, reload_outputs);
        if (reload_tensors.code_ != status_code::ok ||
            reload_inputs.size() != inputs.size() || reload_outputs.size() != outputs.size()) {
            std::fprintf(stderr, "reload tensor identity changed on cycle %d\n", cycle);
            return 1;
        }
        std::vector<tensor_blob> reload_results;
        const auto reloaded = engine.vqec_vision_ai_qcom_qneng_execute(
            input_blobs, reload_results);
        if (reloaded.code_ != status_code::ok) {
            std::fprintf(stderr, "reload execute failed on cycle %d (%d): %s\n", cycle,
                static_cast<int>(reloaded.code_), reloaded.message_.c_str());
            return 1;
        }
        std::printf("reload cycle=%d ok outputs=%zu\n", cycle, reload_results.size());
    }
    for (const auto& blob : output_blobs) {
        std::printf("output_bytes name=%s bytes=%zu\n", blob.spec_.name_.c_str(),
            blob.bytes_.size());
        if (!output_dir.empty()) {
            const std::string path = output_dir + "/" + blob.spec_.name_ + ".raw";
            std::ofstream stream(path, std::ios::binary | std::ios::trunc);
            if (!stream.is_open()) {
                std::fprintf(stderr, "cannot write output: %s\n", path.c_str());
                return 1;
            }
            stream.write(reinterpret_cast<const char*>(blob.bytes_.data()),
                static_cast<std::streamsize>(blob.bytes_.size()));
        }
    }
    engine.vqec_vision_ai_qcom_qneng_close();
    return 0;
}
