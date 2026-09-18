#include <iostream>

#include "vqec/vision/ai/contracts/vqec_vision_encoder_backend.hpp"

int main() {
    using namespace vqec::vision::ai;
    unsigned failures = 0;
    const auto check = [&](bool _condition) {
        if (!_condition) {
            ++failures;
        }
    };
    const preview_frame_key frame{0, 0, 1, 0, 0};
    const preview_geometry geometry{4, 2};
    const submission_ticket ticket{{1, 1}, 1, 0, 0, 0};
    encoder_input input{frame, geometry, ticket, 7,
        std::make_shared<const std::vector<std::uint8_t>>(12)};
    const auto validate = [&]() {
        return vqec_vision_ai_core_encct_validate_input(input, frame, geometry, ticket, 7);
    };
    check(validate().code_ == status_code::ok); // Original zero frame ID/PTS is valid.
    input.dispatch_generation_ = 8;
    check(validate().code_ == status_code::invalid_state);
    input.dispatch_generation_ = 7;
    ++input.ticket_.token_.job_id_;
    check(validate().code_ == status_code::invalid_state);
    input.ticket_ = ticket;
    ++input.ticket_.source_epoch_;
    check(validate().code_ == status_code::invalid_argument);
    input.ticket_ = ticket;
    ++input.ticket_.source_frame_id_;
    check(validate().code_ == status_code::invalid_argument);
    input.ticket_ = ticket;
    input.geometry_ = {2, 4}; // Same byte count, wrong shape.
    check(validate().code_ == status_code::invalid_state);
    input.geometry_ = geometry;
    input.ticket_.source_pts_ns_ = 1;
    check(validate().code_ == status_code::invalid_argument);
    input.ticket_ = ticket;
    for (const unsigned size : {0U, 11U, 13U}) {
        input.pixels_ = std::make_shared<const std::vector<std::uint8_t>>(size);
        check(validate().code_ == status_code::invalid_argument);
    }
    input.pixels_.reset();
    check(validate().code_ == status_code::invalid_argument);
    encoder_event event;
    check(vqec_vision_ai_core_encct_validate_event(event).code_ == status_code::protocol_error);
    event.detail_ = {status_code::io_error, "synthetic backend fault"};
    check(vqec_vision_ai_core_encct_validate_event(event).code_ == status_code::ok);
    event.token_ = {1, 0};
    check(vqec_vision_ai_core_encct_validate_event(event).code_ == status_code::protocol_error);
    event.token_ = {1, 1};
    event.kind_ = encoder_event_kind::input_complete;
    check(vqec_vision_ai_core_encct_validate_event(event).code_ == status_code::protocol_error);
    event.detail_ = {};
    check(vqec_vision_ai_core_encct_validate_event(event).code_ == status_code::ok);
    event.kind_ = encoder_event_kind::output_dropped;
    check(vqec_vision_ai_core_encct_validate_event(event).code_ == status_code::ok);
    event.kind_ = encoder_event_kind::output_ready;
    check(vqec_vision_ai_core_encct_validate_event(event).code_ == status_code::protocol_error);
    const std::uint8_t bytes[]{0, 0, 1, 0x65};
    const h264_access_unit_view view{frame, geometry, true, {bytes, sizeof(bytes)}, {}, {}};
    check(owned_h264_output::vqec_vision_ai_core_encot_copy_output(
        view, frame, geometry, event.output_).code_ == status_code::ok);
    check(vqec_vision_ai_core_encct_validate_event(event).code_ == status_code::ok);
    event.kind_ = encoder_event_kind::input_complete;
    check(vqec_vision_ai_core_encct_validate_event(event).code_ == status_code::protocol_error);
    event.kind_ = static_cast<encoder_event_kind>(99);
    check(vqec_vision_ai_core_encct_validate_event(event).code_ == status_code::protocol_error);
    std::cout << "encoder contract failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
