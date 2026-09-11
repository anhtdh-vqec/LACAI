#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <string>

#include <unistd.h>

#include "vqec_vision_artifact_resolver.hpp"

using namespace vqec::vision::ai;

namespace {

bool vqec_vision_ai_ctest_arsct_write(const std::string& _path, const std::string& _content) {
    std::ofstream out(_path, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }
    out.write(_content.data(), static_cast<std::streamsize>(_content.size()));
    return out.good();
}

model_catalog_entry vqec_vision_ai_ctest_arsct_model() {
    model_catalog_entry model;
    model.model_id_ = "person_detector";
    model.target_id_ = "qcs6490";
    model.artifact_ref_ = "person_detector_qnn_v1";
    // SHA-256 of "abc".
    model.artifact_sha256_ =
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
    return model;
}

}  // namespace

int main() {
    char templ[] = "/tmp/vqec_ai_resolver_XXXXXX";
    const char* directory = ::mkdtemp(templ);
    if (directory == nullptr) {
        return 1;
    }
    const std::string root(directory);
    const std::string model_path = root + "/model.bin";
    const std::string link_path = root + "/link.bin";
    const auto model = vqec_vision_ai_ctest_arsct_model();
    artifact_resolver_config config{
        root, "/usr/lib/libQnnHtp.so", "/usr/lib/libQnnSystem.so", 4096};
    resolved_model_paths paths;

    assert(vqec_vision_ai_ctest_arsct_write(model_path, "abc"));
    assert(vqec_vision_ai_mreg_artsr_resolve_model(
               model, "model.bin", config, paths).code_ == status_code::ok);
    assert(paths.model_path_ == model_path);
    assert(paths.backend_path_ == config.backend_library_ &&
           paths.system_path_ == config.system_library_);

    // Mutated content must be rejected before any load.
    assert(vqec_vision_ai_ctest_arsct_write(model_path, "abd"));
    assert(vqec_vision_ai_mreg_artsr_resolve_model(
               model, "model.bin", config, paths).code_ == status_code::protocol_error);
    assert(vqec_vision_ai_ctest_arsct_write(model_path, "abc"));

    // Traversal and non-relative paths are rejected lexically.
    assert(vqec_vision_ai_mreg_artsr_resolve_model(
               model, "../model.bin", config, paths).code_ == status_code::invalid_argument);
    assert(vqec_vision_ai_mreg_artsr_resolve_model(
               model, "/model.bin", config, paths).code_ == status_code::invalid_argument);

    // A symlink inside the root is refused rather than followed.
    if (::symlink(model_path.c_str(), link_path.c_str()) == 0) {
        assert(vqec_vision_ai_mreg_artsr_resolve_model(
                   model, "link.bin", config, paths).code_ == status_code::unauthorized);
        (void)::unlink(link_path.c_str());
    }

    // Out-of-bound size and an untrusted (relative) library path are rejected.
    auto too_small = config;
    too_small.max_artifact_bytes_ = 1;
    assert(vqec_vision_ai_mreg_artsr_resolve_model(
               model, "model.bin", too_small, paths).code_ == status_code::resource_exhausted);
    auto bad_library = config;
    bad_library.backend_library_ = "libQnnHtp.so";
    assert(vqec_vision_ai_mreg_artsr_resolve_model(
               model, "model.bin", bad_library, paths).code_ == status_code::invalid_argument);

    (void)::unlink(model_path.c_str());
    (void)::rmdir(root.c_str());
    return 0;
}
