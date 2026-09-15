#ifndef VQEC_VISION_AI_APPL_CASCADE_COORDINATOR_HPP
#define VQEC_VISION_AI_APPL_CASCADE_COORDINATOR_HPP

#include <cstddef>
#include <cstdint>
#include <vector>

#include "vqec/vision/ai/contracts/vqec_vision_image_alignment.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_observation.hpp"
#include "vqec/vision/ai/ports/vqec_vision_cascade_frame_lease.hpp"
#include "vqec/vision/ai/ports/vqec_vision_image_alignment.hpp"

namespace vqec::vision::ai {

struct cascade_coordinator_config {
    image_alignment_port* aligner_{nullptr};
    cascade_frame_lease_port* lease_{nullptr};
    alignment_template template_;
    // Maximum faces aligned per primary frame.
    std::size_t max_tasks_per_frame_{0};
};

struct cascade_coordinator_report {
    std::uint16_t accepted_{0};
    std::uint16_t skipped_{0};
    std::uint16_t failed_{0};
};

// Serialized cascade coordinator. For one decoded primary observation batch it acquires the
// exact retained frame once per admitted face, aligns each, completes each ticket and closes
// admission. A per-task failure is counted and isolated; it never faults the primary path.
// Synchronous backend in this slice; the embedding graph and bounded worker are M5.
class cascade_coordinator {
public:
    cascade_coordinator() = default;
    [[nodiscard]] status vqec_vision_ai_appl_cscrd_configure(
        const cascade_coordinator_config& _config);
    [[nodiscard]] status vqec_vision_ai_appl_cscrd_process(
        const observation_batch& _tracked, std::vector<alignment_result>& _aligned,
        cascade_coordinator_report& _report);
    [[nodiscard]] bool vqec_vision_ai_appl_cscrd_is_configured() const noexcept;

private:
    image_alignment_port* aligner_{nullptr};
    cascade_frame_lease_port* lease_{nullptr};
    alignment_template template_;
    alignment_capabilities capabilities_{};
    std::size_t max_tasks_per_frame_{0};
    bool is_configured_{false};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_APPL_CASCADE_COORDINATOR_HPP
