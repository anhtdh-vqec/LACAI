#include <iostream>

#include "vqec/vision/ai/contracts/vqec_vision_feature_event.hpp"

int main() {
    using namespace vqec::vision::ai;
    unsigned failures = 0;
    const auto check = [&](bool _condition) {
        if (!_condition) {
            ++failures;
        }
    };

    const preview_frame_key frame{3, 1, 7, 20, 100};
    const preview_geometry geometry{1280, 720};
    const feature_processor_config config{"source.front", "intrusion", 5, 4, 8, 8};
    feature_event event{frame,
                        "source.front",
                        "intrusion",
                        "event:7:1",
                        "security.intrusion",
                        "1",
                        feature_event_kind::episode_opened,
                        100,
                        5,
                        {"person_detector:4"},
                        {11},
                        {{"zone.id", "1", "front_door", 1.0F,
                          observation_quality::high}},
                        "evidence:7:1"};
    feature_event_batch batch{frame, geometry, {event}};
    check(vqec_vision_ai_core_ftevt_validate_batch(batch, frame, geometry, config).code_ ==
          status_code::ok);
    batch.events_[0].track_ids_.push_back(11);
    check(vqec_vision_ai_core_ftevt_validate_batch(batch, frame, geometry, config).code_ ==
          status_code::invalid_argument);
    batch.events_[0].track_ids_.pop_back();
    batch.events_[0].occurred_at_ns_ = 101;
    check(vqec_vision_ai_core_ftevt_validate_batch(batch, frame, geometry, config).code_ ==
          status_code::invalid_argument);
    batch.events_[0].occurred_at_ns_ = 100;
    batch.events_[0].fields_[0].quality_ = static_cast<observation_quality>(99);
    check(vqec_vision_ai_core_ftevt_validate_batch(batch, frame, geometry, config).code_ ==
          status_code::invalid_argument);
    batch.events_[0].fields_[0].quality_ = observation_quality::high;
    batch.events_[0].fields_.push_back(
        {"zone.id", "2", "secondary", 1.0F, observation_quality::high});
    check(vqec_vision_ai_core_ftevt_validate_batch(batch, frame, geometry, config).code_ ==
          status_code::invalid_argument);
    batch.events_[0].fields_.pop_back();
    batch.events_.push_back(batch.events_[0]);
    check(vqec_vision_ai_core_ftevt_validate_batch(batch, frame, geometry, config).code_ ==
          status_code::invalid_argument);
    batch.events_.pop_back();
    auto narrow = config;
    narrow.max_events_per_update_ = 0;
    check(vqec_vision_ai_core_ftevt_validate_processor_config(narrow).code_ ==
          status_code::invalid_argument);
    std::cout << "feature event failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
