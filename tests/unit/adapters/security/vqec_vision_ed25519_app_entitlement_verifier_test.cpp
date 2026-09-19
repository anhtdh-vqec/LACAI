#include "vqec_vision_ed25519_app_entitlement_verifier.hpp"

#include <openssl/evp.h>
#include <openssl/pem.h>

#include <array>
#include <cassert>
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

std::vector<std::uint8_t> vqec_vision_ai_unit_edetst_read(const std::string& _path) {
    std::ifstream input(_path, std::ios::binary);
    assert(input.is_open());
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

std::string vqec_vision_ai_unit_edetst_sha256(
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

key_owner vqec_vision_ai_unit_edetst_generate_key() {
    context_owner context(EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, nullptr),
        &EVP_PKEY_CTX_free);
    assert(context != nullptr && EVP_PKEY_keygen_init(context.get()) == 1);
    EVP_PKEY* raw_key = nullptr;
    assert(EVP_PKEY_keygen(context.get(), &raw_key) == 1);
    return key_owner(raw_key, &EVP_PKEY_free);
}

std::vector<std::uint8_t> vqec_vision_ai_unit_edetst_sign(
    EVP_PKEY* _key, const std::vector<std::uint8_t>& _payload) {
    digest_owner context(EVP_MD_CTX_new(), &EVP_MD_CTX_free);
    assert(context != nullptr);
    assert(EVP_DigestSignInit(context.get(), nullptr, nullptr, nullptr, _key) == 1);
    std::size_t bytes = 0;
    assert(EVP_DigestSign(context.get(), nullptr, &bytes,
               _payload.data(), _payload.size()) == 1);
    std::vector<std::uint8_t> signature(bytes);
    assert(EVP_DigestSign(context.get(), signature.data(), &bytes,
               _payload.data(), _payload.size()) == 1);
    signature.resize(bytes);
    return signature;
}

}  // namespace

int main() {
    const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto root = std::filesystem::temp_directory_path() /
        ("vqec-ed25519-entitlement-" + std::to_string(::getpid()) + "-" +
            std::to_string(nonce));
    std::filesystem::create_directories(root);
    const auto key_path = root / "trusted-ed25519.pem";
    auto key = vqec_vision_ai_unit_edetst_generate_key();
    FILE* file = std::fopen(key_path.c_str(), "wb");
    assert(file != nullptr && PEM_write_PUBKEY(file, key.get()) == 1);
    assert(std::fclose(file) == 0);
    assert(::chmod(key_path.c_str(), S_IRUSR | S_IWUSR) == 0);

    app_entitlement_candidate candidate;
    candidate.grant_payload_ = vqec_vision_ai_unit_edetst_read(
        VQEC_VISION_AI_APP_ENTITLEMENT_FIXTURE);
    candidate.grant_sha256_ =
        vqec_vision_ai_unit_edetst_sha256(candidate.grant_payload_);
    std::vector<std::uint8_t> signing_payload;
    assert(vqec_vision_ai_secad_edent_build_signing_payload(
               candidate.grant_payload_, signing_payload).code_ == status_code::ok);
    candidate.signature_payload_ =
        vqec_vision_ai_unit_edetst_sign(key.get(), signing_payload);

    ed25519_app_entitlement_verifier verifier(
        {key_path.string(), "vqec_product_signing_key"});
    verified_app_entitlement entitlement;
    assert(verifier.vqec_vision_ai_ports_entvr_verify(candidate, entitlement).code_ ==
        status_code::ok);
    assert(entitlement.grant_.app_id_ == "security.fire_smoke_detection");
    assert(entitlement.grant_.device_id_ == "09c89b1858f54955a3d13f2767622448");

    auto bad_signature = candidate;
    bad_signature.signature_payload_.front() ^= 1U;
    assert(verifier.vqec_vision_ai_ports_entvr_verify(
               bad_signature, entitlement).code_ == status_code::unauthorized);
    auto tampered = candidate;
    tampered.grant_payload_.back() ^= 1U;
    assert(verifier.vqec_vision_ai_ports_entvr_verify(
               tampered, entitlement).code_ != status_code::ok);
    ed25519_app_entitlement_verifier wrong_key_id(
        {key_path.string(), "another_key"});
    assert(wrong_key_id.vqec_vision_ai_ports_entvr_verify(
               candidate, entitlement).code_ == status_code::unauthorized);
    std::filesystem::remove_all(root);
    return 0;
}
