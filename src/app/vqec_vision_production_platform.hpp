#ifndef VQEC_VISION_AI_APPL_PRODUCTION_PLATFORM_HPP
#define VQEC_VISION_AI_APPL_PRODUCTION_PLATFORM_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "vqec/vision/ai/contracts/vqec_vision_deployment_config.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_feature_event.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_model_catalog.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_model_outputs.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_status.hpp"
#include "vqec/vision/ai/ports/vqec_vision_inference_graph.hpp"
#include "vqec/vision/ai/ports/vqec_vision_raw_source.hpp"
#include "vqec_vision_feature_catalog.hpp"
#include "vqec_vision_feature_processor_registry.hpp"
#include "vqec_vision_model_decoder_registry.hpp"
#include "vqec_vision_tracker_registry.hpp"

namespace vqec::vision::ai {

// Production platform owner for the Qualcomm QCS6490 target.
//
// It owns the concrete adapters the neutral runtime must borrow: one RAW-source lifecycle
// per deployment source (released FW camera wire) and one owned QNN engine + inference graph
// per catalog model, plus the concrete decoder/tracker/feature implementations. service_main
// only sees neutral ports and resolved metadata.
//
// It does not start, load, submit or drain anything; activation is driven by the neutral
// runtime. All owners must outlive the runtime bundle and the bundle must reach stopped
// before this owner is destroyed.
struct production_platform_config {
    // Model package directory holding io_manifest.json / preprocess.json / decoder.json.
    std::string package_dir_;
    std::string model_library_;
    std::string backend_library_;
    std::string system_library_;
    // Released FW camera route inputs.
    std::string socket_dir_{"/run/camera_ai"};
    std::uint32_t producer_uid_{0};
    std::uint32_t nv12_format_value_{23};
    std::string tracker_contract_{"reference.tracker.v1"};
    std::string event_schema_id_{"reference.zone"};
    std::string event_schema_version_{"1"};
    std::string consumer_id_prefix_{"lacai_ai"};
};

class production_platform final {
public:
    production_platform();
    ~production_platform() noexcept;
    production_platform(const production_platform& _other) = delete;
    production_platform& operator=(const production_platform& _other) = delete;

    [[nodiscard]] status vqec_vision_ai_appl_pdplt_configure(
        const production_platform_config& _config);
    // Opens every catalog model engine, composes its graph and builds one RAW-source
    // lifecycle per deployment source. Cold path; performs no camera acquisition or load.
    [[nodiscard]] status vqec_vision_ai_appl_pdplt_prepare(
        const deployment_config& _deployment, const model_catalog& _catalog);
    [[nodiscard]] status vqec_vision_ai_appl_pdplt_register_decoders(
        const model_catalog& _catalog, model_decoder_registry& _decoders) const;
    [[nodiscard]] status vqec_vision_ai_appl_pdplt_register_tracker(
        tracker_registry& _trackers) const;
    [[nodiscard]] status vqec_vision_ai_appl_pdplt_register_features(
        const feature_catalog& _features, feature_processor_registry& _registry) const;
    [[nodiscard]] const std::string&
    vqec_vision_ai_appl_pdplt_get_tracker_contract() const noexcept;
    [[nodiscard]] const std::string&
    vqec_vision_ai_appl_pdplt_get_attribute_schema_id() const noexcept;

    [[nodiscard]] raw_source_port* vqec_vision_ai_appl_pdplt_source(
        std::uint16_t _source_slot) noexcept;
    [[nodiscard]] inference_graph_port* vqec_vision_ai_appl_pdplt_graph(
        std::uint16_t _source_slot, std::uint16_t _model_slot) noexcept;
    [[nodiscard]] const model_outputs* vqec_vision_ai_appl_pdplt_outputs(
        const std::string& _model_id) const noexcept;
    [[nodiscard]] resolved_model_paths vqec_vision_ai_appl_pdplt_paths(
        const std::string& _model_id) const noexcept;

private:
    struct implementation;
    std::unique_ptr<implementation> implementation_;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_APPL_PRODUCTION_PLATFORM_HPP
