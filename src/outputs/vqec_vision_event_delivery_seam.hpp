#ifndef VQEC_VISION_AI_OUTPT_EVENT_DELIVERY_SEAM_HPP
#define VQEC_VISION_AI_OUTPT_EVENT_DELIVERY_SEAM_HPP

#include <array>
#include <cstddef>
#include <cstdint>

#include "vqec/vision/ai/contracts/vqec_vision_feature_event.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_status.hpp"
#include "vqec/vision/ai/ports/vqec_vision_feature_event_sink.hpp"

namespace vqec::vision::ai {

namespace event_delivery_seam_limits {
inline constexpr std::size_t g_max_queue_capacity = 256;
inline constexpr std::size_t g_default_queue_capacity = 64;
}  // namespace event_delivery_seam_limits

enum class event_disposition : std::uint8_t {
    empty = 0,
    accepted_pending = 1,
    handed_off = 2,
    rejected = 3
};

struct event_delivery_seam_config {
    std::size_t max_queued_events_{event_delivery_seam_limits::g_default_queue_capacity};
};

struct event_delivery_seam_metrics {
    std::uint64_t events_accepted_{0};
    std::uint64_t events_pending_{0};
    std::uint64_t events_handed_off_{0};
    std::uint64_t events_dropped_{0};
    std::uint64_t events_discarded_{0};
    std::uint64_t events_rejected_{0};
    std::uint64_t oldest_pending_ns_{0};
};

struct queued_event_slot {
    feature_event event_{};
    std::uint64_t enqueued_time_ns_{0};
    event_disposition disposition_{event_disposition::empty};
};

// Neutral event delivery owner that establishes a bounded outbox/handoff seam
// between the application execution pipeline and downstream firmware transport.
// It explicitly distinguishes accepted/pending handoff from actual durable delivery.
class event_delivery_seam final : public feature_event_sink_port {
public:
    explicit event_delivery_seam(
        const event_delivery_seam_config& _config = {}) noexcept;
    event_delivery_seam(const event_delivery_seam& _other) = delete;
    event_delivery_seam& operator=(const event_delivery_seam& _other) = delete;
    ~event_delivery_seam() noexcept override = default;

    [[nodiscard]] status vqec_vision_ai_ports_fesnk_deliver_event(
        const feature_event& _event) override;

    // Transfers one accepted event to a downstream transport owner. `ok` means handed
    // off from this in-memory seam, never durable or remotely acknowledged delivery.
    [[nodiscard]] status vqec_vision_ai_outpt_evdsm_take_next(feature_event& _event);
    // Explicit terminal discard for shutdown/recovery when no downstream transport is
    // wired. Discarded events are never counted as handed off.
    void vqec_vision_ai_outpt_evdsm_discard_pending() noexcept;
    void vqec_vision_ai_outpt_evdsm_request_stop() noexcept;
    [[nodiscard]] bool vqec_vision_ai_outpt_evdsm_is_stopping() const noexcept;
    [[nodiscard]] const event_delivery_seam_metrics&
    vqec_vision_ai_outpt_evdsm_get_metrics() const noexcept;
    [[nodiscard]] std::size_t
    vqec_vision_ai_outpt_evdsm_get_pending_count() const noexcept;
    [[nodiscard]] const feature_event*
    vqec_vision_ai_outpt_evdsm_peek(std::size_t _index) const noexcept;

private:
    event_delivery_seam_config config_;
    bool is_stopping_{false};
    event_delivery_seam_metrics metrics_{};
    std::size_t head_index_{0};
    std::size_t tail_index_{0};
    std::size_t pending_count_{0};
    std::array<queued_event_slot,
        event_delivery_seam_limits::g_max_queue_capacity> queue_{};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_OUTPT_EVENT_DELIVERY_SEAM_HPP
