#include "vqec_vision_ed25519_verifier.hpp"

#include <fcntl.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <memory>
#include <new>

#include "vqec/vision/ai/contracts/lifecycle/vqec_vision_app_lifecycle.hpp"
#include "vqec/vision/ai/contracts/base/vqec_vision_identifier.hpp"

namespace vqec::vision::ai {
namespace {

using bio_owner = std::unique_ptr<BIO, decltype(&BIO_free)>;
using key_owner = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
using context_owner = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;

status vqec_vision_ai_secad_edsig_load_key(
    const ed25519_verifier_config& _config, key_owner& _key) {
    if (_config.public_key_path_.empty() || _config.public_key_path_.front() != '/' ||
        !vqec_vision_ai_cntr_ident_is_valid(
            _config.key_id_, app_lifecycle_limits::g_max_identifier_bytes)) {
        return {status_code::invalid_argument, "invalid Ed25519 trust configuration"};
    }
    const int fd = ::open(_config.public_key_path_.c_str(),
        O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) {
        return {status_code::io_error, "cannot open Ed25519 public key"};
    }
    struct stat metadata {};
    if (::fstat(fd, &metadata) != 0 || !S_ISREG(metadata.st_mode) ||
        (metadata.st_mode & (S_IWGRP | S_IWOTH)) != 0 ||
        (metadata.st_uid != 0 && metadata.st_uid != ::geteuid()) ||
        metadata.st_size <= 0 ||
        static_cast<std::uint64_t>(metadata.st_size) >
            ed25519_verifier_limits::g_max_public_key_file_bytes) {
        (void)::close(fd);
        return {status_code::unauthorized,
            "Ed25519 public key ownership or mode is invalid"};
    }
    std::vector<std::uint8_t> bytes;
    try {
        bytes.resize(static_cast<std::size_t>(metadata.st_size));
    } catch (const std::bad_alloc&) {
        (void)::close(fd);
        return {status_code::resource_exhausted,
            "Ed25519 public key allocation failed"};
    }
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const auto count = ::read(fd, bytes.data() + offset, bytes.size() - offset);
        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            (void)::close(fd);
            return {status_code::io_error, "Ed25519 public key read failed"};
        }
        offset += static_cast<std::size_t>(count);
    }
    (void)::close(fd);
    bio_owner bio(BIO_new_mem_buf(bytes.data(), static_cast<int>(bytes.size())), &BIO_free);
    if (!bio) {
        return {status_code::resource_exhausted, "Ed25519 public key BIO failed"};
    }
    _key.reset(PEM_read_bio_PUBKEY(bio.get(), nullptr, nullptr, nullptr));
    if (!_key || EVP_PKEY_is_a(_key.get(), "ED25519") != 1) {
        return {status_code::unauthorized, "public key is not Ed25519"};
    }
    return {};
}

}  // namespace

status vqec_vision_ai_secad_edsig_verify(
    const ed25519_verifier_config& _config,
    const std::vector<std::uint8_t>& _message,
    const std::vector<std::uint8_t>& _signature) {
    if (_message.empty() || _signature.size() != ed25519_verifier_limits::g_signature_bytes) {
        return {status_code::unauthorized, "Ed25519 signed payload is invalid"};
    }
    key_owner key(nullptr, &EVP_PKEY_free);
    auto current = vqec_vision_ai_secad_edsig_load_key(_config, key);
    if (current.code_ != status_code::ok) {
        return current;
    }
    context_owner context(EVP_MD_CTX_new(), &EVP_MD_CTX_free);
    if (!context || EVP_DigestVerifyInit(
            context.get(), nullptr, nullptr, nullptr, key.get()) != 1) {
        return {status_code::invalid_state,
            "cannot initialize Ed25519 signature verifier"};
    }
    if (EVP_DigestVerify(context.get(), _signature.data(), _signature.size(),
            _message.data(), _message.size()) != 1) {
        return {status_code::unauthorized, "Ed25519 signature verification failed"};
    }
    return {};
}

}  // namespace vqec::vision::ai
