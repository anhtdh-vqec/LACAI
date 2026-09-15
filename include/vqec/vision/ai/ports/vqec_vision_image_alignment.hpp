#ifndef VQEC_VISION_AI_PORTS_IMAGE_ALIGNMENT_HPP
#define VQEC_VISION_AI_PORTS_IMAGE_ALIGNMENT_HPP

#include <cstdint>

#include "vqec/vision/ai/contracts/vqec_vision_image_alignment.hpp"
#include "vqec/vision/ai/ports/vqec_vision_raw_source.hpp"

namespace vqec::vision::ai {

// Neutral landmark-based alignment/crop boundary for secondary (cascade) models. A backend
// that cannot honor a declared template fails activation; there is no silent CPU fallback.
//
// Ownership: the caller keeps the source frame owner alive until poll_completion reports
// complete. Timeout, stop request, source disconnect and FD close are not completion. The
// destination tensor in _result is owned by the caller.
class image_alignment_port {
public:
    virtual ~image_alignment_port() = default;

    // Reports what this backend supports. Performs no pixel work.
    [[nodiscard]] virtual status vqec_vision_ai_ports_imaln_probe_capabilities(
        alignment_capabilities& _capabilities) const = 0;

    // Fails closed when the template exceeds the probed capability.
    [[nodiscard]] virtual status vqec_vision_ai_ports_imaln_validate_template(
        const alignment_template& _template,
        const alignment_capabilities& _capabilities) const = 0;

    // Borrows _source and the request landmarks, writes the aligned destination into
    // _result, returns the transform and a completion ticket. Failure preserves _result.
    [[nodiscard]] virtual status vqec_vision_ai_ports_imaln_align(
        const alignment_request& _request, const raw_frame& _source,
        const alignment_template& _template, alignment_result& _result,
        std::uint64_t& _ticket) = 0;

    // Device completion for a ticket. _complete is true only after the device has finished
    // reading the source and writing the destination. An unknown ticket is invalid_state.
    [[nodiscard]] virtual status vqec_vision_ai_ports_imaln_poll_completion(
        std::uint64_t _ticket, bool& _complete) = 0;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_PORTS_IMAGE_ALIGNMENT_HPP
