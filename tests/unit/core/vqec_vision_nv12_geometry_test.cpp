#include <iostream>

#include "vqec/vision/ai/contracts/base/vqec_vision_identifier.hpp"
#include "vqec/vision/ai/contracts/media/vqec_vision_nv12_geometry.hpp"

// Independent expectations for the shared helpers. If either rule changes, this test must
// be updated deliberately rather than silently drifting with a caller.
static_assert(vqec::vision::ai::vqec_vision_ai_cntr_nvgeo_packed_bytes(1920, 1080) == 3110400ULL);
static_assert(vqec::vision::ai::vqec_vision_ai_cntr_nvgeo_packed_bytes(2, 2) == 6ULL);
static_assert(vqec::vision::ai::vqec_vision_ai_cntr_nvgeo_packed_bytes(3, 2) == 0ULL);
static_assert(vqec::vision::ai::vqec_vision_ai_cntr_nvgeo_packed_bytes(0, 0) == 0ULL);

int main() {
    using namespace vqec::vision::ai;
    unsigned failures = 0;
    const auto check = [&failures](bool _condition, const char* _what) {
        if (!_condition) {
            ++failures;
            std::cerr << "failed: " << _what << '\n';
        }
    };
    check(vqec_vision_ai_cntr_nvgeo_is_even_nonzero(1920, 1080), "even nonzero accepted");
    check(!vqec_vision_ai_cntr_nvgeo_is_even_nonzero(0, 1080), "zero width rejected");
    check(!vqec_vision_ai_cntr_nvgeo_is_even_nonzero(1920, 0), "zero height rejected");
    check(!vqec_vision_ai_cntr_nvgeo_is_even_nonzero(1921, 1080), "odd width rejected");
    check(!vqec_vision_ai_cntr_nvgeo_is_even_nonzero(1920, 1081), "odd height rejected");

    check(vqec_vision_ai_cntr_ident_is_sha256_hex(
              "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"),
        "lowercase 64-char digest accepted");
    check(!vqec_vision_ai_cntr_ident_is_sha256_hex(
              "0123456789ABCDEF0123456789abcdef0123456789abcdef0123456789abcdef"),
        "uppercase hex rejected");
    check(!vqec_vision_ai_cntr_ident_is_sha256_hex(
              "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcde"),
        "short digest rejected");
    check(!vqec_vision_ai_cntr_ident_is_sha256_hex(
              "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdeg"),
        "non-hex character rejected");
    std::cout << "nv12 geometry failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
