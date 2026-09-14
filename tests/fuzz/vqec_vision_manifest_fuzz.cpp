// libFuzzer harness for the bounded output-manifest JSON loader. Host clang only; built
// when the optional manifest loader target exists. Verifies the parser cannot crash, hang,
// over-allocate or corrupt state on arbitrary bytes.
//
// Build: -DVQEC_VISION_AI_BUILD_FUZZERS=ON -DVQEC_VISION_AI_ENABLE_MODEL_MANIFEST=ON.

#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>

#include "vqec_vision_output_manifest.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* _data, std::size_t _size) {
    if (_data == nullptr || _size == 0) {
        return 0;
    }
    const std::string input(reinterpret_cast<const char*>(_data), _size);
    std::istringstream stream(input);
    vqec::vision::ai::model_outputs manifest;
    (void)vqec::vision::ai::vqec_vision_ai_mreg_otman_load_manifest(stream, manifest);
    return 0;
}
