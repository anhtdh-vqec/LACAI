#include "vqec_vision_metadata_runtime.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <limits>
#include <map>
#include <new>
#include <set>
#include <sstream>
#include <utility>

#include "vqec/vision/ai/contracts/base/vqec_vision_identifier.hpp"
#include "vqec/vision/ai/contracts/base/vqec_vision_version_registry.h"

namespace vqec::vision::ai {
namespace {

using json = nlohmann::json;

constexpr const char* g_boot_id_path = "/proc/sys/kernel/random/boot_id";
constexpr const char* g_runtime_id_path = "/proc/sys/kernel/random/uuid";
constexpr std::uint64_t g_microunits_per_event = 1000000U;
constexpr char g_fire_smoke_class_field[] = "security.fire_smoke.class";
constexpr char g_fire_smoke_severity_field[] = "security.fire_smoke.severity";
constexpr char g_fire_smoke_zone_field[] = "security.fire_smoke.zone";
constexpr char g_fire_smoke_region_field[] = "security.fire_smoke.region";

bool vqec_vision_ai_appl_mdrun_read_u64(
    const json& _object, const char* _key, std::uint64_t& _value) {
    const auto item = _object.find(_key);
    if (item == _object.end() || !item->is_number_unsigned()) {
        return false;
    }
    _value = item->get<std::uint64_t>();
    return true;
}

bool vqec_vision_ai_appl_mdrun_read_size(
    const json& _object, const char* _key, std::size_t& _value) {
    std::uint64_t value = 0U;
    if (!vqec_vision_ai_appl_mdrun_read_u64(_object, _key, value) ||
        value > std::numeric_limits<std::size_t>::max()) {
        return false;
    }
    _value = static_cast<std::size_t>(value);
    return true;
}

bool vqec_vision_ai_appl_mdrun_read_string(
    const json& _object, const char* _key, std::string& _value) {
    const auto item = _object.find(_key);
    if (item == _object.end() || !item->is_string()) {
        return false;
    }
    _value = item->get<std::string>();
    return true;
}

std::uint32_t vqec_vision_ai_appl_mdrun_domain_mask(const std::string& _name) {
    if (_name == "aggregate") {
        return vqec_vision_ai_cntr_stmet_get_access_domain_mask(
            spatiotemporal_access_domain::aggregate);
    }
    if (_name == "object") {
        return vqec_vision_ai_cntr_stmet_get_access_domain_mask(
            spatiotemporal_access_domain::object);
    }
    if (_name == "visual_attribute") {
        return vqec_vision_ai_cntr_stmet_get_access_domain_mask(
            spatiotemporal_access_domain::visual_attribute);
    }
    if (_name == "trajectory") {
        return vqec_vision_ai_cntr_stmet_get_access_domain_mask(
            spatiotemporal_access_domain::trajectory);
    }
    if (_name == "plate") {
        return vqec_vision_ai_cntr_stmet_get_access_domain_mask(
            spatiotemporal_access_domain::plate);
    }
    if (_name == "identity") {
        return vqec_vision_ai_cntr_stmet_get_access_domain_mask(
            spatiotemporal_access_domain::identity);
    }
    if (_name == "vector") {
        return vqec_vision_ai_cntr_stmet_get_access_domain_mask(
            spatiotemporal_access_domain::vector);
    }
    if (_name == "media") {
        return vqec_vision_ai_cntr_stmet_get_access_domain_mask(
            spatiotemporal_access_domain::media);
    }
    if (_name == "operational") {
        return vqec_vision_ai_cntr_stmet_get_access_domain_mask(
            spatiotemporal_access_domain::operational);
    }
    if (_name == "audit") {
        return vqec_vision_ai_cntr_stmet_get_access_domain_mask(
            spatiotemporal_access_domain::audit);
    }
    return 0U;
}

bool vqec_vision_ai_appl_mdrun_is_identifier(const std::string& _value) {
    return vqec_vision_ai_cntr_ident_is_valid(
        _value, g_spatiotemporal_max_identifier_bytes);
}

std::string vqec_vision_ai_appl_mdrun_track_key(
    const std::string& _source_id, std::uint64_t _epoch, std::uint64_t _track_id) {
    constexpr char g_separator = '\x1f';
    return _source_id + g_separator + std::to_string(_epoch) + g_separator +
        std::to_string(_track_id);
}

bool vqec_vision_ai_appl_mdrun_checked_i64(
    std::uint64_t _value, std::int64_t& _output) {
    if (_value > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        return false;
    }
    _output = static_cast<std::int64_t>(_value);
    return true;
}

bool vqec_vision_ai_appl_mdrun_round_i32(float _value, std::int32_t& _output) {
    if (!std::isfinite(_value) ||
        _value < static_cast<float>(std::numeric_limits<std::int32_t>::min()) ||
        _value > static_cast<float>(std::numeric_limits<std::int32_t>::max())) {
        return false;
    }
    _output = static_cast<std::int32_t>(std::lround(_value));
    return true;
}

const feature_event_field* vqec_vision_ai_appl_mdrun_find_field(
    const feature_event& _event, const char* _schema_id) noexcept {
    const auto found = std::find_if(_event.fields_.begin(), _event.fields_.end(),
        [_schema_id](const auto& _field) {
            return _field.schema_id_ == _schema_id;
        });
    return found == _event.fields_.end() ? nullptr : &*found;
}

bool vqec_vision_ai_appl_mdrun_append_dimension(
    std::vector<spatiotemporal_dimension>& _dimensions,
    const std::string& _key, const std::string& _value) {
    if (!vqec_vision_ai_appl_mdrun_is_identifier(_key) ||
        !vqec_vision_ai_appl_mdrun_is_identifier(_value) ||
        std::find_if(_dimensions.begin(), _dimensions.end(),
            [&_key](const auto& _dimension) {
                return _dimension.key_ == _key;
            }) != _dimensions.end()) {
        return false;
    }
    _dimensions.push_back({_key, _value});
    return true;
}

bool vqec_vision_ai_appl_mdrun_append_fire_smoke_dimensions(
    const feature_event& _event, const metadata_source_profile& _profile,
    std::uint32_t _grid_columns, std::uint32_t _grid_rows,
    std::vector<spatiotemporal_dimension>& _dimensions) {
    const auto* class_field = vqec_vision_ai_appl_mdrun_find_field(
        _event, g_fire_smoke_class_field);
    const auto* severity_field = vqec_vision_ai_appl_mdrun_find_field(
        _event, g_fire_smoke_severity_field);
    const auto* zone_field = vqec_vision_ai_appl_mdrun_find_field(
        _event, g_fire_smoke_zone_field);
    if (class_field == nullptr || severity_field == nullptr ||
        !vqec_vision_ai_appl_mdrun_append_dimension(
            _dimensions, "class", class_field->value_) ||
        !vqec_vision_ai_appl_mdrun_append_dimension(
            _dimensions, "severity", severity_field->value_)) {
        return false;
    }
    if (zone_field != nullptr &&
        !vqec_vision_ai_appl_mdrun_append_dimension(
            _dimensions, "zone", zone_field->value_)) {
        return false;
    }
    const auto* region_field = vqec_vision_ai_appl_mdrun_find_field(
        _event, g_fire_smoke_region_field);
    if (region_field == nullptr || _profile.source_width_ == 0 ||
        _profile.source_height_ == 0 || _grid_columns == 0 || _grid_rows == 0) {
        return false;
    }
    float x = 0.0F;
    float y = 0.0F;
    float width = 0.0F;
    float height = 0.0F;
    char trailing = 0;
    if (std::sscanf(region_field->value_.c_str(), "%f,%f,%f,%f%c",
            &x, &y, &width, &height, &trailing) != 4 ||
        !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(width) ||
        !std::isfinite(height) || width <= 0.0F || height <= 0.0F) {
        return false;
    }
    const auto center_x = std::clamp(x + width / 2.0F, 0.0F,
        static_cast<float>(_profile.source_width_ - 1U));
    const auto center_y = std::clamp(y + height / 2.0F, 0.0F,
        static_cast<float>(_profile.source_height_ - 1U));
    const auto column = std::min<std::uint32_t>(_grid_columns - 1U,
        static_cast<std::uint32_t>(center_x * _grid_columns /
            static_cast<float>(_profile.source_width_)));
    const auto row = std::min<std::uint32_t>(_grid_rows - 1U,
        static_cast<std::uint32_t>(center_y * _grid_rows /
            static_cast<float>(_profile.source_height_)));
    return vqec_vision_ai_appl_mdrun_append_dimension(_dimensions,
        "hotspot_cell", "x" + std::to_string(column) + "_y" +
            std::to_string(row));
}

bool vqec_vision_ai_appl_mdrun_append_event_claim(
    const feature_event_field& _field,
    std::vector<spatiotemporal_dimension>& _claims) {
    if (_field.schema_id_ != g_fire_smoke_region_field) {
        return vqec_vision_ai_appl_mdrun_append_dimension(
            _claims, _field.schema_id_, _field.value_);
    }
    float x = 0.0F;
    float y = 0.0F;
    float width = 0.0F;
    float height = 0.0F;
    char trailing = 0;
    if (std::sscanf(_field.value_.c_str(), "%f,%f,%f,%f%c",
            &x, &y, &width, &height, &trailing) != 4 ||
        !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(width) ||
        !std::isfinite(height)) {
        return false;
    }
    const auto encode_millipixels = [](float _value, std::int64_t& _encoded) {
        constexpr double g_millipixels_per_pixel = 1000.0;
        const auto scaled = static_cast<double>(_value) * g_millipixels_per_pixel;
        if (scaled < static_cast<double>(std::numeric_limits<std::int64_t>::min()) ||
            scaled > static_cast<double>(std::numeric_limits<std::int64_t>::max())) {
            return false;
        }
        _encoded = static_cast<std::int64_t>(std::llround(scaled));
        return true;
    };
    std::int64_t encoded_x = 0;
    std::int64_t encoded_y = 0;
    std::int64_t encoded_width = 0;
    std::int64_t encoded_height = 0;
    if (!encode_millipixels(x, encoded_x) ||
        !encode_millipixels(y, encoded_y) ||
        !encode_millipixels(width, encoded_width) ||
        !encode_millipixels(height, encoded_height)) {
        return false;
    }
    const auto encoded = "x" + std::to_string(encoded_x) + "_y" +
        std::to_string(encoded_y) + "_w" + std::to_string(encoded_width) + "_h" +
        std::to_string(encoded_height);
    return vqec_vision_ai_appl_mdrun_append_dimension(
        _claims, _field.schema_id_, encoded);
}

}  // namespace

status vqec_vision_ai_appl_mdrun_load_config(
    std::istream& _stream, const deployment_config& _deployment,
    metadata_runtime_config& _config) {
    json document;
    try {
        _stream >> document;
    } catch (const json::exception&) {
        return {status_code::invalid_argument, "metadata runtime profile JSON is invalid"};
    }
    if (!document.is_object()) {
        return {status_code::invalid_argument, "metadata runtime profile must be an object"};
    }
    metadata_runtime_config parsed;
    std::uint64_t schema_version = 0U;
    if (!vqec_vision_ai_appl_mdrun_read_u64(document, "schema_version", schema_version) ||
        schema_version != VQEC_VISION_AI_BASELINE_SCHEMA_VERSION ||
        !vqec_vision_ai_appl_mdrun_read_string(document, "device_id", parsed.device_id_) ||
        !vqec_vision_ai_appl_mdrun_is_identifier(parsed.device_id_)) {
        return {status_code::invalid_argument, "metadata runtime identity is invalid"};
    }
    parsed.schema_version_ = static_cast<std::uint32_t>(schema_version);
    const auto required = document.find("required");
    if (required == document.end() || !required->is_boolean()) {
        return {status_code::invalid_argument, "metadata required policy is missing"};
    }
    parsed.required_ = required->get<bool>();
    const auto service = document.find("service");
    if (service == document.end() || !service->is_object()) {
        return {status_code::invalid_argument, "metadata service profile is invalid"};
    }
    const json& service_object = *service;
    const auto store = service_object.find("store");
    if (store == service_object.end() || !store->is_object() ||
        !vqec_vision_ai_appl_mdrun_read_string(
            *store, "root_directory", parsed.service_.store_.root_directory_) ||
        parsed.service_.store_.root_directory_.empty() ||
        parsed.service_.store_.root_directory_.front() != '/' ||
        !vqec_vision_ai_appl_mdrun_read_u64(
            *store, "shard_duration_ns", parsed.service_.store_.shard_duration_ns_) ||
        !vqec_vision_ai_appl_mdrun_read_u64(
            *store, "maximum_shard_bytes", parsed.service_.store_.maximum_shard_bytes_) ||
        !vqec_vision_ai_appl_mdrun_read_u64(
            *store, "maximum_store_bytes", parsed.service_.store_.maximum_store_bytes_) ||
        !vqec_vision_ai_appl_mdrun_read_u64(
            *store, "reserve_free_bytes", parsed.service_.store_.reserve_free_bytes_) ||
        !vqec_vision_ai_appl_mdrun_read_size(*store, "maximum_encoded_chunk_bytes",
            parsed.service_.store_.maximum_encoded_chunk_bytes_) ||
        !vqec_vision_ai_appl_mdrun_read_size(
            *store, "maximum_query_results", parsed.service_.store_.maximum_query_results_)) {
        return {status_code::invalid_argument, "metadata store profile is invalid"};
    }
    std::uint64_t busy_timeout = 0U;
    std::uint64_t checkpoint_pages = 0U;
    const auto full_sync = store->find("full_sync");
    if (!vqec_vision_ai_appl_mdrun_read_u64(*store, "busy_timeout_ms", busy_timeout) ||
        busy_timeout > std::numeric_limits<std::uint32_t>::max() ||
        !vqec_vision_ai_appl_mdrun_read_u64(
            *store, "wal_autocheckpoint_pages", checkpoint_pages) ||
        checkpoint_pages > std::numeric_limits<std::uint32_t>::max() ||
        full_sync == store->end() || !full_sync->is_boolean() ||
        !vqec_vision_ai_appl_mdrun_read_size(
            service_object, "queue_capacity", parsed.service_.queue_capacity_) ||
        !vqec_vision_ai_appl_mdrun_read_size(
            service_object, "maximum_live_tracks", parsed.service_.maximum_live_tracks_) ||
        !vqec_vision_ai_appl_mdrun_read_size(
            service_object, "maximum_live_deltas", parsed.service_.maximum_live_deltas_) ||
        !vqec_vision_ai_appl_mdrun_read_size(
            service_object, "maximum_batch_records", parsed.service_.maximum_batch_records_)) {
        return {status_code::invalid_argument, "metadata service profile is invalid"};
    }
    parsed.service_.store_.busy_timeout_ms_ = static_cast<std::uint32_t>(busy_timeout);
    parsed.service_.store_.wal_autocheckpoint_pages_ =
        static_cast<std::uint32_t>(checkpoint_pages);
    parsed.service_.store_.is_full_sync_ = full_sync->get<bool>();
    const auto sinks = service_object.find("outbox_sinks");
    if (sinks == service_object.end() || !sinks->is_array() ||
        sinks->size() > g_spatiotemporal_max_outbox_sinks) {
        return {status_code::invalid_argument, "metadata outbox sinks are invalid"};
    }
    for (const auto& sink : *sinks) {
        if (!sink.is_string() || !vqec_vision_ai_appl_mdrun_is_identifier(sink.get<std::string>())) {
            return {status_code::invalid_argument, "metadata outbox sink is invalid"};
        }
        parsed.service_.outbox_sinks_.push_back(sink.get<std::string>());
    }
    const auto trajectory = document.find("trajectory");
    const auto retention = document.find("retention");
    std::uint64_t aggregate_bucket_ns = 0U;
    std::uint64_t hotspot_grid_columns = 0U;
    std::uint64_t hotspot_grid_rows = 0U;
    if (trajectory == document.end() || !trajectory->is_object() ||
        retention == document.end() || !retention->is_object() ||
        !vqec_vision_ai_appl_mdrun_read_size(*trajectory, "maximum_points_per_chunk",
            parsed.maximum_points_per_chunk_) ||
        !vqec_vision_ai_appl_mdrun_read_u64(*trajectory, "maximum_chunk_duration_ns",
            parsed.maximum_chunk_duration_ns_) ||
        !vqec_vision_ai_appl_mdrun_read_u64(*trajectory, "minimum_sample_interval_ns",
            parsed.minimum_sample_interval_ns_) ||
        !vqec_vision_ai_appl_mdrun_read_u64(
            *trajectory, "stale_track_ns", parsed.stale_track_ns_) ||
        !vqec_vision_ai_appl_mdrun_read_u64(
            document, "aggregate_bucket_ns", aggregate_bucket_ns) ||
        !vqec_vision_ai_appl_mdrun_read_u64(
            document, "hotspot_grid_columns", hotspot_grid_columns) ||
        !vqec_vision_ai_appl_mdrun_read_u64(
            document, "hotspot_grid_rows", hotspot_grid_rows) ||
        hotspot_grid_columns == 0U || hotspot_grid_columns > 64U ||
        hotspot_grid_rows == 0U || hotspot_grid_rows > 64U ||
        aggregate_bucket_ns >
            static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) ||
        !vqec_vision_ai_appl_mdrun_read_u64(*retention, "maintenance_interval_ns",
            parsed.maintenance_interval_ns_) ||
        !vqec_vision_ai_appl_mdrun_read_u64(*retention, "trajectory_ns",
            parsed.trajectory_retention_ns_) ||
        !vqec_vision_ai_appl_mdrun_read_u64(*retention, "episode_ns",
            parsed.episode_retention_ns_) ||
        !vqec_vision_ai_appl_mdrun_read_u64(*retention, "contribution_ns",
            parsed.contribution_retention_ns_) ||
        !vqec_vision_ai_appl_mdrun_read_u64(*retention, "rollup_ns",
            parsed.rollup_retention_ns_) ||
        parsed.maximum_points_per_chunk_ < 2U ||
        parsed.maximum_points_per_chunk_ > metadata_runtime_limits::g_maximum_points_per_chunk ||
        parsed.maximum_chunk_duration_ns_ == 0U || parsed.stale_track_ns_ == 0U ||
        aggregate_bucket_ns == 0U || parsed.maintenance_interval_ns_ == 0U ||
        parsed.trajectory_retention_ns_ == 0U || parsed.episode_retention_ns_ == 0U ||
        parsed.contribution_retention_ns_ == 0U || parsed.rollup_retention_ns_ == 0U) {
        return {status_code::invalid_argument, "metadata trajectory or retention is invalid"};
    }
    parsed.aggregate_bucket_ns_ = static_cast<std::int64_t>(aggregate_bucket_ns);
    parsed.hotspot_grid_columns_ = static_cast<std::uint32_t>(hotspot_grid_columns);
    parsed.hotspot_grid_rows_ = static_cast<std::uint32_t>(hotspot_grid_rows);
    const auto sources = document.find("sources");
    if (sources == document.end() || !sources->is_array() || sources->empty() ||
        sources->size() > metadata_runtime_limits::g_maximum_sources) {
        return {status_code::invalid_argument, "metadata source profiles are invalid"};
    }
    std::set<std::string> source_ids;
    for (const auto& item : *sources) {
        metadata_source_profile source;
        if (!item.is_object() ||
            !vqec_vision_ai_appl_mdrun_read_string(item, "source_id", source.source_id_) ||
            !vqec_vision_ai_appl_mdrun_read_string(
                item, "scene_revision", source.scene_revision_) ||
            !vqec_vision_ai_appl_mdrun_read_string(
                item, "coordinate_revision", source.coordinate_revision_) ||
            !vqec_vision_ai_appl_mdrun_read_string(
                item, "clock_mapping_revision", source.clock_mapping_revision_) ||
            !vqec_vision_ai_appl_mdrun_read_string(
                item, "trajectory_model_id", source.trajectory_model_id_) ||
            !vqec_vision_ai_appl_mdrun_read_string(item,
                "trajectory_authorization_feature_id",
                source.trajectory_authorization_feature_id_) ||
            !vqec_vision_ai_appl_mdrun_read_string(item,
                "trajectory_authorization_attribute_id",
                source.trajectory_authorization_attribute_id_) ||
            !vqec_vision_ai_appl_mdrun_is_identifier(source.source_id_) ||
            !vqec_vision_ai_appl_mdrun_is_identifier(source.scene_revision_) ||
            !vqec_vision_ai_appl_mdrun_is_identifier(source.coordinate_revision_) ||
            !vqec_vision_ai_appl_mdrun_is_identifier(source.clock_mapping_revision_) ||
            !vqec_vision_ai_appl_mdrun_is_identifier(source.trajectory_model_id_) ||
            !vqec_vision_ai_appl_mdrun_is_identifier(
                source.trajectory_authorization_feature_id_) ||
            !vqec_vision_ai_appl_mdrun_is_identifier(
                source.trajectory_authorization_attribute_id_) ||
            !source_ids.insert(source.source_id_).second) {
            return {status_code::invalid_argument, "metadata source identity is invalid"};
        }
        const auto deployment_source = std::find_if(_deployment.sources_.begin(),
            _deployment.sources_.end(), [&source](const auto& _candidate) {
                return _candidate.source_id_ == source.source_id_;
            });
        if (deployment_source == _deployment.sources_.end() ||
            deployment_source->profile_.width_ == 0 ||
            deployment_source->profile_.height_ == 0) {
            return {status_code::invalid_argument,
                "metadata source geometry is absent from deployment"};
        }
        source.source_width_ = deployment_source->profile_.width_;
        source.source_height_ = deployment_source->profile_.height_;
        const auto rules = item.find("event_access_rules");
        if (rules == item.end() || !rules->is_array() || rules->empty() ||
            rules->size() > metadata_runtime_limits::g_maximum_event_access_rules) {
            return {status_code::invalid_argument, "metadata event access rules are invalid"};
        }
        std::set<std::string> rule_features;
        for (const auto& rule_item : *rules) {
            metadata_event_access_rule rule;
            if (!rule_item.is_object() ||
                !vqec_vision_ai_appl_mdrun_read_string(
                    rule_item, "feature_id", rule.feature_id_) ||
                !vqec_vision_ai_appl_mdrun_is_identifier(rule.feature_id_) ||
                !rule_features.insert(rule.feature_id_).second) {
                return {status_code::invalid_argument,
                    "metadata event access rule identity is invalid"};
            }
            const auto domains = rule_item.find("access_domains");
            if (domains == rule_item.end() || !domains->is_array() || domains->empty()) {
                return {status_code::invalid_argument,
                    "metadata event access domains are invalid"};
            }
            for (const auto& domain : *domains) {
                if (!domain.is_string()) {
                    return {status_code::invalid_argument,
                        "metadata event access domain is invalid"};
                }
                const auto mask =
                    vqec_vision_ai_appl_mdrun_domain_mask(domain.get<std::string>());
                if (mask == 0U || (rule.access_domain_mask_ & mask) != 0U) {
                    return {status_code::invalid_argument,
                        "metadata event access domain is invalid"};
                }
                rule.access_domain_mask_ |= mask;
            }
            const auto object_mask = vqec_vision_ai_cntr_stmet_get_access_domain_mask(
                spatiotemporal_access_domain::object);
            if ((rule.access_domain_mask_ & object_mask) == 0U) {
                return {status_code::invalid_argument,
                    "metadata event access domains must include object"};
            }
            source.event_access_rules_.push_back(std::move(rule));
        }
        const auto deployed = std::find_if(_deployment.sources_.begin(),
            _deployment.sources_.end(), [&source](const auto& _candidate) {
                return _candidate.source_id_ == source.source_id_;
            });
        if (deployed == _deployment.sources_.end() ||
            std::find(deployed->model_ids_.begin(), deployed->model_ids_.end(),
                source.trajectory_model_id_) == deployed->model_ids_.end()) {
            return {status_code::unsupported,
                "metadata source or trajectory producer is not deployed"};
        }
        parsed.sources_.push_back(std::move(source));
    }
    _config = std::move(parsed);
    return {};
}

class metadata_runtime::implementation final {
public:
    implementation(metadata_runtime_config _config, feature_event_sink_port& _downstream)
        : config_(std::move(_config)), downstream_(_downstream), service_(config_.service_) {}

    struct track_builder {
        trajectory_chunk chunk_;
        std::uint64_t last_seen_ns_{0};
        std::string model_revision_;
        std::string tracker_revision_;
    };

    status vqec_vision_ai_appl_mdrun_start() {
        if (started_) {
            return {status_code::invalid_state, "metadata runtime is already started"};
        }
        std::ifstream stream(g_boot_id_path);
        if (!stream || !std::getline(stream, boot_id_) ||
            !vqec_vision_ai_appl_mdrun_is_identifier(boot_id_)) {
            return {status_code::io_error, "read metadata boot identity failed"};
        }
        std::ifstream runtime_stream(g_runtime_id_path);
        if (!runtime_stream || !std::getline(runtime_stream, runtime_instance_id_) ||
            !vqec_vision_ai_appl_mdrun_is_identifier(runtime_instance_id_)) {
            return {status_code::io_error, "read metadata runtime identity failed"};
        }
        next_chunk_sequence_ = 1U;
        const auto started = service_.vqec_vision_ai_appl_mdsvc_start();
        if (started.code_ == status_code::ok) {
            started_ = true;
        }
        return started;
    }

    status vqec_vision_ai_appl_mdrun_stop(bool _drain) noexcept {
        if (!started_) {
            return {};
        }
        status result;
        if (_drain) {
            for (auto& entry : tracks_) {
                const auto flushed = vqec_vision_ai_appl_mdrun_flush(entry.second, true);
                if (result.code_ == status_code::ok && flushed.code_ != status_code::ok) {
                    result = flushed;
                }
            }
        }
        tracks_.clear();
        const auto stopped = service_.vqec_vision_ai_appl_mdsvc_stop(_drain);
        started_ = false;
        if (result.code_ == status_code::ok) {
            result = stopped;
        }
        return result;
    }

    const metadata_source_profile* vqec_vision_ai_appl_mdrun_find_source(
        const std::string& _source_id) const noexcept {
        const auto source = std::find_if(config_.sources_.begin(), config_.sources_.end(),
            [&_source_id](const auto& _candidate) {
                return _candidate.source_id_ == _source_id;
            });
        return source == config_.sources_.end() ? nullptr : &*source;
    }

    status vqec_vision_ai_appl_mdrun_submit_observations(
        const std::string& _source_id, const std::string& _model_revision,
        const std::string& _tracker_revision, const observation_batch& _batch,
        bool _is_authorized) {
        if (!started_) {
            return {status_code::invalid_state, "metadata runtime is not started"};
        }
        const auto health = service_.vqec_vision_ai_appl_mdsvc_get_health();
        if (config_.required_ && health.code_ != status_code::ok) {
            return health;
        }
        const auto* profile = vqec_vision_ai_appl_mdrun_find_source(_source_id);
        if (profile == nullptr) {
            return {status_code::unsupported, "metadata source capability is unavailable"};
        }
        if (!_is_authorized) {
            return {status_code::unauthorized, "trajectory metadata is not authorized"};
        }
        if (!vqec_vision_ai_appl_mdrun_is_identifier(_model_revision) ||
            !vqec_vision_ai_appl_mdrun_is_identifier(_tracker_revision) ||
            _batch.frame_.source_epoch_ == 0U || _batch.frame_.source_pts_ns_ == UINT64_MAX) {
            return {status_code::invalid_argument, "trajectory producer provenance is invalid"};
        }
        // The metadata locator reserves zero PTS as unavailable. The first frame of a source
        // epoch may legitimately use that sentinel; it is not rewritten to a fabricated time.
        if (_batch.frame_.source_pts_ns_ == 0U) {
            return {};
        }
        std::set<std::string> seen;
        try {
            for (const auto& observation : _batch.observations_) {
                if (observation.track_id_ == 0U) {
                    return {status_code::invalid_argument, "trajectory observation is untracked"};
                }
                const auto key = vqec_vision_ai_appl_mdrun_track_key(
                    _source_id, _batch.frame_.source_epoch_, observation.track_id_);
                seen.insert(key);
                auto current = tracks_.find(key);
                if (current == tracks_.end()) {
                    if (tracks_.size() >= config_.service_.maximum_live_tracks_) {
                        return {status_code::resource_exhausted,
                            "metadata trajectory builder capacity is exhausted"};
                    }
                    current = tracks_.emplace(key, track_builder{}).first;
                }
                auto& builder = current->second;
                if (!builder.chunk_.points_.empty() &&
                    (builder.chunk_.track_.source_epoch_ != _batch.frame_.source_epoch_ ||
                     builder.model_revision_ != _model_revision ||
                     builder.tracker_revision_ != _tracker_revision)) {
                    const auto flushed = vqec_vision_ai_appl_mdrun_flush(builder, true);
                    if (flushed.code_ != status_code::ok) {
                        return flushed;
                    }
                }
                if (!builder.chunk_.points_.empty() &&
                    _batch.frame_.source_pts_ns_ <= builder.last_seen_ns_) {
                    return {status_code::invalid_argument,
                        "trajectory source time is not strictly increasing"};
                }
                if (!builder.chunk_.points_.empty() &&
                    _batch.frame_.source_pts_ns_ - builder.last_seen_ns_ <
                        config_.minimum_sample_interval_ns_) {
                    builder.last_seen_ns_ = _batch.frame_.source_pts_ns_;
                    continue;
                }
                if (builder.chunk_.points_.empty()) {
                    const auto initialized = vqec_vision_ai_appl_mdrun_initialize_chunk(
                        builder, *profile,
                        _source_id, _model_revision, _tracker_revision, observation);
                    if (initialized.code_ != status_code::ok) {
                        return initialized;
                    }
                }
                trajectory_point point;
                if (!vqec_vision_ai_appl_mdrun_make_point(observation, point)) {
                    return {status_code::invalid_argument,
                        "trajectory observation geometry cannot be represented"};
                }
                if (builder.chunk_.points_.empty()) {
                    point.flags_ |= static_cast<std::uint32_t>(trajectory_point_flag::mandatory) |
                        static_cast<std::uint32_t>(trajectory_point_flag::lifecycle_boundary);
                }
                builder.chunk_.points_.push_back(point);
                builder.last_seen_ns_ = _batch.frame_.source_pts_ns_;
                vqec_vision_ai_appl_mdrun_update_chunk(builder.chunk_, *profile,
                    _model_revision, _tracker_revision, observation, point);
                if (builder.chunk_.points_.size() >= config_.maximum_points_per_chunk_ ||
                    builder.chunk_.last_frame_.source_pts_ns_ -
                            builder.chunk_.first_frame_.source_pts_ns_ >=
                        config_.maximum_chunk_duration_ns_) {
                    const auto flushed = vqec_vision_ai_appl_mdrun_flush(builder, false);
                    if (flushed.code_ != status_code::ok) {
                        return flushed;
                    }
                }
            }
        } catch (const std::bad_alloc&) {
            return {status_code::resource_exhausted,
                "metadata trajectory producer allocation failed"};
        }
        for (auto current = tracks_.begin(); current != tracks_.end();) {
            if (current->second.chunk_.track_.source_id_ != _source_id ||
                seen.find(current->first) != seen.end() ||
                _batch.frame_.source_pts_ns_ < current->second.last_seen_ns_ ||
                _batch.frame_.source_pts_ns_ - current->second.last_seen_ns_ <
                    config_.stale_track_ns_) {
                ++current;
                continue;
            }
            const auto flushed = vqec_vision_ai_appl_mdrun_flush(current->second, true);
            if (flushed.code_ != status_code::ok) {
                return flushed;
            }
            current = tracks_.erase(current);
        }
        return {};
    }

    status vqec_vision_ai_appl_mdrun_maintain(std::uint64_t _source_time_ns) {
        if (!started_) {
            return {status_code::invalid_state, "metadata runtime is not started"};
        }
        const auto health = service_.vqec_vision_ai_appl_mdsvc_get_health();
        if (config_.required_ && health.code_ != status_code::ok) {
            return health;
        }
        if (_source_time_ns < last_maintenance_ns_ ||
            _source_time_ns - last_maintenance_ns_ < config_.maintenance_interval_ns_) {
            return {};
        }
        const auto cutoff = [_source_time_ns](std::uint64_t _horizon) {
            return static_cast<std::int64_t>(_source_time_ns > _horizon
                ? _source_time_ns - _horizon : 0U);
        };
        const spatiotemporal_retention_policy policy{
            cutoff(config_.trajectory_retention_ns_),
            cutoff(config_.episode_retention_ns_),
            cutoff(config_.contribution_retention_ns_),
            cutoff(config_.rollup_retention_ns_)};
        spatiotemporal_retention_report report;
        const auto maintained =
            service_.vqec_vision_ai_appl_mdsvc_apply_retention(policy, report);
        if (maintained.code_ == status_code::ok) {
            last_maintenance_ns_ = _source_time_ns;
        }
        return maintained;
    }

    status vqec_vision_ai_ports_fesnk_deliver_event(const feature_event& _event) {
        const auto health = service_.vqec_vision_ai_appl_mdsvc_get_health();
        if (config_.required_ && health.code_ != status_code::ok) {
            return health;
        }
        const auto* profile = vqec_vision_ai_appl_mdrun_find_source(_event.source_id_);
        if (profile == nullptr) {
            return {status_code::unsupported, "metadata event source capability is unavailable"};
        }
        const auto access_rule = std::find_if(profile->event_access_rules_.begin(),
            profile->event_access_rules_.end(), [&_event](const auto& _rule) {
                return _rule.feature_id_ == _event.feature_id_;
            });
        if (access_rule == profile->event_access_rules_.end()) {
            return {status_code::unsupported,
                "metadata event feature capability is unavailable"};
        }
        std::int64_t begin_ns = 0;
        std::int64_t occurred_ns = 0;
        if (!vqec_vision_ai_appl_mdrun_checked_i64(_event.episode_begin_ns_, begin_ns) ||
            !vqec_vision_ai_appl_mdrun_checked_i64(_event.occurred_at_ns_, occurred_ns) ||
            occurred_ns == std::numeric_limits<std::int64_t>::max()) {
            return {status_code::invalid_argument, "metadata episode time is invalid"};
        }
        event_episode_revision episode;
        episode.episode_id_ = _event.event_id_;
        episode.revision_ = _event.episode_revision_;
        episode.supersedes_revision_ = _event.supersedes_episode_revision_;
        episode.source_id_ = _event.source_id_;
        episode.semantic_type_ = _event.event_schema_id_;
        if (!_event.track_ids_.empty()) {
            episode.subject_ref_ = "track." + std::to_string(_event.track_ids_.front());
        }
        episode.scene_revision_ = profile->scene_revision_;
        episode.rule_revision_ = _event.feature_id_ + "." +
            std::to_string(_event.config_revision_);
        if (!vqec_vision_ai_appl_mdrun_is_identifier(episode.rule_revision_) ||
            !vqec_vision_ai_appl_mdrun_is_identifier(
                _event.event_schema_id_ + ".count")) {
            return {status_code::invalid_argument,
                "metadata projection identifier exceeds the baseline contract"};
        }
        episode.begin_ns_ = begin_ns;
        episode.end_ns_ = occurred_ns + 1;
        episode.recorded_ns_ = occurred_ns;
        if (_event.kind_ == feature_event_kind::episode_opened) {
            episode.lifecycle_ = episode_lifecycle::opened;
        } else if (_event.kind_ == feature_event_kind::episode_updated) {
            episode.lifecycle_ = episode_lifecycle::updated;
        } else if (_event.kind_ == feature_event_kind::episode_closed) {
            episode.lifecycle_ = episode_lifecycle::closed;
        } else {
            episode.lifecycle_ = episode_lifecycle::closed;
        }
        episode.required_access_domain_mask_ = access_rule->access_domain_mask_;
        for (const auto& field : _event.fields_) {
            if (!vqec_vision_ai_appl_mdrun_append_event_claim(
                    field, episode.claims_)) {
                return {status_code::invalid_argument,
                    "event field cannot be represented as a metadata claim"};
            }
            const auto scaled = static_cast<std::uint32_t>(
                std::lround(field.confidence_ * g_spatiotemporal_score_scale_ppm));
            episode.severity_ppm_ = std::max(episode.severity_ppm_, scaled);
        }
        if (!_event.evidence_request_id_.empty()) {
            episode.evidence_references_.push_back(_event.evidence_request_id_);
        }
        aggregate_contribution_revision contribution;
        const bool contributes = _event.episode_revision_ == 1U &&
            (_event.kind_ == feature_event_kind::episode_opened ||
             _event.kind_ == feature_event_kind::snapshot);
        if (contributes) {
            contribution.contribution_id_ = _event.event_id_;
            contribution.revision_ = 1U;
            contribution.episode_id_ = _event.event_id_;
            contribution.source_id_ = _event.source_id_;
            contribution.aggregate_definition_id_ = _event.event_schema_id_ + ".count";
            contribution.scene_revision_ = profile->scene_revision_;
            contribution.definition_revision_ = episode.rule_revision_;
            contribution.bucket_begin_ns_ =
                (occurred_ns / config_.aggregate_bucket_ns_) * config_.aggregate_bucket_ns_;
            if (contribution.bucket_begin_ns_ >
                std::numeric_limits<std::int64_t>::max() - config_.aggregate_bucket_ns_) {
                return {status_code::resource_exhausted,
                    "metadata aggregate bucket time overflows"};
            }
            contribution.bucket_end_ns_ =
                contribution.bucket_begin_ns_ + config_.aggregate_bucket_ns_;
            contribution.recorded_ns_ = occurred_ns;
            contribution.numerator_microunits_ =
                static_cast<std::int64_t>(g_microunits_per_event);
            contribution.denominator_microunits_ =
                static_cast<std::int64_t>(g_microunits_per_event);
            contribution.observed_duration_ns_ = 1U;
            contribution.expected_duration_ns_ = 1U;
            contribution.required_access_domain_mask_ =
                vqec_vision_ai_cntr_stmet_get_access_domain_mask(
                    spatiotemporal_access_domain::aggregate);
            contribution.dimensions_.push_back({"semantic_type", _event.event_schema_id_});
            contribution.dimensions_.push_back({"source", _event.source_id_});
            contribution.dimensions_.push_back({"scene", profile->scene_revision_});
            if (_event.event_schema_id_ == "security.fire_smoke.event" &&
                !vqec_vision_ai_appl_mdrun_append_fire_smoke_dimensions(
                    _event, *profile, config_.hotspot_grid_columns_,
                    config_.hotspot_grid_rows_, contribution.dimensions_)) {
                return {status_code::invalid_argument,
                    "fire/smoke aggregate dimensions are incomplete"};
            }
        }
        const auto accepted = service_.vqec_vision_ai_appl_mdsvc_submit_projection(
            episode, contributes ? &contribution : nullptr);
        if (accepted.code_ != status_code::ok && config_.required_) {
            return accepted;
        }
        return downstream_.vqec_vision_ai_ports_fesnk_deliver_event(_event);
    }

    metadata_service_stats vqec_vision_ai_appl_mdrun_get_stats() const noexcept {
        return service_.vqec_vision_ai_appl_mdsvc_get_stats();
    }

private:
    status vqec_vision_ai_appl_mdrun_initialize_chunk(track_builder& _builder,
        const metadata_source_profile& _profile, const std::string& _source_id,
        const std::string& _model_revision, const std::string& _tracker_revision,
        const observation& _observation) {
        if (next_chunk_sequence_ == std::numeric_limits<std::uint64_t>::max()) {
            return {status_code::resource_exhausted,
                "metadata trajectory chunk sequence is exhausted"};
        }
        const auto chunk_sequence = next_chunk_sequence_++;
        _builder.chunk_ = {};
        const auto source = std::find_if(config_.sources_.begin(), config_.sources_.end(),
            [&_source_id](const auto& _candidate) {
                return _candidate.source_id_ == _source_id;
            });
        const auto source_slot = static_cast<std::size_t>(
            std::distance(config_.sources_.begin(), source));
        _builder.chunk_.chunk_id_ = "trajectory." + std::to_string(source_slot) + "." +
            runtime_instance_id_ + "." + std::to_string(chunk_sequence);
        _builder.chunk_.track_.device_id_ = config_.device_id_;
        _builder.chunk_.track_.source_id_ = _source_id;
        _builder.chunk_.track_.boot_id_ = boot_id_;
        _builder.chunk_.track_.source_epoch_ = _observation.frame_.source_epoch_;
        _builder.chunk_.track_.local_track_id_ = _observation.track_id_;
        _builder.chunk_.subject_ref_ = "track." + std::to_string(_observation.track_id_);
        _builder.chunk_.entity_category_ = _observation.class_id_;
        _builder.chunk_.chunk_sequence_ = chunk_sequence;
        _builder.chunk_.coordinate_space_ = spatiotemporal_coordinate_space::source_pixel;
        _builder.chunk_.anchor_ = spatiotemporal_anchor::box_footpoint;
        _builder.chunk_.resolution_ = config_.minimum_sample_interval_ns_ == 0U
            ? trajectory_resolution::observation_exact
            : trajectory_resolution::trajectory_bounded;
        _builder.chunk_.sample_mode_ = config_.minimum_sample_interval_ns_ == 0U
            ? trajectory_sample_mode::exact : trajectory_sample_mode::fixed_gap;
        _builder.chunk_.max_time_error_ns_ = config_.minimum_sample_interval_ns_;
        _builder.chunk_.required_access_domain_mask_ =
            vqec_vision_ai_cntr_stmet_get_access_domain_mask(
                spatiotemporal_access_domain::trajectory);
        _builder.model_revision_ = _model_revision;
        _builder.tracker_revision_ = _tracker_revision;
        (void)_profile;
        return {};
    }

    bool vqec_vision_ai_appl_mdrun_make_point(
        const observation& _observation, trajectory_point& _point) const {
        _point.frame_id_ = _observation.frame_.frame_id_;
        _point.source_pts_ns_ = _observation.frame_.source_pts_ns_;
        return vqec_vision_ai_appl_mdrun_round_i32(
                   _observation.box_.x_ + _observation.box_.width_ / 2.0F,
                   _point.anchor_x_) &&
            vqec_vision_ai_appl_mdrun_round_i32(
                _observation.box_.y_ + _observation.box_.height_, _point.anchor_y_) &&
            vqec_vision_ai_appl_mdrun_round_i32(
                _observation.box_.x_, _point.box_left_) &&
            vqec_vision_ai_appl_mdrun_round_i32(
                _observation.box_.y_, _point.box_top_) &&
            vqec_vision_ai_appl_mdrun_round_i32(
                _observation.box_.x_ + _observation.box_.width_, _point.box_right_) &&
            vqec_vision_ai_appl_mdrun_round_i32(
                _observation.box_.y_ + _observation.box_.height_, _point.box_bottom_) &&
            (_point.flags_ = static_cast<std::uint32_t>(trajectory_point_flag::observed) |
                 static_cast<std::uint32_t>(trajectory_point_flag::has_box),
                true);
    }

    void vqec_vision_ai_appl_mdrun_update_chunk(trajectory_chunk& _chunk,
        const metadata_source_profile& _profile, const std::string& _model_revision,
        const std::string& _tracker_revision, const observation& _observation,
        const trajectory_point& _point) {
        spatiotemporal_frame_locator locator;
        locator.device_id_ = config_.device_id_;
        locator.source_id_ = _profile.source_id_;
        locator.boot_id_ = boot_id_;
        locator.source_epoch_ = _observation.frame_.source_epoch_;
        locator.frame_id_ = _observation.frame_.frame_id_;
        locator.source_pts_ns_ = _observation.frame_.source_pts_ns_;
        locator.clock_mapping_revision_ = _profile.clock_mapping_revision_;
        locator.scene_revision_ = _profile.scene_revision_;
        locator.coordinate_revision_ = _profile.coordinate_revision_;
        locator.model_revision_ = _model_revision;
        locator.tracker_revision_ = _tracker_revision;
        if (_chunk.points_.size() == 1U) {
            _chunk.first_frame_ = locator;
            _chunk.bounds_left_ = _point.box_left_;
            _chunk.bounds_top_ = _point.box_top_;
            _chunk.bounds_right_ = _point.box_right_;
            _chunk.bounds_bottom_ = _point.box_bottom_;
        }
        _chunk.last_frame_ = std::move(locator);
        _chunk.bounds_left_ = std::min(_chunk.bounds_left_, _point.box_left_);
        _chunk.bounds_top_ = std::min(_chunk.bounds_top_, _point.box_top_);
        _chunk.bounds_right_ = std::max(_chunk.bounds_right_, _point.box_right_);
        _chunk.bounds_bottom_ = std::max(_chunk.bounds_bottom_, _point.box_bottom_);
    }

    status vqec_vision_ai_appl_mdrun_flush(track_builder& _builder, bool _is_boundary) {
        if (_builder.chunk_.points_.size() < 2U) {
            if (_is_boundary) {
                _builder.chunk_ = {};
            }
            return {};
        }
        if (_is_boundary) {
            _builder.chunk_.points_.back().flags_ |=
                static_cast<std::uint32_t>(trajectory_point_flag::mandatory) |
                static_cast<std::uint32_t>(trajectory_point_flag::lifecycle_boundary);
        }
        const auto submitted =
            service_.vqec_vision_ai_appl_mdsvc_submit_trajectory(_builder.chunk_);
        if (submitted.code_ != status_code::ok) {
            return submitted;
        }
        _builder.chunk_ = {};
        return {};
    }

    metadata_runtime_config config_;
    feature_event_sink_port& downstream_;
    metadata_service service_;
    std::map<std::string, track_builder> tracks_;
    std::string boot_id_;
    std::string runtime_instance_id_;
    std::uint64_t next_chunk_sequence_{1};
    std::uint64_t last_maintenance_ns_{0};
    bool started_{false};
};

metadata_runtime::metadata_runtime(metadata_runtime_config _config,
    feature_event_sink_port& _downstream_sink)
    : implementation_(std::make_unique<implementation>(
          std::move(_config), _downstream_sink)) {}

metadata_runtime::~metadata_runtime() noexcept = default;

status metadata_runtime::vqec_vision_ai_appl_mdrun_start() {
    return implementation_->vqec_vision_ai_appl_mdrun_start();
}

status metadata_runtime::vqec_vision_ai_appl_mdrun_stop(bool _drain) noexcept {
    return implementation_->vqec_vision_ai_appl_mdrun_stop(_drain);
}

status metadata_runtime::vqec_vision_ai_appl_mdrun_submit_observations(
    const std::string& _source_id, const std::string& _model_revision,
    const std::string& _tracker_revision, const observation_batch& _batch,
    bool _is_authorized) {
    return implementation_->vqec_vision_ai_appl_mdrun_submit_observations(
        _source_id, _model_revision, _tracker_revision, _batch, _is_authorized);
}

status metadata_runtime::vqec_vision_ai_appl_mdrun_maintain(
    std::uint64_t _source_time_ns) {
    return implementation_->vqec_vision_ai_appl_mdrun_maintain(_source_time_ns);
}

const metadata_source_profile* metadata_runtime::vqec_vision_ai_appl_mdrun_find_source(
    const std::string& _source_id) const noexcept {
    return implementation_->vqec_vision_ai_appl_mdrun_find_source(_source_id);
}

status metadata_runtime::vqec_vision_ai_ports_fesnk_deliver_event(
    const feature_event& _event) {
    return implementation_->vqec_vision_ai_ports_fesnk_deliver_event(_event);
}

metadata_service_stats metadata_runtime::vqec_vision_ai_appl_mdrun_get_stats() const noexcept {
    return implementation_->vqec_vision_ai_appl_mdrun_get_stats();
}

}  // namespace vqec::vision::ai
