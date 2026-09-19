#include <cassert>
#include <cstdint>
#include <memory>
#include <string>

#include "vqec_vision_fire_smoke_factory.hpp"

namespace {

using namespace vqec::vision::ai;

constexpr char g_valid_configuration[] = R"json({
  "schema_version":1,
  "identity":{"app_id":"security.fire_smoke_detection","expected_model_id":"yolo11n_fire_smoke","expected_model_version":"1.0"},
  "classes":{"fire_enabled":true,"smoke_enabled":true,"fire_alarm_confidence":0.6,"smoke_alarm_confidence":0.6,"minimum_region_area_ratio":0.001},
  "temporal":{"confirmation_count":2,"confirmation_duration_ms":1,"clear_count":2,"clear_duration_ms":1,"update_interval_ms":100},
  "spatial":{"scene_revision":1,"association_iou":0.2,"include_zone_ids":[],"exclude_zone_ids":[]},
  "incident":{"max_active_incidents":4,"source_gap_policy":"interrupt","config_change_policy":"interrupt"},
  "severity":{"high_confidence":0.85,"critical_duration_ms":10},
  "evidence":{"enabled":true,"profile_ref":"security_alarm_v1","pre_duration_ms":5000,"post_duration_ms":10000,"snapshot":true,"clip":true,"retry_deadline_ms":300000},
  "metadata":{"retention_profile_ref":"security_event_v1","access_profile_ref":"security_operator_v1","aggregation_dimensions":["class","severity"]}
})json";

feature_configuration vqec_vision_ai_unit_fsatst_configuration(
    const std::string& _payload = g_valid_configuration) {
    feature_configuration configuration;
    configuration.schema_id_ = "security.fire_smoke.configuration";
    configuration.revision_ = 7;
    configuration.payload_.assign(_payload.begin(), _payload.end());
    return configuration;
}

feature_catalog_entry vqec_vision_ai_unit_fsatst_catalog_entry() {
    feature_catalog_entry feature;
    feature.feature_id_ = "fire_smoke_alarm";
    feature.feature_version_ = "1";
    feature.processor_contract_ = "fire_smoke_alarm";
    feature.configuration_schema_ = "security.fire_smoke.configuration";
    feature.resources_.max_temporal_bytes_per_source_ = 65536;
    feature.resources_.max_events_per_update_ = 8;
    feature.resources_.max_track_references_per_event_ = 8;
    feature.resources_.max_fields_per_event_ = 8;
    return feature;
}

feature_processor_config vqec_vision_ai_unit_fsatst_processor_config() {
    feature_processor_config config;
    config.source_id_ = "camera_front";
    config.feature_id_ = "fire_smoke_alarm";
    config.config_revision_ = 7;
    config.max_events_per_update_ = 8;
    config.max_track_references_per_event_ = 8;
    config.max_fields_per_event_ = 8;
    return config;
}

observation_batch vqec_vision_ai_unit_fsatst_batch(
    std::uint64_t _frame_id, std::uint64_t _pts_ns, bool _has_fire) {
    observation_batch batch;
    batch.frame_ = {1, 0, 1, _frame_id, _pts_ns};
    batch.geometry_ = {1920, 1080};
    if (_has_fire) {
        observation item;
        item.frame_ = batch.frame_;
        item.track_id_ = 42;
        item.class_id_ = "fire";
        item.box_ = {100.0F, 200.0F, 200.0F, 180.0F, 0xffffffffU, "fire"};
        item.confidence_ = 0.9F;
        item.quality_ = observation_quality::high;
        batch.observations_.push_back(std::move(item));
    }
    return batch;
}

void vqec_vision_ai_unit_fsatst_assert_valid_events(
    const feature_event_batch& _events, const observation_batch& _input) {
    const auto valid = vqec_vision_ai_core_ftevt_validate_batch(
        _events, _input.frame_, _input.geometry_,
        vqec_vision_ai_unit_fsatst_processor_config());
    assert(valid.code_ == status_code::ok);
}

void vqec_vision_ai_unit_fsatst_test_factory_rejects_unknown_and_zones() {
    fire_smoke_factory factory;
    auto feature = vqec_vision_ai_unit_fsatst_catalog_entry();
    auto processor_config = vqec_vision_ai_unit_fsatst_processor_config();
    auto configuration = vqec_vision_ai_unit_fsatst_configuration();
    assert(factory.vqec_vision_ai_ports_ftfac_validate_configuration(
        feature, processor_config, configuration).code_ == status_code::ok);

    const std::string unknown = std::string(g_valid_configuration).replace(
        std::string(g_valid_configuration).find("\"schema_version\":1"),
        std::string("\"schema_version\":1").size(),
        "\"schema_version\":1,\"unexpected\":true");
    configuration = vqec_vision_ai_unit_fsatst_configuration(unknown);
    assert(factory.vqec_vision_ai_ports_ftfac_validate_configuration(
        feature, processor_config, configuration).code_ ==
        status_code::invalid_argument);

    std::string zones = g_valid_configuration;
    const auto empty_zones = zones.find("\"include_zone_ids\":[]");
    assert(empty_zones != std::string::npos);
    zones.replace(empty_zones, std::string("\"include_zone_ids\":[]").size(),
        "\"include_zone_ids\":[\"zone_a\"]");
    configuration = vqec_vision_ai_unit_fsatst_configuration(zones);
    assert(factory.vqec_vision_ai_ports_ftfac_validate_configuration(
        feature, processor_config, configuration).code_ ==
        status_code::invalid_argument);
}

void vqec_vision_ai_unit_fsatst_test_episode_lifecycle() {
    fire_smoke_factory factory;
    std::unique_ptr<feature_processor_port> processor;
    const auto feature = vqec_vision_ai_unit_fsatst_catalog_entry();
    const auto processor_config = vqec_vision_ai_unit_fsatst_processor_config();
    const auto configuration = vqec_vision_ai_unit_fsatst_configuration();
    assert(factory.vqec_vision_ai_ports_ftfac_create_processor(
        feature, processor_config, configuration, processor).code_ == status_code::ok);
    assert(processor != nullptr);
    assert(processor->vqec_vision_ai_ports_ftpro_reset_epoch(1).code_ == status_code::ok);

    auto input = vqec_vision_ai_unit_fsatst_batch(1, 1000000, true);
    feature_event_batch events;
    assert(processor->vqec_vision_ai_ports_ftpro_process_observations(
        input, 1000000, false, events).code_ == status_code::ok);
    assert(events.events_.empty());

    input = vqec_vision_ai_unit_fsatst_batch(2, 2000000, true);
    assert(processor->vqec_vision_ai_ports_ftpro_process_observations(
        input, 2000000, false, events).code_ == status_code::ok);
    assert(events.events_.size() == 1);
    assert(events.events_[0].kind_ == feature_event_kind::episode_opened);
    assert(events.events_[0].episode_revision_ == 1);
    assert(!events.events_[0].evidence_request_id_.empty());
    const auto event_id = events.events_[0].event_id_;
    vqec_vision_ai_unit_fsatst_assert_valid_events(events, input);

    input = vqec_vision_ai_unit_fsatst_batch(3, 102000000, true);
    assert(processor->vqec_vision_ai_ports_ftpro_process_observations(
        input, 102000000, false, events).code_ == status_code::ok);
    assert(events.events_.size() == 1);
    assert(events.events_[0].kind_ == feature_event_kind::episode_updated);
    assert(events.events_[0].event_id_ == event_id);
    assert(events.events_[0].episode_revision_ == 2);
    vqec_vision_ai_unit_fsatst_assert_valid_events(events, input);

    input = vqec_vision_ai_unit_fsatst_batch(4, 102500000, false);
    assert(processor->vqec_vision_ai_ports_ftpro_process_observations(
        input, 102500000, false, events).code_ == status_code::ok);
    assert(events.events_.empty());
    input = vqec_vision_ai_unit_fsatst_batch(5, 104000000, false);
    assert(processor->vqec_vision_ai_ports_ftpro_process_observations(
        input, 104000000, false, events).code_ == status_code::ok);
    assert(events.events_.size() == 1);
    assert(events.events_[0].kind_ == feature_event_kind::episode_closed);
    assert(events.events_[0].event_id_ == event_id);
    assert(events.events_[0].episode_revision_ == 3);
    vqec_vision_ai_unit_fsatst_assert_valid_events(events, input);
}

void vqec_vision_ai_unit_fsatst_test_gap_interrupts_once() {
    fire_smoke_factory factory;
    std::unique_ptr<feature_processor_port> processor;
    const auto feature = vqec_vision_ai_unit_fsatst_catalog_entry();
    const auto config = vqec_vision_ai_unit_fsatst_processor_config();
    const auto configuration = vqec_vision_ai_unit_fsatst_configuration();
    assert(factory.vqec_vision_ai_ports_ftfac_create_processor(
        feature, config, configuration, processor).code_ == status_code::ok);
    assert(processor->vqec_vision_ai_ports_ftpro_reset_epoch(1).code_ == status_code::ok);
    feature_event_batch events;
    auto input = vqec_vision_ai_unit_fsatst_batch(1, 1000000, true);
    assert(processor->vqec_vision_ai_ports_ftpro_process_observations(
        input, 1000000, false, events).code_ == status_code::ok);
    input = vqec_vision_ai_unit_fsatst_batch(2, 2000000, true);
    assert(processor->vqec_vision_ai_ports_ftpro_process_observations(
        input, 2000000, false, events).code_ == status_code::ok);
    assert(events.events_.size() == 1);
    input = vqec_vision_ai_unit_fsatst_batch(3, 3000000, false);
    assert(processor->vqec_vision_ai_ports_ftpro_process_observations(
        input, 3000000, true, events).code_ == status_code::ok);
    assert(events.events_.size() == 1);
    assert(events.events_[0].kind_ == feature_event_kind::episode_closed);
    vqec_vision_ai_unit_fsatst_assert_valid_events(events, input);
    input = vqec_vision_ai_unit_fsatst_batch(4, 4000000, false);
    assert(processor->vqec_vision_ai_ports_ftpro_process_observations(
        input, 4000000, true, events).code_ == status_code::ok);
    assert(events.events_.empty());
}

void vqec_vision_ai_unit_fsatst_test_one_confirmation_per_frame() {
    fire_smoke_factory factory;
    std::unique_ptr<feature_processor_port> processor;
    const auto feature = vqec_vision_ai_unit_fsatst_catalog_entry();
    const auto config = vqec_vision_ai_unit_fsatst_processor_config();
    const auto configuration = vqec_vision_ai_unit_fsatst_configuration();
    assert(factory.vqec_vision_ai_ports_ftfac_create_processor(
        feature, config, configuration, processor).code_ == status_code::ok);
    assert(processor->vqec_vision_ai_ports_ftpro_reset_epoch(1).code_ == status_code::ok);
    auto input = vqec_vision_ai_unit_fsatst_batch(1, 1000000, true);
    auto duplicate = input.observations_[0];
    duplicate.track_id_ = 43;
    input.observations_.push_back(std::move(duplicate));
    feature_event_batch events;
    assert(processor->vqec_vision_ai_ports_ftpro_process_observations(
        input, 1000000, false, events).code_ == status_code::ok);
    assert(events.events_.empty());
    input = vqec_vision_ai_unit_fsatst_batch(2, 2000000, true);
    assert(processor->vqec_vision_ai_ports_ftpro_process_observations(
        input, 2000000, false, events).code_ == status_code::ok);
    assert(events.events_.size() == 1);
    assert(events.events_[0].kind_ == feature_event_kind::episode_opened);
}

void vqec_vision_ai_unit_fsatst_test_failure_preserves_output() {
    fire_smoke_factory factory;
    std::unique_ptr<feature_processor_port> processor;
    const auto feature = vqec_vision_ai_unit_fsatst_catalog_entry();
    const auto config = vqec_vision_ai_unit_fsatst_processor_config();
    const auto configuration = vqec_vision_ai_unit_fsatst_configuration();
    assert(factory.vqec_vision_ai_ports_ftfac_create_processor(
        feature, config, configuration, processor).code_ == status_code::ok);
    assert(processor->vqec_vision_ai_ports_ftpro_reset_epoch(1).code_ == status_code::ok);
    auto input = vqec_vision_ai_unit_fsatst_batch(1, 1000000, true);
    input.frame_.source_epoch_ = 2;
    input.observations_[0].frame_ = input.frame_;
    feature_event_batch output;
    output.frame_.frame_id_ = 99;
    assert(processor->vqec_vision_ai_ports_ftpro_process_observations(
        input, 1000000, false, output).code_ == status_code::invalid_state);
    assert(output.frame_.frame_id_ == 99);
}

}  // namespace

int main() {
    vqec_vision_ai_unit_fsatst_test_factory_rejects_unknown_and_zones();
    vqec_vision_ai_unit_fsatst_test_episode_lifecycle();
    vqec_vision_ai_unit_fsatst_test_gap_interrupts_once();
    vqec_vision_ai_unit_fsatst_test_one_confirmation_per_frame();
    vqec_vision_ai_unit_fsatst_test_failure_preserves_output();
    return 0;
}
