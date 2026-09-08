#include "vqec_vision_artifact_digest.hpp"

#include <algorithm>
#include <array>
#include <memory>
#include <utility>

#include <openssl/evp.h>

namespace vqec::vision::ai {

status vqec_vision_ai_mreg_ardgt_verify_stream(std::istream& _stream,
                                               const std::string& _expected_sha256,
                                               std::uint64_t _max_bytes,
                                               artifact_digest_receipt& _receipt) {
    if (_expected_sha256.size() != 64 ||
        _expected_sha256.find_first_not_of("0123456789abcdef") != std::string::npos ||
        _max_bytes == 0 || _max_bytes > 4ULL * 1024 * 1024 * 1024) {
        return {status_code::invalid_argument, "invalid SHA-256 or artifact size bound"};
    }
    if (!_stream.good()) {
        return {status_code::io_error, "artifact stream is not readable"};
    }
    const auto release_context = [](EVP_MD_CTX* _context) { EVP_MD_CTX_free(_context); };
    std::unique_ptr<EVP_MD_CTX, decltype(release_context)> context(EVP_MD_CTX_new(),
                                                                   release_context);
    if (!context) {
        return {status_code::resource_exhausted, "cannot allocate digest context"};
    }
    if (EVP_DigestInit_ex(context.get(), EVP_sha256(), nullptr) != 1) {
        return {status_code::unsupported, "SHA-256 provider initialization failed"};
    }
    std::array<char, 65536> block{};
    std::uint64_t total = 0;
    for (;;) {
        const auto requested = static_cast<std::streamsize>(
            std::min<std::uint64_t>(block.size(), _max_bytes - total + 1));
        try {
            _stream.read(block.data(), requested);
        } catch (const std::ios_base::failure&) {
            // A short read can set failbit/eofbit and throw after delivering bytes.
            if (!_stream.eof() || _stream.bad()) {
                return {status_code::io_error, "artifact read failed"};
            }
        }
        if (_stream.bad() || (_stream.fail() && !_stream.eof())) {
            return {status_code::io_error, "artifact stream failed before EOF"};
        }
        const auto count = _stream.gcount();
        if (count < 0 || count > requested) {
            return {status_code::io_error, "invalid artifact read count"};
        }
        const auto bytes = static_cast<std::uint64_t>(count);
        if (bytes > _max_bytes - total) {
            return {status_code::resource_exhausted, "artifact exceeds byte limit"};
        }
        if (bytes != 0 &&
            EVP_DigestUpdate(context.get(), block.data(), static_cast<std::size_t>(bytes)) != 1) {
            return {status_code::io_error, "SHA-256 update failed"};
        }
        total += bytes;
        if (_stream.eof()) {
            break;
        }
        if (count == 0) {
            return {status_code::io_error, "artifact read made no progress"};
        }
    }
    if (total == 0) {
        return {status_code::invalid_argument, "empty model artifact"};
    }
    std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
    unsigned digest_size = 0;
    if (EVP_DigestFinal_ex(context.get(), digest.data(), &digest_size) != 1 || digest_size != 32) {
        return {status_code::io_error, "SHA-256 finalization failed"};
    }
    const char* hex = "0123456789abcdef";
    std::string actual(64, '0');
    for (std::size_t index = 0; index < 32; ++index) {
        actual[index * 2] = hex[digest[index] >> 4];
        actual[index * 2 + 1] = hex[digest[index] & 15];
    }
    // Digest is public integrity metadata, not a secret authentication token.
    if (actual != _expected_sha256) {
        return {status_code::protocol_error, "artifact SHA-256 mismatch"};
    }
    artifact_digest_receipt candidate{std::move(actual), total};
    _receipt = std::move(candidate);
    return {};
}

} // namespace vqec::vision::ai
