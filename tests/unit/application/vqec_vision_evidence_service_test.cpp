#include <cassert>
#include <chrono>
#include <filesystem>
#include <mutex>
#include <thread>
#include <unistd.h>

#include "vqec_vision_evidence_service.hpp"
#include "vqec_vision_sqlite_evidence_outbox.hpp"

using namespace vqec::vision::ai;

namespace {

constexpr std::uint64_t g_fixture_database_bytes = 4U * 1024U * 1024U;
constexpr std::uint64_t g_fixture_retry_ns = 1000000U;
constexpr std::uint64_t g_fixture_stop_ns = 100000000U;

class retry_transport final : public evidence_transport_port {
public:
    status vqec_vision_ai_ports_evtrn_exchange(
        const evidence_command& _command, evidence_receipt& _receipt) override {
        std::lock_guard<std::mutex> guard(mutex_);
        ++attempts_;
        if (attempts_ == 1U) {
            return {status_code::io_error, "synthetic lost acknowledgement"};
        }
        _receipt.request_id_ = _command.request_id_;
        _receipt.event_revision_ = _command.event_revision_;
        _receipt.state_ = evidence_receipt_state::ready;
        _receipt.media_id_ = "media.service.1";
        _receipt.actual_begin_ns_ = _command.occurred_at_ns_ - 1U;
        _receipt.actual_end_ns_ = _command.occurred_at_ns_ + 1U;
        return {};
    }

private:
    std::mutex mutex_;
    unsigned attempts_{0};
};

feature_event vqec_vision_ai_unit_evstst_make_event(const std::string& _request_id) {
    feature_event event;
    event.frame_ = {1U, 0U, 1U, 10U, 1000U};
    event.source_id_ = "camera.front";
    event.feature_id_ = "fire_smoke_alarm";
    event.event_id_ = "fire.event." + _request_id;
    event.event_schema_id_ = "security.fire_smoke.event";
    event.event_schema_version_ = "1";
    event.kind_ = feature_event_kind::episode_opened;
    event.occurred_at_ns_ = 1000U;
    event.config_revision_ = 1U;
    event.evidence_request_id_ = _request_id;
    event.episode_revision_ = 1U;
    event.episode_begin_ns_ = 1000U;
    event.policy_revision_ = 1U;
    event.fields_.push_back({"security.fire_smoke.evidence", "1", "profile",
        1.0F, observation_quality::high});
    return event;
}

bool vqec_vision_ai_unit_evstst_wait_for(
    evidence_service& _service, std::uint64_t evidence_service_stats::* _member,
    std::uint64_t _minimum) {
    for (unsigned attempt = 0U; attempt < 200U; ++attempt) {
        if (_service.vqec_vision_ai_appl_evsvc_get_stats().*_member >= _minimum) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return false;
}

}  // namespace

int main() {
    char directory_template[] = "/tmp/vqec_vision_evidence_service_XXXXXX";
    const auto* directory = mkdtemp(directory_template);
    assert(directory != nullptr);
    const auto database = std::filesystem::path(directory) / "outbox.db";
    sqlite_evidence_outbox outbox(
        {database.string(), g_fixture_database_bytes, 1000});
    retry_transport transport;
    output_gate gate;
    output_policy policy{1U, 1U, UINT64_MAX - 1U,
        {{"camera.front", "fire_smoke_alarm", {"security.fire_smoke.evidence"}}}};
    assert(gate.vqec_vision_ai_core_otgat_apply_policy(policy, 0U).code_ ==
           status_code::ok);
    evidence_service service({g_fixture_retry_ns, g_fixture_retry_ns,
        g_fixture_retry_ns, g_fixture_stop_ns, 3U}, outbox, transport, gate);
    assert(service.vqec_vision_ai_appl_evsvc_start().code_ == status_code::ok);
    const auto first = vqec_vision_ai_unit_evstst_make_event("fire.request.service.1");
    assert(service.vqec_vision_ai_ports_fesnk_deliver_event(first).code_ ==
           status_code::ok);
    assert(vqec_vision_ai_unit_evstst_wait_for(
        service, &evidence_service_stats::commands_completed_, 1U));
    const auto first_stats = service.vqec_vision_ai_appl_evsvc_get_stats();
    assert(first_stats.commands_retried_ == 1U);
    assert(first_stats.transport_failures_ == 1U);

    output_policy revoked{2U, 1U, UINT64_MAX - 1U, {}};
    assert(gate.vqec_vision_ai_core_otgat_apply_policy(revoked, 1U).code_ ==
           status_code::ok);
    const auto second = vqec_vision_ai_unit_evstst_make_event("fire.request.service.2");
    assert(service.vqec_vision_ai_ports_fesnk_deliver_event(second).code_ ==
           status_code::ok);
    assert(vqec_vision_ai_unit_evstst_wait_for(
        service, &evidence_service_stats::commands_rejected_by_policy_, 1U));
    assert(service.vqec_vision_ai_appl_evsvc_stop(true).code_ == status_code::ok);
    assert(service.vqec_vision_ai_appl_evsvc_get_health().code_ == status_code::ok);
    std::filesystem::remove_all(directory);
    return 0;
}
