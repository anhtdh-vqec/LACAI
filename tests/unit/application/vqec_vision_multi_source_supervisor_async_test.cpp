// Device-free test for the supervisor's opt-in async mode: sessions run on per-source
// workers, a blocked source does not stall the control loop or the other source, faults stay
// isolated, and stop/drain joins the workers.

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <thread>

#include "vqec_vision_multi_source_supervisor.hpp"

using namespace vqec::vision::ai;

namespace {

constexpr auto g_worker_poll_interval = std::chrono::milliseconds(1);

class async_session final : public source_session_port {
public:
    explicit async_session(bool _gate)
        : gate_(_gate), control_thread_(std::this_thread::get_id()) {}

    [[nodiscard]] status vqec_vision_ai_appl_srcsn_step(
        std::uint64_t _now_ns, tensor_result& _result,
        source_session_progress& _progress) override {
        if (gate_) {
            {
                std::lock_guard<std::mutex> start_lock(start_mutex_);
                ++entered_;
                start_condition_.notify_all();
            }
            std::unique_lock<std::mutex> lock(mutex_);
            condition_.wait(lock, [this]() { return is_open_.load(); });
        }
        health_.phase_ = source_session_phase::running;
        ++steps_;
        _progress = {};
        _progress.has_result_ = true;
        _progress.model_slot_ = 0;
        _result.pipeline_pts_ns_ = _now_ns;
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_appl_srcsn_request_stop(std::uint64_t) override {
        ++stop_calls_;
        health_.phase_ = source_session_phase::stopped;
        return {};
    }
    [[nodiscard]] source_session_health
    vqec_vision_ai_appl_srcsn_get_health() const noexcept override {
        if (exclusive_worker_reads_.load() &&
            std::this_thread::get_id() == control_thread_) {
            ++unexpected_control_health_reads_;
        }
        return health_;
    }

    void arm_exclusive_worker_reads() noexcept {
        exclusive_worker_reads_ = true;
    }
    unsigned unexpected_control_health_reads() const noexcept {
        return unexpected_control_health_reads_.load();
    }

    void open() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            is_open_ = true;
        }
        condition_.notify_all();
    }
    bool wait_started(unsigned _count) {
        std::unique_lock<std::mutex> start_lock(start_mutex_);
        return start_condition_.wait_for(start_lock, std::chrono::seconds(3),
            [this, _count]() { return entered_.load() >= _count; });
    }
    bool has_started() const noexcept { return entered_.load() >= 1U; }

    bool gate_{false};
    unsigned steps_{0};
    unsigned stop_calls_{0};

private:
    source_session_health health_;
    std::mutex mutex_;
    std::condition_variable condition_;
    std::mutex start_mutex_;
    std::condition_variable start_condition_;
    std::atomic<bool> is_open_{false};
    std::atomic<unsigned> entered_{0};
    std::thread::id control_thread_;
    mutable std::atomic<bool> exclusive_worker_reads_{false};
    mutable std::atomic<unsigned> unexpected_control_health_reads_{0};
};

multi_source_supervisor_config make_config(bool _workers) {
    multi_source_supervisor_config config;
    config.deployment_revision_ = 11;
    config.catalog_revision_ = 22;
    config.source_count_ = 2;
    config.use_session_workers_ = _workers;
    return config;
}

}  // namespace

int main() {
    unsigned failures = 0;
    const auto check = [&failures](bool _condition) {
        if (!_condition) {
            ++failures;
        }
    };

    async_session fast(false);
    async_session slow(true);
    multi_source_supervisor supervisor(make_config(true));
    check(supervisor.vqec_vision_ai_appl_mssup_bind_session(0, fast).code_ == status_code::ok);
    check(supervisor.vqec_vision_ai_appl_mssup_bind_session(1, slow).code_ == status_code::ok);
    check(supervisor.vqec_vision_ai_appl_mssup_activate().code_ == status_code::ok);
    fast.arm_exclusive_worker_reads();
    slow.arm_exclusive_worker_reads();

    tensor_result result;
    multi_source_progress_report report;
    std::uint64_t now = 1;
    bool saw_fast_result = false;
    for (unsigned spin = 0; spin < 2000 && !saw_fast_result; ++spin) {
        const auto status = supervisor.vqec_vision_ai_appl_mssup_step(now++, result, report);
        check(status.code_ == status_code::ok || status.code_ == status_code::pending);
        if (report.has_result_ && report.source_index_ == 0) {
            saw_fast_result = true;
        }
        std::this_thread::sleep_for(g_worker_poll_interval);
    }
    check(saw_fast_result);

    // Keep stepping until the round-robin hands a step to the slow source, so its worker is
    // deterministically blocked in the session call.
    for (unsigned spin = 0; spin < 2000 && !slow.has_started(); ++spin) {
        (void)supervisor.vqec_vision_ai_appl_mssup_step(now++, result, report);
        std::this_thread::sleep_for(g_worker_poll_interval);
    }
    // The slow source is blocked in its worker; the control step must return quickly.
    check(slow.wait_started(1));
    check(!supervisor.vqec_vision_ai_appl_mssup_is_quiescent());
    const auto started = std::chrono::steady_clock::now();
    (void)supervisor.vqec_vision_ai_appl_mssup_step(now++, result, report);
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started).count();
    check(elapsed < 500);

    // Release the slow source so its completion is observed.
    slow.open();
    bool saw_slow_result = false;
    for (unsigned spin = 0; spin < 2000 && !saw_slow_result; ++spin) {
        (void)supervisor.vqec_vision_ai_appl_mssup_step(now++, result, report);
        if (report.has_result_ && report.source_index_ == 1) {
            saw_slow_result = true;
        }
        std::this_thread::sleep_for(g_worker_poll_interval);
    }
    check(saw_slow_result);

    check(supervisor.vqec_vision_ai_appl_mssup_request_stop(now++).code_ == status_code::ok);
    for (unsigned spin = 0; spin < 2000; ++spin) {
        (void)supervisor.vqec_vision_ai_appl_mssup_step(now++, result, report);
        if (supervisor.vqec_vision_ai_appl_mssup_get_state() ==
            multi_source_supervisor_state::stopped) {
            break;
        }
        std::this_thread::sleep_for(g_worker_poll_interval);
    }
    check(fast.stop_calls_ == 1 && slow.stop_calls_ == 1);
    check(supervisor.vqec_vision_ai_appl_mssup_get_state() ==
          multi_source_supervisor_state::stopped);
    check(fast.unexpected_control_health_reads() == 0 &&
          slow.unexpected_control_health_reads() == 0);
    check(supervisor.vqec_vision_ai_appl_mssup_drain().code_ == status_code::ok);

    std::cout << "supervisor async failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
