#include <cassert>
#include <filesystem>
#include <string>
#include <unistd.h>

#include "vqec_vision_sqlite_evidence_outbox.hpp"

using namespace vqec::vision::ai;

namespace {

constexpr std::uint64_t g_fixture_database_bytes = 4U * 1024U * 1024U;
constexpr int g_fixture_busy_timeout_ms = 1000;

evidence_command vqec_vision_ai_unit_seotst_make_command(
    const std::string& _request_id) {
    evidence_command command;
    command.request_id_ = _request_id;
    command.event_id_ = "fire.event.1";
    command.event_revision_ = 1U;
    command.source_id_ = "camera.front";
    command.feature_id_ = "fire_smoke_alarm";
    command.schema_id_ = "security.fire_smoke.event";
    command.schema_version_ = "1";
    command.frame_ = {1U, 0U, 1U, 10U, 1000U};
    command.occurred_at_ns_ = 1000U;
    command.policy_revision_ = 2U;
    command.config_revision_ = 3U;
    command.fields_.push_back({"security.fire_smoke.evidence", "1",
        "security_alarm_v1,5000,10000,snapshot,clip,30000", 0.9F,
        observation_quality::high});
    return command;
}

sqlite_evidence_outbox_config vqec_vision_ai_unit_seotst_make_config(
    const std::filesystem::path& _path) {
    return {_path.string(), g_fixture_database_bytes, g_fixture_busy_timeout_ms};
}

}  // namespace

int main() {
    char directory_template[] = "/tmp/vqec_vision_evidence_outbox_XXXXXX";
    const auto* directory = mkdtemp(directory_template);
    assert(directory != nullptr);
    const std::filesystem::path root(directory);
    const auto database = root / "outbox.db";
    const auto first = vqec_vision_ai_unit_seotst_make_command("fire.request.1");

    {
        sqlite_evidence_outbox outbox(
            vqec_vision_ai_unit_seotst_make_config(database));
        assert(outbox.vqec_vision_ai_ports_evobx_open().code_ == status_code::ok);
        assert(outbox.vqec_vision_ai_ports_evobx_enqueue(first).code_ == status_code::ok);
        assert(outbox.vqec_vision_ai_ports_evobx_enqueue(first).code_ == status_code::ok);
        auto conflict = first;
        conflict.config_revision_ = 4U;
        assert(outbox.vqec_vision_ai_ports_evobx_enqueue(conflict).code_ ==
               status_code::invalid_argument);
        evidence_outbox_record record;
        assert(outbox.vqec_vision_ai_ports_evobx_claim_due(10U, record).code_ ==
               status_code::ok);
        assert(record.command_.request_id_ == first.request_id_);
        assert(record.attempt_count_ == 1U);
        assert(outbox.vqec_vision_ai_ports_evobx_retry(
                   first.request_id_, record.attempt_count_, 100U, "receiver_offline")
                   .code_ == status_code::ok);
        assert(outbox.vqec_vision_ai_ports_evobx_claim_due(99U, record).code_ ==
               status_code::pending);
        assert(outbox.vqec_vision_ai_ports_evobx_claim_due(100U, record).code_ ==
               status_code::ok);
        assert(record.attempt_count_ == 2U);
    }
    {
        sqlite_evidence_outbox recovered(
            vqec_vision_ai_unit_seotst_make_config(database));
        assert(recovered.vqec_vision_ai_ports_evobx_open().code_ == status_code::ok);
        evidence_outbox_stats stats;
        assert(recovered.vqec_vision_ai_ports_evobx_get_stats(stats).code_ ==
               status_code::ok);
        assert(stats.pending_commands_ == 1U);
        assert(stats.recovered_claims_ == 1U);
        evidence_outbox_record record;
        assert(recovered.vqec_vision_ai_ports_evobx_claim_due(100U, record).code_ ==
               status_code::ok);
        assert(record.attempt_count_ == 3U);
        evidence_receipt receipt;
        receipt.request_id_ = first.request_id_;
        receipt.event_revision_ = first.event_revision_;
        receipt.state_ = evidence_receipt_state::ready;
        receipt.media_id_ = "media.fire.1";
        receipt.actual_begin_ns_ = 500U;
        receipt.actual_end_ns_ = 1500U;
        assert(recovered.vqec_vision_ai_ports_evobx_complete(receipt).code_ ==
               status_code::ok);
        assert(recovered.vqec_vision_ai_ports_evobx_get_stats(stats).code_ ==
               status_code::ok);
        assert(stats.pending_commands_ == 0U);
        assert(recovered.vqec_vision_ai_ports_evobx_enqueue(first).code_ ==
               status_code::ok);
        assert(recovered.vqec_vision_ai_ports_evobx_claim_due(1000U, record).code_ ==
               status_code::pending);
    }
    std::filesystem::remove_all(root);
    return 0;
}
