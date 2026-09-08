#ifndef VQEC_VISION_AI_FW_OUTPUT_RING_SINK_HPP
#define VQEC_VISION_AI_FW_OUTPUT_RING_SINK_HPP

#include "vqec/vision/ai/contracts/vqec_vision_encoded_sink.hpp"

namespace camera_ai {
class SharedMemoryFrameRingBuffer;
struct RingFrameHeader;
struct SharedRingOpenOptions;
}

namespace vqec::vision::ai {

struct fw_ring_sink_config {
    unsigned detect_index_{0};
    std::uint32_t camera_id_{0};
    std::uint32_t channel_id_{0};
    std::uint64_t source_epoch_{0};
    // SDK-object-local counter: checks that the borrowed mapping has not reopened.
    std::uint64_t mapping_generation_{0};
    preview_geometry geometry_;
    // Runtime-unique output binding ID; never reuse while old outputs can exist.
    std::uint64_t dispatch_generation_{0};
};

// Constructs legacy writer options only; no shared-memory operation or writer lock.
// SDK open may still recreate incompatible mappings with replace_existing=false.
// See fw_ring_sink.md before using these options in runtime startup/recovery.
[[nodiscard]] status vqec_vision_ai_fwout_rgsnk_make_open_options(
    unsigned _detect_index, camera_ai::SharedRingOpenOptions& _options);

// No ring I/O. Validates/copies metadata transactionally; failure preserves _header.
// Private SDK boundary only: callers provide independent expected frame/geometry.
[[nodiscard]] status vqec_vision_ai_fwout_rgsnk_map_header(
    const h264_access_unit_view& _unit, const preview_frame_key& _expected_frame,
    const preview_geometry& _expected_geometry, camera_ai::RingFrameHeader& _header);

// Private FW adapter: ring is borrowed, already open, serialized and outlives this object.
// No open/unlink/destructor I/O; construct a fresh wrapper after a coordinated remap.
class ring_sink final : public encoded_sink {
public:
    ring_sink(camera_ai::SharedMemoryFrameRingBuffer& _ring, fw_ring_sink_config _config);
    ring_sink(const ring_sink&) = delete;
    ring_sink& operator=(const ring_sink&) = delete;
    [[nodiscard]] status vqec_vision_ai_cntr_encsk_query_demand(encoded_sink_demand& _demand) override;
    [[nodiscard]] status vqec_vision_ai_cntr_encsk_write(
        const h264_access_unit_view& _unit, std::uint64_t _expected_generation) override;

private:
    [[nodiscard]] status vqec_vision_ai_fwout_rgsnk_validate_binding() const;
    camera_ai::SharedMemoryFrameRingBuffer& ring_;
    fw_ring_sink_config config_;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_FW_OUTPUT_RING_SINK_HPP
