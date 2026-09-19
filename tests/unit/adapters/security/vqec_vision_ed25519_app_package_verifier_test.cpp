#include "vqec_vision_ed25519_app_package_verifier.hpp"

#include <openssl/evp.h>
#include <openssl/pem.h>

#include <cassert>
#include <array>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace {

using namespace vqec::vision::ai;
using context_owner = std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)>;
using key_owner = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
using digest_owner = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;

std::vector<std::uint8_t> vqec_vision_ai_unit_edvtst_read(
    const std::string& _path) {
    std::ifstream input(_path, std::ios::binary);
    assert(input.is_open());
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

std::string vqec_vision_ai_unit_edvtst_sha256(
    const std::vector<std::uint8_t>& _payload) {
    std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
    unsigned int digest_bytes = 0;
    digest_owner context(EVP_MD_CTX_new(), &EVP_MD_CTX_free);
    assert(context != nullptr);
    assert(EVP_DigestInit_ex(context.get(), EVP_sha256(), nullptr) == 1);
    assert(EVP_DigestUpdate(context.get(), _payload.data(), _payload.size()) == 1);
    assert(EVP_DigestFinal_ex(context.get(), digest.data(), &digest_bytes) == 1);
    std::ostringstream encoded;
    encoded << std::hex << std::setfill('0');
    for (unsigned int index = 0; index < digest_bytes; ++index) {
        encoded << std::setw(2) << static_cast<unsigned int>(digest[index]);
    }
    return encoded.str();
}

key_owner vqec_vision_ai_unit_edvtst_generate_key() {
    context_owner context(EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, nullptr),
        &EVP_PKEY_CTX_free);
    assert(context != nullptr && EVP_PKEY_keygen_init(context.get()) == 1);
    EVP_PKEY* raw_key = nullptr;
    assert(EVP_PKEY_keygen(context.get(), &raw_key) == 1);
    return key_owner(raw_key, &EVP_PKEY_free);
}

void vqec_vision_ai_unit_edvtst_write_public_key(
    const std::filesystem::path& _path, EVP_PKEY* _key) {
    FILE* file = std::fopen(_path.c_str(), "wb");
    assert(file != nullptr);
    assert(PEM_write_PUBKEY(file, _key) == 1);
    assert(std::fclose(file) == 0);
    assert(::chmod(_path.c_str(), S_IRUSR | S_IWUSR) == 0);
}

std::vector<std::uint8_t> vqec_vision_ai_unit_edvtst_sign(
    EVP_PKEY* _key, const std::vector<std::uint8_t>& _payload) {
    digest_owner context(EVP_MD_CTX_new(), &EVP_MD_CTX_free);
    assert(context != nullptr);
    assert(EVP_DigestSignInit(context.get(), nullptr, nullptr, nullptr, _key) == 1);
    std::size_t signature_bytes = 0;
    assert(EVP_DigestSign(context.get(), nullptr, &signature_bytes,
               _payload.data(), _payload.size()) == 1);
    std::vector<std::uint8_t> signature(signature_bytes);
    assert(EVP_DigestSign(context.get(), signature.data(), &signature_bytes,
               _payload.data(), _payload.size()) == 1);
    signature.resize(signature_bytes);
    return signature;
}

}  // namespace

int main() {
    using namespace vqec::vision::ai;
    const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto root = std::filesystem::temp_directory_path() /
        ("vqec-ed25519-package-" + std::to_string(::getpid()) + "-" +
            std::to_string(nonce));
    std::filesystem::create_directories(root);
    const auto key_path = root / "trusted-ed25519.pem";
    auto key = vqec_vision_ai_unit_edvtst_generate_key();
    vqec_vision_ai_unit_edvtst_write_public_key(key_path, key.get());

    app_package_candidate candidate;
    candidate.manifest_payload_ = vqec_vision_ai_unit_edvtst_read(
        VQEC_VISION_AI_APP_MANIFEST_FIXTURE);
    candidate.configuration_payload_ = vqec_vision_ai_unit_edvtst_read(
        VQEC_VISION_AI_FIRE_SMOKE_CONFIG_FIXTURE);
    candidate.manifest_sha256_ =
        vqec_vision_ai_unit_edvtst_sha256(candidate.manifest_payload_);
    candidate.configuration_sha256_ =
        vqec_vision_ai_unit_edvtst_sha256(candidate.configuration_payload_);
    std::vector<std::uint8_t> signing_payload;
    assert(vqec_vision_ai_secad_edver_build_signing_payload(
               candidate.manifest_payload_, candidate.configuration_payload_,
               signing_payload).code_ == status_code::ok);
    candidate.signature_payload_ =
        vqec_vision_ai_unit_edvtst_sign(key.get(), signing_payload);
    assert(candidate.signature_payload_.size() ==
        ed25519_app_package_limits::g_signature_bytes);

    ed25519_app_package_verifier verifier({key_path.string(), "release.primary"});
    verified_app_package verified;
    assert(verifier.vqec_vision_ai_ports_apver_verify(candidate, verified).code_ ==
        status_code::ok);
    assert(verified.manifest_.app_id_ == "security.fire_smoke_detection");
    assert(verified.verification_receipt_id_ ==
        "release.primary." + candidate.manifest_sha256_);

    auto bad_signature = candidate;
    bad_signature.signature_payload_.front() ^= 0x01U;
    assert(verifier.vqec_vision_ai_ports_apver_verify(
               bad_signature, verified).code_ == status_code::unauthorized);
    auto tampered = candidate;
    tampered.configuration_payload_.back() ^= 0x01U;
    assert(verifier.vqec_vision_ai_ports_apver_verify(
               tampered, verified).code_ != status_code::ok);
    assert(::chmod(key_path.c_str(), S_IRUSR | S_IWUSR | S_IWGRP) == 0);
    assert(verifier.vqec_vision_ai_ports_apver_verify(
               candidate, verified).code_ == status_code::unauthorized);
    std::filesystem::remove_all(root);
    return 0;
}
