#include <iostream>
#include <limits>

#include "vqec/vision/ai/contracts/vqec_vision_preview_contract.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_preview_limits.hpp"

// Independent released output boundary, not a fixture derived from production limits.
static_assert(vqec::vision::ai::preview_limits::g_encoded_ring_slots == 16);
static_assert(vqec::vision::ai::preview_limits::g_max_encoded_payload_bytes == 2097152);
static_assert(vqec::vision::ai::preview_limits::g_max_parameter_set_bytes == 512);

int main() {
    using namespace vqec::vision::ai;
    unsigned failures = 0;
    const auto check = [&](status _result, status_code _expected) {
        if (_result.code_ != _expected) {
            ++failures;
            std::cerr << _result.message_ << '\n';
        }
    };
    const preview_frame_key frame{0, 0, 1, 0, 0};
    const preview_geometry geometry{1920, 1080};
    const overlay_batch baseline{frame, geometry, 7, 100,
        {{0, 0, 1920, 1080, 0xffffffffU, "person"}}};
    const auto overlay = [&](const overlay_batch& _batch) {
        return vqec_vision_ai_core_pvctr_validate_overlay(_batch, frame, geometry, 7, 200, 100);
    };
    check(overlay(baseline), status_code::ok);
    auto batch = baseline;
    batch.boxes_.clear();
    check(overlay(batch), status_code::ok);
    batch = baseline;
    batch.frame_.source_epoch_ = 2;
    check(overlay(batch), status_code::invalid_state);
    batch.frame_ = frame;
    batch.frame_.source_pts_ns_ = UINT64_MAX;
    check(overlay(batch), status_code::invalid_argument);
    batch = baseline;
    batch.geometry_.width_ = 1919;
    check(overlay(batch), status_code::invalid_argument);
    batch.geometry_.width_ = 1280;
    check(overlay(batch), status_code::invalid_state);
    batch = baseline;
    batch.policy_revision_ = 8;
    check(overlay(batch), status_code::unauthorized);
    check(vqec_vision_ai_core_pvctr_validate_overlay(baseline, frame, geometry, 0, 200, 100),
        status_code::unauthorized);
    batch = baseline;
    batch.prepared_monotonic_ns_ = 99;
    check(overlay(batch), status_code::timeout);
    batch.prepared_monotonic_ns_ = 201;
    check(overlay(batch), status_code::invalid_argument);
    batch = baseline;
    batch.boxes_[0].x_ = std::numeric_limits<float>::quiet_NaN();
    check(overlay(batch), status_code::invalid_argument);
    batch.boxes_[0].x_ = -1;
    check(overlay(batch), status_code::invalid_argument);
    batch.boxes_[0].x_ = 1;
    check(overlay(batch), status_code::invalid_argument);
    batch = baseline;
    batch.boxes_[0].label_.assign(96, 'a');
    check(overlay(batch), status_code::ok);
    batch.boxes_[0].label_.push_back('a');
    check(overlay(batch), status_code::resource_exhausted);
    batch.boxes_[0].label_ = std::string("a\0b", 3);
    check(overlay(batch), status_code::unsupported);
    batch.boxes_.assign(128, baseline.boxes_[0]);
    check(overlay(batch), status_code::ok);
    batch.boxes_.push_back(baseline.boxes_[0]);
    check(overlay(batch), status_code::resource_exhausted);
    batch.boxes_.assign(43, baseline.boxes_[0]);
    for (auto& box : batch.boxes_) {
        box.label_.assign(96, 'a');
    }
    check(overlay(batch), status_code::resource_exhausted);

    const std::uint8_t annex_b[]{0, 0, 0, 1, 0x65};
    const std::uint8_t short_prefix[]{0, 0, 1, 0x41};
    const std::uint8_t avcc[]{0, 0, 0, 5, 0x65};
    h264_access_unit_view unit{frame, geometry, true, {annex_b, sizeof(annex_b)}, {}, {}};
    const auto access_unit = [&](const h264_access_unit_view& _unit) {
        return vqec_vision_ai_core_pvctr_validate_access_unit(_unit, frame, geometry);
    };
    check(access_unit(unit), status_code::ok);
    unit.payload_ = {short_prefix, sizeof(short_prefix)};
    check(access_unit(unit), status_code::ok);
    unit.payload_ = {avcc, sizeof(avcc)};
    check(access_unit(unit), status_code::protocol_error);
    unit.payload_ = {annex_b, 4};
    check(access_unit(unit), status_code::protocol_error);
    unit.payload_ = {nullptr, 5};
    check(access_unit(unit), status_code::invalid_argument);
    unit.payload_ = {annex_b, 3};
    check(access_unit(unit), status_code::invalid_argument);
    std::vector<std::uint8_t> payload(2U * 1024 * 1024, 0);
    payload[2] = 1;
    payload[3] = 0x41;
    unit.payload_ = {payload.data(), payload.size()};
    check(access_unit(unit), status_code::ok);
    payload.push_back(0);
    unit.payload_ = {payload.data(), payload.size()};
    check(access_unit(unit), status_code::resource_exhausted);
    unit.payload_ = {annex_b, sizeof(annex_b)};
    unit.sps_ = {nullptr, 1};
    check(access_unit(unit), status_code::invalid_argument);
    std::vector<std::uint8_t> parameter_set(513, 0x67);
    unit.sps_ = {parameter_set.data(), 512};
    check(access_unit(unit), status_code::ok);
    unit.sps_.size_ = 513;
    check(access_unit(unit), status_code::resource_exhausted);
    unit.sps_ = {};
    unit.pps_ = {nullptr, 1};
    check(access_unit(unit), status_code::invalid_argument);
    unit.pps_ = {};
    unit.frame_.frame_id_ = 3;
    check(access_unit(unit), status_code::invalid_state);
    std::cout << "preview contract failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
