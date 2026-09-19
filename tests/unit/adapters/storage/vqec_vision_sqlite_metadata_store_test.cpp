#include "vqec_vision_sqlite_metadata_store.hpp"

#include <cassert>
#include <cstdio>
#include <string>
#include <unistd.h>
#include <vector>

namespace {

constexpr std::size_t g_fixture_max_payload_bytes = 4096U;
constexpr std::size_t g_fixture_max_page_size = 16U;
constexpr std::uint32_t g_fixture_busy_timeout_ms = 1000U;
constexpr std::uint32_t g_fixture_wal_checkpoint_pages = 64U;
constexpr const char* g_fixture_source_id = "camera.front_gate";
constexpr const char* g_fixture_device_id = "device.fixture";
constexpr const char* g_fixture_boot_id = "boot.fixture";
constexpr const char* g_fixture_producer_revision = "fixture.v1";

using namespace vqec::vision::ai;

std::uint32_t vqec_vision_ai_unit_mdstst_combine_scopes(
    metadata_scope _left, metadata_scope _right) {
    return vqec_vision_ai_cntr_mdqry_get_scope_mask(_left) |
           vqec_vision_ai_cntr_mdqry_get_scope_mask(_right);
}

metadata_record vqec_vision_ai_unit_mdstst_make_record(
    const std::string& _record_id, metadata_record_family _family,
    metadata_scope _scope, const std::string& _subject_ref,
    const std::string& _semantic_type, const std::string& _typed_value,
    std::int64_t _begin_ns, std::int64_t _end_ns) {
    metadata_record record;
    record.record_id_ = _record_id;
    record.revision_ = 1U;
    record.family_ = _family;
    record.device_id_ = g_fixture_device_id;
    record.source_id_ = g_fixture_source_id;
    record.boot_id_ = g_fixture_boot_id;
    record.source_epoch_ = 1U;
    record.subject_ref_ = _subject_ref;
    record.semantic_type_ = _semantic_type;
    record.typed_value_ = _typed_value;
    record.value_state_ = metadata_value_state::known;
    record.valid_begin_ns_ = _begin_ns;
    record.valid_end_ns_ = _end_ns;
    record.recorded_ns_ = _begin_ns;
    record.confidence_ppm_ = g_metadata_query_confidence_scale_ppm;
    record.sensitivity_scope_ = _scope;
    record.producer_revision_ = g_fixture_producer_revision;
    record.payload_ = "fixture";
    return record;
}

metadata_query_request vqec_vision_ai_unit_mdstst_make_request(
    const std::string& _query_id, metadata_query_kind _kind, std::uint32_t _scopes) {
    metadata_query_request request;
    request.query_id_ = _query_id;
    request.kind_ = _kind;
    request.source_ids_ = {g_fixture_source_id};
    request.begin_ns_ = 1;
    request.end_ns_ = 1000;
    request.projection_ = {"record_id", "typed_value"};
    request.allowed_scope_mask_ = _scopes;
    request.authorization_revision_ = 1U;
    request.page_size_ = 2U;
    return request;
}

void vqec_vision_ai_unit_mdstst_remove_database(const std::string& _path) {
    std::remove(_path.c_str());
    std::remove((_path + "-wal").c_str());
    std::remove((_path + "-shm").c_str());
}

void vqec_vision_ai_unit_mdstst_require_ok(const status& _status) {
    if (_status.code_ != status_code::ok) {
        std::fprintf(stderr, "metadata test failure: %s\n", _status.message_.c_str());
        assert(false);
    }
}

}  // namespace

int main() {
    char path_template[] = "/tmp/vqec_vision_metadata_test_XXXXXX";
    const int temporary_fd = mkstemp(path_template);
    assert(temporary_fd >= 0);
    close(temporary_fd);
    vqec_vision_ai_unit_mdstst_remove_database(path_template);

    sqlite_metadata_store_config config;
    config.database_path_ = path_template;
    config.max_payload_bytes_ = g_fixture_max_payload_bytes;
    config.max_page_size_ = g_fixture_max_page_size;
    config.busy_timeout_ms_ = g_fixture_busy_timeout_ms;
    config.wal_autocheckpoint_pages_ = g_fixture_wal_checkpoint_pages;
    config.is_full_sync_ = true;

    sqlite_metadata_store store(config);
    assert(store.vqec_vision_ai_stor_mdsql_open().code_ == status_code::ok);

    auto coverage = vqec_vision_ai_unit_mdstst_make_record(
        "coverage.1", metadata_record_family::coverage_health_interval,
        metadata_scope::operational, "runtime", "coverage", "complete", 1, 1000);
    auto attribute = vqec_vision_ai_unit_mdstst_make_record(
        "attribute.1", metadata_record_family::attribute_assertion,
        metadata_scope::visual_attribute, "person.1", "upper_clothing_color", "red", 100, 200);
    auto passage = vqec_vision_ai_unit_mdstst_make_record(
        "passage.1", metadata_record_family::passage_presence,
        metadata_scope::trajectory, "person.1", "line_crossing", "cross", 150, 151);
    passage.scene_ref_ = "gate.a";
    const std::vector<std::string> sinks{"kafka.metadata"};
    assert(store.vqec_vision_ai_stor_mdsql_ingest_record(coverage, sinks).code_ == status_code::ok);
    assert(store.vqec_vision_ai_stor_mdsql_ingest_record(attribute, sinks).code_ == status_code::ok);
    assert(store.vqec_vision_ai_stor_mdsql_ingest_record(passage, sinks).code_ == status_code::ok);
    assert(store.vqec_vision_ai_stor_mdsql_ingest_record(attribute, sinks).code_ == status_code::ok);
    auto conflicting_attribute = attribute;
    conflicting_attribute.payload_ = "conflicting-fixture";
    assert(store.vqec_vision_ai_stor_mdsql_ingest_record(
               conflicting_attribute, sinks).code_ == status_code::invalid_argument);

    std::uint64_t outbox_count = 0U;
    assert(store.vqec_vision_ai_stor_mdsql_get_outbox_size(
               "kafka.metadata", outbox_count).code_ == status_code::ok);
    assert(outbox_count == 3U);

    auto attribute_at_passage = vqec_vision_ai_unit_mdstst_make_request(
        "Q02", metadata_query_kind::q02_attribute_at_event,
        vqec_vision_ai_unit_mdstst_combine_scopes(
            metadata_scope::visual_attribute, metadata_scope::trajectory));
    attribute_at_passage.scene_ref_ = "gate.a";
    attribute_at_passage.semantic_type_ = "upper_clothing_color";
    attribute_at_passage.typed_value_ = "red";
    metadata_query_page page;
    vqec_vision_ai_unit_mdstst_require_ok(
        store.vqec_vision_ai_stor_mdsql_query_records(attribute_at_passage, page));
    assert(page.records_.size() == 1U);
    assert(page.records_.front().record_id_ == "passage.1");
    assert(page.records_.front().subject_ref_.empty());
    assert(page.records_.front().payload_.empty());
    assert(page.completeness_ == metadata_query_completeness::complete);

    auto attribute_without_trajectory_scope = attribute_at_passage;
    attribute_without_trajectory_scope.allowed_scope_mask_ =
        vqec_vision_ai_cntr_mdqry_get_scope_mask(metadata_scope::visual_attribute);
    assert(store.vqec_vision_ai_stor_mdsql_query_records(
               attribute_without_trajectory_scope, page).code_ == status_code::unauthorized);

    auto mismatched_query_id = attribute_at_passage;
    mismatched_query_id.query_id_ = "Q01";
    assert(store.vqec_vision_ai_stor_mdsql_query_records(
               mismatched_query_id, page).code_ == status_code::invalid_argument);

    auto unauthorized = vqec_vision_ai_unit_mdstst_make_request(
        "Q12", metadata_query_kind::q12_plate_search,
        vqec_vision_ai_cntr_mdqry_get_scope_mask(metadata_scope::object));
    assert(store.vqec_vision_ai_stor_mdsql_query_records(
               unauthorized, page).code_ == status_code::unauthorized);

    auto unsupported = vqec_vision_ai_unit_mdstst_make_request(
        "Q21", metadata_query_kind::q21_speed,
        vqec_vision_ai_cntr_mdqry_get_scope_mask(metadata_scope::object));
    assert(store.vqec_vision_ai_stor_mdsql_query_records(
               unsupported, page).code_ == status_code::unsupported);
    assert(page.completeness_ == metadata_query_completeness::unsupported);

    auto event_one = vqec_vision_ai_unit_mdstst_make_record(
        "event.1", metadata_record_family::event_episode, metadata_scope::object,
        "scene", "fire_smoke", "active", 300, 400);
    auto event_two = vqec_vision_ai_unit_mdstst_make_record(
        "event.2", metadata_record_family::event_episode, metadata_scope::object,
        "scene", "intrusion", "ended", 400, 500);
    assert(store.vqec_vision_ai_stor_mdsql_ingest_record(event_one, {}).code_ == status_code::ok);
    assert(store.vqec_vision_ai_stor_mdsql_ingest_record(event_two, {}).code_ == status_code::ok);

    auto events = vqec_vision_ai_unit_mdstst_make_request(
        "Q08", metadata_query_kind::q08_event,
        vqec_vision_ai_cntr_mdqry_get_scope_mask(metadata_scope::object));
    events.page_size_ = 1U;
    metadata_query_page first_page;
    assert(store.vqec_vision_ai_stor_mdsql_query_records(events, first_page).code_ == status_code::ok);
    assert(first_page.records_.size() == 1U && first_page.has_more_);

    auto event_three = vqec_vision_ai_unit_mdstst_make_record(
        "event.3", metadata_record_family::event_episode, metadata_scope::object,
        "scene", "crowd", "active", 500, 600);
    assert(store.vqec_vision_ai_stor_mdsql_ingest_record(event_three, {}).code_ == status_code::ok);
    events.snapshot_sequence_ = first_page.snapshot_sequence_;
    events.cursor_sequence_ = first_page.next_cursor_sequence_;
    metadata_query_page second_page;
    assert(store.vqec_vision_ai_stor_mdsql_query_records(events, second_page).code_ == status_code::ok);
    assert(second_page.records_.size() == 1U && !second_page.has_more_);

    assert(store.vqec_vision_ai_stor_mdsql_close().code_ == status_code::ok);
    assert(store.vqec_vision_ai_stor_mdsql_open().code_ == status_code::ok);
    metadata_query_page recovered_page;
    events.snapshot_sequence_ = 0U;
    events.cursor_sequence_ = 0U;
    events.page_size_ = 2U;
    assert(store.vqec_vision_ai_stor_mdsql_query_records(events, recovered_page).code_ ==
           status_code::ok);
    assert(recovered_page.records_.size() == 2U && recovered_page.has_more_);
    assert(store.vqec_vision_ai_stor_mdsql_close().code_ == status_code::ok);

    vqec_vision_ai_unit_mdstst_remove_database(path_template);
    return 0;
}
