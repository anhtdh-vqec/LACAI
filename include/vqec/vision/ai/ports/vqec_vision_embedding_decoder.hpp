#ifndef VQEC_VISION_AI_PORTS_EMBEDDING_DECODER_HPP
#define VQEC_VISION_AI_PORTS_EMBEDDING_DECODER_HPP

#include <cstdint>

#include "vqec/vision/ai/contracts/vqec_vision_embedding.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_model_outputs.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_preview_contract.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_status.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_tensor_result.hpp"

namespace vqec::vision::ai {

// Neutral secondary embedding boundary. It turns one model tensor result into a typed,
// L2-normalized embedding bound to an exact source frame and track. It makes no matching,
// threshold or identity decision and owns no index. Embeddings are sensitive and must not be
// logged or serialized implicitly.
class embedding_decoder_port {
public:
    virtual ~embedding_decoder_port() = default;

    // Validates the declared output identity and dimension against the package manifest.
    [[nodiscard]] virtual status vqec_vision_ai_ports_embdec_validate(
        const model_outputs& _outputs) const = 0;

    // Decodes one tensor result. Failure preserves _embedding. A missing tensor, mismatched
    // dimension, non-finite value or near-zero norm is rejected.
    [[nodiscard]] virtual status vqec_vision_ai_ports_embdec_decode(
        const tensor_result& _result, const preview_frame_key& _frame,
        std::uint64_t _track_id, embedding_result& _embedding) = 0;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_PORTS_EMBEDDING_DECODER_HPP
