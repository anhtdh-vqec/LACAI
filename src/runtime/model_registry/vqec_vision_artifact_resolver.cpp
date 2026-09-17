#include "vqec_vision_artifact_resolver.hpp"

#include <array>
#include <cerrno>
#include <cstdlib>
#include <memory>
#include <new>
#include <string>
#include <utility>

#include <fcntl.h>
#include <linux/memfd.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace vqec::vision::ai {
namespace {

struct fd_owner {
    int value_{-1};
    explicit fd_owner(int _value) noexcept : value_(_value) {}
    ~fd_owner() noexcept {
        if (value_ >= 0) {
            ::close(value_);
        }
    }
};

constexpr std::size_t g_artifact_copy_block_bytes = 64U * 1024U;
constexpr char g_sealed_artifact_name[] = "vqec_vision_model_artifact";

status vqec_vision_ai_mreg_artsr_copy_to_sealed_fd(
    int _source_fd, std::uint64_t _bytes, std::shared_ptr<fd_owner>& _owner) {
    const int sealed_fd = static_cast<int>(::syscall(
        SYS_memfd_create, g_sealed_artifact_name, MFD_CLOEXEC | MFD_ALLOW_SEALING));
    if (sealed_fd < 0) {
        return {status_code::unsupported, "cannot create an immutable model artifact"};
    }
    std::shared_ptr<fd_owner> candidate;
    try {
        candidate = std::make_shared<fd_owner>(sealed_fd);
    } catch (const std::bad_alloc&) {
        ::close(sealed_fd);
        return {status_code::resource_exhausted,
            "cannot retain the immutable model artifact"};
    }
    if (::lseek(_source_fd, 0, SEEK_SET) < 0 ||
        ::ftruncate(sealed_fd, static_cast<off_t>(_bytes)) != 0) {
        return {status_code::io_error, "cannot size the immutable model artifact"};
    }
    std::array<std::uint8_t, g_artifact_copy_block_bytes> block{};
    std::uint64_t copied = 0;
    while (copied < _bytes) {
        const auto remaining = _bytes - copied;
        const auto requested = static_cast<std::size_t>(
            remaining < block.size() ? remaining : block.size());
        const auto read_count = ::read(_source_fd, block.data(), requested);
        if (read_count < 0) {
            if (errno == EINTR) {
                continue;
            }
            return {status_code::io_error, "cannot read the verified model artifact"};
        }
        if (read_count == 0) {
            return {status_code::io_error, "model artifact changed while being retained"};
        }
        std::size_t written = 0;
        while (written < static_cast<std::size_t>(read_count)) {
            const auto write_count = ::pwrite(sealed_fd, block.data() + written,
                static_cast<std::size_t>(read_count) - written,
                static_cast<off_t>(copied + written));
            if (write_count < 0) {
                if (errno == EINTR) {
                    continue;
                }
                return {status_code::io_error,
                    "cannot write the immutable model artifact"};
            }
            if (write_count == 0) {
                return {status_code::io_error,
                    "immutable model artifact copy made no progress"};
            }
            written += static_cast<std::size_t>(write_count);
        }
        copied += static_cast<std::uint64_t>(read_count);
    }
    const int seals = F_SEAL_SEAL | F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_WRITE;
    if (::fcntl(sealed_fd, F_ADD_SEALS, seals) != 0 ||
        ::lseek(sealed_fd, 0, SEEK_SET) < 0) {
        return {status_code::unsupported, "cannot seal the immutable model artifact"};
    }
    _owner = std::move(candidate);
    return {};
}

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
        _model.model_id_.empty() || _model.model_version_.empty() ||
        _model.target_id_.empty() ||
        _model.artifact_ref_.empty() || _model.artifact_sha256_.size() != 64) {
        return {status_code::invalid_argument,
            "invalid resolver config, relative path or model identity"};
    }
    std::string root = _config.model_root_;
    while (root.size() > 1 && root.back() == '/') {
        root.pop_back();
    }
    std::string rel_path = _artifact_relative_path;
    if (rel_path.compare(0, root.size(), root) == 0 &&
        rel_path.size() > root.size() &&
        rel_path[root.size()] == '/') {
        rel_path = rel_path.substr(root.size() + 1);
    }
    if (!vqec_vision_ai_mreg_artsr_is_clean_relative(rel_path)) {
        return {status_code::invalid_argument, "invalid resolver relative path"};
    }
    const std::string full_path = (root == "/" ? "" : root) + "/" + rel_path;
    fd_owner fd(::open(full_path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW));
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
        ::realpath(root.c_str(), root_buffer.data()) == nullptr) {
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
    std::shared_ptr<fd_owner> sealed_owner;
    const auto retained = vqec_vision_ai_mreg_artsr_copy_to_sealed_fd(
        fd.value_, receipt.bytes_, sealed_owner);
    if (retained.code_ != status_code::ok) {
        return retained;
    }
    artifact_digest_receipt sealed_receipt;
    const auto sealed_verified = vqec_vision_ai_mreg_ardgt_verify_fd(
        sealed_owner->value_, _model.artifact_sha256_, _config.max_artifact_bytes_,
        sealed_receipt);
    if (sealed_verified.code_ != status_code::ok ||
        sealed_receipt.bytes_ != receipt.bytes_) {
        return sealed_verified.code_ != status_code::ok ? sealed_verified :
            status{status_code::protocol_error,
                "immutable model artifact byte count changed"};
    }
    resolved_model_paths candidate;
    candidate.model_id_ = _model.model_id_;
    candidate.target_id_ = _model.target_id_;
    candidate.artifact_ref_ = _model.artifact_ref_;
    candidate.model_path_ = "/proc/self/fd/" + std::to_string(sealed_owner->value_);
    candidate.backend_path_ = _config.backend_library_;
    candidate.system_path_ = _config.system_library_;
    candidate.model_artifact_owner_ = std::move(sealed_owner);
    _paths = std::move(candidate);
    return {};
}

}  // namespace vqec::vision::ai
