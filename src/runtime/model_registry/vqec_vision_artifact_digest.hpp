#ifndef VQEC_VISION_AI_MODEL_REGISTRY_ARTIFACT_DIGEST_HPP
#define VQEC_VISION_AI_MODEL_REGISTRY_ARTIFACT_DIGEST_HPP

#include <cstdint>
#include <istream>
#include <string>

#include "vqec/vision/ai/contracts/vqec_vision_status.hpp"

namespace vqec::vision::ai {

struct artifact_digest_receipt {
    std::string sha256_;
    std::uint64_t bytes_{0};
};

// Consumes caller stream from current position, <=limit+1 bytes. Blocking startup I/O.
// Digest must be 64 lowercase hex chars, bound in 1..4 GiB; empty artifacts rejected.
// Error preserves receipt; allocation failures propagate. Hash match is NOT authentication.
// Caller guarantees trusted expected digest and immutable association to the loaded artifact.
[[nodiscard]] status vqec_vision_ai_mreg_ardgt_verify_stream(
    std::istream& _stream, const std::string& _expected_sha256, std::uint64_t _max_bytes,
    artifact_digest_receipt& _receipt);

// Hashes an already-opened descriptor starting at its current offset, so the digest is
// bound to the pinned inode rather than a re-resolved path. The caller owns and opens
// _fd (e.g. O_RDONLY|O_CLOEXEC|O_NOFOLLOW); this function does not close it. Same
// expected-digest/limit rules as verify_stream; a hash match is still not authentication.
[[nodiscard]] status vqec_vision_ai_mreg_ardgt_verify_fd(
    int _fd, const std::string& _expected_sha256, std::uint64_t _max_bytes,
    artifact_digest_receipt& _receipt);

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_MODEL_REGISTRY_ARTIFACT_DIGEST_HPP
