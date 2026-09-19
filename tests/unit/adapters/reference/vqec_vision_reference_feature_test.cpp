// Device-free tests for the neutral reference feature processor: ROI enter/exit, dwell,
// cooldown suppression, line crossing and count-on-change.

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "vqec_vision_reference_feature.hpp"

using namespace vqec::vision::ai;

namespace {

feature_processor_config make_config() {
    feature_processor_config config;
    config.source_id_ = "cam0";
    config.feature_id_ = "zone";
    config.config_revision_ = 1;
    config.max_events_per_update_ = 8;
    config.max_track_references_per_event_ = 1;
    config.max_fields_per_event_ = 1;
    return config;
}

observation make_observation(std::uint64_t _track, float _cx, float _cy,
    const std::string& _class_id = "person") {
    observation item;
    item.frame_.source_epoch_ = 1;
    item.frame_.frame_id_ = 1;
    item.frame_.source_pts_ns_ = 1234;
    item.track_id_ = _track;
    item.class_id_ = _class_id;
    item.box_ = {_cx - 5.0F, _cy - 5.0F, 10.0F, 10.0F, 0xffffffffU, _class_id};
    item.confidence_ = 0.9F;
    item.quality_ = observation_quality::high;
    return item;
}

observation_batch make_batch(const std::vector<observation>& _items) {
    observation_batch batch;
    batch.frame_.source_epoch_ = 1;
    batch.frame_.source_pts_ns_ = 1234;
    batch.geometry_ = {640, 480};
    batch.observations_ = _items;
    return batch;
}

}  // namespace

int main() {
    unsigned failures = 0;
    const auto check = [&failures](bool _condition) {
        if (!_condition) {
            ++failures;
        }
    };

    // Configuration validation.
    {
        reference_feature_params params;
        params.mode_ = reference_feature_mode::roi_presence;
        params.zone_ = {0, 0, 100, 100, 0xffffffffU, ""};
        reference_zone_feature feature(params);
        auto bad = make_config();
        bad.max_events_per_update_ = 0;
        check(feature.vqec_vision_ai_ports_ftpro_validate_activation(bad).code_ ==
              status_code::invalid_argument);
        params.zone_.width_ = 0;
        reference_zone_feature invalid_zone(params);
        check(invalid_zone.vqec_vision_ai_ports_ftpro_validate_activation(make_config()).code_ ==
              status_code::invalid_argument);
    }

    // ROI enter/exit with cooldown suppression.
    {
        reference_feature_params params;
        params.mode_ = reference_feature_mode::roi_presence;
        params.zone_ = {0, 0, 100, 100, 0xffffffffU, ""};
        params.cooldown_ns_ = 1000;
        reference_zone_feature feature(params);
        check(feature.vqec_vision_ai_ports_ftpro_validate_activation(make_config()).code_ ==
              status_code::ok);
        check(feature.vqec_vision_ai_ports_ftpro_reset_epoch(1).code_ == status_code::ok);
        feature_event_batch events;
        check(feature.vqec_vision_ai_ports_ftpro_process_observations(
                  make_batch({make_observation(1, 50, 50)}), 5000, false, events).code_ ==
              status_code::ok);
        check(events.events_.size() == 1 &&
              events.events_[0].kind_ == feature_event_kind::episode_opened &&
              events.events_[0].track_ids_.size() == 1 &&
              events.events_[0].track_ids_[0] == 1);
        // Event time is the frame's source PTS, never the monotonic step clock.
        check(events.events_[0].occurred_at_ns_ == 1234);

        // Exit inside the cooldown window is suppressed.
        check(feature.vqec_vision_ai_ports_ftpro_process_observations(
                  make_batch({make_observation(1, 300, 300)}), 5100, false, events).code_ ==
              status_code::ok);
        check(events.events_.empty());

        // After the cooldown a re-entry emits again.
        check(feature.vqec_vision_ai_ports_ftpro_process_observations(
                  make_batch({make_observation(1, 50, 50)}), 7000, false, events).code_ ==
              status_code::ok);
        check(events.events_.size() == 1 &&
              events.events_[0].kind_ == feature_event_kind::episode_opened);
        check(events.events_[0].episode_revision_ == 1 &&
              events.events_[0].supersedes_episode_revision_ == 0 &&
              events.events_[0].episode_begin_ns_ == events.events_[0].occurred_at_ns_);

        // A source gap does not fabricate an event.
        check(feature.vqec_vision_ai_ports_ftpro_process_observations(
                  make_batch({}), 8000, true, events).code_ == status_code::ok);
        check(events.events_.empty());
    }

    // Dwell: one episode_updated after the configured stay.
    {
        reference_feature_params params;
        params.mode_ = reference_feature_mode::roi_presence;
        params.zone_ = {0, 0, 100, 100, 0xffffffffU, ""};
        params.dwell_ns_ = 1000;
        reference_zone_feature feature(params);
        check(feature.vqec_vision_ai_ports_ftpro_validate_activation(make_config()).code_ ==
              status_code::ok);
        check(feature.vqec_vision_ai_ports_ftpro_reset_epoch(1).code_ == status_code::ok);
        feature_event_batch events;
        check(feature.vqec_vision_ai_ports_ftpro_process_observations(
                  make_batch({make_observation(2, 50, 50)}), 1000, false, events).code_ ==
              status_code::ok);
        check(events.events_.size() == 1 &&
              events.events_[0].kind_ == feature_event_kind::episode_opened);
        const auto episode_id = events.events_[0].event_id_;
        check(events.events_[0].episode_revision_ == 1 &&
              events.events_[0].episode_begin_ns_ == events.events_[0].occurred_at_ns_);
        check(feature.vqec_vision_ai_ports_ftpro_process_observations(
                  make_batch({make_observation(2, 50, 50)}), 1500, false, events).code_ ==
              status_code::ok);
        check(events.events_.empty());
        check(feature.vqec_vision_ai_ports_ftpro_process_observations(
                  make_batch({make_observation(2, 50, 50)}), 2100, false, events).code_ ==
              status_code::ok);
        check(events.events_.size() == 1 &&
              events.events_[0].kind_ == feature_event_kind::episode_updated &&
              events.events_[0].event_id_ == episode_id &&
              events.events_[0].episode_revision_ == 2 &&
              events.events_[0].supersedes_episode_revision_ == 1 &&
              events.events_[0].fields_.size() == 1 &&
              events.events_[0].fields_[0].value_ == "1100");
    }

    // Line crossing direction.
    {
        reference_feature_params params;
        params.mode_ = reference_feature_mode::line_crossing;
        params.line_horizontal_ = true;
        params.line_position_ = 100.0F;
        reference_zone_feature feature(params);
        check(feature.vqec_vision_ai_ports_ftpro_validate_activation(make_config()).code_ ==
              status_code::ok);
        check(feature.vqec_vision_ai_ports_ftpro_reset_epoch(1).code_ == status_code::ok);
        feature_event_batch events;
        check(feature.vqec_vision_ai_ports_ftpro_process_observations(
                  make_batch({make_observation(3, 50, 50)}), 1, false, events).code_ ==
              status_code::ok);
        check(events.events_.empty());
        check(feature.vqec_vision_ai_ports_ftpro_process_observations(
                  make_batch({make_observation(3, 50, 150)}), 2, false, events).code_ ==
              status_code::ok);
        check(events.events_.size() == 1 && events.events_[0].fields_[0].value_ == "positive");
        check(feature.vqec_vision_ai_ports_ftpro_process_observations(
                  make_batch({make_observation(3, 50, 50)}), 3, false, events).code_ ==
              status_code::ok);
        check(events.events_.size() == 1 && events.events_[0].fields_[0].value_ == "negative");
        check(feature.vqec_vision_ai_ports_ftpro_process_observations(
                  make_batch({make_observation(3, 50, 50)}), 4, false, events).code_ ==
              status_code::ok);
        check(events.events_.empty());
    }

    // Count emits only when the inside count changes.
    {
        reference_feature_params params;
        params.mode_ = reference_feature_mode::count;
        params.zone_ = {0, 0, 100, 100, 0xffffffffU, ""};
        reference_zone_feature feature(params);
        check(feature.vqec_vision_ai_ports_ftpro_validate_activation(make_config()).code_ ==
              status_code::ok);
        check(feature.vqec_vision_ai_ports_ftpro_reset_epoch(1).code_ == status_code::ok);
        feature_event_batch events;
        check(feature.vqec_vision_ai_ports_ftpro_process_observations(
                  make_batch({make_observation(1, 50, 50), make_observation(2, 60, 60)}),
                  1, false, events).code_ == status_code::ok);
        check(events.events_.size() == 1 && events.events_[0].fields_[0].value_ == "2");
        check(feature.vqec_vision_ai_ports_ftpro_process_observations(
                  make_batch({make_observation(1, 50, 50), make_observation(2, 60, 60)}),
                  2, false, events).code_ == status_code::ok);
        check(events.events_.empty());
        check(feature.vqec_vision_ai_ports_ftpro_process_observations(
                  make_batch({make_observation(1, 50, 50), make_observation(2, 300, 300)}),
                  3, false, events).code_ == status_code::ok);
        check(events.events_.size() == 1 && events.events_[0].fields_[0].value_ == "1");
    }

    std::cout << "reference feature failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
