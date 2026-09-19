#include "vqec_vision_fire_smoke_alarm.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>

#include "vqec/vision/ai/contracts/vqec_vision_version_registry.h"
#include "vqec/vision/ai/contracts/vqec_vision_identifier.hpp"

namespace vqec::vision::ai {
namespace {

constexpr char g_class_field[] = "security.fire_smoke.class";
constexpr char g_severity_field[] = "security.fire_smoke.severity";
constexpr char g_region_field[] = "security.fire_smoke.region";
constexpr char g_reason_field[] = "security.fire_smoke.reason";
constexpr char g_scene_field[] = "security.fire_smoke.scene_revision";
constexpr char g_evidence_field[] = "security.fire_smoke.evidence";
constexpr char g_reason_detected[] = "confirmed";
constexpr char g_reason_update[] = "active";
constexpr char g_reason_cleared[] = "cleared";
constexpr char g_reason_gap[] = "source_gap";

float vqec_vision_ai_fires_fsalm_intersection_over_union(
    const overlay_box& _left, const overlay_box& _right) noexcept {
    const float left = std::max(_left.x_, _right.x_);
    const float top = std::max(_left.y_, _right.y_);
    const float right = std::min(_left.x_ + _left.width_, _right.x_ + _right.width_);
    const float bottom = std::min(_left.y_ + _left.height_, _right.y_ + _right.height_);
    const float intersection = std::max(0.0F, right - left) *
        std::max(0.0F, bottom - top);
    const float combined = _left.width_ * _left.height_ +
        _right.width_ * _right.height_ - intersection;
    return combined > 0.0F ? intersection / combined : 0.0F;
}

feature_event_field vqec_vision_ai_fires_fsalm_make_field(
    const char* _schema_id, std::string _value, float _confidence,
    observation_quality _quality) {
    feature_event_field field;
    field.schema_id_ = _schema_id;
    field.schema_version_ = VQEC_VISION_AI_BASELINE_VERSION_TEXT;
    field.value_ = std::move(_value);
    field.confidence_ = _confidence;
    field.quality_ = _quality;
    return field;
}

}  // namespace

status vqec_vision_ai_fires_fsalm_validate_config(
    const fire_smoke_alarm_config& _config) noexcept {
    if ((!_config.fire_enabled_ && !_config.smoke_enabled_) ||
        !std::isfinite(_config.fire_alarm_confidence_) ||
        !std::isfinite(_config.smoke_alarm_confidence_) ||
        !std::isfinite(_config.minimum_region_area_ratio_) ||
        !std::isfinite(_config.association_iou_) ||
        !std::isfinite(_config.high_confidence_) ||
        !vqec_vision_ai_cntr_ident_is_valid(
            _config.model_version_id_, fire_smoke_limits::g_max_profile_bytes) ||
        _config.fire_alarm_confidence_ < 0.25F ||
        _config.fire_alarm_confidence_ > 1.0F ||
        _config.smoke_alarm_confidence_ < 0.25F ||
        _config.smoke_alarm_confidence_ > 1.0F ||
        _config.minimum_region_area_ratio_ < 0.0001F ||
        _config.minimum_region_area_ratio_ > 1.0F ||
        _config.confirmation_count_ == 0 || _config.confirmation_count_ > 300 ||
        _config.clear_count_ == 0 || _config.clear_count_ > 300 ||
        _config.confirmation_duration_ns_ > 60000000000ULL ||
        _config.clear_duration_ns_ > 300000000000ULL ||
        _config.update_interval_ns_ < 100000000ULL ||
        _config.update_interval_ns_ > 60000000000ULL ||
        _config.association_iou_ < 0.0F ||
        _config.association_iou_ > 1.0F || _config.scene_revision_ == 0 ||
        _config.max_active_incidents_ == 0 ||
        _config.max_active_incidents_ > fire_smoke_limits::g_max_incidents ||
        _config.high_confidence_ < 0.25F || _config.high_confidence_ > 1.0F ||
        _config.critical_duration_ns_ > 3600000000000ULL ||
        _config.evidence_profile_ref_.empty() ||
        _config.evidence_profile_ref_.size() > fire_smoke_limits::g_max_profile_bytes ||
        _config.evidence_pre_duration_ms_ > 30000ULL ||
        _config.evidence_post_duration_ms_ > 30000ULL ||
        _config.evidence_retry_deadline_ms_ < 1000ULL ||
        _config.evidence_retry_deadline_ms_ > 86400000ULL ||
        (_config.evidence_enabled_ && !_config.evidence_snapshot_ &&
         !_config.evidence_clip_)) {
        return {status_code::invalid_argument, "invalid fire/smoke alarm configuration"};
    }
    return {};
}

fire_smoke_alarm::fire_smoke_alarm(fire_smoke_alarm_config _config)
    : alarm_config_(std::move(_config)) {}

status fire_smoke_alarm::vqec_vision_ai_ports_ftpro_validate_activation(
    const feature_processor_config& _config) const {
    const auto processor_valid =
        vqec_vision_ai_core_ftevt_validate_processor_config(_config);
    if (processor_valid.code_ != status_code::ok) {
        return processor_valid;
    }
    const auto alarm_valid = vqec_vision_ai_fires_fsalm_validate_config(alarm_config_);
    if (alarm_valid.code_ != status_code::ok) {
        return alarm_valid;
    }
    if (_config.max_fields_per_event_ < 6U ||
        _config.max_events_per_update_ < alarm_config_.max_active_incidents_) {
        return {status_code::resource_exhausted,
            "feature resource profile cannot contain configured incidents"};
    }
    processor_config_ = _config;
    configured_ = true;
    return {};
}

status fire_smoke_alarm::vqec_vision_ai_ports_ftpro_reset_epoch(
    std::uint64_t _source_epoch) {
    if (_source_epoch == 0) {
        return {status_code::invalid_argument, "source epoch must be nonzero"};
    }
    incidents_ = {};
    source_epoch_ = _source_epoch;
    event_sequence_ = 0;
    return {};
}

bool fire_smoke_alarm::vqec_vision_ai_fires_fsalm_matches(
    const observation& _item, const preview_geometry& _geometry) const noexcept {
    const bool enabled =
        (_item.class_id_ == fire_smoke_limits::g_fire_class_id &&
         alarm_config_.fire_enabled_) ||
        (_item.class_id_ == fire_smoke_limits::g_smoke_class_id &&
         alarm_config_.smoke_enabled_);
    const float threshold = _item.class_id_ == fire_smoke_limits::g_fire_class_id ?
        alarm_config_.fire_alarm_confidence_ :
        alarm_config_.smoke_alarm_confidence_;
    const double frame_area = static_cast<double>(_geometry.width_) *
        static_cast<double>(_geometry.height_);
    const double region_area = static_cast<double>(_item.box_.width_) *
        static_cast<double>(_item.box_.height_);
    return enabled && _item.confidence_ >= threshold && frame_area > 0.0 &&
        region_area / frame_area >= alarm_config_.minimum_region_area_ratio_;
}

fire_smoke_alarm::incident_state*
fire_smoke_alarm::vqec_vision_ai_fires_fsalm_find_or_allocate(
    const observation& _item) noexcept {
    incident_state* best = nullptr;
    float best_iou = alarm_config_.association_iou_;
    for (std::size_t index = 0; index < alarm_config_.max_active_incidents_; ++index) {
        auto& incident = incidents_[index];
        if (!incident.occupied_ || incident.class_id_ != _item.class_id_) {
            continue;
        }
        if (incident.track_id_ != 0 && incident.track_id_ == _item.track_id_) {
            return &incident;
        }
        const float iou = vqec_vision_ai_fires_fsalm_intersection_over_union(
            incident.region_, _item.box_);
        if (iou >= best_iou) {
            best_iou = iou;
            best = &incident;
        }
    }
    if (best != nullptr) {
        return best;
    }
    for (std::size_t index = 0; index < alarm_config_.max_active_incidents_; ++index) {
        if (!incidents_[index].occupied_) {
            incidents_[index] = {};
            incidents_[index].occupied_ = true;
            incidents_[index].class_id_ = _item.class_id_;
            return &incidents_[index];
        }
    }
    return nullptr;
}

void fire_smoke_alarm::vqec_vision_ai_fires_fsalm_emit(
    incident_state& _incident, const preview_frame_key& _frame,
    feature_event_kind _kind, const char* _reason, feature_event& _event) {
    if (_kind == feature_event_kind::episode_opened) {
        char identifier[128]{};
        std::snprintf(identifier, sizeof(identifier), "fs:%u:%u:%llu:%llu",
            _frame.camera_id_, _frame.channel_id_,
            static_cast<unsigned long long>(_frame.source_epoch_),
            static_cast<unsigned long long>(++event_sequence_));
        _incident.event_id_ = identifier;
        _incident.episode_revision_ = 1;
        _incident.episode_begin_pts_ns_ = _frame.source_pts_ns_;
    } else {
        ++_incident.episode_revision_;
    }
    _event.frame_ = _frame;
    _event.source_id_ = processor_config_.source_id_;
    _event.feature_id_ = processor_config_.feature_id_;
    _event.event_id_ = _incident.event_id_;
    _event.event_schema_id_ = fire_smoke_limits::g_event_schema_id;
    _event.event_schema_version_ = fire_smoke_limits::g_event_schema_version;
    _event.kind_ = _kind;
    _event.occurred_at_ns_ = _frame.source_pts_ns_;
    _event.config_revision_ = processor_config_.config_revision_;
    _event.model_version_ids_.push_back(alarm_config_.model_version_id_);
    _event.episode_revision_ = _incident.episode_revision_;
    _event.supersedes_episode_revision_ = _incident.episode_revision_ - 1U;
    _event.episode_begin_ns_ = _incident.episode_begin_pts_ns_;
    if (_incident.track_id_ != 0) {
        _event.track_ids_.push_back(_incident.track_id_);
    }
    const std::uint64_t duration_ns = _frame.source_pts_ns_ >=
            _incident.first_seen_pts_ns_ ?
        _frame.source_pts_ns_ - _incident.first_seen_pts_ns_ : 0;
    const bool critical = _incident.max_confidence_ >= alarm_config_.high_confidence_ ||
        duration_ns >= alarm_config_.critical_duration_ns_;
    char region[160]{};
    std::snprintf(region, sizeof(region), "%.3f,%.3f,%.3f,%.3f",
        static_cast<double>(_incident.region_.x_),
        static_cast<double>(_incident.region_.y_),
        static_cast<double>(_incident.region_.width_),
        static_cast<double>(_incident.region_.height_));
    _event.fields_.push_back(vqec_vision_ai_fires_fsalm_make_field(
        g_class_field, _incident.class_id_, _incident.max_confidence_,
        observation_quality::high));
    _event.fields_.push_back(vqec_vision_ai_fires_fsalm_make_field(
        g_severity_field, critical ? "critical" : "alarm",
        _incident.max_confidence_, observation_quality::high));
    _event.fields_.push_back(vqec_vision_ai_fires_fsalm_make_field(
        g_region_field, region, _incident.max_confidence_, observation_quality::high));
    _event.fields_.push_back(vqec_vision_ai_fires_fsalm_make_field(
        g_reason_field, _reason, _incident.max_confidence_, observation_quality::high));
    _event.fields_.push_back(vqec_vision_ai_fires_fsalm_make_field(
        g_scene_field, std::to_string(alarm_config_.scene_revision_),
        _incident.max_confidence_, observation_quality::high));
    if (alarm_config_.evidence_enabled_ &&
        _kind == feature_event_kind::episode_opened) {
        _event.evidence_request_id_ = _incident.event_id_ + ":e";
        std::string evidence = alarm_config_.evidence_profile_ref_ + "," +
            std::to_string(alarm_config_.evidence_pre_duration_ms_) + "," +
            std::to_string(alarm_config_.evidence_post_duration_ms_) + "," +
            (alarm_config_.evidence_snapshot_ ? "snapshot" : "no_snapshot") + "," +
            (alarm_config_.evidence_clip_ ? "clip" : "no_clip") + "," +
            std::to_string(alarm_config_.evidence_retry_deadline_ms_);
        _event.fields_.push_back(vqec_vision_ai_fires_fsalm_make_field(
            g_evidence_field, std::move(evidence), _incident.max_confidence_,
            observation_quality::high));
    }
}

void fire_smoke_alarm::vqec_vision_ai_fires_fsalm_clear_incident(
    incident_state& _incident) noexcept {
    _incident = {};
}

status fire_smoke_alarm::vqec_vision_ai_ports_ftpro_process_observations(
    const observation_batch& _tracked, std::uint64_t _now_monotonic_ns,
    bool _is_source_gap, feature_event_batch& _events) {
    (void)_now_monotonic_ns;
    if (!configured_ || source_epoch_ == 0 ||
        _tracked.frame_.source_epoch_ != source_epoch_) {
        return {status_code::invalid_state, "fire/smoke alarm epoch is not active"};
    }
    feature_event_batch batch;
    batch.frame_ = _tracked.frame_;
    batch.geometry_ = _tracked.geometry_;
    for (auto& incident : incidents_) {
        incident.seen_ = false;
    }

    if (_is_source_gap) {
        if (alarm_config_.source_gap_policy_ == fire_smoke_gap_policy::interrupt) {
            for (auto& incident : incidents_) {
                if (incident.occupied_ && incident.active_) {
                    feature_event event;
                    vqec_vision_ai_fires_fsalm_emit(incident, _tracked.frame_,
                        feature_event_kind::episode_closed, g_reason_gap, event);
                    batch.events_.push_back(std::move(event));
                }
                vqec_vision_ai_fires_fsalm_clear_incident(incident);
            }
        }
        _events = std::move(batch);
        return {};
    }

    for (const auto& item : _tracked.observations_) {
        if (!vqec_vision_ai_fires_fsalm_matches(item, _tracked.geometry_)) {
            continue;
        }
        auto* incident = vqec_vision_ai_fires_fsalm_find_or_allocate(item);
        if (incident == nullptr) {
            return {status_code::resource_exhausted,
                "fire/smoke incident capacity reached"};
        }
        if (incident->first_seen_pts_ns_ == 0) {
            incident->first_seen_pts_ns_ = _tracked.frame_.source_pts_ns_;
        }
        incident->seen_ = true;
        incident->region_ = item.box_;
        incident->track_id_ = item.track_id_;
        incident->last_seen_pts_ns_ = _tracked.frame_.source_pts_ns_;
        incident->max_confidence_ = std::max(incident->max_confidence_, item.confidence_);
        incident->clear_count_ = 0;
        if (incident->last_observed_frame_id_ != _tracked.frame_.frame_id_ &&
            incident->confirmation_count_ != UINT32_MAX) {
            ++incident->confirmation_count_;
        }
        incident->last_observed_frame_id_ = _tracked.frame_.frame_id_;
        const std::uint64_t candidate_duration = _tracked.frame_.source_pts_ns_ >=
                incident->first_seen_pts_ns_ ?
            _tracked.frame_.source_pts_ns_ - incident->first_seen_pts_ns_ : 0;
        if (!incident->active_ &&
            incident->confirmation_count_ >= alarm_config_.confirmation_count_ &&
            candidate_duration >= alarm_config_.confirmation_duration_ns_) {
            incident->active_ = true;
            incident->last_update_pts_ns_ = _tracked.frame_.source_pts_ns_;
            feature_event event;
            vqec_vision_ai_fires_fsalm_emit(*incident, _tracked.frame_,
                feature_event_kind::episode_opened, g_reason_detected, event);
            batch.events_.push_back(std::move(event));
        } else if (incident->active_ &&
                   _tracked.frame_.source_pts_ns_ >= incident->last_update_pts_ns_ &&
                   _tracked.frame_.source_pts_ns_ - incident->last_update_pts_ns_ >=
                       alarm_config_.update_interval_ns_) {
            incident->last_update_pts_ns_ = _tracked.frame_.source_pts_ns_;
            feature_event event;
            vqec_vision_ai_fires_fsalm_emit(*incident, _tracked.frame_,
                feature_event_kind::episode_updated, g_reason_update, event);
            batch.events_.push_back(std::move(event));
        }
    }

    for (auto& incident : incidents_) {
        if (!incident.occupied_ || incident.seen_) {
            continue;
        }
        if (!incident.active_) {
            vqec_vision_ai_fires_fsalm_clear_incident(incident);
            continue;
        }
        if (incident.clear_count_ != UINT32_MAX) {
            ++incident.clear_count_;
        }
        const std::uint64_t missing_duration = _tracked.frame_.source_pts_ns_ >=
                incident.last_seen_pts_ns_ ?
            _tracked.frame_.source_pts_ns_ - incident.last_seen_pts_ns_ : 0;
        if (incident.clear_count_ >= alarm_config_.clear_count_ &&
            missing_duration >= alarm_config_.clear_duration_ns_) {
            feature_event event;
            vqec_vision_ai_fires_fsalm_emit(incident, _tracked.frame_,
                feature_event_kind::episode_closed, g_reason_cleared, event);
            batch.events_.push_back(std::move(event));
            vqec_vision_ai_fires_fsalm_clear_incident(incident);
        }
    }
    if (batch.events_.size() > processor_config_.max_events_per_update_) {
        return {status_code::resource_exhausted,
            "fire/smoke event batch exceeds activation limit"};
    }
    _events = std::move(batch);
    return {};
}

}  // namespace vqec::vision::ai
