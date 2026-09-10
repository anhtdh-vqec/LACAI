#ifndef VQEC_VISION_AI_APPL_RUNTIME_COMPOSITION_FACTORY_HPP
#define VQEC_VISION_AI_APPL_RUNTIME_COMPOSITION_FACTORY_HPP

#include <array>
#include <cstdint>
#include <memory>
#include <string>

#include "vqec_vision_activation_snapshot.hpp"
#include "vqec_vision_application_composition.hpp"
#include "vqec_vision_multi_model_session.hpp"
#include "vqec_vision_source_perception_factory.hpp"

namespace vqec::vision::ai {

struct runtime_model_activation {
    std::string model_id_;
    inference_graph_port* graph_{nullptr};
    resolved_model_paths paths_;
    std::string resolved_output_manifest_ref_;
    model_outputs outputs_;
    std::string tracker_contract_;
    source_binding binding_;
    std::uint64_t cycle_id_{0};
    std::uint64_t job_timeout_ns_{0};
};

struct runtime_source_activation {
    std::string source_id_;
    raw_source_port* source_{nullptr};
    std::array<runtime_model_activation,
        deployment_limits::g_max_models_per_source> models_{};
    std::uint16_t model_count_{0};
};

struct runtime_composition_activation {
    std::array<runtime_source_activation, deployment_limits::g_max_sources> sources_{};
    std::uint64_t startup_timeout_ns_{0};
    std::uint64_t stop_timeout_ns_{0};
    std::uint16_t source_count_{0};
    int rpc_timeout_ms_{0};
};

// Owns neutral coordinating objects only. RAW sources, inference graphs, decoder
// registrations and tracker factories are borrowed and must outlive this bundle.
class runtime_composition_bundle final {
public:
    runtime_composition_bundle(const runtime_composition_bundle& _other) = delete;
    runtime_composition_bundle& operator=(const runtime_composition_bundle& _other) = delete;

    [[nodiscard]] application_composition*
    vqec_vision_ai_appl_rcfac_get_composition() noexcept;
    [[nodiscard]] multi_model_session*
    vqec_vision_ai_appl_rcfac_get_session(std::uint16_t _source_slot) noexcept;
    [[nodiscard]] source_perception_bundle*
    vqec_vision_ai_appl_rcfac_get_perception(std::uint16_t _source_slot) noexcept;
    [[nodiscard]] const activation_snapshot&
    vqec_vision_ai_appl_rcfac_get_admission() const noexcept;
    [[nodiscard]] std::uint16_t
    vqec_vision_ai_appl_rcfac_get_source_count() const noexcept;

private:
    runtime_composition_bundle() = default;

    activation_snapshot admission_;
    std::array<std::unique_ptr<multi_model_session>,
        deployment_limits::g_max_sources> sessions_{};
    std::array<std::unique_ptr<source_perception_bundle>,
        deployment_limits::g_max_sources> perceptions_{};
    std::unique_ptr<application_composition> composition_;
    std::uint16_t source_count_{0};

    friend status vqec_vision_ai_appl_rcfac_create_bundle(
        const deployment_config& _deployment, const model_catalog& _catalog,
        const runtime_composition_activation& _activation,
        const model_decoder_registry& _decoder_registry,
        const tracker_registry& _tracker_registry,
        std::unique_ptr<runtime_composition_bundle>& _bundle);
};

// Cold-path transactional composition. Performs no source acquisition, model load,
// graph configuration or frame submission. Failure preserves _bundle.
[[nodiscard]] status vqec_vision_ai_appl_rcfac_create_bundle(
    const deployment_config& _deployment, const model_catalog& _catalog,
    const runtime_composition_activation& _activation,
    const model_decoder_registry& _decoder_registry,
    const tracker_registry& _tracker_registry,
    std::unique_ptr<runtime_composition_bundle>& _bundle);

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_APPL_RUNTIME_COMPOSITION_FACTORY_HPP
