#include "vqec_vision_service_output_runtime.hpp"

#include <cstdint>
#include <fstream>
#include <utility>

#include "vqec_vision_evidence_uds_client.hpp"
#include "vqec_vision_sqlite_evidence_outbox.hpp"

namespace vqec::vision::ai {
namespace {

constexpr std::uint64_t g_nanoseconds_per_millisecond = 1000000ULL;

bool vqec_vision_ai_appl_svout_is_evidence_configured(
    const parsed_arguments& _args) noexcept {
    return !_args.evidence_socket_path.empty() ||
        !_args.evidence_outbox_path.empty() || _args.evidence_peer_uid_set ||
        _args.evidence_io_timeout_ms != 0 ||
        _args.evidence_outbox_busy_timeout_ms != 0 ||
        _args.evidence_outbox_max_bytes != 0U ||
        _args.evidence_initial_retry_ms != 0U ||
        _args.evidence_maximum_retry_ms != 0U ||
        _args.evidence_idle_poll_ms != 0U ||
        _args.evidence_stop_drain_ms != 0U ||
        _args.evidence_maximum_attempts != 0U;
}

status vqec_vision_ai_appl_svout_validate_evidence_options(
    const parsed_arguments& _args) noexcept {
    if (!_args.production_mode || _args.evidence_socket_path.empty() ||
        _args.evidence_outbox_path.empty() || !_args.evidence_peer_uid_set ||
        _args.evidence_io_timeout_ms <= 0 ||
        _args.evidence_outbox_busy_timeout_ms <= 0 ||
        _args.evidence_outbox_max_bytes == 0U ||
        _args.evidence_outbox_max_bytes >
            service_options_limits::g_max_evidence_outbox_bytes ||
        _args.evidence_initial_retry_ms == 0U ||
        _args.evidence_maximum_retry_ms < _args.evidence_initial_retry_ms ||
        _args.evidence_maximum_retry_ms >
            service_options_limits::g_max_evidence_interval_ms ||
        _args.evidence_idle_poll_ms == 0U ||
        _args.evidence_idle_poll_ms >
            service_options_limits::g_max_evidence_interval_ms ||
        _args.evidence_stop_drain_ms == 0U ||
        _args.evidence_stop_drain_ms >
            service_options_limits::g_max_evidence_interval_ms ||
        _args.evidence_maximum_attempts == 0U ||
        _args.evidence_maximum_attempts > evidence_transport_limits::g_max_attempts) {
        return {status_code::invalid_argument,
            "evidence transport requires a complete bounded production configuration"};
    }
    return {};
}

}  // namespace

service_output_runtime::service_output_runtime() = default;

service_output_runtime::~service_output_runtime() noexcept {
    service_output_runtime_report ignored;
    (void)vqec_vision_ai_appl_svout_stop(false, ignored);
}

status service_output_runtime::vqec_vision_ai_appl_svout_start(
    const parsed_arguments& _args, const deployment_config& _deployment,
    feature_event_sink_port& _fallback_sink) {
    if (started_) {
        return {status_code::invalid_state, "service output runtime is already started"};
    }
    fallback_sink_ = &_fallback_sink;
    if (vqec_vision_ai_appl_svout_is_evidence_configured(_args)) {
        auto valid = vqec_vision_ai_appl_svout_validate_evidence_options(_args);
        if (valid.code_ != status_code::ok) {
            return valid;
        }
        evidence_outbox_ = std::make_unique<sqlite_evidence_outbox>(
            sqlite_evidence_outbox_config{_args.evidence_outbox_path,
                _args.evidence_outbox_max_bytes,
                _args.evidence_outbox_busy_timeout_ms});
        evidence_transport_ = std::make_unique<evidence_uds_client>(
            evidence_uds_client_config{_args.evidence_socket_path,
                _args.evidence_peer_uid, _args.evidence_io_timeout_ms});
        evidence_ = std::make_unique<evidence_service>(evidence_service_config{
            static_cast<std::uint64_t>(_args.evidence_initial_retry_ms) *
                g_nanoseconds_per_millisecond,
            static_cast<std::uint64_t>(_args.evidence_maximum_retry_ms) *
                g_nanoseconds_per_millisecond,
            static_cast<std::uint64_t>(_args.evidence_idle_poll_ms) *
                g_nanoseconds_per_millisecond,
            static_cast<std::uint64_t>(_args.evidence_stop_drain_ms) *
                g_nanoseconds_per_millisecond,
            _args.evidence_maximum_attempts},
            *evidence_outbox_, *evidence_transport_, gate_);
    }
    feature_event_sink_port& downstream = evidence_ != nullptr
        ? static_cast<feature_event_sink_port&>(*evidence_) : _fallback_sink;
    if (!_args.metadata_profile_path.empty()) {
        std::ifstream profile(_args.metadata_profile_path, std::ios::binary);
        metadata_runtime_config config;
        const auto loaded = profile
            ? vqec_vision_ai_appl_mdrun_load_config(profile, _deployment, config)
            : status{status_code::io_error, "cannot open metadata runtime profile"};
        if (loaded.code_ != status_code::ok) {
            return loaded;
        }
        metadata_required_ = config.required_;
        metadata_ = std::make_unique<metadata_runtime>(std::move(config), downstream);
        const auto metadata_started = metadata_->vqec_vision_ai_appl_mdrun_start();
        if (metadata_started.code_ != status_code::ok) {
            metadata_.reset();
            return metadata_started;
        }
    }
    started_ = true;
    return {};
}

status service_output_runtime::vqec_vision_ai_appl_svout_start_delivery() {
    if (!started_ || delivery_started_) {
        return {status_code::invalid_state,
            "service output runtime delivery state is invalid"};
    }
    if (evidence_ != nullptr) {
        const auto started = evidence_->vqec_vision_ai_appl_evsvc_start();
        if (started.code_ != status_code::ok) {
            return started;
        }
    }
    delivery_started_ = true;
    return {};
}

status service_output_runtime::vqec_vision_ai_appl_svout_stop(
    bool _drain, service_output_runtime_report& _report) noexcept {
    status result;
    if (metadata_ != nullptr) {
        const auto stopped = metadata_->vqec_vision_ai_appl_mdrun_stop(_drain);
        _report.metadata_ = metadata_->vqec_vision_ai_appl_mdrun_get_stats();
        _report.has_metadata_ = true;
        if (stopped.code_ != status_code::ok) {
            result = stopped;
        }
    }
    if (evidence_ != nullptr && delivery_started_) {
        const auto stopped = evidence_->vqec_vision_ai_appl_evsvc_stop(_drain);
        _report.evidence_ = evidence_->vqec_vision_ai_appl_evsvc_get_stats();
        _report.has_evidence_ = true;
        if (result.code_ == status_code::ok && stopped.code_ != status_code::ok) {
            result = stopped;
        }
    }
    delivery_started_ = false;
    started_ = false;
    return result;
}

output_gate& service_output_runtime::vqec_vision_ai_appl_svout_get_gate() noexcept {
    return gate_;
}

feature_event_sink_port& service_output_runtime::vqec_vision_ai_appl_svout_get_sink() noexcept {
    if (metadata_ != nullptr) {
        return *metadata_;
    }
    if (evidence_ != nullptr) {
        return *evidence_;
    }
    return *fallback_sink_;
}

metadata_runtime* service_output_runtime::vqec_vision_ai_appl_svout_get_metadata() noexcept {
    return metadata_.get();
}

bool service_output_runtime::vqec_vision_ai_appl_svout_is_metadata_required() const noexcept {
    return metadata_required_;
}

}  // namespace vqec::vision::ai
