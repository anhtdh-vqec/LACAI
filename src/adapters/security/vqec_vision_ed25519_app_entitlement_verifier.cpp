#include "vqec_vision_ed25519_app_entitlement_verifier.hpp"

#include <cstring>
#include <limits>
#include <new>
#include <sstream>
#include <utility>

#include "vqec/vision/ai/contracts/vqec_vision_identifier.hpp"
#include "vqec_vision_app_entitlement.hpp"
#include "vqec_vision_artifact_digest.hpp"

namespace vqec::vision::ai {
namespace {

void vqec_vision_ai_secad_edent_append_u64_be(
    std::uint64_t _value, std::vector<std::uint8_t>& _payload) {
    constexpr std::size_t g_u64_bytes = 8;
    for (std::size_t index = 0; index < g_u64_bytes; ++index) {
        const auto shift = static_cast<unsigned int>((g_u64_bytes - index - 1U) * 8U);
        _payload.push_back(static_cast<std::uint8_t>((_value >> shift) & 0xffU));
    }
}

}  // namespace

status vqec_vision_ai_secad_edent_build_signing_payload(
    const std::vector<std::uint8_t>& _grant_payload,
    std::vector<std::uint8_t>& _signing_payload) {
    if (_grant_payload.empty() ||
        _grant_payload.size() > app_lifecycle_limits::g_max_document_bytes) {
        return {status_code::invalid_argument, "signed entitlement payload is invalid"};
    }
    const auto domain_bytes = std::strlen(ed25519_app_entitlement_limits::g_signing_domain);
    constexpr std::size_t g_length_bytes = 8;
    if (domain_bytes > std::numeric_limits<std::size_t>::max() - g_length_bytes ||
        _grant_payload.size() > std::numeric_limits<std::size_t>::max() -
            domain_bytes - g_length_bytes) {
        return {status_code::resource_exhausted,
            "signed entitlement payload size overflows"};
    }
    try {
        std::vector<std::uint8_t> payload;
        payload.reserve(domain_bytes + g_length_bytes + _grant_payload.size());
        payload.insert(payload.end(), ed25519_app_entitlement_limits::g_signing_domain,
            ed25519_app_entitlement_limits::g_signing_domain + domain_bytes);
        vqec_vision_ai_secad_edent_append_u64_be(
            static_cast<std::uint64_t>(_grant_payload.size()), payload);
        payload.insert(payload.end(), _grant_payload.begin(), _grant_payload.end());
        _signing_payload = std::move(payload);
        return {};
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted,
            "signed entitlement payload allocation failed"};
    }
}

ed25519_app_entitlement_verifier::ed25519_app_entitlement_verifier(
    ed25519_app_entitlement_verifier_config _config)
    : config_(std::move(_config)) {}

status ed25519_app_entitlement_verifier::vqec_vision_ai_ports_entvr_verify(
    const app_entitlement_candidate& _candidate,
    verified_app_entitlement& _entitlement) const {
    _entitlement = {};
    if (!vqec_vision_ai_cntr_ident_is_sha256_hex(_candidate.grant_sha256_) ||
        _candidate.grant_payload_.empty() ||
        _candidate.grant_payload_.size() > app_lifecycle_limits::g_max_document_bytes ||
        _candidate.signature_payload_.size() !=
            ed25519_app_entitlement_limits::g_signature_bytes) {
        return {status_code::unauthorized, "entitlement candidate is invalid"};
    }
    const std::string document(
        _candidate.grant_payload_.begin(), _candidate.grant_payload_.end());
    std::istringstream digest_stream(document);
    artifact_digest_receipt digest_receipt;
    auto current = vqec_vision_ai_mreg_ardgt_verify_stream(digest_stream,
        _candidate.grant_sha256_, app_lifecycle_limits::g_max_document_bytes,
        digest_receipt);
    if (current.code_ != status_code::ok) {
        return current;
    }
    std::vector<std::uint8_t> signing_payload;
    current = vqec_vision_ai_secad_edent_build_signing_payload(
        _candidate.grant_payload_, signing_payload);
    if (current.code_ != status_code::ok) {
        return current;
    }
    current = vqec_vision_ai_secad_edsig_verify(
        {config_.public_key_path_, config_.key_id_}, signing_payload,
        _candidate.signature_payload_);
    if (current.code_ != status_code::ok) {
        return current;
    }
    std::istringstream grant_stream(document);
    app_entitlement_grant grant;
    current = vqec_vision_ai_lifec_apent_load(grant_stream, grant);
    if (current.code_ != status_code::ok) {
        return current;
    }
    if (grant.key_id_ != config_.key_id_) {
        return {status_code::unauthorized,
            "entitlement key identifier does not match trust configuration"};
    }
    verified_app_entitlement verified;
    verified.grant_ = std::move(grant);
    verified.grant_sha256_ = _candidate.grant_sha256_;
    verified.verification_receipt_id_ =
        config_.key_id_ + "." + _candidate.grant_sha256_;
    _entitlement = std::move(verified);
    return {};
}

}  // namespace vqec::vision::ai
