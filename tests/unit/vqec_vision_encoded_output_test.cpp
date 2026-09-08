#include <iostream>

#include "vqec/vision/ai/contracts/vqec_vision_encoded_output.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_encoded_sink.hpp"

int main() {
    using namespace vqec::vision::ai;
    unsigned failures = 0;
    const auto check = [&](bool _condition) {
        if (!_condition) {
            ++failures;
        }
    };
    const preview_frame_key frame{0, 0, 1, 42, 10};
    const preview_geometry geometry{4, 2};
    std::shared_ptr<const owned_h264_output> owned;
    {
        std::uint8_t payload[]{0, 0, 1, 0x65};
        std::uint8_t sps[]{0x67, 1};
        std::uint8_t pps[]{0x68, 2};
        const h264_access_unit_view input{frame, geometry, true,
            {payload, sizeof(payload)}, {sps, sizeof(sps)}, {pps, sizeof(pps)}};
        check(owned_h264_output::vqec_vision_ai_core_encot_copy_output(
            input, frame, geometry, owned).code_ == status_code::ok);
        payload[3] = 0;
        sps[0] = 0;
        pps[0] = 0;
    }
    if (!owned) {
        return 1;
    }
    auto view = owned->vqec_vision_ai_core_encot_borrow_view();
    check(view.payload_.size_ == 4 && view.payload_.data_[3] == 0x65);
    check(view.sps_.size_ == 2 && view.sps_.data_[0] == 0x67);
    check(view.pps_.size_ == 2 && view.pps_.data_[0] == 0x68);
    check(view.frame_.frame_id_ == 42 && view.geometry_.width_ == 4 && view.is_keyframe_);
    const auto* identity = owned.get();
    auto wrong_frame = frame;
    ++wrong_frame.source_epoch_;
    check(owned_h264_output::vqec_vision_ai_core_encot_copy_output(
        view, wrong_frame, geometry, owned).code_ == status_code::invalid_state);
    check(owned.get() == identity);
    auto malformed = view;
    malformed.payload_.data_ = nullptr;
    check(owned_h264_output::vqec_vision_ai_core_encot_copy_output(
        malformed, frame, geometry, owned).code_ == status_code::invalid_argument);
    check(owned.get() == identity);
    malformed = view;
    malformed.payload_.size_ = 2U * 1024 * 1024 + 1;
    check(owned_h264_output::vqec_vision_ai_core_encot_copy_output(
        malformed, frame, geometry, owned).code_ == status_code::resource_exhausted);
    check(owned.get() == identity);
    // Copy from the current output's own borrow; old owner remains alive until copying completes.
    check(owned_h264_output::vqec_vision_ai_core_encot_copy_output(
        view, frame, geometry, owned).code_ == status_code::ok);
    view = owned->vqec_vision_ai_core_encot_borrow_view();
    view.sps_ = {};
    view.pps_ = {};
    check(owned_h264_output::vqec_vision_ai_core_encot_copy_output(
        view, frame, geometry, owned).code_ == status_code::ok);
    view = owned->vqec_vision_ai_core_encot_borrow_view();
    check(view.sps_.size_ == 0 && view.pps_.size_ == 0 && view.payload_.data_[3] == 0x65);
    auto reader = owned;
    std::weak_ptr<const owned_h264_output> observer = owned;
    owned.reset();
    check(!observer.expired() && reader->vqec_vision_ai_core_encot_borrow_view().payload_.size_ == 4);
    reader.reset();
    check(observer.expired());
    const encoded_sink_demand initial_demand;
    check(initial_demand.mapping_generation_ == 0 && initial_demand.active_consumers_ == 0);
    std::cout << "encoded output failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
