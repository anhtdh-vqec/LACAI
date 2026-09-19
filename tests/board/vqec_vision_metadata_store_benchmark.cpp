#include "vqec_vision_sqlite_metadata_store.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace {

constexpr std::size_t g_fixture_default_record_count = 2000U;
constexpr std::size_t g_fixture_default_query_count = 200U;
constexpr std::size_t g_fixture_max_payload_bytes = 4096U;
constexpr std::size_t g_fixture_max_page_size = 128U;
constexpr std::uint32_t g_fixture_busy_timeout_ms = 1000U;
constexpr std::uint32_t g_fixture_wal_checkpoint_pages = 64U;
constexpr std::int64_t g_fixture_time_step_ns = 1000000;
constexpr const char* g_fixture_device_id = "device.benchmark";
constexpr const char* g_fixture_source_id = "camera.benchmark";
constexpr const char* g_fixture_boot_id = "boot.benchmark";
constexpr const char* g_fixture_producer_revision = "benchmark.v1";

using namespace vqec::vision::ai;

void vqec_vision_ai_board_mdben_remove_database(const std::string& _path) {
    std::remove(_path.c_str());
    std::remove((_path + "-wal").c_str());
    std::remove((_path + "-shm").c_str());
}

bool vqec_vision_ai_board_mdben_parse_count(
    const char* _text, std::size_t& _value) {
    if (_text == nullptr || *_text == '\0') {
        return false;
    }
    char* end = nullptr;
    const auto parsed = std::strtoull(_text, &end, 10);
    if (*end != '\0' || parsed == 0U) {
        return false;
    }
    _value = static_cast<std::size_t>(parsed);
    return static_cast<unsigned long long>(_value) == parsed;
}

metadata_record vqec_vision_ai_board_mdben_make_record(std::size_t _index) {
    const auto begin_ns = static_cast<std::int64_t>(_index) * g_fixture_time_step_ns;
    metadata_record record;
    record.record_id_ = "event." + std::to_string(_index);
    record.revision_ = 1U;
    record.family_ = metadata_record_family::event_episode;
    record.device_id_ = g_fixture_device_id;
    record.source_id_ = g_fixture_source_id;
    record.boot_id_ = g_fixture_boot_id;
    record.source_epoch_ = 1U;
    record.subject_ref_ = "scene";
    record.semantic_type_ = "benchmark_event";
    record.typed_value_ = "active";
    record.value_state_ = metadata_value_state::known;
    record.valid_begin_ns_ = begin_ns;
    record.valid_end_ns_ = begin_ns + g_fixture_time_step_ns;
    record.recorded_ns_ = begin_ns;
    record.confidence_ppm_ = g_metadata_query_confidence_scale_ppm;
    record.sensitivity_scope_ = metadata_scope::object;
    record.producer_revision_ = g_fixture_producer_revision;
    record.payload_ = "benchmark";
    return record;
}

std::uint64_t vqec_vision_ai_board_mdben_get_percentile(
    const std::vector<std::uint64_t>& _sorted_samples, std::size_t _percentile) {
    const auto numerator = _percentile * _sorted_samples.size();
    const auto index = std::min(_sorted_samples.size() - 1U,
        (numerator + 99U) / 100U - 1U);
    return _sorted_samples[index];
}

std::uint64_t vqec_vision_ai_board_mdben_get_file_bytes(const std::string& _path) {
    struct stat file_status {};
    return stat(_path.c_str(), &file_status) == 0
               ? static_cast<std::uint64_t>(file_status.st_size)
               : 0U;
}

int vqec_vision_ai_board_mdben_fail(
    const char* _operation, const status& _status, const std::string& _path) {
    std::fprintf(stderr, "%s: %s\n", _operation, _status.message_.c_str());
    vqec_vision_ai_board_mdben_remove_database(_path);
    return EXIT_FAILURE;
}

}  // namespace

int main(int _argument_count, char** _arguments) {
    std::size_t record_count = g_fixture_default_record_count;
    std::size_t query_count = g_fixture_default_query_count;
    if ((_argument_count > 1 &&
            !vqec_vision_ai_board_mdben_parse_count(_arguments[1], record_count)) ||
        (_argument_count > 2 &&
            !vqec_vision_ai_board_mdben_parse_count(_arguments[2], query_count)) ||
        _argument_count > 3) {
        std::fprintf(stderr, "usage: %s [record_count] [query_count]\n", _arguments[0]);
        return EXIT_FAILURE;
    }

    char path_template[] = "/tmp/vqec_vision_metadata_benchmark_XXXXXX";
    const int temporary_fd = mkstemp(path_template);
    if (temporary_fd < 0) {
        std::perror("create metadata benchmark path");
        return EXIT_FAILURE;
    }
    close(temporary_fd);
    const std::string database_path(path_template);
    vqec_vision_ai_board_mdben_remove_database(database_path);

    sqlite_metadata_store_config config;
    config.database_path_ = database_path;
    config.max_payload_bytes_ = g_fixture_max_payload_bytes;
    config.max_page_size_ = g_fixture_max_page_size;
    config.busy_timeout_ms_ = g_fixture_busy_timeout_ms;
    config.wal_autocheckpoint_pages_ = g_fixture_wal_checkpoint_pages;
    config.is_full_sync_ = true;

    sqlite_metadata_store store(config);
    auto result = store.vqec_vision_ai_stor_mdsql_open();
    if (result.code_ != status_code::ok) {
        return vqec_vision_ai_board_mdben_fail("open", result, database_path);
    }

    const auto insert_begin = std::chrono::steady_clock::now();
    for (std::size_t index = 0U; index < record_count; ++index) {
        const auto record = vqec_vision_ai_board_mdben_make_record(index);
        result = store.vqec_vision_ai_stor_mdsql_ingest_record(record, {});
        if (result.code_ != status_code::ok) {
            return vqec_vision_ai_board_mdben_fail("ingest", result, database_path);
        }
    }
    const auto insert_end = std::chrono::steady_clock::now();

    metadata_query_request request;
    request.query_id_ = "Q08";
    request.kind_ = metadata_query_kind::q08_event;
    request.source_ids_ = {g_fixture_source_id};
    request.begin_ns_ = 0;
    request.end_ns_ = static_cast<std::int64_t>(record_count + 1U) * g_fixture_time_step_ns;
    request.projection_ = {"record_id", "typed_value"};
    request.allowed_scope_mask_ =
        vqec_vision_ai_cntr_mdqry_get_scope_mask(metadata_scope::object);
    request.authorization_revision_ = 1U;
    request.page_size_ = g_fixture_max_page_size;

    std::vector<std::uint64_t> query_latency_us;
    query_latency_us.reserve(query_count);
    for (std::size_t index = 0U; index < query_count; ++index) {
        metadata_query_page page;
        const auto query_begin = std::chrono::steady_clock::now();
        result = store.vqec_vision_ai_stor_mdsql_query_records(request, page);
        const auto query_end = std::chrono::steady_clock::now();
        if (result.code_ != status_code::ok || page.records_.empty()) {
            return vqec_vision_ai_board_mdben_fail("query", result, database_path);
        }
        query_latency_us.push_back(static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(query_end - query_begin).count()));
    }
    std::sort(query_latency_us.begin(), query_latency_us.end());

    result = store.vqec_vision_ai_stor_mdsql_close();
    if (result.code_ != status_code::ok) {
        return vqec_vision_ai_board_mdben_fail("close", result, database_path);
    }
    const auto insert_us = std::chrono::duration_cast<std::chrono::microseconds>(
        insert_end - insert_begin).count();
    const auto records_per_second = insert_us == 0
        ? 0U
        : static_cast<std::uint64_t>(record_count) * 1000000U /
              static_cast<std::uint64_t>(insert_us);
    struct rusage usage {};
    getrusage(RUSAGE_SELF, &usage);
    std::printf(
        "{\"schema_version\":%u,\"sync\":\"full\",\"records\":%zu,"
        "\"queries\":%zu,\"insert_records_per_second\":%llu,"
        "\"query_p50_us\":%llu,\"query_p95_us\":%llu,\"query_p99_us\":%llu,"
        "\"max_rss_kib\":%ld,\"database_bytes\":%llu}\n",
        g_metadata_query_schema_version, record_count, query_count,
        static_cast<unsigned long long>(records_per_second),
        static_cast<unsigned long long>(vqec_vision_ai_board_mdben_get_percentile(
            query_latency_us, 50U)),
        static_cast<unsigned long long>(vqec_vision_ai_board_mdben_get_percentile(
            query_latency_us, 95U)),
        static_cast<unsigned long long>(vqec_vision_ai_board_mdben_get_percentile(
            query_latency_us, 99U)),
        usage.ru_maxrss,
        static_cast<unsigned long long>(
            vqec_vision_ai_board_mdben_get_file_bytes(database_path)));
    vqec_vision_ai_board_mdben_remove_database(database_path);
    return EXIT_SUCCESS;
}
