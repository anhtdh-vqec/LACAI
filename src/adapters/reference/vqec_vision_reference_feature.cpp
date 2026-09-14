#include "vqec_vision_reference_feature.hpp"

#include <utility>

namespace vqec::vision::ai {

reference_zone_feature::reference_zone_feature(reference_feature_params _params)
    : params_(std::move(_params)) {}

bool reference_zone_feature::vqec_vision_ai_refer_rfeat_matches(
    const observation& _item) const noexcept {
    return params_.class_filter_.empty() || _item.class_id_ == params_.class_filter_;
}

bool reference_zone_feature::vqec_vision_ai_refer_rfeat_inside(
    const observation& _item) const noexcept {
    const float centre_x = _item.box_.x_ + _item.box_.width_ / 2.0F;
    const float centre_y = _item.box_.y_ + _item.box_.height_ / 2.0F;
    return centre_x >= params_.zone_.x_ &&
        centre_x < params_.zone_.x_ + params_.zone_.width_ &&
        centre_y >= params_.zone_.y_ &&
        centre_y < params_.zone_.y_ + params_.zone_.height_;
}

reference_zone_feature::track_state* reference_zone_feature::
vqec_vision_ai_refer_rfeat_find_track(std::uint64_t _track_id) noexcept {
    for (std::uint16_t index = 0; index < track_count_; ++index) {
        if (tracks_[index].track_id_ == _track_id) {
            return &tracks_[index];
        }
    }
    return nullptr;
}

void reference_zone_feature::vqec_vision_ai_refer_rfeat_make_event(
    const observation& _item, feature_event_kind _kind, const std::string& _value,
    feature_event& _event) {
    _event.frame_ = _item.frame_;
    _event.source_id_ = config_.source_id_;
    _event.feature_id_ = config_.feature_id_;
    _event.event_id_ = config_.source_id_ + ":" + config_.feature_id_ + ":" +
        std::to_string(++event_sequence_);
    _event.event_schema_id_ = params_.event_schema_id_;
    _event.event_schema_version_ = params_.event_schema_version_;
    _event.kind_ = _kind;
    // Event time lives in the source frame's PTS domain; the monotonic step clock is
    // only used for dwell/cooldown arithmetic and must not leak into the event.
    _event.occurred_at_ns_ = _item.frame_.source_pts_ns_;
    _event.config_revision_ = config_.config_revision_;
    _event.track_ids_.push_back(_item.track_id_);
    feature_event_field field;
    field.schema_id_ = params_.event_schema_id_;
    field.schema_version_ = params_.event_schema_version_;
    field.value_ = _value;
    field.confidence_ = params_.confidence_;
    field.quality_ = _item.quality_;
    _event.fields_.push_back(std::move(field));
}

status reference_zone_feature::vqec_vision_ai_ports_ftpro_validate_activation(
    const feature_processor_config& _config) const {
    const auto valid = vqec_vision_ai_core_ftevt_validate_processor_config(_config);
    if (valid.code_ != status_code::ok) {
        return valid;
    }
    if (params_.event_schema_id_.empty() || params_.event_schema_version_.empty() ||
        params_.class_filter_.size() > reference_feature_limits::g_max_identifier_bytes) {
        return {status_code::invalid_argument, "invalid reference feature parameters"};
    }
    if (params_.mode_ != reference_feature_mode::line_crossing &&
        (params_.zone_.width_ <= 0.0F || params_.zone_.height_ <= 0.0F)) {
        return {status_code::invalid_argument, "reference zone must have a positive extent"};
    }
    config_ = _config;
    is_configured_ = true;
    return {};
}

status reference_zone_feature::vqec_vision_ai_ports_ftpro_reset_epoch(
    std::uint64_t _source_epoch) {
    if (_source_epoch == 0) {
        return {status_code::invalid_argument, "source epoch must be nonzero"};
    }
    tracks_ = {};
    track_count_ = 0;
    event_sequence_ = 0;
    count_initialized_ = false;
    last_count_ = -1;
    return {};
}

status reference_zone_feature::vqec_vision_ai_ports_ftpro_process_observations(
    const observation_batch& _tracked, std::uint64_t _now_monotonic_ns,
    bool _is_source_gap, feature_event_batch& _events) {
    if (!is_configured_) {
        return {status_code::invalid_state, "reference feature is not activated"};
    }
    feature_event_batch batch;
    batch.frame_ = _tracked.frame_;
    batch.geometry_ = _tracked.geometry_;
    if (_is_source_gap) {
        // Retain zone state across a gap; the stage resets the epoch if it faults.
        _events = std::move(batch);
        return {};
    }
    const std::size_t max_events = config_.max_events_per_update_;

    if (params_.mode_ == reference_feature_mode::count) {
        int inside_count = 0;
        for (const auto& item : _tracked.observations_) {
            if (item.track_id_ != 0 && vqec_vision_ai_refer_rfeat_matches(item) &&
                vqec_vision_ai_refer_rfeat_inside(item)) {
                ++inside_count;
            }
        }
        if (!count_initialized_ || inside_count != last_count_) {
            feature_event event;
            event.frame_ = _tracked.frame_;
            event.source_id_ = config_.source_id_;
            event.feature_id_ = config_.feature_id_;
            event.event_id_ = config_.source_id_ + ":" + config_.feature_id_ + ":" +
                std::to_string(++event_sequence_);
            event.event_schema_id_ = params_.event_schema_id_;
            event.event_schema_version_ = params_.event_schema_version_;
            event.kind_ = feature_event_kind::snapshot;
            event.occurred_at_ns_ = _tracked.frame_.source_pts_ns_;
            event.config_revision_ = config_.config_revision_;
            feature_event_field field;
            field.schema_id_ = params_.event_schema_id_;
            field.schema_version_ = params_.event_schema_version_;
            field.value_ = std::to_string(inside_count);
            field.confidence_ = params_.confidence_;
            field.quality_ = observation_quality::high;
            event.fields_.push_back(std::move(field));
            batch.events_.push_back(std::move(event));
        }
        count_initialized_ = true;
        last_count_ = inside_count;
        _events = std::move(batch);
        return {};
    }

    for (const auto& item : _tracked.observations_) {
        if (item.track_id_ == 0 || !vqec_vision_ai_refer_rfeat_matches(item)) {
            continue;
        }
        auto* state = vqec_vision_ai_refer_rfeat_find_track(item.track_id_);
        if (state == nullptr) {
            if (track_count_ >= reference_feature_limits::g_max_tracks) {
                return {status_code::resource_exhausted, "reference feature track state is full"};
            }
            state = &tracks_[track_count_++];
            *state = {};
            state->track_id_ = item.track_id_;
        }
        const bool cooldown_ok = state->last_event_ns_ == 0 ||
            _now_monotonic_ns - state->last_event_ns_ >= params_.cooldown_ns_;

        if (params_.mode_ == reference_feature_mode::roi_presence) {
            const bool inside = vqec_vision_ai_refer_rfeat_inside(item);
            if (inside && !state->inside_) {
                state->inside_ = true;
                state->entered_ns_ = _now_monotonic_ns;
                state->dwell_emitted_ = false;
                if (cooldown_ok) {
                    feature_event event;
                    vqec_vision_ai_refer_rfeat_make_event(
                        item, feature_event_kind::episode_opened, "inside", event);
                    state->last_event_ns_ = _now_monotonic_ns;
                    batch.events_.push_back(std::move(event));
                }
            } else if (!inside && state->inside_) {
                state->inside_ = false;
                state->dwell_emitted_ = false;
                if (cooldown_ok) {
                    feature_event event;
                    vqec_vision_ai_refer_rfeat_make_event(
                        item, feature_event_kind::episode_closed, "outside", event);
                    state->last_event_ns_ = _now_monotonic_ns;
                    batch.events_.push_back(std::move(event));
                }
            } else if (inside && params_.dwell_ns_ != 0 && !state->dwell_emitted_ &&
                       _now_monotonic_ns - state->entered_ns_ >= params_.dwell_ns_) {
                feature_event event;
                vqec_vision_ai_refer_rfeat_make_event(item, feature_event_kind::episode_updated,
                    std::to_string(_now_monotonic_ns - state->entered_ns_), event);
                state->dwell_emitted_ = true;
                state->last_event_ns_ = _now_monotonic_ns;
                batch.events_.push_back(std::move(event));
            }
        } else {
            const float position = params_.line_horizontal_ ?
                (item.box_.y_ + item.box_.height_ / 2.0F) :
                (item.box_.x_ + item.box_.width_ / 2.0F);
            const bool side = position > params_.line_position_;
            if (!state->seen_) {
                state->seen_ = true;
                state->side_ = side;
            } else if (side != state->side_) {
                state->side_ = side;
                if (cooldown_ok) {
                    feature_event event;
                    vqec_vision_ai_refer_rfeat_make_event(
                        item, feature_event_kind::snapshot, side ? "positive" : "negative",
                        event);
                    state->last_event_ns_ = _now_monotonic_ns;
                    batch.events_.push_back(std::move(event));
                }
            }
        }
        if (batch.events_.size() >= max_events) {
            break;
        }
    }
    _events = std::move(batch);
    return {};
}

}  // namespace vqec::vision::ai
