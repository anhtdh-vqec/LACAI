#include "vqec_vision_artifact_resolver.hpp"

#include <array>
#include <cerrno>
#include <cstdlib>
#include <string>
#include <utility>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace vqec::vision::ai {
namespace {

struct fd_owner {
    int value_{-1};
    ~fd_owner() noexcept {
        if (value_ >= 0) {
            ::close(value_);
        }
    }
};

bool vqec_vision_ai_mreg_artsr_is_clean_absolute(
    const std::string& _path) noexcept {
    return !_path.empty() && _path.front() == '/' &&
        _path.find('\0') == std::string::npos &&
        _path.find("/../") == std::string::npos &&
        (_path.size() < 3 || _path.compare(_path.size() - 3, 3, "/..") != 0);
}

bool vqec_vision_ai_mreg_artsr_is_clean_relative(
    const std::string& _path) noexcept {
    return !_path.empty() && _path.front() != '/' &&
        _path.find('\0') == std::string::npos &&
        _path.find("..") == std::string::npos &&
        _path.find("//") == std::string::npos;
}

bool vqec_vision_ai_mreg_artsr_is_under_root(
    const std::string& _file, const std::string& _root) noexcept {
    if (_root == "/") {
        return !_file.empty() && _file.front() == '/';
    }
    return _file.size() > _root.size() &&
        _file.compare(0, _root.size(), _root) == 0 && _file[_root.size()] == '/';
}

}  // namespace

status vqec_vision_ai_mreg_artsr_resolve_model(
    const model_catalog_entry& _model, const std::string& _artifact_relative_path,
    const artifact_resolver_config& _config, resolved_model_paths& _paths) {
    if (!vqec_vision_ai_mreg_artsr_is_clean_absolute(_config.model_root_) ||
        !vqec_vision_ai_mreg_artsr_is_clean_absolute(_config.backend_library_) ||
        !vqec_vision_ai_mreg_artsr_is_clean_absolute(_config.system_library_) ||
        _config.max_artifact_bytes_ == 0 ||
        _config.max_artifact_bytes_ > 4ULL * 1024 * 1024 * 1024 ||
        !vqec_vision_ai_mreg_artsr_is_clean_relative(_artifact_relative_path) ||
        _model.model_id_.empty() || _model.target_id_.empty() ||
        _model.artifact_ref_.empty()) {
        return {status_code::invalid_argument,
            "invalid resolver config, relative path or model identity"};
    }
    const std::string full_path =
        _config.model_root_ + "/" + _artifact_relative_path;
    fd_owner fd{::open(full_path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW)};
    if (fd.value_ < 0) {
        return {errno == ELOOP ? status_code::unauthorized : status_code::io_error,
            "cannot open model artifact under the allowed root"};
    }
    struct stat info {};
    if (::fstat(fd.value_, &info) != 0 || !S_ISREG(info.st_mode)) {
        return {status_code::invalid_argument, "model artifact is not a regular file"};
    }
    if (info.st_size <= 0 ||
        static_cast<std::uint64_t>(info.st_size) > _config.max_artifact_bytes_) {
        return {status_code::resource_exhausted,
            "model artifact size is invalid or exceeds the bound"};
    }
    // Bind containment to the opened inode through /proc, not a re-resolved path, so a
    // rename or symlink swap after open cannot move the checked file outside the root.
    std::array<char, 4096> link_buffer{};
    const std::string fd_path = "/proc/self/fd/" + std::to_string(fd.value_);
    const auto link_length = ::readlink(fd_path.c_str(), link_buffer.data(),
                                        link_buffer.size() - 1);
    std::array<char, 4096> root_buffer{};
    if (link_length <= 0 ||
        ::realpath(_config.model_root_.c_str(), root_buffer.data()) == nullptr) {
        return {status_code::io_error,
            "cannot canonicalize the model artifact or allowed root"};
    }
    std::string canonical_file(link_buffer.data(), static_cast<std::size_t>(link_length));
    std::string canonical_root(root_buffer.data());
    if (canonical_root.size() > 1 && canonical_root.back() == '/') {
        canonical_root.pop_back();
    }
    if (!vqec_vision_ai_mreg_artsr_is_under_root(canonical_file, canonical_root)) {
        return {status_code::unauthorized, "model artifact escapes the allowed root"};
    }
    artifact_digest_receipt receipt;
    const auto verified = vqec_vision_ai_mreg_ardgt_verify_fd(
        fd.value_, _model.artifact_sha256_, _config.max_artifact_bytes_, receipt);
    if (verified.code_ != status_code::ok) {
        return verified;
    }
    resolved_model_paths candidate;
    candidate.model_id_ = _model.model_id_;
    candidate.target_id_ = _model.target_id_;
    candidate.artifact_ref_ = _model.artifact_ref_;
    candidate.model_path_ = std::move(canonical_file);
    candidate.backend_path_ = _config.backend_library_;
    candidate.system_path_ = _config.system_library_;
    _paths = std::move(candidate);
    return {};
}

}  // namespace vqec::vision::ai
