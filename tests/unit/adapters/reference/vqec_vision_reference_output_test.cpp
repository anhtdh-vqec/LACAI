// Device-free tests for the fake output tail: encoder lifecycle/ownership and a bounded ring
// sink with backpressure, generation rejection and consumer drain, including a full
// encoder -> sink handoff.

#include <cstdint>
#include <iostream>
#include <memory>
#include <vector>

#include "vqec_vision_reference_encoder.hpp"
#include "vqec_vision_reference_ring_sink.hpp"

using namespace vqec::vision::ai;

namespace {

encoder_input make_input(std::uint64_t _frame_id) {
    encoder_input input;
    input.frame_.camera_id_ = 1;
    input.frame_.channel_id_ = 0;
    input.frame_.source_epoch_ = 1;
    input.frame_.frame_id_ = _frame_id;
    input.frame_.source_pts_ns_ = _frame_id * 40000000ULL;
    input.geometry_ = {640, 480};
    input.ticket_.token_ = {7, _frame_id};
    input.ticket_.source_epoch_ = 1;
    input.ticket_.source_frame_id_ = _frame_id;
    input.ticket_.source_pts_ns_ = input.frame_.source_pts_ns_;
    input.ticket_.pipeline_pts_ns_ = input.frame_.source_pts_ns_;
    input.dispatch_generation_ = 1;
    input.pixels_ = std::make_shared<const std::vector<std::uint8_t>>(64, 0U);
    return input;
}

}  // namespace

int main() {
    unsigned failures = 0;
    const auto check = [&failures](bool _condition) {
        if (!_condition) {
            ++failures;
        }
    };

    // Encoder bounds, one output per input and drain state.
    {
        reference_encoder_config config;
        config.max_pending_ = 2;
        reference_encoder encoder(config);
        check(encoder.vqec_vision_ai_cntr_encbk_submit(make_input(1)).code_ == status_code::ok);
        check(encoder.vqec_vision_ai_cntr_encbk_submit(make_input(2)).code_ == status_code::ok);
        check(encoder.vqec_vision_ai_cntr_encbk_submit(make_input(3)).code_ ==
              status_code::resource_exhausted);
        check(encoder.vqec_vision_ai_refer_renc_get_pending() == 2);

        encoder_event event;
        check(encoder.vqec_vision_ai_cntr_encbk_poll(event).code_ == status_code::ok);
        check(event.kind_ == encoder_event_kind::output_ready && event.output_ != nullptr &&
              event.token_.job_id_ == 1);
        const auto view = event.output_->vqec_vision_ai_core_encot_borrow_view();
        check(view.payload_.size_ > 0 && view.is_keyframe_);
        check(encoder.vqec_vision_ai_cntr_encbk_poll(event).code_ == status_code::ok);
        check(encoder.vqec_vision_ai_refer_renc_get_delivered() == 2);

        check(encoder.vqec_vision_ai_cntr_encbk_poll(event).code_ == status_code::pending);
        check(encoder.vqec_vision_ai_cntr_encbk_begin_drain().code_ == status_code::ok);
        check(encoder.vqec_vision_ai_cntr_encbk_submit(make_input(4)).code_ ==
              status_code::invalid_state);
    }

    // Bounded ring sink: capacity, backpressure, generation rejection and drain.
    {
        reference_ring_sink_config config;
        config.capacity_ = 2;
        config.mapping_generation_ = 4;
        config.max_access_unit_bytes_ = 64;
        reference_ring_sink ring(config);
        encoded_sink_demand demand;
        check(ring.vqec_vision_ai_cntr_encsk_query_demand(demand).code_ == status_code::ok);
        check(demand.mapping_generation_ == 4 && demand.active_consumers_ == 0);
        ring.vqec_vision_ai_refer_rring_set_active_consumers(2);
        check(ring.vqec_vision_ai_cntr_encsk_query_demand(demand).code_ == status_code::ok);
        check(demand.active_consumers_ == 2);

        std::vector<std::uint8_t> bytes{1, 2, 3, 4};
        h264_access_unit_view unit;
        unit.frame_ = make_input(1).frame_;
        unit.geometry_ = {640, 480};
        unit.is_keyframe_ = true;
        unit.payload_ = {bytes.data(), bytes.size()};

        check(ring.vqec_vision_ai_cntr_encsk_write(unit, 4).code_ == status_code::ok);
        check(ring.vqec_vision_ai_cntr_encsk_write(unit, 4).code_ == status_code::ok);
        check(ring.vqec_vision_ai_refer_rring_get_depth() == 2);
        check(ring.vqec_vision_ai_cntr_encsk_write(unit, 4).code_ ==
              status_code::resource_exhausted);
        check(ring.vqec_vision_ai_refer_rring_get_rejected() == 1);
        ring.vqec_vision_ai_refer_rring_consume_one();
        check(ring.vqec_vision_ai_refer_rring_get_depth() == 1);
        check(ring.vqec_vision_ai_cntr_encsk_write(unit, 4).code_ == status_code::ok);

        // Wrong generation and malformed units are rejected.
        check(ring.vqec_vision_ai_cntr_encsk_write(unit, 9).code_ ==
              status_code::invalid_argument);
        h264_access_unit_view empty;
        empty.frame_ = unit.frame_;
        empty.geometry_ = unit.geometry_;
        check(ring.vqec_vision_ai_cntr_encsk_write(empty, 4).code_ ==
              status_code::invalid_argument);
        std::vector<std::uint8_t> oversize(128, 0U);
        auto big = unit;
        big.payload_ = {oversize.data(), oversize.size()};
        check(ring.vqec_vision_ai_cntr_encsk_write(big, 4).code_ ==
              status_code::invalid_argument);
        check(ring.vqec_vision_ai_refer_rring_get_written() == 3);
    }

    // End-to-end tail: encoder output feeds the sink.
    {
        reference_encoder encoder(reference_encoder_config{2});
        reference_ring_sink ring(reference_ring_sink_config{4, 1, 1024});
        check(encoder.vqec_vision_ai_cntr_encbk_submit(make_input(9)).code_ == status_code::ok);
        encoder_event event;
        check(encoder.vqec_vision_ai_cntr_encbk_poll(event).code_ == status_code::ok);
        check(event.output_ != nullptr);
        const auto view = event.output_->vqec_vision_ai_core_encot_borrow_view();
        check(ring.vqec_vision_ai_cntr_encsk_write(view, 1).code_ == status_code::ok);
        check(ring.vqec_vision_ai_refer_rring_get_written() == 1 &&
              ring.vqec_vision_ai_refer_rring_get_last_bytes() == view.payload_.size_);
    }

    std::cout << "reference output failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
