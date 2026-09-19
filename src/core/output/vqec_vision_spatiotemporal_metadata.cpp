#include "vqec/vision/ai/contracts/output/vqec_vision_spatiotemporal_metadata.hpp"

#include <algorithm>
#include <limits>
#include <unordered_set>

#include "vqec/vision/ai/contracts/base/vqec_vision_identifier.hpp"

namespace vqec::vision::ai {
namespace {

bool vqec_vision_ai_core_stmet_is_optional_identifier_valid(const std::string& _value) {
    return _value.empty() || vqec_vision_ai_cntr_ident_is_valid(
        _value, g_spatiotemporal_max_identifier_bytes);
}

bool vqec_vision_ai_core_stmet_is_bounds_valid(
    std::int32_t _left, std::int32_t _top, std::int32_t _right, std::int32_t _bottom) {
    return _right > _left && _bottom > _top;
}

bool vqec_vision_ai_core_stmet_is_point_in_bounds(
    const trajectory_point& _point, const trajectory_chunk& _chunk) {
    return _point.anchor_x_ >= _chunk.bounds_left_ &&
           _point.anchor_x_ <= _chunk.bounds_right_ &&
           _point.anchor_y_ >= _chunk.bounds_top_ &&
           _point.anchor_y_ <= _chunk.bounds_bottom_;
}

bool vqec_vision_ai_core_stmet_are_dimensions_valid(
    const std::vector<spatiotemporal_dimension>& _dimensions, std::size_t _maximum) {
    if (_dimensions.size() > _maximum) {
        return false;
    }
    std::unordered_set<std::string> keys;
    for (const auto& dimension : _dimensions) {
        if (!vqec_vision_ai_cntr_ident_is_valid(
                dimension.key_, g_spatiotemporal_max_identifier_bytes) ||
            !vqec_vision_ai_cntr_ident_is_valid(
                dimension.value_, g_spatiotemporal_max_identifier_bytes) ||
            !keys.insert(dimension.key_).second) {
            return false;
        }
    }
    return true;
}

bool vqec_vision_ai_core_stmet_are_references_valid(
    const std::vector<std::string>& _references) {
    if (_references.size() > g_spatiotemporal_max_episode_references) {
        return false;
    }
    std::unordered_set<std::string> unique;
    return std::all_of(_references.begin(), _references.end(),
        [&unique](const auto& _reference) {
            return vqec_vision_ai_cntr_ident_is_valid(
                       _reference, g_spatiotemporal_max_identifier_bytes) &&
                unique.insert(_reference).second;
        });
}

bool vqec_vision_ai_core_stmet_is_revision_chain_valid(
    std::uint64_t _revision, std::uint64_t _supersedes_revision) {
    return _revision != 0U &&
        ((_revision == 1U && _supersedes_revision == 0U) ||
            (_revision > 1U && _supersedes_revision == _revision - 1U)) &&
        _revision <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
}

}  // namespace

status vqec_vision_ai_cntr_stmet_validate_frame_locator(
    const spatiotemporal_frame_locator& _locator) {
    if (_locator.schema_version_ != g_spatiotemporal_metadata_schema_version ||
        !vqec_vision_ai_cntr_ident_is_valid(
            _locator.device_id_, g_spatiotemporal_max_identifier_bytes) ||
        !vqec_vision_ai_cntr_ident_is_valid(
            _locator.source_id_, g_spatiotemporal_max_identifier_bytes) ||
        !vqec_vision_ai_cntr_ident_is_valid(
            _locator.boot_id_, g_spatiotemporal_max_identifier_bytes) ||
        !vqec_vision_ai_cntr_ident_is_valid(
            _locator.clock_mapping_revision_, g_spatiotemporal_max_identifier_bytes) ||
        !vqec_vision_ai_cntr_ident_is_valid(
            _locator.scene_revision_, g_spatiotemporal_max_identifier_bytes) ||
        !vqec_vision_ai_cntr_ident_is_valid(
            _locator.coordinate_revision_, g_spatiotemporal_max_identifier_bytes) ||
        !vqec_vision_ai_cntr_ident_is_valid(
            _locator.model_revision_, g_spatiotemporal_max_identifier_bytes) ||
        !vqec_vision_ai_cntr_ident_is_valid(
            _locator.tracker_revision_, g_spatiotemporal_max_identifier_bytes) ||
        !vqec_vision_ai_core_stmet_is_optional_identifier_valid(_locator.media_reference_)) {
        return {status_code::invalid_argument, "frame locator identity is invalid"};
    }
    if (_locator.source_epoch_ == 0U || _locator.frame_id_ == 0U ||
        _locator.source_epoch_ >
            static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) ||
        _locator.frame_id_ >
            static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) ||
        _locator.source_pts_ns_ >=
            static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) ||
        _locator.clock_uncertainty_ns_ >
            static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) ||
        (_locator.has_capture_utc_ && _locator.capture_utc_ns_ < 0) ||
        (!_locator.has_capture_utc_ && _locator.capture_utc_ns_ != 0)) {
        return {status_code::invalid_argument, "frame locator time is invalid"};
    }
    return {};
}

status vqec_vision_ai_cntr_stmet_validate_track_key(
    const spatiotemporal_track_key& _track) {
    if (!vqec_vision_ai_cntr_ident_is_valid(
            _track.device_id_, g_spatiotemporal_max_identifier_bytes) ||
        !vqec_vision_ai_cntr_ident_is_valid(
            _track.source_id_, g_spatiotemporal_max_identifier_bytes) ||
        !vqec_vision_ai_cntr_ident_is_valid(
            _track.boot_id_, g_spatiotemporal_max_identifier_bytes) ||
        _track.source_epoch_ == 0U || _track.local_track_id_ == 0U ||
        _track.source_epoch_ >
            static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) ||
        _track.local_track_id_ >
            static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        return {status_code::invalid_argument, "track key is invalid"};
    }
    return {};
}

status vqec_vision_ai_cntr_stmet_validate_trajectory_chunk(
    const trajectory_chunk& _chunk) {
    if (_chunk.schema_version_ != g_spatiotemporal_metadata_schema_version ||
        !vqec_vision_ai_cntr_ident_is_valid(
            _chunk.chunk_id_, g_spatiotemporal_max_identifier_bytes) ||
        !vqec_vision_ai_cntr_ident_is_valid(
            _chunk.subject_ref_, g_spatiotemporal_max_identifier_bytes) ||
        !vqec_vision_ai_cntr_ident_is_valid(
            _chunk.entity_category_, g_spatiotemporal_max_identifier_bytes) ||
        _chunk.chunk_sequence_ == 0U || _chunk.points_.empty() ||
        _chunk.points_.size() > g_spatiotemporal_max_points_per_chunk ||
        !vqec_vision_ai_core_stmet_is_bounds_valid(_chunk.bounds_left_, _chunk.bounds_top_,
            _chunk.bounds_right_, _chunk.bounds_bottom_)) {
        return {status_code::invalid_argument, "trajectory chunk identity or bounds are invalid"};
    }
    auto result = vqec_vision_ai_cntr_stmet_validate_track_key(_chunk.track_);
    if (result.code_ != status_code::ok) {
        return result;
    }
    result = vqec_vision_ai_cntr_stmet_validate_frame_locator(_chunk.first_frame_);
    if (result.code_ != status_code::ok) {
        return result;
    }
    result = vqec_vision_ai_cntr_stmet_validate_frame_locator(_chunk.last_frame_);
    if (result.code_ != status_code::ok) {
        return result;
    }
    const auto coordinate_space = static_cast<unsigned int>(_chunk.coordinate_space_);
    const auto anchor = static_cast<unsigned int>(_chunk.anchor_);
    const auto resolution = static_cast<unsigned int>(_chunk.resolution_);
    const auto sample_mode = static_cast<unsigned int>(_chunk.sample_mode_);
    if (coordinate_space == 0U || coordinate_space > 3U || anchor == 0U || anchor > 4U ||
        resolution == 0U || resolution > 2U || sample_mode == 0U || sample_mode > 4U ||
        _chunk.required_access_domain_mask_ == 0U ||
        (_chunk.required_access_domain_mask_ & ~g_spatiotemporal_all_access_domains) != 0U ||
        (_chunk.resolution_ == trajectory_resolution::observation_exact &&
            (_chunk.sample_mode_ != trajectory_sample_mode::exact ||
                _chunk.max_spatial_error_units_ != 0U || _chunk.max_time_error_ns_ != 0U))) {
        return {status_code::invalid_argument, "trajectory resolution policy is invalid"};
    }
    if (_chunk.first_frame_.device_id_ != _chunk.track_.device_id_ ||
        _chunk.first_frame_.source_id_ != _chunk.track_.source_id_ ||
        _chunk.first_frame_.boot_id_ != _chunk.track_.boot_id_ ||
        _chunk.first_frame_.source_epoch_ != _chunk.track_.source_epoch_ ||
        _chunk.last_frame_.device_id_ != _chunk.track_.device_id_ ||
        _chunk.last_frame_.source_id_ != _chunk.track_.source_id_ ||
        _chunk.last_frame_.boot_id_ != _chunk.track_.boot_id_ ||
        _chunk.last_frame_.source_epoch_ != _chunk.track_.source_epoch_) {
        return {status_code::invalid_argument, "trajectory frame and track domains differ"};
    }
    if (_chunk.first_frame_.scene_revision_ != _chunk.last_frame_.scene_revision_ ||
        _chunk.first_frame_.coordinate_revision_ != _chunk.last_frame_.coordinate_revision_) {
        return {status_code::invalid_argument, "trajectory chunk crosses a scene revision"};
    }
    const auto& first_point = _chunk.points_.front();
    const auto& last_point = _chunk.points_.back();
    if (first_point.frame_id_ != _chunk.first_frame_.frame_id_ ||
        first_point.source_pts_ns_ != _chunk.first_frame_.source_pts_ns_ ||
        last_point.frame_id_ != _chunk.last_frame_.frame_id_ ||
        last_point.source_pts_ns_ != _chunk.last_frame_.source_pts_ns_) {
        return {status_code::invalid_argument, "trajectory endpoints do not match frame locators"};
    }
    std::uint64_t previous_frame = 0U;
    std::uint64_t previous_pts = 0U;
    for (const auto& point : _chunk.points_) {
        if (point.frame_id_ == 0U || point.source_pts_ns_ == 0U ||
            (previous_frame != 0U && point.frame_id_ <= previous_frame) ||
            (previous_pts != 0U && point.source_pts_ns_ <= previous_pts) ||
            (point.flags_ & ~g_trajectory_point_known_flags) != 0U ||
            (point.flags_ & (static_cast<std::uint32_t>(trajectory_point_flag::observed) |
                static_cast<std::uint32_t>(trajectory_point_flag::predicted))) == 0U ||
            !vqec_vision_ai_core_stmet_is_point_in_bounds(point, _chunk) ||
            (point.has_capture_utc_ && point.capture_utc_ns_ < 0) ||
            (!point.has_capture_utc_ && point.capture_utc_ns_ != 0)) {
            return {status_code::invalid_argument, "trajectory point is invalid"};
        }
        const bool has_box =
            (point.flags_ & static_cast<std::uint32_t>(trajectory_point_flag::has_box)) != 0U;
        if (has_box && !vqec_vision_ai_core_stmet_is_bounds_valid(point.box_left_, point.box_top_,
                           point.box_right_, point.box_bottom_)) {
            return {status_code::invalid_argument, "trajectory point box is invalid"};
        }
        previous_frame = point.frame_id_;
        previous_pts = point.source_pts_ns_;
    }
    return {};
}

status vqec_vision_ai_cntr_stmet_validate_association_revision(
    const track_association_revision& _association) {
    if (_association.schema_version_ != g_spatiotemporal_metadata_schema_version ||
        !vqec_vision_ai_cntr_ident_is_valid(
            _association.association_id_, g_spatiotemporal_max_identifier_bytes) ||
        !vqec_vision_ai_cntr_ident_is_valid(
            _association.entity_id_, g_spatiotemporal_max_identifier_bytes) ||
        !vqec_vision_ai_cntr_ident_is_valid(
            _association.left_chunk_id_, g_spatiotemporal_max_identifier_bytes) ||
        !vqec_vision_ai_cntr_ident_is_valid(
            _association.right_chunk_id_, g_spatiotemporal_max_identifier_bytes) ||
        !vqec_vision_ai_cntr_ident_is_valid(
            _association.method_revision_, g_spatiotemporal_max_identifier_bytes) ||
        !vqec_vision_ai_core_stmet_is_optional_identifier_valid(_association.topology_path_) ||
        _association.left_chunk_id_ == _association.right_chunk_id_) {
        return {status_code::invalid_argument, "association identity is invalid"};
    }
    const auto review_state = static_cast<unsigned int>(_association.review_state_);
    if (_association.revision_ == 0U ||
        (_association.revision_ == 1U && _association.supersedes_revision_ != 0U) ||
        (_association.revision_ > 1U &&
            _association.supersedes_revision_ != _association.revision_ - 1U) ||
        _association.score_ppm_ > g_spatiotemporal_score_scale_ppm ||
        _association.maximum_travel_ns_ < _association.minimum_travel_ns_ ||
        _association.revision_ >
            static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) ||
        _association.maximum_travel_ns_ >
            static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) ||
        _association.clock_uncertainty_ns_ >
            static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) ||
        review_state == 0U || review_state > 4U || _association.recorded_ns_ < 0) {
        return {status_code::invalid_argument, "association revision is invalid"};
    }
    return {};
}

status vqec_vision_ai_cntr_stmet_validate_episode_revision(
    const event_episode_revision& _episode) {
    if (_episode.schema_version_ != g_spatiotemporal_metadata_schema_version ||
        !vqec_vision_ai_cntr_ident_is_valid(
            _episode.episode_id_, g_spatiotemporal_max_identifier_bytes) ||
        !vqec_vision_ai_cntr_ident_is_valid(
            _episode.source_id_, g_spatiotemporal_max_identifier_bytes) ||
        !vqec_vision_ai_cntr_ident_is_valid(
            _episode.semantic_type_, g_spatiotemporal_max_identifier_bytes) ||
        !vqec_vision_ai_core_stmet_is_optional_identifier_valid(_episode.subject_ref_) ||
        !vqec_vision_ai_cntr_ident_is_valid(
            _episode.scene_revision_, g_spatiotemporal_max_identifier_bytes) ||
        !vqec_vision_ai_cntr_ident_is_valid(
            _episode.rule_revision_, g_spatiotemporal_max_identifier_bytes) ||
        !vqec_vision_ai_core_stmet_is_revision_chain_valid(
            _episode.revision_, _episode.supersedes_revision_) ||
        _episode.begin_ns_ < 0 || _episode.end_ns_ <= _episode.begin_ns_ ||
        _episode.recorded_ns_ < 0 ||
        _episode.severity_ppm_ > g_spatiotemporal_score_scale_ppm ||
        _episode.required_access_domain_mask_ == 0U ||
        (_episode.required_access_domain_mask_ & ~g_spatiotemporal_all_access_domains) != 0U ||
        !vqec_vision_ai_core_stmet_are_dimensions_valid(
            _episode.claims_, g_spatiotemporal_max_episode_claims) ||
        !vqec_vision_ai_core_stmet_are_references_valid(_episode.evidence_references_)) {
        return {status_code::invalid_argument, "episode revision is invalid"};
    }
    const auto lifecycle = static_cast<unsigned int>(_episode.lifecycle_);
    if (lifecycle == 0U || lifecycle > 5U ||
        (_episode.revision_ == 1U &&
            (_episode.lifecycle_ == episode_lifecycle::corrected ||
                _episode.lifecycle_ == episode_lifecycle::tombstoned))) {
        return {status_code::invalid_argument, "episode lifecycle is invalid"};
    }
    return {};
}

status vqec_vision_ai_cntr_stmet_validate_aggregate_contribution(
    const aggregate_contribution_revision& _contribution) {
    if (_contribution.schema_version_ != g_spatiotemporal_metadata_schema_version ||
        !vqec_vision_ai_cntr_ident_is_valid(
            _contribution.contribution_id_, g_spatiotemporal_max_identifier_bytes) ||
        !vqec_vision_ai_core_stmet_is_optional_identifier_valid(_contribution.episode_id_) ||
        !vqec_vision_ai_cntr_ident_is_valid(
            _contribution.source_id_, g_spatiotemporal_max_identifier_bytes) ||
        !vqec_vision_ai_cntr_ident_is_valid(
            _contribution.aggregate_definition_id_, g_spatiotemporal_max_identifier_bytes) ||
        !vqec_vision_ai_cntr_ident_is_valid(
            _contribution.scene_revision_, g_spatiotemporal_max_identifier_bytes) ||
        !vqec_vision_ai_cntr_ident_is_valid(
            _contribution.definition_revision_, g_spatiotemporal_max_identifier_bytes) ||
        !vqec_vision_ai_core_stmet_is_revision_chain_valid(
            _contribution.revision_, _contribution.supersedes_revision_) ||
        _contribution.bucket_begin_ns_ < 0 ||
        _contribution.bucket_end_ns_ <= _contribution.bucket_begin_ns_ ||
        _contribution.recorded_ns_ < 0 ||
        _contribution.observed_duration_ns_ > _contribution.expected_duration_ns_ ||
        _contribution.expected_duration_ns_ >
            static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) ||
        _contribution.required_access_domain_mask_ == 0U ||
        (_contribution.required_access_domain_mask_ &
            ~g_spatiotemporal_all_access_domains) != 0U ||
        !vqec_vision_ai_core_stmet_are_dimensions_valid(
            _contribution.dimensions_, g_spatiotemporal_max_query_dimensions)) {
        return {status_code::invalid_argument, "aggregate contribution is invalid"};
    }
    const auto operation = static_cast<unsigned int>(_contribution.operation_);
    if (operation == 0U || operation > 2U ||
        (_contribution.revision_ == 1U &&
            _contribution.operation_ == aggregate_contribution_operation::retract)) {
        return {status_code::invalid_argument, "aggregate operation is invalid"};
    }
    return {};
}

status vqec_vision_ai_cntr_stmet_validate_query(const spatiotemporal_query& _query) {
    if (_query.schema_version_ != g_spatiotemporal_metadata_schema_version ||
        !vqec_vision_ai_cntr_ident_is_valid(
            _query.request_id_, g_spatiotemporal_max_identifier_bytes) ||
        _query.source_ids_.empty() ||
        _query.source_ids_.size() > g_spatiotemporal_max_query_sources ||
        _query.begin_ns_ < 0 || _query.end_ns_ <= _query.begin_ns_ ||
        _query.allowed_access_domain_mask_ == 0U || _query.authorization_revision_ == 0U ||
        _query.budget_.maximum_scan_bytes_ == 0U ||
        _query.budget_.maximum_result_bytes_ == 0U ||
        _query.budget_.deadline_ns_ == 0U || _query.budget_.maximum_results_ == 0U) {
        return {status_code::invalid_argument, "spatiotemporal query envelope is invalid"};
    }
    std::unordered_set<std::string> sources;
    for (const auto& source : _query.source_ids_) {
        if (!vqec_vision_ai_cntr_ident_is_valid(
                source, g_spatiotemporal_max_identifier_bytes) ||
            !sources.insert(source).second) {
            return {status_code::invalid_argument, "spatiotemporal query source is invalid"};
        }
    }
    if (!vqec_vision_ai_core_stmet_is_optional_identifier_valid(_query.subject_ref_) ||
        !vqec_vision_ai_core_stmet_is_optional_identifier_valid(_query.entity_id_) ||
        !vqec_vision_ai_core_stmet_is_optional_identifier_valid(_query.semantic_type_)) {
        return {status_code::invalid_argument, "spatiotemporal query filter is invalid"};
    }
    const auto collection = static_cast<unsigned int>(_query.collection_);
    const auto temporal = static_cast<unsigned int>(_query.temporal_relation_);
    const auto spatial = static_cast<unsigned int>(_query.spatial_relation_);
    const auto resolution = static_cast<unsigned int>(_query.minimum_resolution_);
    const auto revision = static_cast<unsigned int>(_query.revision_view_);
    if (collection == 0U || collection > 7U || temporal == 0U || temporal > 7U ||
        spatial == 0U || spatial > 5U || resolution == 0U || resolution > 4U ||
        revision == 0U || revision > 3U ||
        (_query.has_spatial_bounds_ && !vqec_vision_ai_core_stmet_is_bounds_valid(
            _query.bounds_left_, _query.bounds_top_, _query.bounds_right_,
            _query.bounds_bottom_)) ||
        (!_query.has_spatial_bounds_ &&
            _query.spatial_relation_ != spatiotemporal_spatial_relation::none) ||
        (_query.revision_view_ == spatiotemporal_revision_view::as_known_at &&
            _query.known_at_ns_ <= 0) ||
        (_query.revision_view_ != spatiotemporal_revision_view::as_known_at &&
            _query.known_at_ns_ != 0) ||
        (_query.snapshot_sequence_ != 0U &&
            _query.cursor_sequence_ > _query.snapshot_sequence_)) {
        return {status_code::invalid_argument, "spatiotemporal query semantics are invalid"};
    }
    return {};
}

}  // namespace vqec::vision::ai
