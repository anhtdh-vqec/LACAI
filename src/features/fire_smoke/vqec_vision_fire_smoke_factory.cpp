#include "vqec_vision_fire_smoke_factory.hpp"

#include <algorithm>
#include <initializer_list>
#include <limits>
#include <new>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

namespace vqec::vision::ai {
namespace {

using nlohmann::json;

constexpr char g_configuration_schema[] = "security.fire_smoke.configuration";
constexpr char g_feature_id[] = "fire_smoke_alarm";
constexpr char g_processor_contract[] = "fire_smoke_alarm";
constexpr char g_app_id[] = "security.fire_smoke_detection";
constexpr std::size_t g_max_json_depth = 12;
constexpr std::uint64_t g_nanoseconds_per_millisecond = 1000000ULL;

struct invalid_fire_smoke_configuration {};

void vqec_vision_ai_fires_fsfac_require_keys(
    const json& _value, std::initializer_list<const char*> _keys) {
    if (!_value.is_object() || _value.size() != _keys.size()) {
        throw invalid_fire_smoke_configuration{};
    }
    for (const auto* key : _keys) {
        if (!_value.contains(key)) {
            throw invalid_fire_smoke_configuration{};
        }
    }
}

bool vqec_vision_ai_fires_fsfac_read_bool(const json& _value) {
    if (!_value.is_boolean()) {
        throw invalid_fire_smoke_configuration{};
    }
    return _value.get<bool>();
}

float vqec_vision_ai_fires_fsfac_read_float(const json& _value) {
    if (!_value.is_number()) {
        throw invalid_fire_smoke_configuration{};
    }
    return _value.get<float>();
}

std::uint64_t vqec_vision_ai_fires_fsfac_read_uint(const json& _value) {
    if (!_value.is_number_unsigned()) {
        throw invalid_fire_smoke_configuration{};
    }
    return _value.get<std::uint64_t>();
}

std::string vqec_vision_ai_fires_fsfac_read_text(const json& _value) {
    if (!_value.is_string()) {
        throw invalid_fire_smoke_configuration{};
    }
    return _value.get<std::string>();
}

void vqec_vision_ai_fires_fsfac_require_empty_zone_list(const json& _value) {
    if (!_value.is_array() || !_value.empty()) {
        throw invalid_fire_smoke_configuration{};
    }
}

std::uint64_t vqec_vision_ai_fires_fsfac_milliseconds_to_nanoseconds(
    const json& _value) {
    const auto milliseconds = vqec_vision_ai_fires_fsfac_read_uint(_value);
    if (milliseconds >
        std::numeric_limits<std::uint64_t>::max() / g_nanoseconds_per_millisecond) {
        throw invalid_fire_smoke_configuration{};
    }
    return milliseconds * g_nanoseconds_per_millisecond;
}

bool vqec_vision_ai_fires_fsfac_is_aggregation_dimension(
    const std::string& _value) noexcept {
    return _value == "class" || _value == "severity" || _value == "source" ||
        _value == "zone" || _value == "hour" || _value == "day" ||
        _value == "month";
}

}  // namespace

status fire_smoke_factory::vqec_vision_ai_ports_apcfg_validate(
    const std::string& _schema_id, std::uint64_t _revision,
    const std::vector<std::uint8_t>& _payload) const {
    feature_configuration configuration;
    configuration.schema_id_ = _schema_id;
    configuration.revision_ = _revision;
    configuration.payload_ = _payload;
    fire_smoke_alarm_config config;
    return vqec_vision_ai_fires_fsfac_parse_configuration(configuration, config);
}

status vqec_vision_ai_fires_fsfac_parse_configuration(
    const feature_configuration& _configuration, fire_smoke_alarm_config& _config) {
    if (_configuration.schema_id_ != g_configuration_schema ||
        _configuration.revision_ == 0 || _configuration.payload_.empty() ||
        _configuration.payload_.size() > feature_catalog_limits::g_max_configuration_bytes) {
        return {status_code::invalid_argument,
            "invalid fire/smoke configuration envelope"};
    }
    try {
        const std::string document(_configuration.payload_.begin(),
            _configuration.payload_.end());
        const auto depth_guard = [](int _depth, json::parse_event_t _event, json&) -> bool {
            if (_depth > static_cast<int>(g_max_json_depth)) {
                throw invalid_fire_smoke_configuration{};
            }
            return _event != json::parse_event_t::key ||
                _depth <= static_cast<int>(g_max_json_depth);
        };
        const json root = json::parse(document, depth_guard);
        vqec_vision_ai_fires_fsfac_require_keys(root,
            {"schema_version", "identity", "classes", "temporal", "spatial", "incident",
                "severity", "evidence", "metadata"});
        if (!root.at("schema_version").is_number_unsigned() ||
            root.at("schema_version").get<std::uint32_t>() !=
                VQEC_VISION_AI_BASELINE_SCHEMA_VERSION) {
            throw invalid_fire_smoke_configuration{};
        }

        fire_smoke_alarm_config candidate;
        const auto& identity = root.at("identity");
        vqec_vision_ai_fires_fsfac_require_keys(identity,
            {"app_id", "expected_model_id", "expected_model_version"});
        if (vqec_vision_ai_fires_fsfac_read_text(identity.at("app_id")) != g_app_id) {
            throw invalid_fire_smoke_configuration{};
        }
        const auto model_id = vqec_vision_ai_fires_fsfac_read_text(
            identity.at("expected_model_id"));
        const auto model_version = vqec_vision_ai_fires_fsfac_read_text(
            identity.at("expected_model_version"));
        if (model_id.empty() || model_version.empty()) {
            throw invalid_fire_smoke_configuration{};
        }
        candidate.model_version_id_ = model_id + ":" + model_version;
        const auto& classes = root.at("classes");
        vqec_vision_ai_fires_fsfac_require_keys(classes,
            {"fire_enabled", "smoke_enabled", "fire_alarm_confidence",
                "smoke_alarm_confidence", "minimum_region_area_ratio"});
        candidate.fire_enabled_ =
            vqec_vision_ai_fires_fsfac_read_bool(classes.at("fire_enabled"));
        candidate.smoke_enabled_ =
            vqec_vision_ai_fires_fsfac_read_bool(classes.at("smoke_enabled"));
        candidate.fire_alarm_confidence_ =
            vqec_vision_ai_fires_fsfac_read_float(classes.at("fire_alarm_confidence"));
        candidate.smoke_alarm_confidence_ =
            vqec_vision_ai_fires_fsfac_read_float(classes.at("smoke_alarm_confidence"));
        candidate.minimum_region_area_ratio_ =
            vqec_vision_ai_fires_fsfac_read_float(classes.at("minimum_region_area_ratio"));

        const auto& temporal = root.at("temporal");
        vqec_vision_ai_fires_fsfac_require_keys(temporal,
            {"confirmation_count", "confirmation_duration_ms", "clear_count",
                "clear_duration_ms", "update_interval_ms"});
        const auto confirmation_count =
            vqec_vision_ai_fires_fsfac_read_uint(temporal.at("confirmation_count"));
        const auto clear_count =
            vqec_vision_ai_fires_fsfac_read_uint(temporal.at("clear_count"));
        if (confirmation_count > std::numeric_limits<std::uint32_t>::max() ||
            clear_count > std::numeric_limits<std::uint32_t>::max()) {
            throw invalid_fire_smoke_configuration{};
        }
        candidate.confirmation_count_ = static_cast<std::uint32_t>(confirmation_count);
        candidate.clear_count_ = static_cast<std::uint32_t>(clear_count);
        candidate.confirmation_duration_ns_ =
            vqec_vision_ai_fires_fsfac_milliseconds_to_nanoseconds(
                temporal.at("confirmation_duration_ms"));
        candidate.clear_duration_ns_ =
            vqec_vision_ai_fires_fsfac_milliseconds_to_nanoseconds(
                temporal.at("clear_duration_ms"));
        candidate.update_interval_ns_ =
            vqec_vision_ai_fires_fsfac_milliseconds_to_nanoseconds(
                temporal.at("update_interval_ms"));

        const auto& spatial = root.at("spatial");
        vqec_vision_ai_fires_fsfac_require_keys(spatial,
            {"scene_revision", "association_iou", "include_zone_ids",
                "exclude_zone_ids"});
        candidate.scene_revision_ =
            vqec_vision_ai_fires_fsfac_read_uint(spatial.at("scene_revision"));
        candidate.association_iou_ =
            vqec_vision_ai_fires_fsfac_read_float(spatial.at("association_iou"));
        // Zone geometry is not yet available at this processor boundary. Reject configured
        // zones instead of pretending an ID was spatially enforced.
        vqec_vision_ai_fires_fsfac_require_empty_zone_list(
            spatial.at("include_zone_ids"));
        vqec_vision_ai_fires_fsfac_require_empty_zone_list(
            spatial.at("exclude_zone_ids"));

        const auto& incident = root.at("incident");
        vqec_vision_ai_fires_fsfac_require_keys(incident,
            {"max_active_incidents", "source_gap_policy", "config_change_policy"});
        candidate.max_active_incidents_ = static_cast<std::size_t>(
            vqec_vision_ai_fires_fsfac_read_uint(incident.at("max_active_incidents")));
        const auto gap_policy = vqec_vision_ai_fires_fsfac_read_text(
            incident.at("source_gap_policy"));
        if (gap_policy == "retain") {
            candidate.source_gap_policy_ = fire_smoke_gap_policy::retain;
        } else if (gap_policy == "interrupt") {
            candidate.source_gap_policy_ = fire_smoke_gap_policy::interrupt;
        } else {
            throw invalid_fire_smoke_configuration{};
        }
        if (vqec_vision_ai_fires_fsfac_read_text(
                incident.at("config_change_policy")) != "interrupt") {
            throw invalid_fire_smoke_configuration{};
        }

        const auto& severity = root.at("severity");
        vqec_vision_ai_fires_fsfac_require_keys(severity,
            {"high_confidence", "critical_duration_ms"});
        candidate.high_confidence_ =
            vqec_vision_ai_fires_fsfac_read_float(severity.at("high_confidence"));
        candidate.critical_duration_ns_ =
            vqec_vision_ai_fires_fsfac_milliseconds_to_nanoseconds(
                severity.at("critical_duration_ms"));

        const auto& evidence = root.at("evidence");
        vqec_vision_ai_fires_fsfac_require_keys(evidence,
            {"enabled", "profile_ref", "pre_duration_ms", "post_duration_ms",
                "snapshot", "clip", "retry_deadline_ms"});
        candidate.evidence_enabled_ =
            vqec_vision_ai_fires_fsfac_read_bool(evidence.at("enabled"));
        candidate.evidence_profile_ref_ =
            vqec_vision_ai_fires_fsfac_read_text(evidence.at("profile_ref"));
        candidate.evidence_pre_duration_ms_ =
            vqec_vision_ai_fires_fsfac_read_uint(evidence.at("pre_duration_ms"));
        candidate.evidence_post_duration_ms_ =
            vqec_vision_ai_fires_fsfac_read_uint(evidence.at("post_duration_ms"));
        candidate.evidence_snapshot_ =
            vqec_vision_ai_fires_fsfac_read_bool(evidence.at("snapshot"));
        candidate.evidence_clip_ =
            vqec_vision_ai_fires_fsfac_read_bool(evidence.at("clip"));
        candidate.evidence_retry_deadline_ms_ =
            vqec_vision_ai_fires_fsfac_read_uint(evidence.at("retry_deadline_ms"));

        const auto& metadata = root.at("metadata");
        vqec_vision_ai_fires_fsfac_require_keys(metadata,
            {"retention_profile_ref", "access_profile_ref",
                "aggregation_dimensions"});
        const auto retention_profile = vqec_vision_ai_fires_fsfac_read_text(
            metadata.at("retention_profile_ref"));
        const auto access_profile = vqec_vision_ai_fires_fsfac_read_text(
            metadata.at("access_profile_ref"));
        if (retention_profile.empty() ||
            retention_profile.size() > fire_smoke_limits::g_max_profile_bytes ||
            access_profile.empty() ||
            access_profile.size() > fire_smoke_limits::g_max_profile_bytes) {
            throw invalid_fire_smoke_configuration{};
        }
        if (!metadata.at("aggregation_dimensions").is_array() ||
            metadata.at("aggregation_dimensions").size() > 8U) {
            throw invalid_fire_smoke_configuration{};
        }
        std::vector<std::string> dimensions;
        for (const auto& dimension : metadata.at("aggregation_dimensions")) {
            auto value = vqec_vision_ai_fires_fsfac_read_text(dimension);
            if (!vqec_vision_ai_fires_fsfac_is_aggregation_dimension(value) ||
                std::find(dimensions.begin(), dimensions.end(), value) !=
                    dimensions.end()) {
                throw invalid_fire_smoke_configuration{};
            }
            dimensions.push_back(std::move(value));
        }

        const auto valid = vqec_vision_ai_fires_fsalm_validate_config(candidate);
        if (valid.code_ != status_code::ok) {
            return valid;
        }
        _config = std::move(candidate);
        return {};
    } catch (const invalid_fire_smoke_configuration&) {
        return {status_code::invalid_argument,
            "invalid fire/smoke configuration document"};
    } catch (const json::exception&) {
        return {status_code::invalid_argument,
            "invalid fire/smoke configuration JSON"};
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted,
            "fire/smoke configuration allocation failed"};
    }
}

status fire_smoke_factory::vqec_vision_ai_ports_ftfac_validate_configuration(
    const feature_catalog_entry& _feature,
    const feature_processor_config& _processor_config,
    const feature_configuration& _configuration) const {
    if (_feature.feature_id_ != g_feature_id ||
        _feature.processor_contract_ != g_processor_contract ||
        _feature.configuration_schema_ != g_configuration_schema) {
        return {status_code::invalid_argument,
            "fire/smoke feature catalog binding mismatch"};
    }
    const auto processor_valid =
        vqec_vision_ai_core_ftevt_validate_processor_config(_processor_config);
    if (processor_valid.code_ != status_code::ok) {
        return processor_valid;
    }
    fire_smoke_alarm_config config;
    const auto parsed =
        vqec_vision_ai_fires_fsfac_parse_configuration(_configuration, config);
    if (parsed.code_ != status_code::ok) {
        return parsed;
    }
    if (_processor_config.max_fields_per_event_ < 6U ||
        _processor_config.max_events_per_update_ < config.max_active_incidents_) {
        return {status_code::resource_exhausted,
            "fire/smoke feature resource profile is too small"};
    }
    return {};
}

status fire_smoke_factory::vqec_vision_ai_ports_ftfac_create_processor(
    const feature_catalog_entry& _feature,
    const feature_processor_config& _processor_config,
    const feature_configuration& _configuration,
    std::unique_ptr<feature_processor_port>& _processor) {
    const auto valid = vqec_vision_ai_ports_ftfac_validate_configuration(
        _feature, _processor_config, _configuration);
    if (valid.code_ != status_code::ok) {
        return valid;
    }
    fire_smoke_alarm_config config;
    const auto parsed =
        vqec_vision_ai_fires_fsfac_parse_configuration(_configuration, config);
    if (parsed.code_ != status_code::ok) {
        return parsed;
    }
    try {
        auto candidate = std::make_unique<fire_smoke_alarm>(std::move(config));
        const auto activated =
            candidate->vqec_vision_ai_ports_ftpro_validate_activation(_processor_config);
        if (activated.code_ != status_code::ok) {
            return activated;
        }
        _processor = std::move(candidate);
        return {};
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted,
            "fire/smoke processor allocation failed"};
    }
}

}  // namespace vqec::vision::ai
