#include "vqec_vision_encrypted_face_gallery_store.hpp"
#include "vqec_vision_exact_embedding_index.hpp"
#include "vqec_vision_recognition_session.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace vqec::vision::ai {
namespace {

constexpr std::size_t g_dimensions = 3;
constexpr std::size_t g_capacity = 8;
constexpr std::size_t g_max_templates = 3;
constexpr std::size_t g_max_bytes = 1024U * 1024U;
constexpr std::uint64_t g_preprocess_revision = 7;
constexpr char g_gallery_id[] = "gallery_a";
constexpr char g_model_id[] = "face_embedding";
constexpr char g_model_version[] = "1";
constexpr char g_subject[] = "subject_private";
constexpr char g_gallery_file[] = "gallery.bin";
constexpr char g_key_file[] = "gallery.key";
constexpr char g_lock_file[] = "gallery.lock";

std::string vqec_vision_ai_unit_efgtst_join(
    const std::string& _directory, const std::string& _name) {
    return _directory + "/" + _name;
}

void vqec_vision_ai_unit_efgtst_remove_tree(const std::string& _directory) noexcept {
    (void)::unlink(vqec_vision_ai_unit_efgtst_join(_directory, g_gallery_file).c_str());
    (void)::unlink(vqec_vision_ai_unit_efgtst_join(_directory, g_key_file).c_str());
    (void)::unlink(vqec_vision_ai_unit_efgtst_join(_directory, g_lock_file).c_str());
    (void)::rmdir(_directory.c_str());
}

std::vector<unsigned char> vqec_vision_ai_unit_efgtst_read_file(const std::string& _path) {
    const int fd = ::open(_path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) throw std::runtime_error("cannot read encrypted test file");
    std::vector<unsigned char> bytes;
    std::array<unsigned char, 512> block{};
    for (;;) {
        const auto count = ::read(fd, block.data(), block.size());
        if (count < 0 && errno == EINTR) continue;
        if (count < 0) { ::close(fd); throw std::runtime_error("encrypted test file read failed"); }
        if (count == 0) break;
        bytes.insert(bytes.end(), block.begin(), block.begin() + count);
    }
    ::close(fd);
    return bytes;
}

void vqec_vision_ai_unit_efgtst_write_file(
    const std::string& _path, const std::vector<unsigned char>& _bytes) {
    const int fd = ::open(_path.c_str(), O_WRONLY | O_TRUNC | O_CLOEXEC);
    if (fd < 0) throw std::runtime_error("cannot tamper encrypted test file");
    std::size_t written = 0;
    while (written < _bytes.size()) {
        const auto count = ::write(fd, _bytes.data() + written, _bytes.size() - written);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) { ::close(fd); throw std::runtime_error("tamper write failed"); }
        written += static_cast<std::size_t>(count);
    }
    ::close(fd);
}

void vqec_vision_ai_unit_efgtst_require_private_file(const std::string& _path) {
    struct stat value{};
    if (::stat(_path.c_str(), &value) != 0 || !S_ISREG(value.st_mode) ||
        (value.st_mode & (S_IRWXG | S_IRWXO)) != 0) {
        throw std::runtime_error("protected gallery file permissions are not private");
    }
}

void vqec_vision_ai_unit_efgtst_check_store() {
    std::array<char, 96> directory_template{};
    const std::string prefix = "/tmp/vqec_vision_gallery_XXXXXX";
    std::copy(prefix.begin(), prefix.end(), directory_template.begin());
    char* created = ::mkdtemp(directory_template.data());
    if (created == nullptr) throw std::runtime_error("cannot create protected test directory");
    const std::string directory(created);
    try {
        encrypted_face_gallery_store_config store_config{directory, g_gallery_file,
            g_key_file, g_lock_file, static_cast<std::uint32_t>(::getuid()), g_max_bytes};
        face_gallery_config gallery_config{g_gallery_id, g_model_id, g_model_version,
            g_preprocess_revision, g_dimensions, g_capacity, g_max_templates};
        encrypted_face_gallery_store store(store_config);
        face_gallery_snapshot empty;
        if (store.vqec_vision_ai_ports_fgstr_load(gallery_config, empty).code_ !=
                status_code::ok || empty.revision_ != 1 || !empty.templates_.empty()) {
            throw std::runtime_error("encrypted gallery initialization failed");
        }
        face_gallery_snapshot replacement = empty;
        replacement.revision_ = 2;
        replacement.next_record_id_ = 2;
        replacement.templates_.push_back({1, g_subject, {1.0F, 0.0F, 0.0F}});
        if (store.vqec_vision_ai_ports_fgstr_replace(
                gallery_config, 1, replacement).code_ != status_code::ok) {
            throw std::runtime_error("encrypted gallery replacement failed");
        }
        if (store.vqec_vision_ai_ports_fgstr_replace(
                gallery_config, 1, replacement).code_ != status_code::invalid_state) {
            throw std::runtime_error("encrypted gallery accepted stale CAS");
        }
        encrypted_face_gallery_store restarted(store_config);
        face_gallery_snapshot recovered;
        if (restarted.vqec_vision_ai_ports_fgstr_load(
                gallery_config, recovered).code_ != status_code::ok ||
            recovered.revision_ != 2 || recovered.templates_.size() != 1 ||
            recovered.templates_[0].subject_ref_ != g_subject) {
            throw std::runtime_error("encrypted gallery restart recovery failed");
        }
        exact_embedding_index rebuilt_index;
        recognition_session recognition;
        recognition_session_config recognition_config;
        recognition_config.index_ = {g_model_id, g_model_version, g_dimensions,
            g_capacity, g_capacity, embedding_metric::cosine_similarity, 1};
        recognition_config.policy_ = {0.8F, 0.05F, g_capacity};
        recognition_config.max_templates_per_subject_ = g_max_templates;
        recognition_config.search_top_k_ = g_capacity;
        recognition_config.search_minimum_similarity_ = -1.0F;
        if (recognition.vqec_vision_ai_embed_rcses_configure_persistent(
                rebuilt_index, restarted, recognition_config, gallery_config).code_ !=
                status_code::ok) {
            throw std::runtime_error("recognition did not rebuild from encrypted gallery");
        }
        embedding_result query;
        query.frame_ = {1, 1, 1, 1, 1};
        query.track_id_ = 1;
        query.model_id_ = g_model_id;
        query.model_version_ = g_model_version;
        query.values_ = {1.0F, 0.0F, 0.0F};
        query.is_l2_normalized_ = true;
        recognition_match_result match;
        if (recognition.vqec_vision_ai_embed_rcses_recognize(
                query, match).code_ != status_code::ok ||
            match.decision_ != recognition_decision::known ||
            match.subject_ref_ != g_subject) {
            throw std::runtime_error("recognition failed after encrypted restart rebuild");
        }
        const auto gallery_path = vqec_vision_ai_unit_efgtst_join(directory, g_gallery_file);
        const auto key_path = vqec_vision_ai_unit_efgtst_join(directory, g_key_file);
        vqec_vision_ai_unit_efgtst_require_private_file(gallery_path);
        vqec_vision_ai_unit_efgtst_require_private_file(key_path);
        if (::chmod(key_path.c_str(), S_IRUSR | S_IWUSR | S_IRGRP) != 0 ||
            restarted.vqec_vision_ai_ports_fgstr_load(
                gallery_config, recovered).code_ != status_code::unauthorized ||
            ::chmod(key_path.c_str(), S_IRUSR | S_IWUSR) != 0) {
            throw std::runtime_error("encrypted gallery accepted unsafe key permissions");
        }
        face_gallery_config incompatible = gallery_config;
        ++incompatible.preprocess_revision_;
        if (restarted.vqec_vision_ai_ports_fgstr_load(
                incompatible, recovered).code_ != status_code::protocol_error) {
            throw std::runtime_error("encrypted gallery accepted incompatible identity");
        }
        auto bytes = vqec_vision_ai_unit_efgtst_read_file(gallery_path);
        const std::string ciphertext(bytes.begin(), bytes.end());
        if (ciphertext.find(g_subject) != std::string::npos || bytes.empty()) {
            throw std::runtime_error("encrypted gallery leaked plaintext identity");
        }
        bytes.back() ^= 1U;
        vqec_vision_ai_unit_efgtst_write_file(gallery_path, bytes);
        if (restarted.vqec_vision_ai_ports_fgstr_load(
                gallery_config, recovered).code_ != status_code::unauthorized) {
            throw std::runtime_error("encrypted gallery accepted tampered ciphertext");
        }
        if (::unlink(gallery_path.c_str()) != 0 ||
            restarted.vqec_vision_ai_ports_fgstr_load(
                gallery_config, recovered).code_ != status_code::protocol_error) {
            throw std::runtime_error("encrypted gallery silently reset missing durable data");
        }
    } catch (...) {
        vqec_vision_ai_unit_efgtst_remove_tree(directory);
        throw;
    }
    vqec_vision_ai_unit_efgtst_remove_tree(directory);
}

}  // namespace
}  // namespace vqec::vision::ai

int main() {
    vqec::vision::ai::vqec_vision_ai_unit_efgtst_check_store();
    return 0;
}
