#include "vqec_vision_encrypted_face_gallery_store.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <limits>
#include <memory>
#include <new>
#include <random>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <utility>
#include <vector>

#include <openssl/evp.h>
#include <openssl/crypto.h>
#include <openssl/rand.h>

namespace vqec::vision::ai {
namespace {

constexpr std::array<unsigned char, 8> g_magic{'L', 'A', 'C', 'A', 'I', 'F', 'G', 1};
constexpr std::uint32_t g_format_version = 1;
constexpr std::size_t g_nonce_bytes = 12;
constexpr std::size_t g_tag_bytes = 16;
constexpr std::size_t g_header_bytes = g_magic.size() + sizeof(std::uint32_t) +
    sizeof(std::uint64_t) + g_nonce_bytes;
constexpr std::size_t g_key_bytes = 32;
constexpr std::size_t g_max_store_bytes = 64U * 1024U * 1024U;
constexpr std::size_t g_max_file_name_bytes = 96;
constexpr mode_t g_private_file_mode = S_IRUSR | S_IWUSR;
constexpr mode_t g_private_directory_mode = S_IRUSR | S_IWUSR | S_IXUSR;

void vqec_vision_ai_stor_efgal_close(int _dir_fd, int _lock_fd) noexcept;

class vqec_vision_ai_stor_efgal_fd_owner final {
public:
    explicit vqec_vision_ai_stor_efgal_fd_owner(int _value = -1) noexcept
        : value_(_value) {}
    ~vqec_vision_ai_stor_efgal_fd_owner() noexcept {
        if (value_ >= 0) (void)::close(value_);
    }
    vqec_vision_ai_stor_efgal_fd_owner(
        const vqec_vision_ai_stor_efgal_fd_owner&) = delete;
    vqec_vision_ai_stor_efgal_fd_owner& operator=(
        const vqec_vision_ai_stor_efgal_fd_owner&) = delete;
    int vqec_vision_ai_stor_efgal_get() const noexcept { return value_; }
private:
    int value_;
};

class vqec_vision_ai_stor_efgal_store_owner final {
public:
    vqec_vision_ai_stor_efgal_store_owner(int _directory, int _lock) noexcept
        : directory_(_directory), lock_(_lock) {}
    ~vqec_vision_ai_stor_efgal_store_owner() noexcept {
        vqec_vision_ai_stor_efgal_close(directory_, lock_);
    }
    vqec_vision_ai_stor_efgal_store_owner(
        const vqec_vision_ai_stor_efgal_store_owner&) = delete;
    vqec_vision_ai_stor_efgal_store_owner& operator=(
        const vqec_vision_ai_stor_efgal_store_owner&) = delete;
private:
    int directory_;
    int lock_;
};

class vqec_vision_ai_stor_efgal_key_owner final {
public:
    ~vqec_vision_ai_stor_efgal_key_owner() noexcept {
        OPENSSL_cleanse(value_.data(), value_.size());
    }
    std::array<unsigned char, g_key_bytes> value_{};
};

bool vqec_vision_ai_stor_efgal_valid_name(const std::string& _name) noexcept {
    return !_name.empty() && _name.size() <= g_max_file_name_bytes &&
        _name != "." && _name != ".." &&
        _name.find('/') == std::string::npos && _name.find('\\') == std::string::npos;
}

void vqec_vision_ai_stor_efgal_put_u32(std::vector<unsigned char>& _out,
                                       std::uint32_t _value) {
    for (unsigned shift = 0; shift < 32U; shift += 8U) {
        _out.push_back(static_cast<unsigned char>(_value >> shift));
    }
}

void vqec_vision_ai_stor_efgal_put_u64(std::vector<unsigned char>& _out,
                                       std::uint64_t _value) {
    for (unsigned shift = 0; shift < 64U; shift += 8U) {
        _out.push_back(static_cast<unsigned char>(_value >> shift));
    }
}

bool vqec_vision_ai_stor_efgal_get_u32(const std::vector<unsigned char>& _in,
                                       std::size_t& _offset, std::uint32_t& _value) noexcept {
    if (_offset > _in.size() || _in.size() - _offset < sizeof(std::uint32_t)) {
        return false;
    }
    _value = 0;
    for (unsigned shift = 0; shift < 32U; shift += 8U) {
        _value |= static_cast<std::uint32_t>(_in[_offset++]) << shift;
    }
    return true;
}

bool vqec_vision_ai_stor_efgal_get_u64(const std::vector<unsigned char>& _in,
                                       std::size_t& _offset, std::uint64_t& _value) noexcept {
    if (_offset > _in.size() || _in.size() - _offset < sizeof(std::uint64_t)) {
        return false;
    }
    _value = 0;
    for (unsigned shift = 0; shift < 64U; shift += 8U) {
        _value |= static_cast<std::uint64_t>(_in[_offset++]) << shift;
    }
    return true;
}

bool vqec_vision_ai_stor_efgal_put_string(std::vector<unsigned char>& _out,
                                          const std::string& _value) {
    if (_value.size() > std::numeric_limits<std::uint32_t>::max()) {
        return false;
    }
    vqec_vision_ai_stor_efgal_put_u32(_out, static_cast<std::uint32_t>(_value.size()));
    _out.insert(_out.end(), _value.begin(), _value.end());
    return true;
}

bool vqec_vision_ai_stor_efgal_get_string(const std::vector<unsigned char>& _in,
                                          std::size_t& _offset, std::string& _value) {
    std::uint32_t length = 0;
    if (!vqec_vision_ai_stor_efgal_get_u32(_in, _offset, length) ||
        static_cast<std::size_t>(length) > _in.size() - _offset) {
        return false;
    }
    _value.assign(reinterpret_cast<const char*>(_in.data() + _offset), length);
    _offset += length;
    return true;
}

bool vqec_vision_ai_stor_efgal_serialize(const face_gallery_snapshot& _snapshot,
                                         std::vector<unsigned char>& _payload) {
    _payload.clear();
    _payload.reserve(128U + _snapshot.templates_.size() * 64U);
    vqec_vision_ai_stor_efgal_put_u32(_payload, _snapshot.schema_version_);
    vqec_vision_ai_stor_efgal_put_u64(_payload, _snapshot.revision_);
    vqec_vision_ai_stor_efgal_put_u64(_payload, _snapshot.next_record_id_);
    if (!vqec_vision_ai_stor_efgal_put_string(_payload, _snapshot.gallery_id_) ||
        !vqec_vision_ai_stor_efgal_put_string(_payload, _snapshot.model_id_) ||
        !vqec_vision_ai_stor_efgal_put_string(_payload, _snapshot.model_version_)) {
        return false;
    }
    vqec_vision_ai_stor_efgal_put_u64(_payload, _snapshot.preprocess_revision_);
    vqec_vision_ai_stor_efgal_put_u64(_payload, _snapshot.dimensions_);
    vqec_vision_ai_stor_efgal_put_u64(_payload, _snapshot.templates_.size());
    for (const auto& item : _snapshot.templates_) {
        vqec_vision_ai_stor_efgal_put_u64(_payload, item.record_id_);
        if (!vqec_vision_ai_stor_efgal_put_string(_payload, item.subject_ref_)) {
            return false;
        }
        for (const float value : item.values_) {
            std::uint32_t bits = 0;
            static_assert(sizeof(bits) == sizeof(value));
            std::memcpy(&bits, &value, sizeof(bits));
            vqec_vision_ai_stor_efgal_put_u32(_payload, bits);
        }
    }
    return true;
}

bool vqec_vision_ai_stor_efgal_deserialize(const std::vector<unsigned char>& _payload,
                                           const face_gallery_config& _config,
                                           face_gallery_snapshot& _snapshot) {
    std::size_t offset = 0;
    std::uint64_t dimensions = 0;
    std::uint64_t template_count = 0;
    if (!vqec_vision_ai_stor_efgal_get_u32(_payload, offset, _snapshot.schema_version_) ||
        !vqec_vision_ai_stor_efgal_get_u64(_payload, offset, _snapshot.revision_) ||
        !vqec_vision_ai_stor_efgal_get_u64(_payload, offset, _snapshot.next_record_id_) ||
        !vqec_vision_ai_stor_efgal_get_string(_payload, offset, _snapshot.gallery_id_) ||
        !vqec_vision_ai_stor_efgal_get_string(_payload, offset, _snapshot.model_id_) ||
        !vqec_vision_ai_stor_efgal_get_string(_payload, offset, _snapshot.model_version_) ||
        !vqec_vision_ai_stor_efgal_get_u64(_payload, offset, _snapshot.preprocess_revision_) ||
        !vqec_vision_ai_stor_efgal_get_u64(_payload, offset, dimensions) ||
        !vqec_vision_ai_stor_efgal_get_u64(_payload, offset, template_count) ||
        dimensions != _config.dimensions_ || template_count > _config.capacity_ ||
        dimensions > std::numeric_limits<std::size_t>::max() ||
        template_count > face_gallery_limits::g_max_records) {
        return false;
    }
    _snapshot.dimensions_ = static_cast<std::size_t>(dimensions);
    _snapshot.templates_.clear();
    _snapshot.templates_.reserve(static_cast<std::size_t>(template_count));
    for (std::uint64_t index = 0; index < template_count; ++index) {
        face_gallery_template item;
        if (!vqec_vision_ai_stor_efgal_get_u64(_payload, offset, item.record_id_) ||
            !vqec_vision_ai_stor_efgal_get_string(_payload, offset, item.subject_ref_)) {
            return false;
        }
        if (_snapshot.dimensions_ > (_payload.size() - offset) / sizeof(float)) {
            return false;
        }
        item.values_.resize(_snapshot.dimensions_);
        for (float& value : item.values_) {
            std::uint32_t bits = 0;
            if (!vqec_vision_ai_stor_efgal_get_u32(_payload, offset, bits)) {
                return false;
            }
            std::memcpy(&value, &bits, sizeof(value));
        }
        _snapshot.templates_.push_back(std::move(item));
    }
    return offset == _payload.size();
}

status vqec_vision_ai_stor_efgal_validate_config(
    const encrypted_face_gallery_store_config& _config) {
    if (_config.directory_path_.empty() || _config.directory_path_.front() != '/' ||
        !vqec_vision_ai_stor_efgal_valid_name(_config.gallery_file_name_) ||
        !vqec_vision_ai_stor_efgal_valid_name(_config.key_file_name_) ||
        !vqec_vision_ai_stor_efgal_valid_name(_config.lock_file_name_) ||
        _config.max_serialized_bytes_ == 0 ||
        _config.max_serialized_bytes_ > g_max_store_bytes ||
        _config.gallery_file_name_ == _config.key_file_name_ ||
        _config.gallery_file_name_ == _config.lock_file_name_ ||
        _config.key_file_name_ == _config.lock_file_name_) {
        return {status_code::invalid_argument, "encrypted gallery store configuration is invalid"};
    }
    return {};
}

status vqec_vision_ai_stor_efgal_check_private_stat(const struct stat& _stat,
                                                    std::uint32_t _owner,
                                                    bool _directory) {
    const mode_t required = _directory ? S_IFDIR : S_IFREG;
    const mode_t actual_type = _stat.st_mode & S_IFMT;
    const mode_t allowed_mode = _directory ? g_private_directory_mode : g_private_file_mode;
    if (actual_type != required || static_cast<std::uint32_t>(_stat.st_uid) != _owner ||
        (_stat.st_mode & (S_IRWXG | S_IRWXO)) != 0 ||
        (_stat.st_mode & (S_IRWXU | S_IWUSR | S_IXUSR)) != allowed_mode) {
        return {status_code::unauthorized, "encrypted gallery storage permissions are unsafe"};
    }
    return {};
}

status vqec_vision_ai_stor_efgal_read_all(int _fd, std::size_t _max,
                                          std::vector<unsigned char>& _data) {
    _data.clear();
    std::array<unsigned char, 16384> block{};
    for (;;) {
        const auto count = ::read(_fd, block.data(), block.size());
        if (count < 0) {
            if (errno == EINTR) continue;
            return {status_code::io_error, "encrypted gallery read failed"};
        }
        if (count == 0) break;
        const auto bytes = static_cast<std::size_t>(count);
        if (bytes > _max - _data.size()) {
            return {status_code::resource_exhausted, "encrypted gallery exceeds byte limit"};
        }
        _data.insert(_data.end(), block.begin(), block.begin() + count);
    }
    return {};
}

status vqec_vision_ai_stor_efgal_load_key(int _dir_fd, const std::string& _name,
                                          std::uint32_t _owner,
                                          std::array<unsigned char, g_key_bytes>& _key,
                                          bool& _created) {
    _created = false;
    int fd = ::openat(_dir_fd, _name.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0 && errno == ENOENT) {
        fd = ::openat(_dir_fd, _name.c_str(), O_RDWR | O_CREAT | O_EXCL |
            O_CLOEXEC | O_NOFOLLOW, g_private_file_mode);
        if (fd < 0) return {status_code::io_error, "encrypted gallery key creation failed"};
        if (RAND_bytes(_key.data(), static_cast<int>(_key.size())) != 1) {
            ::close(fd); return {status_code::io_error, "encrypted gallery key generation failed"};
        }
        std::size_t written = 0;
        while (written < _key.size()) {
            const auto count = ::write(fd, _key.data() + written, _key.size() - written);
            if (count < 0 && errno == EINTR) continue;
            if (count <= 0) { ::close(fd); return {status_code::io_error, "encrypted gallery key write failed"}; }
            written += static_cast<std::size_t>(count);
        }
        if (::fsync(fd) != 0 || ::fsync(_dir_fd) != 0) {
            ::close(fd);
            return {status_code::io_error, "encrypted gallery key sync failed"};
        }
        _created = true;
    } else if (fd < 0) {
        return {status_code::io_error, "encrypted gallery key open failed"};
    }
    struct stat key_stat{};
    if (::fstat(fd, &key_stat) != 0) {
        ::close(fd);
        return {status_code::io_error, "encrypted gallery key stat failed"};
    }
    const auto safe = vqec_vision_ai_stor_efgal_check_private_stat(key_stat, _owner, false);
    if (safe.code_ != status_code::ok || key_stat.st_size != static_cast<off_t>(_key.size())) {
        ::close(fd); return safe.code_ == status_code::ok ?
            status{status_code::protocol_error, "encrypted gallery key size is invalid"} : safe;
    }
    if (::lseek(fd, 0, SEEK_SET) < 0) {
        ::close(fd);
        return {status_code::io_error, "encrypted gallery key seek failed"};
    }
    std::size_t read_bytes = 0;
    while (read_bytes < _key.size()) {
        const auto count = ::read(fd, _key.data() + read_bytes, _key.size() - read_bytes);
        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            ::close(fd);
            return {status_code::io_error, "encrypted gallery key read failed"};
        }
        read_bytes += static_cast<std::size_t>(count);
    }
    ::close(fd);
    return {};
}

status vqec_vision_ai_stor_efgal_decrypt(const std::vector<unsigned char>& _file,
                                         const std::array<unsigned char, g_key_bytes>& _key,
                                         std::size_t _max_payload,
                                         std::vector<unsigned char>& _payload) {
    if (_file.size() < g_header_bytes + g_tag_bytes ||
        !std::equal(g_magic.begin(), g_magic.end(), _file.begin())) {
        return {status_code::protocol_error, "encrypted gallery header is invalid"};
    }
    std::size_t offset = g_magic.size();
    std::uint32_t version = 0;
    std::uint64_t payload_size = 0;
    if (!vqec_vision_ai_stor_efgal_get_u32(_file, offset, version) ||
        !vqec_vision_ai_stor_efgal_get_u64(_file, offset, payload_size) ||
        version != g_format_version || payload_size > _max_payload ||
        payload_size != _file.size() - g_header_bytes - g_tag_bytes) {
        return {status_code::protocol_error, "encrypted gallery header bounds are invalid"};
    }
    const unsigned char* nonce = _file.data() + offset;
    offset += g_nonce_bytes;
    const unsigned char* tag = _file.data() + offset;
    offset += g_tag_bytes;
    const auto release = [](EVP_CIPHER_CTX* _ctx) { EVP_CIPHER_CTX_free(_ctx); };
    std::unique_ptr<EVP_CIPHER_CTX, decltype(release)> ctx(EVP_CIPHER_CTX_new(), release);
    if (!ctx || EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1 ||
        EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, g_nonce_bytes, nullptr) != 1 ||
        EVP_DecryptInit_ex(ctx.get(), nullptr, nullptr, _key.data(), nonce) != 1) {
        return {status_code::unsupported, "encrypted gallery decrypt initialization failed"};
    }
    int aad_len = 0;
    if (EVP_DecryptUpdate(ctx.get(), nullptr, &aad_len, _file.data(), g_header_bytes) != 1) {
        return {status_code::protocol_error, "encrypted gallery authentication failed"};
    }
    _payload.assign(static_cast<std::size_t>(payload_size), 0);
    int out_len = 0;
    if (payload_size != 0 && EVP_DecryptUpdate(ctx.get(), _payload.data(), &out_len,
            _file.data() + offset, static_cast<int>(payload_size)) != 1) {
        return {status_code::protocol_error, "encrypted gallery decrypt failed"};
    }
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, g_tag_bytes,
            const_cast<unsigned char*>(tag)) != 1 ||
        EVP_DecryptFinal_ex(ctx.get(), _payload.data() + out_len, &out_len) != 1) {
        return {status_code::unauthorized, "encrypted gallery authentication failed"};
    }
    return {};
}

status vqec_vision_ai_stor_efgal_encrypt(const std::vector<unsigned char>& _payload,
                                         const std::array<unsigned char, g_key_bytes>& _key,
                                         std::vector<unsigned char>& _file) {
    if (_payload.size() > std::numeric_limits<std::uint64_t>::max()) {
        return {status_code::resource_exhausted, "encrypted gallery payload is too large"};
    }
    std::array<unsigned char, g_nonce_bytes> nonce{};
    if (RAND_bytes(nonce.data(), static_cast<int>(nonce.size())) != 1) {
        return {status_code::io_error, "encrypted gallery nonce generation failed"};
    }
    std::vector<unsigned char> header;
    header.reserve(g_header_bytes);
    header.insert(header.end(), g_magic.begin(), g_magic.end());
    vqec_vision_ai_stor_efgal_put_u32(header, g_format_version);
    vqec_vision_ai_stor_efgal_put_u64(header, _payload.size());
    header.insert(header.end(), nonce.begin(), nonce.end());
    _file.assign(g_header_bytes + g_tag_bytes + _payload.size(), 0);
    std::copy(header.begin(), header.end(), _file.begin());
    const auto release = [](EVP_CIPHER_CTX* _ctx) { EVP_CIPHER_CTX_free(_ctx); };
    std::unique_ptr<EVP_CIPHER_CTX, decltype(release)> ctx(EVP_CIPHER_CTX_new(), release);
    if (!ctx || EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1 ||
        EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, g_nonce_bytes, nullptr) != 1 ||
        EVP_EncryptInit_ex(ctx.get(), nullptr, nullptr, _key.data(), nonce.data()) != 1) {
        return {status_code::unsupported, "encrypted gallery encrypt initialization failed"};
    }
    int aad_len = 0;
    if (EVP_EncryptUpdate(ctx.get(), nullptr, &aad_len, header.data(), g_header_bytes) != 1) {
        return {status_code::io_error, "encrypted gallery authentication setup failed"};
    }
    int out_len = 0;
    if (!_payload.empty() && EVP_EncryptUpdate(ctx.get(), _file.data() + g_header_bytes + g_tag_bytes,
            &out_len, _payload.data(), static_cast<int>(_payload.size())) != 1) {
        return {status_code::io_error, "encrypted gallery encryption failed"};
    }
    int final_len = 0;
    if (EVP_EncryptFinal_ex(ctx.get(), _file.data() + g_header_bytes + g_tag_bytes + out_len,
            &final_len) != 1 || EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG,
            g_tag_bytes, _file.data() + g_header_bytes) != 1) {
        return {status_code::io_error, "encrypted gallery tag generation failed"};
    }
    return {};
}

status vqec_vision_ai_stor_efgal_write_atomic(int _dir_fd, const std::string& _name,
                                              const std::vector<unsigned char>& _file) {
    std::array<unsigned char, 8> random_bytes{};
    if (RAND_bytes(random_bytes.data(), static_cast<int>(random_bytes.size())) != 1) {
        return {status_code::io_error, "encrypted gallery temporary name generation failed"};
    }
    std::string temporary = "." + _name + ".tmp-";
    const char* hex = "0123456789abcdef";
    for (const auto byte : random_bytes) {
        temporary.push_back(hex[byte >> 4]);
        temporary.push_back(hex[byte & 15U]);
    }
    int fd = ::openat(_dir_fd, temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL |
        O_CLOEXEC | O_NOFOLLOW, g_private_file_mode);
    if (fd < 0) {
        return {status_code::io_error, "encrypted gallery temporary file open failed"};
    }
    bool ok = true;
    std::size_t written = 0;
    while (written < _file.size()) {
        const auto count = ::write(fd, _file.data() + written, _file.size() - written);
        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            ok = false;
            break;
        }
        written += static_cast<std::size_t>(count);
    }
    if (ok && ::fsync(fd) != 0) {
        ok = false;
    }
    ::close(fd);
    if (!ok || ::renameat(_dir_fd, temporary.c_str(), _dir_fd, _name.c_str()) != 0 ||
        ::fsync(_dir_fd) != 0) {
        (void)::unlinkat(_dir_fd, temporary.c_str(), 0);
        return {status_code::io_error, "encrypted gallery atomic commit failed"};
    }
    return {};
}

status vqec_vision_ai_stor_efgal_open(const encrypted_face_gallery_store_config& _config,
                                      int& _dir_fd, int& _lock_fd,
                                      std::array<unsigned char, g_key_bytes>& _key,
                                      bool& _key_created) {
    _dir_fd = ::open(_config.directory_path_.c_str(),
        O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    if (_dir_fd < 0) {
        return {status_code::io_error, "encrypted gallery directory open failed"};
    }
    struct stat directory_stat{};
    if (::fstat(_dir_fd, &directory_stat) != 0) {
        ::close(_dir_fd);
        return {status_code::io_error, "encrypted gallery directory stat failed"};
    }
    auto checked = vqec_vision_ai_stor_efgal_check_private_stat(
        directory_stat, _config.expected_owner_uid_, true);
    if (checked.code_ != status_code::ok) {
        ::close(_dir_fd);
        return checked;
    }
    _lock_fd = ::openat(_dir_fd, _config.lock_file_name_.c_str(),
        O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW, g_private_file_mode);
    if (_lock_fd < 0 || ::flock(_lock_fd, LOCK_EX) != 0) {
        if (_lock_fd >= 0) {
            ::close(_lock_fd);
        }
        ::close(_dir_fd);
        return {status_code::io_error, "encrypted gallery lock failed"};
    }
    struct stat lock_stat{};
    if (::fstat(_lock_fd, &lock_stat) != 0) {
        vqec_vision_ai_stor_efgal_close(_dir_fd, _lock_fd);
        _dir_fd = -1;
        _lock_fd = -1;
        return {status_code::io_error, "encrypted gallery lock stat failed"};
    }
    checked = vqec_vision_ai_stor_efgal_check_private_stat(lock_stat, _config.expected_owner_uid_, false);
    if (checked.code_ != status_code::ok) {
        vqec_vision_ai_stor_efgal_close(_dir_fd, _lock_fd);
        _dir_fd = -1;
        _lock_fd = -1;
        return checked;
    }
    checked = vqec_vision_ai_stor_efgal_load_key(_dir_fd, _config.key_file_name_,
        _config.expected_owner_uid_, _key, _key_created);
    if (checked.code_ != status_code::ok) {
        vqec_vision_ai_stor_efgal_close(_dir_fd, _lock_fd);
        _dir_fd = -1;
        _lock_fd = -1;
        return checked;
    }
    return {};
}

void vqec_vision_ai_stor_efgal_close(int _dir_fd, int _lock_fd) noexcept {
    if (_lock_fd >= 0) { (void)::flock(_lock_fd, LOCK_UN); (void)::close(_lock_fd); }
    if (_dir_fd >= 0) (void)::close(_dir_fd);
}

}  // namespace

encrypted_face_gallery_store::encrypted_face_gallery_store(
    encrypted_face_gallery_store_config _config) : config_(std::move(_config)) {}

status encrypted_face_gallery_store::vqec_vision_ai_ports_fgstr_load(
    const face_gallery_config& _config, face_gallery_snapshot& _snapshot) {
    try {
        auto valid = vqec_vision_ai_stor_efgal_validate_config(config_);
        if (valid.code_ != status_code::ok) return valid;
        int dir_fd = -1;
        int lock_fd = -1;
        bool key_created = false;
        vqec_vision_ai_stor_efgal_key_owner key;
        valid = vqec_vision_ai_stor_efgal_open(
            config_, dir_fd, lock_fd, key.value_, key_created);
        if (valid.code_ != status_code::ok) return valid;
        vqec_vision_ai_stor_efgal_store_owner store_owner(dir_fd, lock_fd);
        const int opened_fd = ::openat(dir_fd, config_.gallery_file_name_.c_str(),
            O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
        if (opened_fd < 0 && errno == ENOENT) {
            if (!key_created) {
                return {status_code::protocol_error,
                    "encrypted gallery is missing while its key already exists"};
            }
            face_gallery_snapshot empty{face_gallery_limits::g_schema_version, 1, 1,
                _config.gallery_id_, _config.model_id_, _config.model_version_,
                _config.preprocess_revision_, _config.dimensions_, {}};
            valid = vqec_vision_ai_core_fgalr_validate_snapshot(_config, empty);
            if (valid.code_ == status_code::ok) {
                std::vector<unsigned char> payload;
                std::vector<unsigned char> file;
                if (!vqec_vision_ai_stor_efgal_serialize(empty, payload) ||
                    payload.size() > config_.max_serialized_bytes_) {
                    valid = {status_code::resource_exhausted,
                        "encrypted gallery serialization exceeds byte limit"};
                } else {
                    valid = vqec_vision_ai_stor_efgal_encrypt(
                        payload, key.value_, file);
                    if (valid.code_ == status_code::ok) {
                        valid = vqec_vision_ai_stor_efgal_write_atomic(
                            dir_fd, config_.gallery_file_name_, file);
                    }
                }
            }
            if (valid.code_ == status_code::ok) _snapshot = std::move(empty);
            return valid;
        }
        if (opened_fd < 0) {
            return {status_code::io_error, "encrypted gallery file open failed"};
        }
        vqec_vision_ai_stor_efgal_fd_owner file_owner(opened_fd);
        struct stat file_stat{};
        if (::fstat(opened_fd, &file_stat) != 0) {
            return {status_code::io_error, "encrypted gallery file stat failed"};
        }
        valid = vqec_vision_ai_stor_efgal_check_private_stat(
            file_stat, config_.expected_owner_uid_, false);
        std::vector<unsigned char> file;
        const auto max_file_bytes = config_.max_serialized_bytes_ +
            g_header_bytes + g_tag_bytes;
        if (valid.code_ == status_code::ok) {
            valid = vqec_vision_ai_stor_efgal_read_all(
                opened_fd, max_file_bytes, file);
        }
        std::vector<unsigned char> payload;
        if (valid.code_ == status_code::ok) {
            valid = vqec_vision_ai_stor_efgal_decrypt(
                file, key.value_, config_.max_serialized_bytes_, payload);
        }
        face_gallery_snapshot loaded;
        if (valid.code_ == status_code::ok &&
            (!vqec_vision_ai_stor_efgal_deserialize(payload, _config, loaded) ||
             vqec_vision_ai_core_fgalr_validate_snapshot(
                 _config, loaded).code_ != status_code::ok)) {
            valid = {status_code::protocol_error,
                "encrypted gallery snapshot is invalid"};
        }
        if (valid.code_ == status_code::ok) _snapshot = std::move(loaded);
        return valid;
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted,
            "encrypted gallery load allocation failed"};
    } catch (...) {
        return {status_code::io_error, "encrypted gallery load failed"};
    }
}

status encrypted_face_gallery_store::vqec_vision_ai_ports_fgstr_replace(
    const face_gallery_config& _config, std::uint64_t _expected_revision,
    const face_gallery_snapshot& _replacement) {
    try {
        auto valid = vqec_vision_ai_stor_efgal_validate_config(config_);
        if (valid.code_ != status_code::ok) return valid;
        valid = vqec_vision_ai_core_fgalr_validate_replacement(
            _config, _expected_revision, _replacement);
        if (valid.code_ != status_code::ok) return valid;
        int dir_fd = -1;
        int lock_fd = -1;
        bool key_created = false;
        vqec_vision_ai_stor_efgal_key_owner key;
        valid = vqec_vision_ai_stor_efgal_open(
            config_, dir_fd, lock_fd, key.value_, key_created);
        if (valid.code_ != status_code::ok) return valid;
        vqec_vision_ai_stor_efgal_store_owner store_owner(dir_fd, lock_fd);
        const int opened_fd = ::openat(dir_fd, config_.gallery_file_name_.c_str(),
            O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
        if (opened_fd < 0) {
            return errno == ENOENT
                ? status{status_code::invalid_state,
                      "encrypted gallery CAS base is missing"}
                : status{status_code::io_error,
                      "encrypted gallery file open failed"};
        }
        vqec_vision_ai_stor_efgal_fd_owner file_owner(opened_fd);
        struct stat file_stat{};
        std::vector<unsigned char> file;
        const auto max_file_bytes = config_.max_serialized_bytes_ +
            g_header_bytes + g_tag_bytes;
        if (::fstat(opened_fd, &file_stat) != 0) {
            valid = {status_code::io_error,
                "encrypted gallery CAS file stat failed"};
        } else {
            valid = vqec_vision_ai_stor_efgal_check_private_stat(
                file_stat, config_.expected_owner_uid_, false);
        }
        if (valid.code_ == status_code::ok) {
            valid = vqec_vision_ai_stor_efgal_read_all(
                opened_fd, max_file_bytes, file);
        }
        std::vector<unsigned char> payload;
        if (valid.code_ == status_code::ok) {
            valid = vqec_vision_ai_stor_efgal_decrypt(
                file, key.value_, config_.max_serialized_bytes_, payload);
        }
        face_gallery_snapshot current;
        if (valid.code_ == status_code::ok &&
            (!vqec_vision_ai_stor_efgal_deserialize(payload, _config, current) ||
             vqec_vision_ai_core_fgalr_validate_snapshot(
                 _config, current).code_ != status_code::ok)) {
            valid = {status_code::protocol_error,
                "encrypted gallery CAS base is invalid"};
        }
        if (valid.code_ == status_code::ok &&
            current.revision_ != _expected_revision) {
            valid = {status_code::invalid_state,
                "encrypted gallery CAS revision conflict"};
        }
        if (valid.code_ == status_code::ok) {
            std::vector<unsigned char> replacement_payload;
            std::vector<unsigned char> encrypted_file;
            if (!vqec_vision_ai_stor_efgal_serialize(
                    _replacement, replacement_payload) ||
                replacement_payload.size() > config_.max_serialized_bytes_) {
                valid = {status_code::resource_exhausted,
                    "encrypted gallery serialization exceeds byte limit"};
            } else {
                valid = vqec_vision_ai_stor_efgal_encrypt(
                    replacement_payload, key.value_, encrypted_file);
                if (valid.code_ == status_code::ok) {
                    valid = vqec_vision_ai_stor_efgal_write_atomic(
                        dir_fd, config_.gallery_file_name_, encrypted_file);
                }
            }
        }
        return valid;
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted,
            "encrypted gallery replace allocation failed"};
    } catch (...) {
        return {status_code::io_error, "encrypted gallery replace failed"};
    }
}

}  // namespace vqec::vision::ai
