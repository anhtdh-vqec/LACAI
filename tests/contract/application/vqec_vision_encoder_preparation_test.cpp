#include <iostream>
#include <stdexcept>

#include "vqec_vision_encoder_preparation.hpp"

namespace vqec::vision::ai {
class preparation_test_backend final : public encoder_backend {
public:
    unsigned calls_{0};
    bool reject_{false};
    bool throw_after_retain_{false};
    std::shared_ptr<const std::vector<std::uint8_t>> retained_;
    status vqec_vision_ai_cntr_encbk_submit(const encoder_input& _input) override {
        ++calls_;
        if (reject_) {
            return {status_code::resource_exhausted, "synthetic admission rejection"};
        }
        retained_ = _input.pixels_;
        if (throw_after_retain_) {
            throw std::runtime_error("synthetic ambiguous submit failure");
        }
        return {};
    }
    status vqec_vision_ai_cntr_encbk_poll(encoder_event& _event) override {
        (void)_event;
        return {status_code::pending, "test does not generate events"};
    }
    status vqec_vision_ai_cntr_encbk_begin_drain() override {
        return retained_ ? status{status_code::pending, "test input retained"} : status{};
    }
};
}  // namespace vqec::vision::ai

int main() {
    using namespace vqec::vision::ai;
    static_assert(submission_limits::g_max_jobs == 4);
    static_assert(submission_limits::g_default_job_timeout_ns == 1000000000ULL);
    unsigned failures = 0;
    const auto check = [&](bool _condition) {
        if (!_condition) {
            ++failures;
        }
    };
    encoder_window_config config;
    config.submission_ = {1, 1, 0, 100, 2};
    config.geometry_ = {4, 2};
    config.max_input_bytes_ = 24;
    encoder_window window;
    preview_surface_pool pool;
    check(window.vqec_vision_ai_core_encwn_configure(config).code_ == status_code::ok);
    check(pool.vqec_vision_ai_core_pvpol_configure({4, 2}, 1, 12).code_ == status_code::ok);
    writable_preview_surface writer;
    preview_frame_key frame{0, 0, 1, 1, 10};
    submission_ticket ticket{{9, 9}, 9, 9, 9, 9};
    check(vqec_vision_ai_appl_enprp_prepare_input(window, pool, frame, false, 0, ticket, writer).code_ ==
        status_code::pending);
    check(ticket.token_.job_id_ == 9 && pool.vqec_vision_ai_core_pvpol_available() == 1);
    preview_surface_pool wrong_shape;
    check(wrong_shape.vqec_vision_ai_core_pvpol_configure({2, 4}, 1, 12).code_ == status_code::ok);
    check(vqec_vision_ai_appl_enprp_prepare_input(
        window, wrong_shape, frame, true, 0, ticket, writer).code_ == status_code::invalid_state);
    check(window.vqec_vision_ai_core_encwn_outstanding() == 0 && ticket.token_.job_id_ == 9);
    check(vqec_vision_ai_appl_enprp_prepare_input(window, pool, frame, true, 0, ticket, writer).code_ ==
        status_code::ok);
    check(writer.vqec_vision_ai_core_pvsrf_size_bytes() == 12);
    const auto first_ticket = ticket;
    writable_preview_surface empty_writer;
    check(vqec_vision_ai_appl_enprp_cancel_input(window, ticket.token_, empty_writer).code_ ==
        status_code::invalid_state);
    check(window.vqec_vision_ai_core_encwn_outstanding() == 1 &&
        window.vqec_vision_ai_core_encwn_reserved_bytes() == 12);
    check(writer.vqec_vision_ai_core_pvsrf_size_bytes() == 12 &&
        pool.vqec_vision_ai_core_pvpol_available() == 0);
    check(vqec_vision_ai_appl_enprp_prepare_input(window, pool, frame, true, 0, ticket, writer).code_ ==
        status_code::invalid_state);
    check(ticket.token_.job_id_ == first_ticket.token_.job_id_);
    check(vqec_vision_ai_appl_enprp_cancel_input(window, {99, 99}, writer).code_ ==
        status_code::invalid_state);
    check(writer.vqec_vision_ai_core_pvsrf_size_bytes() == 12);
    check(vqec_vision_ai_appl_enprp_cancel_input(window, ticket.token_, writer).code_ == status_code::ok);
    check(window.vqec_vision_ai_core_encwn_reserved_bytes() == 0 &&
        pool.vqec_vision_ai_core_pvpol_available() == 1);

    // Hold the only slot outside this preparation, simulating a retained downstream reader.
    writable_preview_surface held_writer;
    check(pool.vqec_vision_ai_core_pvpol_acquire(held_writer).code_ == status_code::ok);
    auto held_reader = held_writer.vqec_vision_ai_core_pvsrf_seal();
    ++frame.source_pts_ns_;
    ++frame.frame_id_;
    check(vqec_vision_ai_appl_enprp_prepare_input(window, pool, frame, true, 1, ticket, writer).code_ ==
        status_code::resource_exhausted);
    check(ticket.token_.job_id_ != first_ticket.token_.job_id_);
    check(window.vqec_vision_ai_core_encwn_outstanding() == 0 &&
        window.vqec_vision_ai_core_encwn_reserved_bytes() == 0);
    check(writer.vqec_vision_ai_core_pvsrf_size_bytes() == 0);
    held_reader.reset();
    check(vqec_vision_ai_appl_enprp_prepare_input(window, pool, frame, true, 2, ticket, writer).code_ ==
        status_code::invalid_argument);  // Rolled-back reservation consumed its source PTS.
    ++frame.source_pts_ns_;
    ++frame.frame_id_;
    check(vqec_vision_ai_appl_enprp_prepare_input(window, pool, frame, true, 2, ticket, writer).code_ ==
        status_code::ok);
    check(window.vqec_vision_ai_core_encwn_commit(ticket.token_).code_ == status_code::ok);
    check(vqec_vision_ai_appl_enprp_cancel_input(window, ticket.token_, writer).code_ ==
        status_code::invalid_state);
    check(writer.vqec_vision_ai_core_pvsrf_size_bytes() == 12);
    auto encoder_reader = writer.vqec_vision_ai_core_pvsrf_seal();
    const std::uint8_t payload[]{0, 0, 1, 0x65};
    h264_access_unit_view result{frame, {4, 2}, true, {payload, sizeof(payload)}, {}, {}};
    check(window.vqec_vision_ai_core_encwn_complete_result(ticket.token_, result).code_ == status_code::ok);
    check(pool.vqec_vision_ai_core_pvpol_available() == 0 &&
        window.vqec_vision_ai_core_encwn_reserved_bytes() == 12);
    check(window.vqec_vision_ai_core_encwn_check_deadlines(102).code_ == status_code::timeout);
    check(pool.vqec_vision_ai_core_pvpol_available() == 0);
    encoder_reader.reset();  // Simulated final input-reader completion, NOT timeout cleanup.
    check(window.vqec_vision_ai_core_encwn_complete_input(ticket.token_).code_ == status_code::ok);
    check(pool.vqec_vision_ai_core_pvpol_available() == 1 &&
        window.vqec_vision_ai_core_encwn_outstanding() == 0);
    encoder_window bound_window;
    check(bound_window.vqec_vision_ai_core_encwn_configure(config).code_ == status_code::ok);
    encoder_preparation preparation(bound_window);
    frame = {0, 0, 1, 1, 10};
    check(preparation.vqec_vision_ai_appl_enprp_prepare(pool, frame, true, 0).code_ ==
        status_code::ok);
    check(preparation.vqec_vision_ai_appl_enprp_borrow_data() != nullptr);
    check(preparation.vqec_vision_ai_appl_enprp_cancel().code_ == status_code::ok);
    check(bound_window.vqec_vision_ai_core_encwn_outstanding() == 0);
    check(preparation.vqec_vision_ai_appl_enprp_cancel().code_ == status_code::invalid_state);
    ++frame.frame_id_;
    ++frame.source_pts_ns_;
    check(preparation.vqec_vision_ai_appl_enprp_prepare(pool, frame, true, 1).code_ ==
        status_code::ok);
    auto occupied = std::make_shared<const std::vector<std::uint8_t>>(1);
    submission_ticket handed_ticket{{99, 99}, 99, 99, 99, 99};
    check(preparation.vqec_vision_ai_appl_enprp_commit_input(handed_ticket, occupied).code_ ==
        status_code::invalid_state);
    check(handed_ticket.token_.job_id_ == 99 && occupied->size() == 1);
    occupied.reset();
    check(preparation.vqec_vision_ai_appl_enprp_commit_input(handed_ticket, occupied).code_ ==
        status_code::ok);
    check(occupied && occupied->size() == 12);
    check(preparation.vqec_vision_ai_appl_enprp_borrow_data() == nullptr);
    check(preparation.vqec_vision_ai_appl_enprp_cancel().code_ == status_code::invalid_state);
    check(bound_window.vqec_vision_ai_core_encwn_outstanding() == 1 &&
        pool.vqec_vision_ai_core_pvpol_available() == 0);
    occupied.reset(); // Simulated terminal input read, not a timeout.
    check(bound_window.vqec_vision_ai_core_encwn_complete_input(handed_ticket.token_).code_ ==
        status_code::ok);
    check(bound_window.vqec_vision_ai_core_encwn_complete_dropped_result(
        handed_ticket.token_).code_ == status_code::ok);
    check(bound_window.vqec_vision_ai_core_encwn_outstanding() == 0 &&
        pool.vqec_vision_ai_core_pvpol_available() == 1);
    // Stop/fault between prepare and commit must preserve the writer for safe cancellation.
    for (const bool expire : {false, true}) {
        encoder_window stopping_window;
        check(stopping_window.vqec_vision_ai_core_encwn_configure(config).code_ == status_code::ok);
        encoder_preparation stopping(stopping_window);
        check(stopping.vqec_vision_ai_appl_enprp_prepare(pool, frame, true, 0).code_ ==
            status_code::ok);
        auto* original_pixels = stopping.vqec_vision_ai_appl_enprp_borrow_data();
        if (expire) {
            check(stopping_window.vqec_vision_ai_core_encwn_check_deadlines(100).code_ ==
                status_code::timeout);
        } else {
            stopping_window.vqec_vision_ai_core_encwn_begin_drain();
        }
        std::shared_ptr<const std::vector<std::uint8_t>> rejected_owner;
        submission_ticket unchanged{{99, 99}, 99, 99, 99, 99};
        check(stopping.vqec_vision_ai_appl_enprp_commit_input(unchanged, rejected_owner).code_ ==
            status_code::invalid_state);
        check(!rejected_owner && unchanged.token_.job_id_ == 99);
        check(stopping.vqec_vision_ai_appl_enprp_borrow_data() == original_pixels);
        check(stopping_window.vqec_vision_ai_core_encwn_outstanding() == 1 &&
            pool.vqec_vision_ai_core_pvpol_available() == 0);
        // Never submitted: explicit cancellation is valid even after deadline/drain.
        check(stopping.vqec_vision_ai_appl_enprp_cancel().code_ == status_code::ok);
        check(stopping_window.vqec_vision_ai_core_encwn_outstanding() == 0 &&
            stopping_window.vqec_vision_ai_core_encwn_reserved_bytes() == 0 &&
            pool.vqec_vision_ai_core_pvpol_available() == 1);
    }
    encoder_window backend_window;
    check(backend_window.vqec_vision_ai_core_encwn_configure(config).code_ == status_code::ok);
    encoder_preparation backend_preparation(backend_window);
    check(backend_preparation.vqec_vision_ai_appl_enprp_prepare_backend(
        pool, frame, true, 0, 0).code_ == status_code::invalid_argument);
    check(backend_window.vqec_vision_ai_core_encwn_outstanding() == 0);
    check(backend_preparation.vqec_vision_ai_appl_enprp_prepare_backend(
        pool, frame, true, 0, 17).code_ == status_code::ok);
    // An occupied preparation cannot be silently rebound to a newer sink.
    check(backend_preparation.vqec_vision_ai_appl_enprp_prepare_backend(
        pool, frame, true, 0, 18).code_ == status_code::invalid_state);
    encoder_input backend_input;
    check(backend_preparation.vqec_vision_ai_appl_enprp_commit_backend(backend_input).code_ ==
        status_code::ok);
    check(backend_input.dispatch_generation_ == 17 && backend_input.pixels_);
    check(vqec_vision_ai_core_encct_validate_input(backend_input, frame, config.geometry_,
        backend_input.ticket_, 17).code_ == status_code::ok);
    check(backend_preparation.vqec_vision_ai_appl_enprp_commit_backend(backend_input).code_ ==
        status_code::invalid_state);
    preparation_test_backend backend;
    check(vqec_vision_ai_appl_enprp_submit_backend(backend_window, backend, backend_input, 17).code_ ==
        status_code::ok);
    check(vqec_vision_ai_appl_enprp_submit_backend(backend_window, backend, backend_input, 17).code_ ==
        status_code::invalid_state);
    check(backend.calls_ == 1 && backend_window.vqec_vision_ai_core_encwn_outstanding() == 1);
    backend_input.pixels_.reset();
    check(pool.vqec_vision_ai_core_pvpol_available() == 0);
    backend.retained_.reset(); // Simulated actual input completion.
    check(backend_window.vqec_vision_ai_core_encwn_complete_input(
        backend_input.ticket_.token_).code_ == status_code::ok);
    check(backend_window.vqec_vision_ai_core_encwn_complete_dropped_result(
        backend_input.ticket_.token_).code_ == status_code::ok);
    encoder_window rejected_window;
    check(rejected_window.vqec_vision_ai_core_encwn_configure(config).code_ == status_code::ok);
    encoder_preparation rejected_preparation(rejected_window);
    check(rejected_preparation.vqec_vision_ai_appl_enprp_prepare_backend(
        pool, frame, true, 0, 18).code_ == status_code::ok);
    encoder_input rejected_input;
    check(rejected_preparation.vqec_vision_ai_appl_enprp_commit_backend(rejected_input).code_ ==
        status_code::ok);
    backend.reject_ = true;
    check(vqec_vision_ai_appl_enprp_submit_backend(rejected_window, backend, rejected_input, 18).code_ ==
        status_code::resource_exhausted);
    check(rejected_window.vqec_vision_ai_core_encwn_outstanding() == 0 && !backend.retained_);
    check(pool.vqec_vision_ai_core_pvpol_available() == 0); // Caller still owns rejected pixels.
    rejected_input.pixels_.reset();
    check(pool.vqec_vision_ai_core_pvpol_available() == 1);
    encoder_window ambiguous_window;
    check(ambiguous_window.vqec_vision_ai_core_encwn_configure(config).code_ == status_code::ok);
    encoder_preparation ambiguous_preparation(ambiguous_window);
    check(ambiguous_preparation.vqec_vision_ai_appl_enprp_prepare_backend(
        pool, frame, true, 0, 19).code_ == status_code::ok);
    encoder_input ambiguous_input;
    check(ambiguous_preparation.vqec_vision_ai_appl_enprp_commit_backend(ambiguous_input).code_ ==
        status_code::ok);
    backend.reject_ = false;
    backend.throw_after_retain_ = true;
    bool caught = false;
    try {
        const auto unexpected = vqec_vision_ai_appl_enprp_submit_backend(
            ambiguous_window, backend, ambiguous_input, 19);
        (void)unexpected;
    } catch (const std::runtime_error&) {
        caught = true;
    }
    check(caught && ambiguous_window.vqec_vision_ai_core_encwn_outstanding() == 1 &&
        ambiguous_window.vqec_vision_ai_core_encwn_reserved_bytes() == 12);
    const auto calls_before_retry = backend.calls_;
    check(vqec_vision_ai_appl_enprp_submit_backend(
        ambiguous_window, backend, ambiguous_input, 19).code_ == status_code::invalid_state);
    check(backend.calls_ == calls_before_retry);
    ambiguous_input.pixels_.reset();
    check(pool.vqec_vision_ai_core_pvpol_available() == 0);
    check(backend.vqec_vision_ai_cntr_encbk_begin_drain().code_ == status_code::pending);
    backend.retained_.reset(); // Test-only explicit simulated quiescence; not exception cleanup.
    check(ambiguous_window.vqec_vision_ai_core_encwn_complete_input(
        ambiguous_input.ticket_.token_).code_ == status_code::ok);
    check(ambiguous_window.vqec_vision_ai_core_encwn_complete_dropped_result(
        ambiguous_input.ticket_.token_).code_ == status_code::ok);
    check(pool.vqec_vision_ai_core_pvpol_available() == 1 &&
        ambiguous_window.vqec_vision_ai_core_encwn_outstanding() == 0);
    encoder_window drain_window;
    check(drain_window.vqec_vision_ai_core_encwn_configure(config).code_ == status_code::ok);
    encoder_preparation drain_preparation(drain_window);
    check(drain_preparation.vqec_vision_ai_appl_enprp_prepare_backend(
        pool, frame, true, 0, 20).code_ == status_code::ok);
    // Backend has no work, but the caller still has an unsubmitted reservation.
    check(vqec_vision_ai_appl_enprp_drain_backend(drain_window, backend).code_ ==
        status_code::pending);
    check(pool.vqec_vision_ai_core_pvpol_available() == 0);
    check(drain_preparation.vqec_vision_ai_appl_enprp_cancel().code_ == status_code::ok);
    check(vqec_vision_ai_appl_enprp_drain_backend(drain_window, backend).code_ == status_code::ok);
    check(vqec_vision_ai_appl_enprp_drain_backend(drain_window, backend).code_ == status_code::ok);
    check(drain_preparation.vqec_vision_ai_appl_enprp_prepare_backend(
        pool, frame, true, 1, 20).code_ == status_code::invalid_state);
    std::cout << "encoder preparation failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
