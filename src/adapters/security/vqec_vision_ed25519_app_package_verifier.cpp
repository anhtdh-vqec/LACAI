#include "vqec_vision_ed25519_app_package_verifier.hpp"

#include <cstring>
#include <limits>
#include <new>
#include <sstream>
#include <utility>

#include "vqec/vision/ai/contracts/base/vqec_vision_identifier.hpp"
#include "vqec_vision_app_manifest.hpp"
#include "vqec_vision_artifact_digest.hpp"

namespace vqec::vision::ai {
namespace {

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
    std::vector<std::uint8_t> signing_payload;
    current = vqec_vision_ai_secad_edver_build_signing_payload(
        _candidate.manifest_payload_, _candidate.configuration_payload_, signing_payload);
    if (current.code_ != status_code::ok) {
        return current;
    }
    current = vqec_vision_ai_secad_edsig_verify(
        {config_.public_key_path_, config_.key_id_}, signing_payload,
        _candidate.signature_payload_);
    if (current.code_ != status_code::ok) {
        return current;
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
