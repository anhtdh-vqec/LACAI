#include <cassert>
#include <string>
#include <utility>

#include "vqec_vision_feature_event_dispatch.hpp"

using namespace vqec::vision::ai;

namespace {

class fake_event_sink final : public feature_event_sink_port {
public:
    [[nodiscard]] status vqec_vision_ai_ports_fesnk_deliver_event(
        const feature_event& _event) override {
        ++deliveries_;
        last_event_id_ = _event.event_id_;
        return result_;
    }

    status result_{};
    unsigned deliveries_{0};
    std::string last_event_id_;
};

feature_event_batch vqec_vision_ai_ctest_fedct_make_batch(
    const feature_processor_config& _config) {
    const preview_frame_key frame{1, 0, 3, 8, 20};
    feature_event event{frame,
                        _config.source_id_,
                        _config.feature_id_,
                        "event:3:8",
                        "security.intrusion",
                        "1",
                        feature_event_kind::episode_opened,
                        20,
                        _config.config_revision_,
                        {"person_detector:4"},
                        {7},
                        {{"zone.id", "1", "front", 1.0F, observation_quality::high}},
                        {}};
    return {frame, {640, 360}, {std::move(event)}};
}

}  // namespace

int main() {
    const feature_processor_config config{"source.front", "intrusion", 4, 4, 8, 8};
    output_gate gate;
    output_policy policy{1, 10, 100, {{"source.front", "intrusion", {"zone.id"}}}};
    assert(gate.vqec_vision_ai_core_otgat_apply_policy(policy, 0).code_ == status_code::ok);
    fake_event_sink sink;
    auto batch = vqec_vision_ai_ctest_fedct_make_batch(config);
    assert(vqec_vision_ai_outpt_ftdsp_dispatch_event(
               batch, 0, config, 1, 20, gate, sink).code_ == status_code::ok);
    assert(sink.deliveries_ == 1U && sink.last_event_id_ == "event:3:8");

    batch.events_[0].fields_.push_back(
        {"human.face_embedding", "1", "opaque", 0.8F, observation_quality::high});
    assert(vqec_vision_ai_outpt_ftdsp_dispatch_event(
               batch, 0, config, 1, 21, gate, sink).code_ == status_code::unauthorized);
    assert(sink.deliveries_ == 1U);
    batch.events_[0].fields_.pop_back();

    sink.result_ = {status_code::io_error, "synthetic delivery failure"};
    assert(vqec_vision_ai_outpt_ftdsp_dispatch_event(
               batch, 0, config, 1, 22, gate, sink).code_ == status_code::io_error);
    assert(sink.deliveries_ == 2U && sink.last_event_id_ == "event:3:8");
    assert(vqec_vision_ai_outpt_ftdsp_dispatch_event(
               batch, 0, config, 2, 23, gate, sink).code_ == status_code::unauthorized);
    assert(sink.deliveries_ == 2U);
    return 0;
}
