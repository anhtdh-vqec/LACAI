#include "vqec_vision_image_path_authorizer.hpp"

#include <array>
#include <cerrno>
#include <cstdlib>
#include <limits.h>
#include <memory>
#include <utility>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace vqec::vision::ai {
namespace {
constexpr std::array<std::uint8_t, 3> g_jpeg_signature{0xffU, 0xd8U, 0xffU};
constexpr auto g_proc_fd_prefix = "/proc/self/fd/";

struct retained_fd {
    explicit retained_fd(int _value) noexcept : value_(_value) {}
    ~retained_fd() noexcept { if (value_ >= 0) ::close(value_); }
    int value_{-1};
};

bool vqec_vision_ai_fwctl_ipath_is_under_root(
    const std::string& _file, const std::string& _root) noexcept {
    if (_root == "/") return !_file.empty() && _file.front() == '/';
    return _file.size() > _root.size() && _file.compare(0, _root.size(), _root) == 0 &&
        _file[_root.size()] == '/';
}
}

status image_path_authorizer::vqec_vision_ai_fwctl_ipath_configure(
    const image_path_authorizer_config& _config) {
    if (is_configured_ || _config.allowed_roots_.empty() ||
        _config.allowed_roots_.size() > image_path_authorizer_limits::g_max_allowed_roots ||
        _config.max_file_bytes_ == 0 ||
        _config.max_file_bytes_ > image_path_authorizer_limits::g_max_file_bytes) {
        return {status_code::invalid_argument, "invalid enrollment image path policy"};
    }
    std::vector<std::string> canonical_roots;
    canonical_roots.reserve(_config.allowed_roots_.size());
    for (const auto& root : _config.allowed_roots_) {
        std::array<char, PATH_MAX> buffer{};
        struct stat info {};
        if (root.empty() || root.front() != '/' ||
            ::realpath(root.c_str(), buffer.data()) == nullptr ||
            ::stat(buffer.data(), &info) != 0 || !S_ISDIR(info.st_mode)) {
            return {status_code::invalid_argument, "enrollment image root is invalid"};
        }
        std::string canonical(buffer.data());
        if (canonical.size() > 1 && canonical.back() == '/') canonical.pop_back();
        // The filesystem root is not a valid enrollment root: it would authorize every
        // absolute path and defeat the containment check. Reject it at configuration time.
        if (canonical == "/") {
            return {status_code::invalid_argument,
                "enrollment image root must not be the filesystem root"};
        }
        canonical_roots.push_back(std::move(canonical));
    }
    canonical_roots_ = std::move(canonical_roots);
    max_file_bytes_ = _config.max_file_bytes_;
    is_configured_ = true;
    return {};
}

status image_path_authorizer::vqec_vision_ai_ports_ipath_authorize(
    const std::string& _requested_path, authorized_image_path& _authorized_path) {
    if (!is_configured_) {
        return {status_code::invalid_state, "enrollment image path policy is not configured"};
    }
    if (_requested_path.empty() || _requested_path.front() != '/' ||
        _requested_path.find('\0') != std::string::npos || _requested_path.size() >= PATH_MAX) {
        return {status_code::invalid_argument, "enrollment image path is invalid"};
    }
    const int descriptor = ::open(_requested_path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (descriptor < 0) {
        return {errno == ELOOP ? status_code::unauthorized : status_code::io_error,
            "cannot open enrollment image"};
    }
    retained_fd descriptor_guard(descriptor);
    auto owner = std::make_shared<retained_fd>(-1);
    owner->value_ = descriptor_guard.value_;
    descriptor_guard.value_ = -1;
    struct stat info {};
    if (::fstat(descriptor, &info) != 0 || !S_ISREG(info.st_mode)) {
        return {status_code::invalid_argument, "enrollment image is not a regular file"};
    }
    if (info.st_size <= 0 || static_cast<std::uint64_t>(info.st_size) > max_file_bytes_) {
        return {status_code::resource_exhausted, "enrollment image exceeds file byte policy"};
    }
    const std::string retained_path = std::string(g_proc_fd_prefix) +
        std::to_string(descriptor);
    std::array<char, PATH_MAX> link_buffer{};
    const auto link_length = ::readlink(
        retained_path.c_str(), link_buffer.data(), link_buffer.size() - 1);
    if (link_length <= 0) {
        return {status_code::io_error, "cannot resolve retained enrollment image"};
    }
    const std::string canonical_file(
        link_buffer.data(), static_cast<std::size_t>(link_length));
    bool authorized = false;
    for (const auto& root : canonical_roots_) {
        authorized = authorized ||
            vqec_vision_ai_fwctl_ipath_is_under_root(canonical_file, root);
    }
    if (!authorized) {
        return {status_code::unauthorized, "enrollment image is outside allowed roots"};
    }
    std::array<std::uint8_t, g_jpeg_signature.size()> signature{};
    if (::pread(descriptor, signature.data(), signature.size(), 0) !=
            static_cast<ssize_t>(signature.size()) || signature != g_jpeg_signature) {
        return {status_code::invalid_argument, "enrollment image is not JPEG"};
    }
    authorized_image_path candidate;
    candidate.path_ = retained_path;
    candidate.owner_ = std::move(owner);
    _authorized_path = std::move(candidate);
    return {};
}

}  // namespace vqec::vision::ai
