#include <charconv>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#include <unistd.h>

#include "vqec/vision/ai/contracts/output/vqec_vision_output_gate.hpp"
#include "vqec_vision_evidence_service.hpp"
#include "vqec_vision_evidence_uds_client.hpp"
#include "vqec_vision_sqlite_evidence_outbox.hpp"

namespace vqec::vision::ai {
namespace {

constexpr std::uint64_t g_nanoseconds_per_millisecond = 1000000ULL;
constexpr std::uint64_t g_policy_revision = 1;
constexpr std::uint64_t g_event_revision = 1;
constexpr std::uint64_t g_config_revision = 1;
constexpr std::uint64_t g_outbox_bytes = 4U * 1024U * 1024U;
constexpr int g_busy_timeout_ms = 1000;
constexpr int g_io_timeout_ms = 1000;
constexpr std::uint64_t g_retry_ms = 10;
constexpr std::uint64_t g_idle_poll_ms = 5;
constexpr std::uint64_t g_stop_drain_ms = 1000;
constexpr std::size_t g_maximum_attempts = 5;
constexpr std::uint64_t g_maximum_timeout_ms = 60000;
constexpr char g_feature_id[] = "fire_smoke_alarm";
constexpr char g_event_schema_id[] = "security.fire_smoke.event";
constexpr char g_event_schema_version[] = "1";
constexpr char g_evidence_field[] = "security.fire_smoke.evidence";

struct evidence_probe_options {
    std::string socket_path_;
    std::string outbox_path_;
    std::string source_id_;
    std::string request_id_;
    std::uint32_t peer_uid_{0};
    std::uint64_t timeout_ms_{0};
};

bool vqec_vision_ai_tools_evprb_read_u64(
    std::string_view _text, std::uint64_t& _value) noexcept {
    if (_text.empty()) {
        return false;
    }
    const auto converted = std::from_chars(
        _text.data(), _text.data() + _text.size(), _value);
    return converted.ec == std::errc{} &&
        converted.ptr == _text.data() + _text.size();
}

bool vqec_vision_ai_tools_evprb_parse(
    int _argc, char** _argv, evidence_probe_options& _options) {
    evidence_probe_options candidate;
    for (int index = 1; index < _argc; ++index) {
        if (_argv[index] == nullptr || index + 1 >= _argc ||
            _argv[index + 1] == nullptr) {
            return false;
        }
        const std::string_view option(_argv[index]);
        const std::string value(_argv[++index]);
        if (option == "--socket") {
            candidate.socket_path_ = value;
        } else if (option == "--outbox") {
            candidate.outbox_path_ = value;
        } else if (option == "--source-id") {
            candidate.source_id_ = value;
        } else if (option == "--request-id") {
            candidate.request_id_ = value;
        } else if (option == "--peer-uid") {
            std::uint64_t peer_uid = 0;
            if (!vqec_vision_ai_tools_evprb_read_u64(value, peer_uid) ||
                peer_uid > UINT32_MAX) {
                return false;
            }
            candidate.peer_uid_ = static_cast<std::uint32_t>(peer_uid);
        } else if (option == "--timeout-ms") {
            if (!vqec_vision_ai_tools_evprb_read_u64(
                    value, candidate.timeout_ms_) || candidate.timeout_ms_ == 0 ||
                candidate.timeout_ms_ > g_maximum_timeout_ms) {
                return false;
            }
        } else {
            return false;
        }
    }
    if (candidate.socket_path_.empty() || candidate.outbox_path_.empty() ||
        candidate.source_id_.empty() || candidate.request_id_.empty() ||
        candidate.timeout_ms_ == 0) {
        return false;
    }
    _options = std::move(candidate);
    return true;
}

std::uint64_t vqec_vision_ai_tools_evprb_monotonic_ns() noexcept {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
}

feature_event vqec_vision_ai_tools_evprb_make_event(
    const evidence_probe_options& _options, std::uint64_t _now_ns) {
    feature_event event;
    event.frame_ = {1U, 0U, 1U, 1U, _now_ns};
    event.source_id_ = _options.source_id_;
    event.feature_id_ = g_feature_id;
    event.event_id_ = _options.request_id_ + ".event";
    event.event_schema_id_ = g_event_schema_id;
    event.event_schema_version_ = g_event_schema_version;
    event.kind_ = feature_event_kind::episode_opened;
    event.occurred_at_ns_ = _now_ns;
    event.config_revision_ = g_config_revision;
    event.evidence_request_id_ = _options.request_id_;
    event.episode_revision_ = g_event_revision;
    event.episode_begin_ns_ = _now_ns;
    event.policy_revision_ = g_policy_revision;
    event.fields_.push_back({g_evidence_field, g_event_schema_version,
        "security_alarm_v1,5000,10000,snapshot,clip,300000",
        1.0F, observation_quality::high});
    return event;
}

int vqec_vision_ai_tools_evprb_run(const evidence_probe_options& _options) {
    sqlite_evidence_outbox outbox({_options.outbox_path_, g_outbox_bytes,
        g_busy_timeout_ms});
    evidence_uds_client transport({_options.socket_path_, _options.peer_uid_,
        g_io_timeout_ms});
    output_gate gate;
    const auto now = vqec_vision_ai_tools_evprb_monotonic_ns();
    output_policy policy{g_policy_revision, now,
        now + (_options.timeout_ms_ + g_stop_drain_ms) *
            g_nanoseconds_per_millisecond,
        {{_options.source_id_, g_feature_id, {g_evidence_field}}}};
    auto result = gate.vqec_vision_ai_core_otgat_apply_policy(policy, 0);
    if (result.code_ != status_code::ok) {
        std::cerr << "cannot apply probe output policy: " << result.message_ << '\n';
        return 1;
    }
    evidence_service service({g_retry_ms * g_nanoseconds_per_millisecond,
        g_retry_ms * g_nanoseconds_per_millisecond,
        g_idle_poll_ms * g_nanoseconds_per_millisecond,
        g_stop_drain_ms * g_nanoseconds_per_millisecond,
        g_maximum_attempts}, outbox, transport, gate);
    result = service.vqec_vision_ai_appl_evsvc_start();
    if (result.code_ != status_code::ok) {
        std::cerr << "cannot start evidence probe: " << result.message_ << '\n';
        return 1;
    }
    const auto event = vqec_vision_ai_tools_evprb_make_event(_options, now);
    result = service.vqec_vision_ai_ports_fesnk_deliver_event(event);
    const auto deadline = std::chrono::steady_clock::now() +
        std::chrono::milliseconds(_options.timeout_ms_);
    while (result.code_ == status_code::ok &&
           std::chrono::steady_clock::now() < deadline) {
        const auto stats = service.vqec_vision_ai_appl_evsvc_get_stats();
        if (stats.commands_completed_ == 1U) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(g_idle_poll_ms));
    }
    const auto stopped = service.vqec_vision_ai_appl_evsvc_stop(true);
    const auto stats = service.vqec_vision_ai_appl_evsvc_get_stats();
    std::cout << "request_id=" << _options.request_id_
              << " durable=" << stats.commands_durable_
              << " completed=" << stats.commands_completed_
              << " retries=" << stats.commands_retried_
              << " transport_failures=" << stats.transport_failures_ << '\n';
    if (result.code_ != status_code::ok || stopped.code_ != status_code::ok ||
        stats.commands_durable_ != 1U || stats.commands_completed_ != 1U) {
        std::cerr << "evidence probe did not reach a terminal receipt\n";
        return 1;
    }
    return 0;
}

void vqec_vision_ai_tools_evprb_usage() {
    std::cerr << "usage: vqec_vision_evidence_probe --socket <path> "
                 "--outbox <path> --source-id <id> --request-id <id> "
                 "--peer-uid <uid> --timeout-ms <ms>\n";
}

}  // namespace
}  // namespace vqec::vision::ai

int main(int argc, char** argv) {
    vqec::vision::ai::evidence_probe_options options;
    if (!vqec::vision::ai::vqec_vision_ai_tools_evprb_parse(argc, argv, options)) {
        vqec::vision::ai::vqec_vision_ai_tools_evprb_usage();
        return 2;
    }
    return vqec::vision::ai::vqec_vision_ai_tools_evprb_run(options);
}
