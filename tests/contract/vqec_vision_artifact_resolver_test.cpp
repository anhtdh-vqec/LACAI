#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <string>

#include <sys/stat.h>
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

    // Resolving via absolute path under root must succeed and resolve to canonical path.
    resolved_model_paths abs_paths;
    assert(vqec_vision_ai_mreg_artsr_resolve_model(
               model, model_path, config, abs_paths).code_ == status_code::ok);
    assert(abs_paths.model_path_ == model_path);

    // Root with trailing slash must resolve cleanly.
    auto config_slash = config;
    config_slash.model_root_ = root + "/";
    resolved_model_paths slash_paths;
    assert(vqec_vision_ai_mreg_artsr_resolve_model(
               model, "model.bin", config_slash, slash_paths).code_ == status_code::ok);
    assert(slash_paths.model_path_ == model_path);

    // Mutated content must be rejected before any load.
    assert(vqec_vision_ai_ctest_arsct_write(model_path, "abd"));
    assert(vqec_vision_ai_mreg_artsr_resolve_model(
               model, "model.bin", config, paths).code_ == status_code::protocol_error);
    assert(vqec_vision_ai_ctest_arsct_write(model_path, "abc"));

    // Traversal and non-relative paths outside root are rejected lexically.
    assert(vqec_vision_ai_mreg_artsr_resolve_model(
               model, "../model.bin", config, paths).code_ == status_code::invalid_argument);
    assert(vqec_vision_ai_mreg_artsr_resolve_model(
               model, "/model.bin", config, paths).code_ == status_code::invalid_argument);
    assert(vqec_vision_ai_mreg_artsr_resolve_model(
               model, "/etc/passwd", config, paths).code_ == status_code::invalid_argument);
    assert(vqec_vision_ai_mreg_artsr_resolve_model(
               model, "sub/../../outside.bin", config, paths).code_ == status_code::invalid_argument);
    assert(vqec_vision_ai_mreg_artsr_resolve_model(
               model, root + "/../outside.bin", config, paths).code_ == status_code::invalid_argument);
    assert(vqec_vision_ai_mreg_artsr_resolve_model(
               model, "sub//file.bin", config, paths).code_ == status_code::invalid_argument);

    // A symlink inside the root is refused rather than followed.
    if (::symlink(model_path.c_str(), link_path.c_str()) == 0) {
        assert(vqec_vision_ai_mreg_artsr_resolve_model(
                   model, "link.bin", config, paths).code_ == status_code::unauthorized);
        (void)::unlink(link_path.c_str());
    }

    // A symlink pointing outside the root is refused.
    const std::string out_link = root + "/out_link.bin";
    if (::symlink("/etc/passwd", out_link.c_str()) == 0) {
        assert(vqec_vision_ai_mreg_artsr_resolve_model(
                   model, "out_link.bin", config, paths).code_ == status_code::unauthorized);
        (void)::unlink(out_link.c_str());
    }

    // Non-regular file (directory) is rejected.
    const std::string dir_art = root + "/dir_art";
    if (::mkdir(dir_art.c_str(), 0755) == 0) {
        assert(vqec_vision_ai_mreg_artsr_resolve_model(
                   model, "dir_art", config, paths).code_ == status_code::invalid_argument);
        (void)::rmdir(dir_art.c_str());
    }

    // Zero-size file is rejected.
    const std::string zero_file = root + "/zero.bin";
    assert(vqec_vision_ai_ctest_arsct_write(zero_file, ""));
    assert(vqec_vision_ai_mreg_artsr_resolve_model(
               model, "zero.bin", config, paths).code_ == status_code::resource_exhausted);
    (void)::unlink(zero_file.c_str());

    // Out-of-bound size and an untrusted (relative) library path are rejected.
    auto too_small = config;
    too_small.max_artifact_bytes_ = 1;
    assert(vqec_vision_ai_mreg_artsr_resolve_model(
               model, "model.bin", too_small, paths).code_ == status_code::resource_exhausted);
    auto bad_library = config;
    bad_library.backend_library_ = "libQnnHtp.so";
    assert(vqec_vision_ai_mreg_artsr_resolve_model(
               model, "model.bin", bad_library, paths).code_ == status_code::invalid_argument);

    // Corrupted/empty catalog identities are rejected fail-closed.
    auto bad_model = model;
    bad_model.model_id_ = "";
    assert(vqec_vision_ai_mreg_artsr_resolve_model(
               bad_model, "model.bin", config, paths).code_ == status_code::invalid_argument);
    bad_model = model;
    bad_model.target_id_ = "";
    assert(vqec_vision_ai_mreg_artsr_resolve_model(
               bad_model, "model.bin", config, paths).code_ == status_code::invalid_argument);
    bad_model = model;
    bad_model.artifact_ref_ = "";
    assert(vqec_vision_ai_mreg_artsr_resolve_model(
               bad_model, "model.bin", config, paths).code_ == status_code::invalid_argument);
    bad_model = model;
    bad_model.artifact_sha256_ = "ba7816";
    assert(vqec_vision_ai_mreg_artsr_resolve_model(
               bad_model, "model.bin", config, paths).code_ == status_code::invalid_argument);
    bad_model.artifact_sha256_ = "";
    assert(vqec_vision_ai_mreg_artsr_resolve_model(
               bad_model, "model.bin", config, paths).code_ == status_code::invalid_argument);

    (void)::unlink(model_path.c_str());
    (void)::rmdir(root.c_str());
    return 0;
}
