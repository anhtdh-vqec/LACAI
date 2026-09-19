#include "vqec_vision_ed25519_app_package_verifier.hpp"

#include <fcntl.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <sys/stat.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstring>
#include <limits>
#include <memory>
#include <new>
#include <sstream>
#include <utility>

#include "vqec/vision/ai/contracts/vqec_vision_identifier.hpp"
#include "vqec_vision_app_manifest.hpp"
#include "vqec_vision_artifact_digest.hpp"

namespace vqec::vision::ai {
namespace {

using bio_owner = std::unique_ptr<BIO, decltype(&BIO_free)>;
using key_owner = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
using context_owner = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;

void vqec_vision_ai_secad_edver_append_u64_be(
    std::uint64_t _value, std::vector<std::uint8_t>& _payload) {
    constexpr std::size_t g_u64_bytes = 8;
    for (std::size_t index = 0; index < g_u64_bytes; ++index) {
        const auto shift = static_cast<unsigned int>((g_u64_bytes - index - 1U) * 8U);
        _payload.push_back(static_cast<std::uint8_t>((_value >> shift) & 0xffU));
    }
}

status vqec_vision_ai_secad_edver_verify_digest(
    const std::vector<std::uint8_t>& _payload, const std::string& _sha256) {
    if (_payload.empty() ||
        _payload.size() > app_lifecycle_limits::g_max_document_bytes ||
        !vqec_vision_ai_cntr_ident_is_sha256_hex(_sha256)) {
        return {status_code::invalid_argument, "invalid signed package artifact"};
    }
    const std::string document(_payload.begin(), _payload.end());
    std::istringstream stream(document);
    artifact_digest_receipt receipt;
    return vqec_vision_ai_mreg_ardgt_verify_stream(stream, _sha256,
        app_lifecycle_limits::g_max_document_bytes, receipt);
}

status vqec_vision_ai_secad_edver_load_public_key(
    const ed25519_app_package_verifier_config& _config,
    key_owner& _key) {
    if (_config.public_key_path_.empty() || _config.public_key_path_.front() != '/' ||
        !vqec_vision_ai_cntr_ident_is_valid(
            _config.key_id_, app_lifecycle_limits::g_max_identifier_bytes)) {
        return {status_code::invalid_argument, "invalid package trust configuration"};
    }
    const int fd = ::open(_config.public_key_path_.c_str(),
        O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) {
        return {status_code::io_error, "cannot open package public key"};
    }
    struct stat metadata {};
    if (::fstat(fd, &metadata) != 0 || !S_ISREG(metadata.st_mode) ||
        (metadata.st_mode & (S_IWGRP | S_IWOTH)) != 0 ||
        (metadata.st_uid != 0 && metadata.st_uid != ::geteuid()) ||
        metadata.st_size <= 0 ||
        static_cast<std::uint64_t>(metadata.st_size) >
            ed25519_app_package_limits::g_max_public_key_file_bytes) {
        (void)::close(fd);
        return {status_code::unauthorized, "package public key ownership or mode is invalid"};
    }
    std::vector<std::uint8_t> key_bytes;
    try {
        key_bytes.resize(static_cast<std::size_t>(metadata.st_size));
    } catch (const std::bad_alloc&) {
        (void)::close(fd);
        return {status_code::resource_exhausted, "package public key allocation failed"};
    }
    std::size_t offset = 0;
    while (offset < key_bytes.size()) {
        const auto count = ::read(fd, key_bytes.data() + offset,
            key_bytes.size() - offset);
        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            (void)::close(fd);
            return {status_code::io_error, "package public key read failed"};
        }
        offset += static_cast<std::size_t>(count);
    }
    (void)::close(fd);
    bio_owner bio(BIO_new_mem_buf(key_bytes.data(),
                      static_cast<int>(key_bytes.size())),
        &BIO_free);
    if (!bio) {
        return {status_code::resource_exhausted, "package public key BIO failed"};
    }
    _key.reset(PEM_read_bio_PUBKEY(bio.get(), nullptr, nullptr, nullptr));
    if (!_key || EVP_PKEY_is_a(_key.get(), "ED25519") != 1) {
        return {status_code::unauthorized, "package public key is not Ed25519"};
    }
    return {};
}

}  // namespace

status vqec_vision_ai_secad_edver_build_signing_payload(
    const std::vector<std::uint8_t>& _manifest_payload,
    const std::vector<std::uint8_t>& _configuration_payload,
    std::vector<std::uint8_t>& _signing_payload) {
    if (_manifest_payload.empty() || _configuration_payload.empty() ||
        _manifest_payload.size() > app_lifecycle_limits::g_max_document_bytes ||
        _configuration_payload.size() > app_lifecycle_limits::g_max_document_bytes) {
        return {status_code::invalid_argument, "signed package payload is invalid"};
    }
    const auto domain_bytes = std::strlen(ed25519_app_package_limits::g_signing_domain);
    constexpr std::size_t g_length_bytes = 8;
    if (domain_bytes > std::numeric_limits<std::size_t>::max() - 2U * g_length_bytes ||
        _manifest_payload.size() >
            std::numeric_limits<std::size_t>::max() - domain_bytes - 2U * g_length_bytes ||
        _configuration_payload.size() >
            std::numeric_limits<std::size_t>::max() - domain_bytes - 2U * g_length_bytes -
                _manifest_payload.size()) {
        return {status_code::resource_exhausted, "signed package payload size overflows"};
    }
    try {
        std::vector<std::uint8_t> payload;
        payload.reserve(domain_bytes + 2U * g_length_bytes + _manifest_payload.size() +
            _configuration_payload.size());
        payload.insert(payload.end(),
            ed25519_app_package_limits::g_signing_domain,
            ed25519_app_package_limits::g_signing_domain + domain_bytes);
        vqec_vision_ai_secad_edver_append_u64_be(
            static_cast<std::uint64_t>(_manifest_payload.size()), payload);
        payload.insert(payload.end(), _manifest_payload.begin(), _manifest_payload.end());
        vqec_vision_ai_secad_edver_append_u64_be(
            static_cast<std::uint64_t>(_configuration_payload.size()), payload);
        payload.insert(payload.end(), _configuration_payload.begin(),
            _configuration_payload.end());
        _signing_payload = std::move(payload);
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted, "signed package payload allocation failed"};
    }
    return {};
}

ed25519_app_package_verifier::ed25519_app_package_verifier(
    ed25519_app_package_verifier_config _config)
    : config_(std::move(_config)) {}

status ed25519_app_package_verifier::vqec_vision_ai_ports_apver_verify(
    const app_package_candidate& _candidate,
    verified_app_package& _package) const {
    _package = {};
    if (_candidate.signature_payload_.size() !=
        ed25519_app_package_limits::g_signature_bytes) {
        return {status_code::unauthorized, "package signature size is invalid"};
    }
    auto current = vqec_vision_ai_secad_edver_verify_digest(
        _candidate.manifest_payload_, _candidate.manifest_sha256_);
    if (current.code_ != status_code::ok) {
        return current;
    }
    current = vqec_vision_ai_secad_edver_verify_digest(
        _candidate.configuration_payload_, _candidate.configuration_sha256_);
    if (current.code_ != status_code::ok) {
        return current;
    }
    key_owner key(nullptr, &EVP_PKEY_free);
    current = vqec_vision_ai_secad_edver_load_public_key(config_, key);
    if (current.code_ != status_code::ok) {
        return current;
    }
    std::vector<std::uint8_t> signing_payload;
    current = vqec_vision_ai_secad_edver_build_signing_payload(
        _candidate.manifest_payload_, _candidate.configuration_payload_, signing_payload);
    if (current.code_ != status_code::ok) {
        return current;
    }
    context_owner context(EVP_MD_CTX_new(), &EVP_MD_CTX_free);
    if (!context || EVP_DigestVerifyInit(
            context.get(), nullptr, nullptr, nullptr, key.get()) != 1) {
        return {status_code::invalid_state, "cannot initialize package signature verifier"};
    }
    if (EVP_DigestVerify(context.get(), _candidate.signature_payload_.data(),
            _candidate.signature_payload_.size(), signing_payload.data(),
            signing_payload.size()) != 1) {
        return {status_code::unauthorized, "package signature verification failed"};
    }
    const std::string manifest_document(
        _candidate.manifest_payload_.begin(), _candidate.manifest_payload_.end());
    std::istringstream manifest_stream(manifest_document);
    usecase_app_manifest manifest;
    current = vqec_vision_ai_lifec_apmft_load(manifest_stream, manifest);
    if (current.code_ != status_code::ok) {
        return current;
    }
    verified_app_package verified;
    verified.manifest_ = std::move(manifest);
    verified.manifest_sha256_ = _candidate.manifest_sha256_;
    verified.configuration_payload_ = _candidate.configuration_payload_;
    verified.configuration_sha256_ = _candidate.configuration_sha256_;
    verified.verification_receipt_id_ =
        config_.key_id_ + "." + _candidate.manifest_sha256_;
    _package = std::move(verified);
    return {};
}

}  // namespace vqec::vision::ai
