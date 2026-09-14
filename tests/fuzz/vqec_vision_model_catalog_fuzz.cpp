// libFuzzer harness for the bounded model-catalog JSON loader. Host clang only; built when
// the optional model-catalog loader target exists.
//
// Build: -DVQEC_VISION_AI_BUILD_FUZZERS=ON -DVQEC_VISION_AI_ENABLE_MODEL_CATALOG=ON.

#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>

#include "vqec_vision_model_catalog.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* _data, std::size_t _size) {
    if (_data == nullptr || _size == 0) {
        return 0;
    }
    const std::string input(reinterpret_cast<const char*>(_data), _size);
    std::istringstream stream(input);
    vqec::vision::ai::model_catalog catalog;
    std::uint64_t resident_bytes = 0;
    (void)vqec::vision::ai::vqec_vision_ai_mreg_mdcat_load_catalog(
        stream, catalog, resident_bytes);
    return 0;
}
