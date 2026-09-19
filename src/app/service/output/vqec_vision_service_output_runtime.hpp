#ifndef VQEC_VISION_AI_APPL_SERVICE_OUTPUT_RUNTIME_HPP
#define VQEC_VISION_AI_APPL_SERVICE_OUTPUT_RUNTIME_HPP

#include <memory>

#include "vqec/vision/ai/contracts/vqec_vision_deployment_config.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_output_gate.hpp"
#include "vqec/vision/ai/ports/vqec_vision_feature_event_sink.hpp"
#include "vqec_vision_evidence_service.hpp"
#include "vqec_vision_metadata_runtime.hpp"
#include "vqec_vision_service_options.hpp"

namespace vqec::vision::ai {

class evidence_uds_client;
class sqlite_evidence_outbox;

struct service_output_runtime_report {
    metadata_service_stats metadata_{};
    evidence_service_stats evidence_{};
    bool has_metadata_{false};
    bool has_evidence_{false};
};

// Owns cold-path metadata/evidence composition and their worker lifetime. The generation
// controller retains only a neutral sink and policy gate; it never constructs storage or IPC.
class service_output_runtime final {
public:
    service_output_runtime();
    ~service_output_runtime() noexcept;
    service_output_runtime(const service_output_runtime&) = delete;
    service_output_runtime& operator=(const service_output_runtime&) = delete;

    [[nodiscard]] status vqec_vision_ai_appl_svout_start(
        const parsed_arguments& _args, const deployment_config& _deployment,
        feature_event_sink_port& _fallback_sink);
    [[nodiscard]] status vqec_vision_ai_appl_svout_start_delivery();
    [[nodiscard]] status vqec_vision_ai_appl_svout_stop(
        bool _drain, service_output_runtime_report& _report) noexcept;

    [[nodiscard]] output_gate& vqec_vision_ai_appl_svout_get_gate() noexcept;
    [[nodiscard]] feature_event_sink_port&
    vqec_vision_ai_appl_svout_get_sink() noexcept;
    [[nodiscard]] metadata_runtime*
    vqec_vision_ai_appl_svout_get_metadata() noexcept;
    [[nodiscard]] bool
    vqec_vision_ai_appl_svout_is_metadata_required() const noexcept;

private:
    output_gate gate_{};
    feature_event_sink_port* fallback_sink_{nullptr};
    std::unique_ptr<sqlite_evidence_outbox> evidence_outbox_{};
    std::unique_ptr<evidence_uds_client> evidence_transport_{};
    std::unique_ptr<evidence_service> evidence_{};
    std::unique_ptr<metadata_runtime> metadata_{};
    bool metadata_required_{false};
    bool started_{false};
    bool delivery_started_{false};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_APPL_SERVICE_OUTPUT_RUNTIME_HPP
