#include "vqec_vision_service_shutdown.hpp"

#include <chrono>
#include <cstdio>
#include <thread>

#include "vqec_vision_event_delivery_seam.hpp"
#include "vqec_vision_service_enrollment_runtime.hpp"
#include "vqec_vision_service_fixture.hpp"
#include "vqec_vision_service_options.hpp"
#include "vqec_vision_service_output_runtime.hpp"

namespace vqec::vision::ai {
namespace {

constexpr int g_reconcile_generation_exit_code = 4;
constexpr int g_recovery_required_exit_code = 5;
constexpr std::uint64_t g_stop_drain_final_observation_steps = 1U;

std::uint64_t vqec_vision_ai_appl_svshd_monotonic_ns() noexcept {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

void vqec_vision_ai_appl_svshd_print_metrics(
    const runtime_executor_metrics& _metrics, bool _stopped,
    std::uint32_t _routed_sources, status_code _first_error_code) noexcept {
    const auto route_avg_us = _metrics.end_to_end_samples_ == 0 ? 0ULL :
        _metrics.end_to_end_ns_sum_ / (1000ULL * _metrics.end_to_end_samples_);
    std::printf("metrics steps=%llu routed=%llu accepted=%llu denied=%llu failed=%llu "
        "cascade_tasks=%llu cascade_embeddings=%llu cascade_failed=%llu "
        "route_latency_avg_us=%llu route_latency_min_us=%llu route_latency_max_us=%llu "
        "samples=%u\n",
        static_cast<unsigned long long>(_metrics.steps_),
        static_cast<unsigned long long>(_metrics.results_routed_),
        static_cast<unsigned long long>(_metrics.events_accepted_),
        static_cast<unsigned long long>(_metrics.events_denied_),
        static_cast<unsigned long long>(_metrics.events_failed_),
        static_cast<unsigned long long>(_metrics.cascade_tasks_accepted_),
        static_cast<unsigned long long>(_metrics.cascade_embeddings_),
        static_cast<unsigned long long>(_metrics.cascade_tasks_failed_),
        static_cast<unsigned long long>(route_avg_us),
        static_cast<unsigned long long>(_metrics.end_to_end_samples_ == 0 ? 0ULL :
            _metrics.end_to_end_ns_min_ / 1000ULL),
        static_cast<unsigned long long>(_metrics.end_to_end_ns_max_ / 1000ULL),
        _metrics.end_to_end_samples_);
    std::printf("service stopped=%s routed_sources=%u first_error=%d\n",
        _stopped ? "true" : "false", _routed_sources,
        static_cast<int>(_first_error_code));
}

void vqec_vision_ai_appl_svshd_print_output_report(
    const service_output_runtime_report& _report) noexcept {
    if (_report.has_metadata_) {
        const auto& stats = _report.metadata_;
        std::printf("metadata accepted=%llu committed=%llu rejected=%llu failed=%llu\n",
            static_cast<unsigned long long>(stats.accepted_records_),
            static_cast<unsigned long long>(stats.committed_records_),
            static_cast<unsigned long long>(stats.rejected_records_),
            static_cast<unsigned long long>(stats.failed_records_));
    }
    if (_report.has_evidence_) {
        const auto& stats = _report.evidence_;
        std::printf("evidence durable=%llu retried=%llu completed=%llu rejected=%llu "
            "exhausted=%llu transport_failures=%llu\n",
            static_cast<unsigned long long>(stats.commands_durable_),
            static_cast<unsigned long long>(stats.commands_retried_),
            static_cast<unsigned long long>(stats.commands_completed_),
            static_cast<unsigned long long>(stats.commands_rejected_by_policy_),
            static_cast<unsigned long long>(stats.commands_exhausted_),
            static_cast<unsigned long long>(stats.transport_failures_));
    }
}

}  // namespace

int vqec_vision_ai_appl_svshd_decide_exit(
    bool _executor_stopped, bool _enrollment_stopped, bool _cascade_stopped,
    std::uint32_t _routed_sources, bool _generation_published,
    status_code _first_error_code, bool _reconcile_requested,
    std::uint32_t _required_sources) noexcept {
    if (!_executor_stopped || !_enrollment_stopped || !_cascade_stopped) {
        return g_recovery_required_exit_code;
    }
    if (!_generation_published || _first_error_code != status_code::ok) {
        return 1;
    }
    if (_reconcile_requested) {
        return g_reconcile_generation_exit_code;
    }
    return _routed_sources >= _required_sources ? 0 : 1;
}

int vqec_vision_ai_appl_svshd_stop_and_report(
    service_shutdown_context& _context) noexcept {
    if (_context.arguments_ == nullptr || _context.executor_ == nullptr ||
        _context.cascade_owners_ == nullptr || _context.enrollment_ == nullptr ||
        _context.output_ == nullptr) {
        return g_recovery_required_exit_code;
    }
    const auto& arguments = *_context.arguments_;
    auto& executor = *_context.executor_;
    auto& cascade_owners = *_context.cascade_owners_;
    auto first_error_code = _context.first_error_code_;
    auto steady_now_ns = _context.steady_now_ns_;

    std::printf("stopping after %llu steps\n",
        static_cast<unsigned long long>(_context.steps_));
    bool stopped = executor.vqec_vision_ai_appl_rtexe_get_snapshot().state_ ==
        application_composition_state::stopped;
    if (!stopped) {
        (void)executor.vqec_vision_ai_appl_rtexe_request_stop(steady_now_ns);
    }
    const auto workers_drained =
        vqec_vision_ai_appl_svcsc_drain_workers(cascade_owners, steady_now_ns);
    if (workers_drained.code_ != status_code::ok &&
        first_error_code == status_code::ok) {
        first_error_code = workers_drained.code_;
    }

    const auto stop_drain_steps =
        service_harness::g_default_stop_timeout_ns / arguments.runtime_step_interval_ns +
        (service_harness::g_default_stop_timeout_ns % arguments.runtime_step_interval_ns != 0U
                ? 1U : 0U) +
        g_stop_drain_final_observation_steps;
    for (std::uint64_t drain = 0U; drain < stop_drain_steps && !stopped; ++drain) {
        if (executor.vqec_vision_ai_appl_rtexe_has_pending()) {
            executor.vqec_vision_ai_appl_rtexe_discard_pending();
        }
        const auto clock_now = vqec_vision_ai_appl_svshd_monotonic_ns();
        steady_now_ns = clock_now > steady_now_ns ? clock_now :
            steady_now_ns + arguments.runtime_step_interval_ns;
        std::this_thread::sleep_for(
            std::chrono::nanoseconds(arguments.runtime_step_interval_ns));
        runtime_executor_report drain_report;
        const auto progressed =
            executor.vqec_vision_ai_appl_rtexe_step(steady_now_ns, drain_report);
        if (drain_report.first_error_code_ != status_code::ok &&
            first_error_code == status_code::ok) {
            first_error_code = drain_report.first_error_code_;
        }
        if (progressed.code_ != status_code::ok &&
            progressed.code_ != status_code::pending) {
            if (first_error_code == status_code::ok) {
                first_error_code = progressed.code_;
            }
            break;
        }
        stopped = executor.vqec_vision_ai_appl_rtexe_get_snapshot().state_ ==
            application_composition_state::stopped;
    }
    if (!stopped && first_error_code == status_code::ok) {
        first_error_code = status_code::timeout;
    }

    std::uint32_t routed_sources = 0;
    for (std::uint32_t mask = _context.routed_source_mask_;
         mask != 0; mask &= mask - 1U) {
        ++routed_sources;
    }
    const auto metrics = executor.vqec_vision_ai_appl_rtexe_get_metrics();
    const auto enrollment_stopped = _context.recognition_enabled_
        ? _context.enrollment_->vqec_vision_ai_appl_svenr_stop() : status{};
    if (enrollment_stopped.code_ != status_code::ok &&
        first_error_code == status_code::ok) {
        first_error_code = enrollment_stopped.code_;
    }
    const auto cascade_stopped =
        vqec_vision_ai_appl_svcsc_stop_graphs(cascade_owners);
    if (cascade_stopped.code_ != status_code::ok &&
        first_error_code == status_code::ok) {
        first_error_code = cascade_stopped.code_;
    }
    service_output_runtime_report output_report;
    const auto output_stopped =
        _context.output_->vqec_vision_ai_appl_svout_stop(true, output_report);
    if (output_stopped.code_ != status_code::ok &&
        first_error_code == status_code::ok) {
        first_error_code = output_stopped.code_;
    }
    vqec_vision_ai_appl_svshd_print_output_report(output_report);
    if (_context.production_platform_ && _context.production_seam_ != nullptr) {
        _context.production_seam_->vqec_vision_ai_outpt_evdsm_request_stop();
        _context.production_seam_->vqec_vision_ai_outpt_evdsm_discard_pending();
    }
    vqec_vision_ai_appl_svshd_print_metrics(
        metrics, stopped, routed_sources, first_error_code);
    return vqec_vision_ai_appl_svshd_decide_exit(
        stopped, enrollment_stopped.code_ == status_code::ok,
        cascade_stopped.code_ == status_code::ok, routed_sources,
        _context.generation_published_, first_error_code,
        _context.reconcile_requested_, arguments.require_sources);
}

}  // namespace vqec::vision::ai
