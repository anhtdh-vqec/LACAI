#include "vqec/vision/ai/contracts/output/vqec_vision_trajectory_codec.hpp"

#include <cassert>
#include <cstdint>

namespace {

using namespace vqec::vision::ai;

constexpr std::size_t g_fixture_encoded_budget_bytes = 4096U;

trajectory_chunk vqec_vision_ai_unit_tctst_make_chunk() {
    trajectory_chunk chunk;
    chunk.chunk_id_ = "chunk.codec";
    chunk.track_.device_id_ = "device.fixture";
    chunk.track_.source_id_ = "camera.fixture";
    chunk.track_.boot_id_ = "boot.fixture";
    chunk.track_.source_epoch_ = 2U;
    chunk.track_.local_track_id_ = 9U;
    chunk.subject_ref_ = "person.9";
    chunk.entity_category_ = "person";
    chunk.chunk_sequence_ = 1U;
    for (auto* locator : {&chunk.first_frame_, &chunk.last_frame_}) {
        locator->device_id_ = chunk.track_.device_id_;
        locator->source_id_ = chunk.track_.source_id_;
        locator->boot_id_ = chunk.track_.boot_id_;
        locator->source_epoch_ = chunk.track_.source_epoch_;
        locator->clock_mapping_revision_ = "clock.v1";
        locator->scene_revision_ = "scene.v1";
        locator->coordinate_revision_ = "coordinate.v1";
        locator->model_revision_ = "model.v1";
        locator->tracker_revision_ = "tracker.v1";
    }
    chunk.first_frame_.frame_id_ = 100U;
    chunk.first_frame_.source_pts_ns_ = 100000U;
    chunk.last_frame_.frame_id_ = 102U;
    chunk.last_frame_.source_pts_ns_ = 166666U;
    chunk.bounds_left_ = -100;
    chunk.bounds_top_ = -100;
    chunk.bounds_right_ = 1000;
    chunk.bounds_bottom_ = 1000;
    chunk.required_access_domain_mask_ =
        vqec_vision_ai_cntr_stmet_get_access_domain_mask(
            spatiotemporal_access_domain::trajectory);
    for (std::uint64_t index = 0U; index < 3U; ++index) {
        trajectory_point point;
        point.frame_id_ = 100U + index;
        point.source_pts_ns_ = 100000U + index * 33333U;
        point.anchor_x_ = static_cast<std::int32_t>(index * 10U) - 5;
        point.anchor_y_ = static_cast<std::int32_t>(index * 20U);
        point.flags_ = static_cast<std::uint32_t>(trajectory_point_flag::observed);
        if (index == 0U) {
            point.flags_ |= static_cast<std::uint32_t>(trajectory_point_flag::mandatory);
        }
        chunk.points_.push_back(point);
    }
    return chunk;
}

}  // namespace

int main() {
    const auto chunk = vqec_vision_ai_unit_tctst_make_chunk();
    encoded_trajectory_points encoded;
    assert(vqec_vision_ai_cntr_trcod_encode_points(
               chunk, g_fixture_encoded_budget_bytes, encoded).code_ == status_code::ok);
    assert(!encoded.bytes_.empty() && encoded.checksum_crc32_ != 0U);

    std::vector<trajectory_point> decoded;
    assert(vqec_vision_ai_cntr_trcod_decode_points(encoded,
               g_fixture_encoded_budget_bytes, g_spatiotemporal_max_points_per_chunk,
               decoded).code_ == status_code::ok);
    assert(decoded.size() == chunk.points_.size());
    for (std::size_t index = 0U; index < decoded.size(); ++index) {
        assert(decoded[index].frame_id_ == chunk.points_[index].frame_id_);
        assert(decoded[index].source_pts_ns_ == chunk.points_[index].source_pts_ns_);
        assert(decoded[index].anchor_x_ == chunk.points_[index].anchor_x_);
        assert(decoded[index].anchor_y_ == chunk.points_[index].anchor_y_);
        assert(decoded[index].flags_ == chunk.points_[index].flags_);
    }

    auto corrupt = encoded;
    corrupt.bytes_[12] ^= 0x1U;
    assert(vqec_vision_ai_cntr_trcod_decode_points(corrupt,
               g_fixture_encoded_budget_bytes, g_spatiotemporal_max_points_per_chunk,
               decoded).code_ == status_code::protocol_error);

    auto trailing = encoded;
    trailing.bytes_.insert(trailing.bytes_.end() - 4, 0U);
    assert(vqec_vision_ai_cntr_trcod_decode_points(trailing,
               g_fixture_encoded_budget_bytes, g_spatiotemporal_max_points_per_chunk,
               decoded).code_ == status_code::protocol_error);

    encoded_trajectory_points too_small;
    assert(vqec_vision_ai_cntr_trcod_encode_points(
               chunk, 17U, too_small).code_ == status_code::resource_exhausted);
    return 0;
}
