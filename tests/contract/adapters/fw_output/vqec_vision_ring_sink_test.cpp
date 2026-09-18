#include <iostream>

#include <camera_ai/common/ring_buffer.hpp>

#include "vqec_vision_ring_sink.hpp"

int main() {
    using namespace vqec::vision::ai;
    unsigned failures = 0;
    const auto check = [&](bool _condition) {
        if (!_condition) {
            ++failures;
        }
    };
    camera_ai::SharedRingOpenOptions options;
    check(vqec_vision_ai_fwout_rgsnk_make_open_options(0, options).code_ == status_code::ok);
    check(options.ring_id == "encoded_ai_detect0_cam0_ch0");
    check(options.slot_count == 16 && options.payload_size == 2097152);
    check(options.create_if_missing && !options.replace_existing);
    check(vqec_vision_ai_fwout_rgsnk_make_open_options(1, options).code_ == status_code::ok);
    check(options.ring_id == "encoded_ai_detect1_cam0_ch0");
    check(vqec_vision_ai_fwout_rgsnk_make_open_options(2, options).code_ ==
        status_code::invalid_argument);
    check(options.ring_id == "encoded_ai_detect1_cam0_ch0" && options.slot_count == 16 &&
        options.payload_size == 2097152 && options.create_if_missing && !options.replace_existing);
    camera_ai::SharedMemoryFrameRingBuffer ring;  // Deliberately unopened: no shared-memory writes.
    fw_ring_sink_config config{0, 0, 0, 1, 1, {1920, 1080}, 17};
    ring_sink sink(ring, config);
    encoded_sink_demand demand{99, 7};
    check(sink.vqec_vision_ai_cntr_encsk_query_demand(demand).code_ == status_code::invalid_state);
    check(demand.mapping_generation_ == 99 && demand.active_consumers_ == 7);
    const h264_access_unit_view unit;
    check(sink.vqec_vision_ai_cntr_encsk_write(unit, 1).code_ == status_code::invalid_state);
    config.detect_index_ = 2;
    ring_sink invalid(ring, config);
    check(invalid.vqec_vision_ai_cntr_encsk_query_demand(demand).code_ == status_code::invalid_argument);
    config.detect_index_ = 1;
    config.mapping_generation_ = 0;
    ring_sink unset_generation(ring, config);
    check(unset_generation.vqec_vision_ai_cntr_encsk_query_demand(demand).code_ ==
        status_code::invalid_argument);
    check(!ring.is_open());
    config.mapping_generation_ = 1;
    config.dispatch_generation_ = 0;
    ring_sink no_dispatch_identity(ring, config);
    check(no_dispatch_identity.vqec_vision_ai_cntr_encsk_query_demand(demand).code_ ==
        status_code::invalid_argument);

    // Header mapping uses the real SDK type without opening or writing any shared memory.
    const preview_frame_key frame{1, 2, 3, 42, 100};
    const preview_geometry geometry{1920, 1080};
    std::uint8_t payload[]{0, 0, 1, 0x65};
    std::uint8_t sps[]{0x67, 1};
    std::uint8_t pps[]{0x68, 2};
    h264_access_unit_view encoded{frame, geometry, true, {payload, 4}, {sps, 2}, {pps, 2}};
    camera_ai::RingFrameHeader header;
    check(vqec_vision_ai_fwout_rgsnk_map_header(encoded, frame, geometry, header).code_ ==
        status_code::ok);
    check(header.frame_id == 42 && header.timestamp_ns == 100);
    check(header.camera_id == 1 && header.channel_id == 2 && header.stream_id == "ai");
    check(header.width == 1920 && header.height == 1080 && header.stride == 0);
    check(header.codec == "H264" && header.format == "byte-stream" && header.is_keyframe);
    check(header.data_size == 4 && header.h264_sps.size() == 2 && header.h264_pps.size() == 2);
    sps[0] = 0;
    pps[0] = 0;
    check(header.h264_sps[0] == 0x67 && header.h264_pps[0] == 0x68);
    auto stale_frame = frame;
    ++stale_frame.source_epoch_;
    check(vqec_vision_ai_fwout_rgsnk_map_header(encoded, stale_frame, geometry, header).code_ ==
        status_code::invalid_state);
    check(header.frame_id == 42 && header.h264_sps[0] == 0x67);
    auto invalid_output = encoded;
    invalid_output.payload_ = {nullptr, 4};
    check(vqec_vision_ai_fwout_rgsnk_map_header(invalid_output, frame, geometry, header).code_ ==
        status_code::invalid_argument);
    check(header.frame_id == 42 && header.h264_pps[0] == 0x68);
    encoded.sps_ = {};
    encoded.pps_ = {};
    encoded.is_keyframe_ = false;
    encoded.frame_.source_pts_ns_ = 0;
    auto zero_pts = frame;
    zero_pts.source_pts_ns_ = 0;
    check(vqec_vision_ai_fwout_rgsnk_map_header(encoded, zero_pts, geometry, header).code_ ==
        status_code::ok);
    check(header.timestamp_ns == 0 && !header.is_keyframe && header.h264_sps.empty() &&
        header.h264_pps.empty());
    std::cout << "FW ring sink guard failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
