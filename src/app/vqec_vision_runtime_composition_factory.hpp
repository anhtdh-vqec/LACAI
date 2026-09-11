#ifndef VQEC_VISION_AI_APPL_RUNTIME_COMPOSITION_FACTORY_HPP
#define VQEC_VISION_AI_APPL_RUNTIME_COMPOSITION_FACTORY_HPP

#include <array>
#include <cstdint>
#include <memory>
#include <string>

#include "vqec_vision_activation_snapshot.hpp"
#include "vqec_vision_application_composition.hpp"
#include "vqec_vision_feature_fanout.hpp"
#include "vqec_vision_multi_model_session.hpp"
#include "vqec_vision_runtime_executor.hpp"
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

// Optional activation-time feature wiring. Fan-out pointers are borrowed, already
// configured and owned by the caller (typically a feature activation manager). A null
// slot means that model has no direct feature consumer yet.
struct runtime_source_feature_activation {
    std::array<feature_fanout*, deployment_limits::g_max_models_per_source> fanouts_{};
};

// Binding descriptor: pins the feature wiring to the exact deployment and catalog
// revisions it was activated against, so it cannot be applied to another composition.
struct runtime_feature_activation {
    std::array<runtime_source_feature_activation, deployment_limits::g_max_sources>
        sources_{};
    std::uint64_t deployment_revision_{0};
    std::uint64_t catalog_revision_{0};
    std::uint16_t source_count_{0};
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
    [[nodiscard]] multi_model_feature_pipeline*
    vqec_vision_ai_appl_rcfac_get_feature_pipeline(std::uint16_t _source_slot) noexcept;
    [[nodiscard]] runtime_executor*
    vqec_vision_ai_appl_rcfac_get_executor() noexcept;
    [[nodiscard]] const activation_snapshot&
    vqec_vision_ai_appl_rcfac_get_admission() const noexcept;
    [[nodiscard]] std::uint16_t
    vqec_vision_ai_appl_rcfac_get_source_count() const noexcept;
    // True only when the bundle has never been activated or has fully stopped, so a
    // caller may safely replace it without destroying owners still in use.
    [[nodiscard]] bool
    vqec_vision_ai_appl_rcfac_is_replaceable() const noexcept;

private:
    runtime_composition_bundle() = default;

    activation_snapshot admission_;
    std::array<std::unique_ptr<multi_model_session>,
        deployment_limits::g_max_sources> sessions_{};
    std::array<std::unique_ptr<source_perception_bundle>,
        deployment_limits::g_max_sources> perceptions_{};
    std::array<std::unique_ptr<multi_model_feature_pipeline>,
        deployment_limits::g_max_sources> pipelines_{};
    std::unique_ptr<application_composition> composition_;
    std::unique_ptr<runtime_executor> executor_;
    std::uint16_t source_count_{0};

    friend status vqec_vision_ai_appl_rcfac_create_bundle(
        const deployment_config& _deployment, const model_catalog& _catalog,
        const runtime_composition_activation& _activation,
        const model_decoder_registry& _decoder_registry,
        const tracker_registry& _tracker_registry,
        std::unique_ptr<runtime_composition_bundle>& _bundle,
        const runtime_feature_activation* _features);
};

// Cold-path transactional composition. Performs no source acquisition, model load,
// graph configuration or frame submission. Failure preserves _bundle. Feature wiring
// is optional; a null _features builds pipelines with no direct feature consumers.
[[nodiscard]] status vqec_vision_ai_appl_rcfac_create_bundle(
    const deployment_config& _deployment, const model_catalog& _catalog,
    const runtime_composition_activation& _activation,
    const model_decoder_registry& _decoder_registry,
    const tracker_registry& _tracker_registry,
    std::unique_ptr<runtime_composition_bundle>& _bundle,
    const runtime_feature_activation* _features = nullptr);

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_APPL_RUNTIME_COMPOSITION_FACTORY_HPP
