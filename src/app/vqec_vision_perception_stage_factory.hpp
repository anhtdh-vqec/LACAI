#ifndef VQEC_VISION_AI_APPL_PERCEPTION_STAGE_FACTORY_HPP
#define VQEC_VISION_AI_APPL_PERCEPTION_STAGE_FACTORY_HPP

#include <cstdint>
#include <memory>
#include <string>

#include "vqec_vision_perception_result_stage.hpp"
#include "vqec_vision_model_decoder_registry.hpp"
#include "vqec_vision_tracker_registry.hpp"

namespace vqec::vision::ai {

// Owns one decoder/tracker/result-stage chain for a source/model binding. The
// decoder implementation remains borrowed from model_decoder_registry; tracker
// state is owned here and is therefore unique to this binding.
class perception_stage_bundle final {
public:
    perception_stage_bundle(const perception_stage_bundle& _other) = delete;
    perception_stage_bundle& operator=(const perception_stage_bundle& _other) = delete;

    [[nodiscard]] perception_result_stage*
    vqec_vision_ai_appl_prfac_get_result_stage() noexcept;
    [[nodiscard]] const perception_result_stage*
    vqec_vision_ai_appl_prfac_get_result_stage() const noexcept;

private:
    perception_stage_bundle(model_decoder_port& _decoder,
        std::unique_ptr<tracker_port> _tracker, preview_geometry _geometry);

    std::unique_ptr<tracker_port> tracker_;
    std::unique_ptr<tracking_stage> tracking_stage_;
    std::unique_ptr<model_decode_stage> decoder_stage_;
    std::unique_ptr<perception_result_stage> result_stage_;

    friend status vqec_vision_ai_appl_prfac_create_bundle(
        const model_catalog_entry& _model, const source_deployment_config& _source,
        const std::string& _tracker_contract,
        const model_decoder_registry& _decoder_registry,
        const tracker_registry& _tracker_registry,
        std::unique_ptr<perception_stage_bundle>& _bundle);
};

// Resolves borrowed decoder/factory registrations, creates one owned tracker and
// wires a configured neutral decoder->tracker result chain transactionally.
[[nodiscard]] status vqec_vision_ai_appl_prfac_create_bundle(
    const model_catalog_entry& _model, const source_deployment_config& _source,
    const std::string& _tracker_contract,
    const model_decoder_registry& _decoder_registry,
    const tracker_registry& _tracker_registry,
    std::unique_ptr<perception_stage_bundle>& _bundle);

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_APPL_PERCEPTION_STAGE_FACTORY_HPP
