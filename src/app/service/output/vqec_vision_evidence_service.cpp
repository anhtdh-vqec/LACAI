#include "vqec_vision_evidence_service.hpp"

#include <algorithm>
#include <chrono>
#include <limits>
#include <new>
#include <system_error>
#include <utility>

namespace vqec::vision::ai {
namespace {

std::uint64_t vqec_vision_ai_appl_evsvc_monotonic_ns() noexcept {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
}

bool vqec_vision_ai_appl_evsvc_is_terminal(evidence_receipt_state _state) noexcept {
    return _state == evidence_receipt_state::ready ||
        _state == evidence_receipt_state::partial ||
        _state == evidence_receipt_state::failed ||
        _state == evidence_receipt_state::rejected ||
        _state == evidence_receipt_state::expired;
}

}  // namespace

evidence_service::evidence_service(evidence_service_config _config,
    evidence_outbox_port& _outbox, evidence_transport_port& _transport,
    output_gate& _output_gate)
    : config_(_config), outbox_(_outbox), transport_(_transport),
      output_gate_(_output_gate) {}

evidence_service::~evidence_service() noexcept {
    (void)vqec_vision_ai_appl_evsvc_stop(false);
}

status evidence_service::vqec_vision_ai_appl_evsvc_start() {
    std::lock_guard<std::mutex> guard(mutex_);
    if (running_) {
        return {status_code::invalid_state, "evidence service is already running"};
    }
    if (config_.initial_retry_ns_ == 0U || config_.maximum_retry_ns_ == 0U ||
        config_.initial_retry_ns_ > config_.maximum_retry_ns_ ||
        config_.idle_poll_ns_ == 0U || config_.stop_drain_ns_ == 0U ||
        config_.maximum_attempts_ == 0U ||
        config_.maximum_attempts_ > evidence_transport_limits::g_max_attempts) {
        return {status_code::invalid_argument,
            "invalid evidence service configuration"};
    }
    auto result = outbox_.vqec_vision_ai_ports_evobx_open();
    if (result.code_ != status_code::ok) {
        return result;
    }
    stopping_ = false;
    drain_requested_ = false;
    stop_deadline_ns_ = 0U;
    health_ = {};
    try {
        worker_ = std::thread(&evidence_service::vqec_vision_ai_appl_evsvc_run, this);
    } catch (const std::system_error&) {
        return {status_code::resource_exhausted,
            "cannot create evidence delivery worker"};
    }
    running_ = true;
    return {};
}

status evidence_service::vqec_vision_ai_appl_evsvc_stop(bool _drain) noexcept {
    {
        std::lock_guard<std::mutex> guard(mutex_);
        if (!running_) {
            return {};
        }
        stopping_ = true;
        drain_requested_ = _drain;
        const auto now = vqec_vision_ai_appl_evsvc_monotonic_ns();
        stop_deadline_ns_ = now > std::numeric_limits<std::uint64_t>::max() -
                config_.stop_drain_ns_ ?
            std::numeric_limits<std::uint64_t>::max() : now + config_.stop_drain_ns_;
    }
    wake_.notify_all();
    if (worker_.joinable()) {
        worker_.join();
    }
    std::lock_guard<std::mutex> guard(mutex_);
    running_ = false;
    return health_;
}

status evidence_service::vqec_vision_ai_ports_fesnk_deliver_event(
    const feature_event& _event) {
    {
        std::lock_guard<std::mutex> guard(mutex_);
        if (!running_ || stopping_) {
            return {status_code::invalid_state,
                "evidence service is not accepting events"};
        }
        if (_event.evidence_request_id_.empty()) {
            ++stats_.events_without_intent_;
            return {};
        }
    }
    evidence_command command;
    auto result = vqec_vision_ai_core_evtrn_make_command(_event, command);
    if (result.code_ != status_code::ok) {
        return result;
    }
    result = outbox_.vqec_vision_ai_ports_evobx_enqueue(command);
    if (result.code_ == status_code::ok) {
        std::lock_guard<std::mutex> guard(mutex_);
        ++stats_.commands_durable_;
        wake_.notify_one();
    }
    return result;
}

std::uint64_t evidence_service::vqec_vision_ai_appl_evsvc_retry_delay_ns(
    std::size_t _attempt_count) const noexcept {
    std::uint64_t delay = config_.initial_retry_ns_;
    for (std::size_t attempt = 1U; attempt < _attempt_count; ++attempt) {
        if (delay >= config_.maximum_retry_ns_ / 2U) {
            return config_.maximum_retry_ns_;
        }
        delay *= 2U;
    }
    return std::min(delay, config_.maximum_retry_ns_);
}

void evidence_service::vqec_vision_ai_appl_evsvc_run() noexcept {
    while (true) {
        const auto now = vqec_vision_ai_appl_evsvc_monotonic_ns();
        {
            std::unique_lock<std::mutex> guard(mutex_);
            if (stopping_ && (!drain_requested_ || now >= stop_deadline_ns_)) {
                break;
            }
        }
        evidence_outbox_record record;
        const auto claimed = outbox_.vqec_vision_ai_ports_evobx_claim_due(now, record);
        if (claimed.code_ == status_code::pending) {
            evidence_outbox_stats outbox_stats;
            const auto inspected = outbox_.vqec_vision_ai_ports_evobx_get_stats(outbox_stats);
            std::unique_lock<std::mutex> guard(mutex_);
            if (inspected.code_ != status_code::ok) {
                health_ = inspected;
                break;
            }
            if (stopping_ && drain_requested_ && outbox_stats.pending_commands_ == 0U) {
                break;
            }
            wake_.wait_for(guard, std::chrono::nanoseconds(config_.idle_poll_ns_));
            continue;
        }
        if (claimed.code_ != status_code::ok) {
            std::lock_guard<std::mutex> guard(mutex_);
            health_ = claimed;
            break;
        }

        output_authorization authorization;
        try {
            authorization.policy_revision_ = record.command_.policy_revision_;
            authorization.source_id_ = record.command_.source_id_;
            authorization.feature_id_ = record.command_.feature_id_;
            authorization.attributes_.reserve(record.command_.fields_.size());
            for (const auto& field : record.command_.fields_) {
                authorization.attributes_.push_back(field.schema_id_);
            }
        } catch (const std::bad_alloc&) {
            std::lock_guard<std::mutex> guard(mutex_);
            health_ = {status_code::resource_exhausted,
                "evidence authorization allocation failed"};
            break;
        }
        const auto authorized = output_gate_.vqec_vision_ai_core_otgat_authorize(
            authorization, now);
        if (authorized.code_ != status_code::ok) {
            evidence_receipt rejected;
            rejected.request_id_ = record.command_.request_id_;
            rejected.event_revision_ = record.command_.event_revision_;
            rejected.state_ = evidence_receipt_state::rejected;
            rejected.reason_ = "policy_not_authorized";
            const auto completed = outbox_.vqec_vision_ai_ports_evobx_complete(rejected);
            std::lock_guard<std::mutex> guard(mutex_);
            if (completed.code_ != status_code::ok) {
                health_ = completed;
                break;
            }
            ++stats_.commands_rejected_by_policy_;
            continue;
        }

        evidence_receipt receipt;
        const auto delivered = transport_.vqec_vision_ai_ports_evtrn_exchange(
            record.command_, receipt);
        if (delivered.code_ == status_code::ok &&
            vqec_vision_ai_appl_evsvc_is_terminal(receipt.state_)) {
            const auto completed = outbox_.vqec_vision_ai_ports_evobx_complete(receipt);
            std::lock_guard<std::mutex> guard(mutex_);
            if (completed.code_ != status_code::ok) {
                health_ = completed;
                break;
            }
            ++stats_.commands_completed_;
            continue;
        }

        std::string retry_reason = delivered.code_ == status_code::ok ?
            "receiver_not_terminal" : "transport_unavailable";
        if (record.attempt_count_ >= config_.maximum_attempts_) {
            evidence_receipt exhausted;
            exhausted.request_id_ = record.command_.request_id_;
            exhausted.event_revision_ = record.command_.event_revision_;
            exhausted.state_ = evidence_receipt_state::failed;
            exhausted.reason_ = "retry_exhausted";
            const auto completed = outbox_.vqec_vision_ai_ports_evobx_complete(exhausted);
            std::lock_guard<std::mutex> guard(mutex_);
            if (completed.code_ != status_code::ok) {
                health_ = completed;
                break;
            }
            ++stats_.commands_exhausted_;
            continue;
        }
        const auto delay = vqec_vision_ai_appl_evsvc_retry_delay_ns(
            record.attempt_count_);
        const auto next = now > std::numeric_limits<std::uint64_t>::max() - delay ?
            std::numeric_limits<std::uint64_t>::max() : now + delay;
        const auto retried = outbox_.vqec_vision_ai_ports_evobx_retry(
            record.command_.request_id_, record.attempt_count_, next, retry_reason);
        std::lock_guard<std::mutex> guard(mutex_);
        if (retried.code_ != status_code::ok) {
            health_ = retried;
            break;
        }
        ++stats_.commands_retried_;
        if (delivered.code_ != status_code::ok) {
            ++stats_.transport_failures_;
        }
    }
}

evidence_service_stats evidence_service::vqec_vision_ai_appl_evsvc_get_stats() const noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    return stats_;
}

status evidence_service::vqec_vision_ai_appl_evsvc_get_health() const noexcept {
    std::lock_guard<std::mutex> guard(mutex_);
    return health_;
}

}  // namespace vqec::vision::ai
