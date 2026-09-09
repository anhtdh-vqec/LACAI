#ifndef VQEC_VISION_AI_APP_APPLICATION_COMPOSITION_HPP
#define VQEC_VISION_AI_APP_APPLICATION_COMPOSITION_HPP

#include "vqec/vision/ai/contracts/vqec_vision_application_composition.hpp"
#include "vqec_vision_multi_source_supervisor.hpp"

namespace vqec::vision::ai {

class application_composition final : public application_composition_port {
public:
    explicit application_composition(
        std::uint64_t _deployment_revision, std::uint64_t _catalog_revision,
        std::uint16_t _declared_sources) noexcept;
    [[nodiscard]] status vqec_vision_ai_appl_acomp_bind_session(
        std::uint16_t _source_index, source_session_port& _session);
    [[nodiscard]] status vqec_vision_ai_appl_acomp_take_result(
        tensor_result& _result, multi_source_progress_report& _report);
    [[nodiscard]] status vqec_vision_ai_cntr_acomp_validate() override;
    [[nodiscard]] status vqec_vision_ai_cntr_acomp_activate() override;
    [[nodiscard]] status vqec_vision_ai_cntr_acomp_step(
        std::uint64_t _steady_now_ns) override;
    [[nodiscard]] status vqec_vision_ai_cntr_acomp_request_stop(
        std::uint64_t _steady_now_ns) override;
    [[nodiscard]] application_composition_snapshot
    vqec_vision_ai_cntr_acomp_get_snapshot() const noexcept override;

private:
    void vqec_vision_ai_appl_acomp_refresh_snapshot() noexcept;
    application_composition_snapshot snapshot_;
    multi_source_supervisor supervisor_;
    tensor_result pending_result_;
    multi_source_progress_report pending_report_;
    bool has_pending_result_{false};
    std::uint64_t last_now_ns_{0};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_APP_APPLICATION_COMPOSITION_HPP
