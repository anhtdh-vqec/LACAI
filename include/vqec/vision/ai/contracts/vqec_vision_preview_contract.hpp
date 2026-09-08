#ifndef VQEC_VISION_AI_CONTRACTS_PREVIEW_CONTRACT_HPP
#define VQEC_VISION_AI_CONTRACTS_PREVIEW_CONTRACT_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "vqec/vision/ai/contracts/vqec_vision_status.hpp"

namespace vqec::vision::ai {

struct preview_frame_key {
    std::uint32_t camera_id_{0};
    std::uint32_t channel_id_{0};
    std::uint64_t source_epoch_{0};
    std::uint64_t frame_id_{0};
    std::uint64_t source_pts_ns_{UINT64_MAX};
};

struct preview_geometry {
    std::uint32_t width_{0};
    std::uint32_t height_{0};
};

struct overlay_box {
    float x_{0};
    float y_{0};
    float width_{0};
    float height_{0};
    std::uint32_t rgba_{0xffffffffU};
    std::string label_;
};

struct overlay_batch {
    preview_frame_key frame_;
    preview_geometry geometry_;
    std::uint64_t policy_revision_{0};
    std::uint64_t prepared_monotonic_ns_{0};
    std::vector<overlay_box> boxes_;
};

// Read-only borrow. Size validation cannot establish actual allocation capacity.
struct preview_bytes_view {
    const std::uint8_t* data_{nullptr};
    std::size_t size_{0};
};

struct h264_access_unit_view {
    preview_frame_key frame_;
    preview_geometry geometry_;
    bool is_keyframe_{false};
    preview_bytes_view payload_;
    preview_bytes_view sps_;
    preview_bytes_view pps_;
};

// Pure exact frame/profile validation shared by overlay, observations and encoded output.
// Validates only identity and even linear NV12 geometry; no freshness or authorization.
[[nodiscard]] status vqec_vision_ai_core_pvctr_validate_identity(
    const preview_frame_key& _frame, const preview_frame_key& _expected_frame,
    const preview_geometry& _geometry, const preview_geometry& _expected_geometry);

// Pure pixel-space rectangle/printable-label validation. No policy check or allocation.
[[nodiscard]] status vqec_vision_ai_core_pvctr_validate_box(
    const overlay_box& _box, const preview_geometry& _geometry);

// Pure, synchronous, no I/O or mutation; caller serializes metadata with policy state.
// Exact-frame/identity-geometry only. This is NOT an authorization evaluator.
[[nodiscard]] status vqec_vision_ai_core_pvctr_validate_overlay(
    const overlay_batch& _batch, const preview_frame_key& _expected_frame,
    const preview_geometry& _expected_geometry, std::uint64_t _expected_revision,
    std::uint64_t _now_monotonic_ns, std::uint64_t _max_age_ns);

// Borrowed bytes remain alive/immutable through return. Checks envelope/start code,
// not full H264 syntax or hardware completion. No borrowed pointer is retained.
[[nodiscard]] status vqec_vision_ai_core_pvctr_validate_access_unit(
    const h264_access_unit_view& _unit, const preview_frame_key& _expected_frame,
    const preview_geometry& _expected_geometry);

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_CONTRACTS_PREVIEW_CONTRACT_HPP
