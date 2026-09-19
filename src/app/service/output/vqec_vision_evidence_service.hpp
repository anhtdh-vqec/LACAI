#ifndef VQEC_VISION_AI_APP_EVIDENCE_SERVICE_HPP
#define VQEC_VISION_AI_APP_EVIDENCE_SERVICE_HPP

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <thread>

#include "vqec/vision/ai/contracts/output/vqec_vision_output_gate.hpp"
#include "vqec/vision/ai/ports/output/vqec_vision_evidence_outbox.hpp"
#include "vqec/vision/ai/ports/output/vqec_vision_evidence_transport.hpp"
#include "vqec/vision/ai/ports/features/vqec_vision_feature_event_sink.hpp"

namespace vqec::vision::ai {

struct evidence_service_config {
    std::uint64_t initial_retry_ns_{0};
    std::uint64_t maximum_retry_ns_{0};
    std::uint64_t idle_poll_ns_{0};
    std::uint64_t stop_drain_ns_{0};
    std::size_t maximum_attempts_{0};
};

struct evidence_service_stats {
    std::uint64_t events_without_intent_{0};
    std::uint64_t commands_durable_{0};
    std::uint64_t commands_retried_{0};
    std::uint64_t commands_completed_{0};
    std::uint64_t commands_rejected_by_policy_{0};
    std::uint64_t commands_exhausted_{0};
    std::uint64_t transport_failures_{0};
};

class evidence_service final : public feature_event_sink_port {
public:
    evidence_service(evidence_service_config _config, evidence_outbox_port& _outbox,
        evidence_transport_port& _transport, output_gate& _output_gate);
    ~evidence_service() noexcept override;

    evidence_service(const evidence_service&) = delete;
    evidence_service& operator=(const evidence_service&) = delete;

    [[nodiscard]] status vqec_vision_ai_appl_evsvc_start();
    [[nodiscard]] status vqec_vision_ai_appl_evsvc_stop(bool _drain) noexcept;
    [[nodiscard]] status vqec_vision_ai_ports_fesnk_deliver_event(
        const feature_event& _event) override;
    [[nodiscard]] evidence_service_stats
    vqec_vision_ai_appl_evsvc_get_stats() const noexcept;
    [[nodiscard]] status vqec_vision_ai_appl_evsvc_get_health() const noexcept;

private:
    void vqec_vision_ai_appl_evsvc_run() noexcept;
    [[nodiscard]] std::uint64_t vqec_vision_ai_appl_evsvc_retry_delay_ns(
        std::size_t _attempt_count) const noexcept;

    evidence_service_config config_;
    evidence_outbox_port& outbox_;
    evidence_transport_port& transport_;
    output_gate& output_gate_;
    mutable std::mutex mutex_;
    std::condition_variable wake_;
    std::thread worker_;
    bool running_{false};
    bool stopping_{false};
    bool drain_requested_{false};
    std::uint64_t stop_deadline_ns_{0};
    evidence_service_stats stats_{};
    status health_{};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_APP_EVIDENCE_SERVICE_HPP
