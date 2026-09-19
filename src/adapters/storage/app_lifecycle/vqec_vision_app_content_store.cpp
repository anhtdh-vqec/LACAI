#include "vqec_vision_app_content_store.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <filesystem>
#include <limits>
#include <streambuf>
#include <string>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "vqec/vision/ai/contracts/base/vqec_vision_identifier.hpp"
#include "vqec_vision_artifact_digest.hpp"

namespace vqec::vision::ai {
namespace {

constexpr std::uint64_t g_max_supported_store_bytes = 4ULL * 1024ULL * 1024ULL * 1024ULL;
constexpr std::size_t g_content_copy_block_bytes = 64U * 1024U;
constexpr std::size_t g_max_supported_blob_count = 65536U;
constexpr char g_staging_prefix[] = ".staging.";

class staged_file final {
public:
    staged_file() = default;
    ~staged_file() noexcept {
        if (fd_ >= 0) {
            (void)::close(fd_);
        }
        if (!path_.empty()) {
            (void)::unlink(path_.c_str());
        }
    }
    staged_file(const staged_file&) = delete;
    staged_file& operator=(const staged_file&) = delete;

    int fd_{-1};
    std::string path_;
};

class descriptor_stream_buffer final : public std::streambuf {
public:
    explicit descriptor_stream_buffer(int _fd) : fd_(_fd) {
        setg(buffer_.data(), buffer_.data(), buffer_.data());
    }
    ~descriptor_stream_buffer() noexcept override {
        if (fd_ >= 0) {
            (void)::close(fd_);
        }
    }
    descriptor_stream_buffer(const descriptor_stream_buffer&) = delete;
    descriptor_stream_buffer& operator=(const descriptor_stream_buffer&) = delete;

    [[nodiscard]] bool vqec_vision_ai_stor_apcst_has_error() const noexcept {
        return has_error_;
    }

protected:
    int_type underflow() override {
        if (gptr() < egptr()) {
            return traits_type::to_int_type(*gptr());
        }
        ssize_t count = -1;
        do {
            count = ::pread(fd_, buffer_.data(), buffer_.size(), offset_);
        } while (count < 0 && errno == EINTR);
        if (count < 0) {
            has_error_ = true;
            return traits_type::eof();
        }
        if (count == 0) {
            return traits_type::eof();
        }
        offset_ += count;
        setg(buffer_.data(), buffer_.data(), buffer_.data() + count);
        return traits_type::to_int_type(*gptr());
    }

private:
    int fd_{-1};
    off_t offset_{0};
    bool has_error_{false};
    std::array<char, g_content_copy_block_bytes> buffer_{};
};

bool vqec_vision_ai_stor_apcst_is_digest_name(const std::string& _name) noexcept {
    return vqec_vision_ai_cntr_ident_is_sha256_hex(_name);
}

std::string vqec_vision_ai_stor_apcst_blob_path(
    const app_content_store_config& _config, const std::string& _sha256) {
    return _config.root_directory_ + "/" + _sha256;
}

status vqec_vision_ai_stor_apcst_validate_root(
    const app_content_store_config& _config) {
    if (_config.root_directory_.empty() ||
        !std::filesystem::path(_config.root_directory_).is_absolute() ||
        _config.max_store_bytes_ == 0 ||
        _config.max_store_bytes_ > g_max_supported_store_bytes ||
        _config.max_blob_bytes_ == 0 ||
        _config.max_blob_bytes_ > _config.max_store_bytes_ ||
        _config.max_blob_count_ == 0 ||
        _config.max_blob_count_ > g_max_supported_blob_count) {
        return {status_code::invalid_argument, "invalid app content store configuration"};
    }
    struct stat root_status {};
    if (::lstat(_config.root_directory_.c_str(), &root_status) != 0 ||
        !S_ISDIR(root_status.st_mode) || root_status.st_uid != ::getuid() ||
        (root_status.st_mode & (S_IRWXG | S_IRWXO)) != 0) {
        return {status_code::unauthorized,
            "app content store root must be an owner-only directory"};
    }
    return {};
}

status vqec_vision_ai_stor_apcst_validate_blob_stat(
    const struct stat& _file_status, std::uint64_t _max_blob_bytes) {
    if (!S_ISREG(_file_status.st_mode) || _file_status.st_uid != ::getuid() ||
        (_file_status.st_mode & (S_IRWXG | S_IRWXO)) != 0 ||
        _file_status.st_nlink != 1 || _file_status.st_size <= 0 ||
        static_cast<std::uint64_t>(_file_status.st_size) > _max_blob_bytes) {
        return {status_code::unauthorized,
            "app content blob is not an immutable owner-only regular file"};
    }
    return {};
}

status vqec_vision_ai_stor_apcst_sync_root(const std::string& _root_directory) {
    const int root_fd = ::open(_root_directory.c_str(),
        O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    if (root_fd < 0) {
        return {status_code::io_error, "cannot open app content store root"};
    }
    const int synced = ::fsync(root_fd);
    const int saved_error = errno;
    (void)::close(root_fd);
    if (synced != 0) {
        errno = saved_error;
        return {status_code::io_error, "cannot sync app content store root"};
    }
    return {};
}

status vqec_vision_ai_stor_apcst_copy_source(
    std::istream& _source, int _destination_fd, std::uint64_t _max_bytes,
    std::uint64_t& _copied_bytes) {
    if (!_source.good() || _destination_fd < 0 || _max_bytes == 0) {
        return {status_code::invalid_argument, "invalid app content copy request"};
    }
    std::array<char, g_content_copy_block_bytes> block{};
    std::uint64_t total = 0;
    for (;;) {
        const auto requested = static_cast<std::streamsize>(
            std::min<std::uint64_t>(block.size(), _max_bytes - total + 1U));
        try {
            _source.read(block.data(), requested);
        } catch (const std::ios_base::failure&) {
            if (!_source.eof() || _source.bad()) {
                return {status_code::io_error, "app content source read failed"};
            }
        }
        if (_source.bad() || (_source.fail() && !_source.eof())) {
            return {status_code::io_error, "app content source failed before EOF"};
        }
        const auto count = _source.gcount();
        if (count < 0 || count > requested) {
            return {status_code::io_error, "invalid app content source read count"};
        }
        const auto bytes = static_cast<std::uint64_t>(count);
        if (bytes > _max_bytes - total) {
            return {status_code::resource_exhausted,
                "app content blob exceeds configured byte limit"};
        }
        std::size_t offset = 0;
        while (offset < static_cast<std::size_t>(count)) {
            const auto written = ::write(_destination_fd, block.data() + offset,
                static_cast<std::size_t>(count) - offset);
            if (written < 0 && errno == EINTR) {
                continue;
            }
            if (written <= 0) {
                return {errno == ENOSPC ? status_code::resource_exhausted :
                    status_code::io_error, "app content staging write failed"};
            }
            offset += static_cast<std::size_t>(written);
        }
        total += bytes;
        if (_source.eof()) {
            break;
        }
        if (count == 0) {
            return {status_code::io_error, "app content source made no progress"};
        }
    }
    if (total == 0) {
        return {status_code::invalid_argument, "app content blob is empty"};
    }
    _copied_bytes = total;
    return {};
}

status vqec_vision_ai_stor_apcst_open_staging(
    const std::string& _root_directory, staged_file& _staging) {
    std::string pattern = _root_directory + "/" + g_staging_prefix + "XXXXXX";
    std::vector<char> mutable_pattern(pattern.begin(), pattern.end());
    mutable_pattern.push_back('\0');
    const int fd = ::mkstemp(mutable_pattern.data());
    if (fd < 0) {
        return {errno == ENOSPC ? status_code::resource_exhausted :
            status_code::io_error, "cannot create app content staging file"};
    }
    const int flags = ::fcntl(fd, F_GETFD);
    if (flags < 0 || ::fcntl(fd, F_SETFD, flags | FD_CLOEXEC) != 0) {
        (void)::close(fd);
        (void)::unlink(mutable_pattern.data());
        return {status_code::io_error, "cannot protect app content staging descriptor"};
    }
    _staging.fd_ = fd;
    _staging.path_ = mutable_pattern.data();
    return {};
}

}  // namespace

app_content_store::app_content_store(app_content_store_config _config)
    : config_(std::move(_config)) {}

status app_content_store::vqec_vision_ai_ports_apcst_open() {
    if (open_) {
        return {};
    }
    const auto valid = vqec_vision_ai_stor_apcst_validate_root(config_);
    if (valid.code_ != status_code::ok) {
        return valid;
    }
    std::uint64_t bytes = 0;
    std::size_t count = 0;
    bool recovered_staging = false;
    try {
        for (const auto& entry : std::filesystem::directory_iterator(
                 config_.root_directory_, std::filesystem::directory_options::none)) {
            const auto name = entry.path().filename().string();
            if (name.rfind(g_staging_prefix, 0) == 0) {
                struct stat staging_status {};
                if (::lstat(entry.path().c_str(), &staging_status) != 0 ||
                    !S_ISREG(staging_status.st_mode) ||
                    staging_status.st_uid != ::getuid() ||
                    ::unlink(entry.path().c_str()) != 0) {
                    return {status_code::invalid_state,
                        "cannot recover app content staging file"};
                }
                recovered_staging = true;
                continue;
            }
            if (!vqec_vision_ai_stor_apcst_is_digest_name(name)) {
                return {status_code::invalid_state,
                    "unknown entry in app content store"};
            }
            struct stat file_status {};
            if (::lstat(entry.path().c_str(), &file_status) != 0) {
                return {status_code::io_error, "cannot inspect app content blob"};
            }
            const auto blob_valid = vqec_vision_ai_stor_apcst_validate_blob_stat(
                file_status, config_.max_blob_bytes_);
            if (blob_valid.code_ != status_code::ok) {
                return blob_valid;
            }
            const auto file_bytes = static_cast<std::uint64_t>(file_status.st_size);
            if (count == config_.max_blob_count_ ||
                file_bytes > config_.max_store_bytes_ - bytes) {
                return {status_code::resource_exhausted,
                    "app content store exceeds configured capacity"};
            }
            bytes += file_bytes;
            ++count;
        }
    } catch (const std::filesystem::filesystem_error&) {
        return {status_code::io_error, "cannot enumerate app content store"};
    }
    if (recovered_staging) {
        const auto synced = vqec_vision_ai_stor_apcst_sync_root(
            config_.root_directory_);
        if (synced.code_ != status_code::ok) {
            return synced;
        }
    }
    stored_bytes_ = bytes;
    stored_blob_count_ = count;
    open_ = true;
    return {};
}

status app_content_store::vqec_vision_ai_ports_apcst_get(
    const std::string& _sha256, app_content_record& _record) const {
    if (!open_) {
        return {status_code::invalid_state, "app content store is not open"};
    }
    if (!vqec_vision_ai_stor_apcst_is_digest_name(_sha256)) {
        return {status_code::invalid_argument, "invalid app content digest"};
    }
    const auto path = vqec_vision_ai_stor_apcst_blob_path(config_, _sha256);
    const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) {
        return {errno == ENOENT ? status_code::source_lost : status_code::io_error,
            "app content blob is unavailable"};
    }
    struct stat file_status {};
    auto current = ::fstat(fd, &file_status) == 0 ?
        vqec_vision_ai_stor_apcst_validate_blob_stat(
            file_status, config_.max_blob_bytes_) :
        status{status_code::io_error, "cannot inspect app content blob"};
    artifact_digest_receipt receipt;
    if (current.code_ == status_code::ok) {
        current = vqec_vision_ai_mreg_ardgt_verify_fd(
            fd, _sha256, config_.max_blob_bytes_, receipt);
    }
    (void)::close(fd);
    if (current.code_ != status_code::ok) {
        return current;
    }
    app_content_record candidate{receipt.sha256_, receipt.bytes_, path};
    _record = std::move(candidate);
    return {};
}

status app_content_store::vqec_vision_ai_ports_apcst_put(
    std::istream& _source, const std::string& _expected_sha256,
    std::uint64_t _expected_bytes, app_content_record& _record) {
    if (!open_) {
        return {status_code::invalid_state, "app content store is not open"};
    }
    if (!vqec_vision_ai_stor_apcst_is_digest_name(_expected_sha256) ||
        _expected_bytes == 0 || _expected_bytes > config_.max_blob_bytes_) {
        return {status_code::invalid_argument, "invalid app content put request"};
    }
    app_content_record existing;
    const auto found = vqec_vision_ai_ports_apcst_get(_expected_sha256, existing);
    if (found.code_ == status_code::ok) {
        if (existing.byte_size_ != _expected_bytes) {
            return {status_code::protocol_error,
                "existing app content size differs from package declaration"};
        }
        _record = std::move(existing);
        return {};
    }
    if (found.code_ != status_code::source_lost) {
        return found;
    }
    if (stored_blob_count_ >= config_.max_blob_count_ ||
        _expected_bytes > config_.max_store_bytes_ - stored_bytes_) {
        return {status_code::resource_exhausted,
            "app content store has no declared capacity"};
    }

    staged_file staging;
    auto current = vqec_vision_ai_stor_apcst_open_staging(
        config_.root_directory_, staging);
    std::uint64_t copied_bytes = 0;
    if (current.code_ == status_code::ok) {
        current = vqec_vision_ai_stor_apcst_copy_source(
            _source, staging.fd_, config_.max_blob_bytes_, copied_bytes);
    }
    if (current.code_ == status_code::ok && copied_bytes != _expected_bytes) {
        current = {status_code::protocol_error,
            "app content byte size differs from package declaration"};
    }
    if (current.code_ == status_code::ok && ::fsync(staging.fd_) != 0) {
        current = {errno == ENOSPC ? status_code::resource_exhausted :
            status_code::io_error, "cannot sync app content staging file"};
    }
    if (current.code_ == status_code::ok && ::lseek(staging.fd_, 0, SEEK_SET) != 0) {
        current = {status_code::io_error, "cannot rewind app content staging file"};
    }
    artifact_digest_receipt receipt;
    if (current.code_ == status_code::ok) {
        current = vqec_vision_ai_mreg_ardgt_verify_fd(staging.fd_,
            _expected_sha256, config_.max_blob_bytes_, receipt);
    }
    if (current.code_ == status_code::ok && ::fchmod(staging.fd_, S_IRUSR) != 0) {
        current = {status_code::io_error, "cannot protect app content staging file"};
    }
    if (current.code_ == status_code::ok && ::fsync(staging.fd_) != 0) {
        current = {errno == ENOSPC ? status_code::resource_exhausted :
            status_code::io_error, "cannot sync protected app content staging file"};
    }
    if (current.code_ != status_code::ok) {
        return current;
    }

    const auto target = vqec_vision_ai_stor_apcst_blob_path(config_, _expected_sha256);
    bool published = false;
    if (::link(staging.path_.c_str(), target.c_str()) == 0) {
        published = true;
    } else if (errno != EEXIST) {
        return {errno == ENOSPC ? status_code::resource_exhausted :
            status_code::io_error, "cannot publish app content blob"};
    }
    if (published) {
        if (::unlink(staging.path_.c_str()) != 0) {
            (void)::unlink(target.c_str());
            return {status_code::io_error,
                "cannot retire app content staging link"};
        }
        staging.path_.clear();
        const auto synced = vqec_vision_ai_stor_apcst_sync_root(config_.root_directory_);
        if (synced.code_ != status_code::ok) {
            (void)::unlink(target.c_str());
            return synced;
        }
        stored_bytes_ += receipt.bytes_;
        ++stored_blob_count_;
    }
    app_content_record candidate;
    current = vqec_vision_ai_ports_apcst_get(_expected_sha256, candidate);
    if (current.code_ != status_code::ok || candidate.byte_size_ != _expected_bytes) {
        return current.code_ == status_code::ok ?
            status{status_code::protocol_error,
                "published app content size differs from package declaration"} : current;
    }
    _record = std::move(candidate);
    return {};
}

status app_content_store::vqec_vision_ai_ports_apcst_put_descriptor(
    int _source_fd, const std::string& _expected_sha256,
    std::uint64_t _expected_bytes, app_content_record& _record) {
    if (_source_fd < 0) {
        return {status_code::invalid_argument,
            "app content source descriptor is invalid"};
    }
    const int flags = ::fcntl(_source_fd, F_GETFL);
    struct stat source_status {};
    if (flags < 0 || (flags & O_ACCMODE) != O_RDONLY ||
        ::fstat(_source_fd, &source_status) != 0 ||
        !S_ISREG(source_status.st_mode) || source_status.st_size <= 0 ||
        static_cast<std::uint64_t>(source_status.st_size) != _expected_bytes) {
        return {status_code::invalid_argument,
            "app content source must be an exact-size read-only regular file"};
    }
    const int duplicate = ::fcntl(_source_fd, F_DUPFD_CLOEXEC, 0);
    if (duplicate < 0) {
        return {status_code::io_error,
            "cannot duplicate app content source descriptor"};
    }
    descriptor_stream_buffer buffer(duplicate);
    std::istream stream(&buffer);
    const auto stored = vqec_vision_ai_ports_apcst_put(
        stream, _expected_sha256, _expected_bytes, _record);
    if (buffer.vqec_vision_ai_stor_apcst_has_error()) {
        return {status_code::io_error, "app content descriptor read failed"};
    }
    return stored;
}

status app_content_store::vqec_vision_ai_ports_apcst_remove(
    const std::string& _sha256) {
    app_content_record record;
    const auto found = vqec_vision_ai_ports_apcst_get(_sha256, record);
    if (found.code_ != status_code::ok) {
        return found;
    }
    if (::unlink(record.immutable_location_.c_str()) != 0) {
        return {status_code::io_error, "cannot remove app content blob"};
    }
    stored_bytes_ -= record.byte_size_;
    --stored_blob_count_;
    const auto synced = vqec_vision_ai_stor_apcst_sync_root(config_.root_directory_);
    if (synced.code_ != status_code::ok) {
        return synced;
    }
    return {};
}

}  // namespace vqec::vision::ai
