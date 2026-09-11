#ifndef VQEC_VISION_AI_REFER_REFERENCE_SINK_HPP
#define VQEC_VISION_AI_REFER_REFERENCE_SINK_HPP

#include <cstdint>

#include "vqec/vision/ai/ports/vqec_vision_feature_event_sink.hpp"

namespace vqec::vision::ai {

// Device-free development sink: accepts every authorized event and records bounded
// counters plus a copy of the last event for tests and harness output. It performs no
// transport, durability, dedup or retry, and proves nothing about delivery guarantees.
class reference_event_sink final : public feature_event_sink_port {
public:
    reference_event_sink() = default;
    reference_event_sink(const reference_event_sink& _other) = delete;
    reference_event_sink& operator=(const reference_event_sink& _other) = delete;
    ~reference_event_sink() noexcept override = default;

    [[nodiscard]] status vqec_vision_ai_ports_fesnk_deliver_event(
        const feature_event& _event) override;
    [[nodiscard]] std::uint64_t
    vqec_vision_ai_refer_rfsnk_get_delivered() const noexcept;
    [[nodiscard]] const feature_event&
    vqec_vision_ai_refer_rfsnk_get_last() const noexcept;

private:
    feature_event last_;
    std::uint64_t delivered_{0};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_REFER_REFERENCE_SINK_HPP
