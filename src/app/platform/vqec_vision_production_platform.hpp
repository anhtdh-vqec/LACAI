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
#include "vqec/vision/ai/contracts/vqec_vision_model_package_registry.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_preview_contract.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_status.hpp"
#include "vqec/vision/ai/ports/vqec_vision_inference_graph.hpp"
#include "vqec/vision/ai/ports/vqec_vision_image_processor.hpp"
#include "vqec/vision/ai/ports/vqec_vision_embedding_decoder.hpp"
#include "vqec/vision/ai/ports/vqec_vision_image_alignment.hpp"
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
namespace production_platform_limits {
inline constexpr std::uint64_t g_default_max_artifact_bytes = 256ULL * 1024 * 1024;
inline constexpr std::uint64_t g_max_artifact_bytes_ceiling = 4ULL * 1024 * 1024 * 1024;
}  // namespace production_platform_limits

// The platform does not carry deployment defaults: every field below is supplied by
// validated startup configuration. Empty/zero values are rejected by configure(), so a
// missing policy fails closed instead of silently using a built-in product default.
struct production_platform_config {
    // Exact per-model package/artifact bindings, cross-validated with the model catalog.
    model_package_registry model_packages_;
    std::string backend_library_;
    std::string system_library_;
    // Allows QAIC pointer copy for memfd-based test sources. False requires FastRPC
    // registration and is the only mode eligible for production performance evidence.
    bool allow_qaic_copy_input_{false};
    std::string model_root_;
    // Explicit accelerator deployment paths. Empty means that operation family is not
    // installed; prepare() fails closed only when an activated package requires it.
    std::string dsp_v1_skel_dir_;
    std::string dsp_legacy_skel_dir_;
    std::int32_t dsp_legacy_clock_corner_{0};
    std::int32_t dsp_legacy_latency_us_{0};
    bool dsp_enable_unsigned_pd_{false};
    std::uint64_t max_artifact_bytes_{0};
    inference_execution_policy execution_policy_;
    // Released FW camera route inputs.
    std::string socket_dir_;
    std::uint32_t producer_uid_{0};
    std::uint32_t nv12_format_value_{0};
    std::uint64_t preprocess_output_timeout_ns_{0};
    std::string tracker_contract_;
    std::string event_schema_id_;
    std::string event_schema_version_;
    std::string consumer_id_prefix_;
    // AI-owned encoded output. Empty disables rendering. A non-empty ring configures a
    // single-source preview; a multi-source deployment is rejected until FW provides a
    // versioned per-source output registry.
    std::string output_ring_id_;
    std::uint32_t output_fps_{0};
    std::uint32_t output_bitrate_bps_{0};
    std::uint32_t output_keyframe_interval_frames_{0};
    std::uint32_t output_box_color_rgba_{0};
    std::uint32_t output_surface_count_{0};
    std::string output_colorimetry_;
    std::string output_interlace_mode_;
};

// Borrowed neutral ports and package metadata for one dependency-activated cascade model.
// The caller owns graph lifecycle and keeps the platform alive until the graph is drained.
struct production_cascade_binding {
    std::string model_id_;
    std::string model_version_;
    std::size_t embedding_dimensions_{0};
    inference_graph_port* graph_{nullptr};
    embedding_decoder_port* decoder_{nullptr};
    image_alignment_port* aligner_{nullptr};
    alignment_template alignment_;
    preprocess_spec preprocess_;
    inference_plan plan_;
    std::vector<tensor_spec> outputs_;
    std::uint64_t max_output_bytes_{0};
};

// Owns an isolated model graph for serialized offline/file inference. The concrete QNN
// backend and Qualcomm preprocessing resources stay behind this application-layer PIMPL.
// Decoder pointers are borrowed from production_platform; this owner and every consumer
// must therefore be destroyed before the platform.
class production_offline_model final {
public:
    production_offline_model();
    ~production_offline_model() noexcept;
    production_offline_model(const production_offline_model& _other) = delete;
    production_offline_model& operator=(const production_offline_model& _other) = delete;

    [[nodiscard]] inference_graph_port* vqec_vision_ai_appl_pdplt_get_graph() noexcept;
    [[nodiscard]] image_processor_port* vqec_vision_ai_appl_pdplt_get_processor() noexcept;
    [[nodiscard]] model_decoder_port* vqec_vision_ai_appl_pdplt_get_decoder() noexcept;
    [[nodiscard]] embedding_decoder_port*
    vqec_vision_ai_appl_pdplt_get_embedding_decoder() noexcept;
    [[nodiscard]] image_alignment_port* vqec_vision_ai_appl_pdplt_get_aligner() noexcept;
    [[nodiscard]] const production_cascade_binding&
    vqec_vision_ai_appl_pdplt_get_binding() const noexcept;

private:
    struct implementation;
    std::unique_ptr<implementation> implementation_;
    friend class production_platform;
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
        std::uint16_t _source_slot, const std::string& _model_id) noexcept;
    [[nodiscard]] image_processor_port* vqec_vision_ai_appl_pdplt_processor(
        std::uint16_t _source_slot, const std::string& _model_id) noexcept;
    [[nodiscard]] const model_outputs* vqec_vision_ai_appl_pdplt_outputs(
        const std::string& _model_id) const noexcept;
    [[nodiscard]] resolved_model_paths vqec_vision_ai_appl_pdplt_paths(
        const std::string& _model_id) const noexcept;
    // Resolves a secondary embedding model activated by this source's primary assignment.
    // Failure preserves _binding. This performs no graph lifecycle operation.
    [[nodiscard]] status vqec_vision_ai_appl_pdplt_cascade_binding(
        std::uint16_t _source_slot, const std::string& _model_id,
        production_cascade_binding& _binding);
    // Creates a graph isolated from the live camera graph for file enrollment. The model
    // must be activated by _source_slot. Failure preserves _owner.
    [[nodiscard]] status vqec_vision_ai_appl_pdplt_create_offline_model(
        std::uint16_t _source_slot, const std::string& _model_id,
        std::unique_ptr<production_offline_model>& _owner);
    // Renders one source frame with the prepared overlay and writes the encoded AU to
    // the FW ring. No-op with ok when output is disabled. Borrowed frame; call on the
    // serialized runtime owner.
    [[nodiscard]] status vqec_vision_ai_appl_pdplt_render(
        std::uint16_t _source_slot, const raw_frame& _frame,
        const prepared_overlay& _payload);

private:
    struct implementation;
    std::unique_ptr<implementation> implementation_;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_APPL_PRODUCTION_PLATFORM_HPP
