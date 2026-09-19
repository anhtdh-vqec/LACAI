#include <cstdint>
#include <iostream>
#include <memory>
#include <utility>

#include "vqec_vision_multi_model_session.hpp"

namespace {

class fake_session_source final : public vqec::vision::ai::raw_source_port {
public:
    [[nodiscard]] vqec::vision::ai::status vqec_vision_ai_ports_rawsr_start(
        int _timeout_ms) override {
        (void)_timeout_ms;
        ++start_calls_;
        state_ = vqec::vision::ai::raw_source_state::running;
        return {};
    }

    [[nodiscard]] vqec::vision::ai::status vqec_vision_ai_ports_rawsr_receive(
        vqec::vision::ai::raw_frame& _frame, int _timeout_ms) override {
        (void)_timeout_ms;
        if (!has_frame_) {
            return {vqec::vision::ai::status_code::timeout, "fixture frame unavailable"};
        }
        _frame = std::move(frame_);
        has_frame_ = false;
        return {};
    }

    [[nodiscard]] vqec::vision::ai::status vqec_vision_ai_ports_rawsr_stop(
        int _timeout_ms) override {
        (void)_timeout_ms;
        ++stop_calls_;
        state_ = vqec::vision::ai::raw_source_state::stopped;
        return {};
    }

    [[nodiscard]] vqec::vision::ai::raw_source_state
    vqec_vision_ai_ports_rawsr_get_state() const noexcept override {
        return state_;
    }

    [[nodiscard]] vqec::vision::ai::raw_source_profile
    vqec_vision_ai_ports_rawsr_get_profile() const noexcept override {
        return {1920, 1080, 25, 1};
    }

    [[nodiscard]] unsigned
    vqec_vision_ai_ports_rawsr_get_outstanding() const noexcept override {
        return has_frame_ ? 1U : 0U;
    }

    void vqec_vision_ai_unit_mmsts_supply_frame(std::uint64_t _buffer_id) {
        frame_ = {};
        frame_.descriptor_.buffer_id_ = _buffer_id;
        frame_.descriptor_.session_epoch_ = 3;
        frame_.descriptor_.pts_ns_ = _buffer_id * 1000;
        frame_.descriptor_.allocation_size_bytes_ = 16;
        frame_.native_handle_ = 17;
        frame_.owner_ = std::make_shared<int>(1);
        has_frame_ = true;
    }

    vqec::vision::ai::raw_source_state state_{
        vqec::vision::ai::raw_source_state::idle};
    vqec::vision::ai::raw_frame frame_;
    unsigned start_calls_{0};
    unsigned stop_calls_{0};
    bool has_frame_{false};
};

class fake_session_graph final : public vqec::vision::ai::inference_graph_port {
public:
    [[nodiscard]] vqec::vision::ai::status
    vqec_vision_ai_ports_infgr_validate_activation() const override {
        return can_activate_ ? vqec::vision::ai::status{} :
            vqec::vision::ai::status{vqec::vision::ai::status_code::resource_exhausted,
                                     "fixture activation rejected"};
    }

    [[nodiscard]] vqec::vision::ai::status vqec_vision_ai_ports_infgr_configure(
        const vqec::vision::ai::inference_plan& _plan) override {
        (void)_plan;
        ++configure_calls_;
        state_ = vqec::vision::ai::inference_graph_state::configured;
        return {};
    }

    [[nodiscard]] vqec::vision::ai::status
    vqec_vision_ai_ports_infgr_load() override {
        ++load_calls_;
        state_ = vqec::vision::ai::inference_graph_state::ready;
        return {};
    }

    [[nodiscard]] vqec::vision::ai::status
    vqec_vision_ai_ports_infgr_poll_state() override {
        return {};
    }

    [[nodiscard]] vqec::vision::ai::status vqec_vision_ai_ports_infgr_bind_source(
        const vqec::vision::ai::source_binding& _binding) override {
        (void)_binding;
        ++bind_calls_;
        return {};
    }

    [[nodiscard]] vqec::vision::ai::status vqec_vision_ai_ports_infgr_start(
        const std::vector<vqec::vision::ai::tensor_spec>& _outputs,
        std::uint64_t _max_output_bytes) override {
        (void)_outputs;
        (void)_max_output_bytes;
        ++start_calls_;
        if (!can_start_) {
            return {vqec::vision::ai::status_code::incompatible_plugin,
                    "fixture graph start rejected"};
        }
        state_ = vqec::vision::ai::inference_graph_state::running;
        return {};
    }

    [[nodiscard]] vqec::vision::ai::status vqec_vision_ai_ports_infgr_arm(
        std::uint64_t _cycle_id, std::uint64_t _source_epoch,
        std::uint64_t _job_timeout_ns, vqec::vision::ai::submission_sequence_policy) override {
        (void)_source_epoch;
        (void)_job_timeout_ns;
        cycle_id_ = _cycle_id;
        return {};
    }

    [[nodiscard]] vqec::vision::ai::status vqec_vision_ai_ports_infgr_submit_frame(
        const vqec::vision::ai::raw_frame& _frame, std::uint64_t _steady_now_ns,
        vqec::vision::ai::submission_ticket& _ticket) override {
        (void)_steady_now_ns;
        owner_ = _frame.owner_;
        ticket_.token_.cycle_id_ = cycle_id_;
        ticket_.token_.job_id_ = _frame.descriptor_.buffer_id_;
        ticket_.source_epoch_ = _frame.descriptor_.session_epoch_;
        ticket_.source_frame_id_ = _frame.descriptor_.buffer_id_;
        ticket_.source_pts_ns_ = _frame.descriptor_.pts_ns_;
        ticket_.pipeline_pts_ns_ = _frame.descriptor_.pts_ns_;
        _ticket = ticket_;
        is_outstanding_ = true;
        return {};
    }

    [[nodiscard]] vqec::vision::ai::status vqec_vision_ai_ports_infgr_poll_result(
        std::uint64_t _steady_now_ns,
        vqec::vision::ai::tensor_result& _result) override {
        (void)_steady_now_ns;
        if (!is_outstanding_ || !is_result_ready_) {
            return {vqec::vision::ai::status_code::pending, "fixture result pending"};
        }
        _result.pipeline_pts_ns_ = ticket_.pipeline_pts_ns_;
        is_outstanding_ = false;
        is_result_ready_ = false;
        owner_.reset();
        return {};
    }

    [[nodiscard]] vqec::vision::ai::status
    vqec_vision_ai_ports_infgr_request_drain() override {
        ++drain_calls_;
        state_ = vqec::vision::ai::inference_graph_state::drained;
        return {};
    }

    [[nodiscard]] vqec::vision::ai::status
    vqec_vision_ai_ports_infgr_unload() override {
        ++unload_calls_;
        state_ = vqec::vision::ai::inference_graph_state::configured;
        return {};
    }

    [[nodiscard]] vqec::vision::ai::inference_graph_state
    vqec_vision_ai_ports_infgr_get_state() const noexcept override {
        return state_;
    }

    [[nodiscard]] unsigned
    vqec_vision_ai_ports_infgr_get_outstanding() const noexcept override {
        return is_outstanding_ ? 1U : 0U;
    }

    [[nodiscard]] vqec::vision::ai::submission_ticket
    vqec_vision_ai_ports_infgr_get_pending_ticket() const noexcept override {
        return ticket_;
    }

    void vqec_vision_ai_unit_mmsts_complete_result() noexcept {
        is_result_ready_ = true;
    }

    vqec::vision::ai::inference_graph_state state_{
        vqec::vision::ai::inference_graph_state::empty};
    std::shared_ptr<const void> owner_;
    vqec::vision::ai::submission_ticket ticket_;
    std::uint64_t cycle_id_{0};
    unsigned configure_calls_{0};
    unsigned load_calls_{0};
    unsigned bind_calls_{0};
    unsigned start_calls_{0};
    unsigned drain_calls_{0};
    unsigned unload_calls_{0};
    bool can_activate_{true};
    bool can_start_{true};
    bool is_outstanding_{false};
    bool is_result_ready_{false};
};

vqec::vision::ai::multi_model_graph_config
vqec_vision_ai_unit_mmsts_make_graph_config(
    fake_session_graph& _graph, std::uint64_t _cycle_id) {
    vqec::vision::ai::multi_model_graph_config config;
    config.graph_ = &_graph;
    config.plan_.source_width_ = 1920;
    config.plan_.source_height_ = 1080;
    config.plan_.fps_numerator_ = 25;
    config.plan_.fps_denominator_ = 1;
    config.plan_.tensor_width_ = 640;
    config.plan_.tensor_height_ = 640;
    config.plan_.placement_ = vqec::vision::ai::image_placement::centre;
    config.plan_.model_path_ = "/fixture/model.bin";
    config.plan_.backend_path_ = "/fixture/backend.so";
    config.plan_.system_path_ = "/fixture/system.so";
    config.plan_.input_queue_bytes_ = 8U * 1024U * 1024U;
    config.plan_.output_queue_buffers_ = 2;
    config.binding_.width_ = 1920;
    config.binding_.height_ = 1080;
    config.binding_.fps_numerator_ = 25;
    config.binding_.fps_denominator_ = 1;
    config.binding_.memory_kind_ = vqec::vision::ai::source_memory_kind::dmabuf;
    config.binding_.layout_ = vqec::vision::ai::source_memory_layout::linear_nv12;
    config.binding_.sync_mode_ = vqec::vision::ai::source_sync_mode::implicit_ready;
    config.binding_.color_profile_ =
        vqec::vision::ai::source_color_profile::bt709_limited;
    config.binding_.chroma_site_ = vqec::vision::ai::source_chroma_site::mpeg2;
    config.binding_.fw_memory_contract_ = "fixture:fw";
    config.binding_.backend_memory_contract_ = "fixture:backend";
    config.binding_.preprocess_contract_ = "fixture:preprocess";
    config.outputs_ = {
        {"scores", {1}, vqec::vision::ai::tensor_element_type::float32, {}}};
    config.max_output_bytes_ = 4;
    config.cycle_id_ = _cycle_id;
    config.job_timeout_ns_ = 1000000;
    return config;
}

vqec::vision::ai::multi_model_session_config
vqec_vision_ai_unit_mmsts_make_session_config(
    fake_session_graph& _first, fake_session_graph& _second) {
    vqec::vision::ai::multi_model_session_config config;
    config.graph_count_ = 2;
    config.graphs_[0] = vqec_vision_ai_unit_mmsts_make_graph_config(_first, 11);
    config.graphs_[1] = vqec_vision_ai_unit_mmsts_make_graph_config(_second, 12);
    config.cadence_.source_fps_numerator_ = 25;
    config.cadence_.source_fps_denominator_ = 1;
    config.cadence_.model_count_ = 2;
    config.cadence_.model_fps_numerators_[0] = 25;
    config.cadence_.model_fps_denominators_[0] = 1;
    config.cadence_.model_fps_numerators_[1] = 25;
    config.cadence_.model_fps_denominators_[1] = 1;
    config.startup_timeout_ns_ = 1000;
    config.stop_timeout_ns_ = 1000;
    return config;
}

}  // namespace

int main() {
    using namespace vqec::vision::ai;
    unsigned failures = 0;
    unsigned check_index = 0;
    const auto check = [&failures, &check_index](bool _condition) {
        ++check_index;
        if (!_condition) {
            ++failures;
            std::cerr << "failed check " << check_index << "\n";
        }
    };

    fake_session_source rejected_source;
    fake_session_graph rejected_first;
    fake_session_graph rejected_second;
    rejected_second.can_activate_ = false;
    multi_model_session rejected(rejected_source,
        vqec_vision_ai_unit_mmsts_make_session_config(
            rejected_first, rejected_second));
    tensor_result result;
    source_session_progress progress;
    check(rejected.vqec_vision_ai_appl_mmses_step(0, result, progress).code_ ==
          status_code::resource_exhausted);
    check(rejected_source.start_calls_ == 0 && rejected_first.configure_calls_ == 0 &&
          rejected_second.configure_calls_ == 0);

    // A live control process may start before FW produces media. Source acquisition stays
    // pending indefinitely and no graph/DSP/HTP activation is attempted without a real frame.
    fake_session_source frame_gated_source;
    fake_session_graph frame_gated_first;
    fake_session_graph frame_gated_second;
    multi_model_session frame_gated(frame_gated_source,
        vqec_vision_ai_unit_mmsts_make_session_config(
            frame_gated_first, frame_gated_second));
    check(frame_gated.vqec_vision_ai_appl_mmses_step(0, result, progress).code_ ==
          status_code::pending);
    check(frame_gated.vqec_vision_ai_appl_mmses_step(1, result, progress).code_ ==
          status_code::pending);
    check(frame_gated.vqec_vision_ai_appl_mmses_step(5000, result, progress).code_ ==
          status_code::pending);
    check(frame_gated.vqec_vision_ai_appl_mmses_get_snapshot().session_state_ ==
          multi_model_session_state::awaiting_first_frame);
    check(frame_gated_first.configure_calls_ == 0 && frame_gated_first.load_calls_ == 0 &&
          frame_gated_second.configure_calls_ == 0 && frame_gated_second.load_calls_ == 0);
    frame_gated_source.vqec_vision_ai_unit_mmsts_supply_frame(99);
    check(frame_gated.vqec_vision_ai_appl_mmses_step(5001, result, progress).code_ ==
          status_code::pending);
    check(frame_gated.vqec_vision_ai_appl_mmses_get_snapshot().session_state_ ==
          multi_model_session_state::configuring);
    check(frame_gated.vqec_vision_ai_appl_mmses_request_stop(5002).code_ == status_code::ok);
    for (std::uint64_t now = 5003; now <= 5010; ++now) {
        (void)frame_gated.vqec_vision_ai_appl_mmses_step(now, result, progress);
    }
    check(frame_gated.vqec_vision_ai_appl_mmses_get_snapshot().session_state_ ==
          multi_model_session_state::stopped);

    fake_session_source partial_source;
    fake_session_graph partial_first;
    fake_session_graph partial_second;
    partial_second.can_start_ = false;
    multi_model_session partial(partial_source,
        vqec_vision_ai_unit_mmsts_make_session_config(
            partial_first, partial_second));
    check(partial.vqec_vision_ai_appl_mmses_step(0, result, progress).code_ ==
          status_code::pending);
    partial_source.vqec_vision_ai_unit_mmsts_supply_frame(100);
    for (std::uint64_t now = 1; now <= 9; ++now) {
        const auto status = partial.vqec_vision_ai_appl_mmses_step(
            now, result, progress);
        check(status.code_ == status_code::pending || status.code_ == status_code::ok);
    }
    check(partial.vqec_vision_ai_appl_mmses_step(10, result, progress).code_ ==
          status_code::incompatible_plugin);
    check(partial.vqec_vision_ai_appl_srcsn_get_health().phase_ ==
          source_session_phase::draining);
    for (std::uint64_t now = 11; now <= 17; ++now) {
        const auto status = partial.vqec_vision_ai_appl_mmses_step(
            now, result, progress);
        check(status.code_ == status_code::pending || status.code_ == status_code::ok);
    }
    check(partial_source.stop_calls_ == 1 && partial_first.drain_calls_ == 1 &&
          partial_first.unload_calls_ == 1 && partial_second.drain_calls_ == 0 &&
          partial_second.unload_calls_ == 1);
    check(partial.vqec_vision_ai_appl_mmses_get_last_error().code_ ==
          status_code::incompatible_plugin);

    fake_session_source source;
    fake_session_graph first;
    fake_session_graph second;
    multi_model_session session(source,
        vqec_vision_ai_unit_mmsts_make_session_config(first, second));
    check(session.vqec_vision_ai_appl_mmses_step(0, result, progress).code_ ==
          status_code::pending);
    check(source.start_calls_ == 0);
    source.vqec_vision_ai_unit_mmsts_supply_frame(100);
    for (std::uint64_t now = 1; now <= 10; ++now) {
        const auto status = session.vqec_vision_ai_appl_mmses_step(now, result, progress);
        check(status.code_ == status_code::pending || status.code_ == status_code::ok);
    }
    auto running = session.vqec_vision_ai_appl_mmses_get_snapshot();
    check(running.session_state_ == multi_model_session_state::running);
    check(running.graph_count_ == 2 && running.running_graph_count_ == 2);
    check(source.start_calls_ == 1 && first.configure_calls_ == 1 &&
          second.configure_calls_ == 1);

    source.vqec_vision_ai_unit_mmsts_supply_frame(1);
    check(session.vqec_vision_ai_appl_mmses_step(11, result, progress).code_ ==
          status_code::ok);
    check(progress.has_submission_ && progress.submitted_model_mask_ == 3 &&
          progress.model_slot_ == 0);
    first.vqec_vision_ai_unit_mmsts_complete_result();
    check(session.vqec_vision_ai_appl_mmses_step(12, result, progress).code_ ==
          status_code::ok);
    check(progress.has_result_ && progress.model_slot_ == 0);
    second.vqec_vision_ai_unit_mmsts_complete_result();
    check(session.vqec_vision_ai_appl_mmses_step(13, result, progress).code_ ==
          status_code::ok);
    check(progress.has_result_ && progress.model_slot_ == 1);

    check(session.vqec_vision_ai_appl_mmses_request_stop(14).code_ == status_code::ok);
    check(!session.vqec_vision_ai_appl_mmses_get_result_frame().owner_);
    for (std::uint64_t now = 15; now <= 22; ++now) {
        const auto status = session.vqec_vision_ai_appl_mmses_step(now, result, progress);
        check(status.code_ == status_code::pending || status.code_ == status_code::ok);
    }
    const auto stopped = session.vqec_vision_ai_appl_mmses_get_snapshot();
    check(stopped.session_state_ == multi_model_session_state::stopped);
    check(source.stop_calls_ == 1 && first.drain_calls_ == 1 &&
          second.drain_calls_ == 1);
    check(first.unload_calls_ == 1 && second.unload_calls_ == 1);
    check(session.vqec_vision_ai_appl_srcsn_get_health().phase_ ==
          source_session_phase::stopped);
    // Default drain_and_discard retains nothing.
    check(session.vqec_vision_ai_appl_mmses_take_drain_result(result).code_ ==
          status_code::pending);

    // Section 13: drain policy is explicit. drain_and_deliver retains the last result that
    // becomes ready while draining instead of leaving the stop semantics implicit.
    {
        fake_session_source drain_source;
        fake_session_graph drain_graph;
        multi_model_session_config drain_config;
        drain_config.graph_count_ = 1;
        drain_config.graphs_[0] =
            vqec_vision_ai_unit_mmsts_make_graph_config(drain_graph, 31);
        drain_config.cadence_.source_fps_numerator_ = 25;
        drain_config.cadence_.source_fps_denominator_ = 1;
        drain_config.cadence_.model_count_ = 1;
        drain_config.cadence_.model_fps_numerators_[0] = 25;
        drain_config.cadence_.model_fps_denominators_[0] = 1;
        drain_config.startup_timeout_ns_ = 1000;
        drain_config.stop_timeout_ns_ = 1000;
        drain_config.drain_policy_ = multi_model_drain_policy::drain_and_deliver;
        multi_model_session drain_session(drain_source, drain_config);
        check(drain_session.vqec_vision_ai_appl_mmses_step(0, result, progress).code_ ==
              status_code::pending);
        drain_source.vqec_vision_ai_unit_mmsts_supply_frame(100);
        std::uint64_t now = 1;
        while (drain_session.vqec_vision_ai_appl_mmses_get_snapshot().session_state_ !=
                   multi_model_session_state::running && now <= 20) {
            const auto status =
                drain_session.vqec_vision_ai_appl_mmses_step(now, result, progress);
            check(status.code_ == status_code::pending || status.code_ == status_code::ok);
            ++now;
        }
        drain_source.vqec_vision_ai_unit_mmsts_supply_frame(1);
        const std::weak_ptr<const void> draining_owner = drain_source.frame_.owner_;
        check(drain_session.vqec_vision_ai_appl_mmses_step(now++, result, progress).code_ ==
              status_code::ok);
        drain_graph.vqec_vision_ai_unit_mmsts_complete_result();
        check(drain_session.vqec_vision_ai_appl_mmses_request_stop(now++).code_ ==
              status_code::ok);
        check(!draining_owner.expired());
        while (drain_session.vqec_vision_ai_appl_mmses_get_snapshot().session_state_ !=
                   multi_model_session_state::stopped && now <= 40) {
            (void)drain_session.vqec_vision_ai_appl_mmses_step(now++, result, progress);
        }
        check(drain_session.vqec_vision_ai_appl_mmses_get_snapshot().session_state_ ==
              multi_model_session_state::stopped);
        check(draining_owner.expired());
        tensor_result drained;
        check(drain_session.vqec_vision_ai_appl_mmses_take_drain_result(drained).code_ ==
              status_code::ok);
        check(drain_session.vqec_vision_ai_appl_mmses_take_drain_result(drained).code_ ==
              status_code::pending);
    }

    // Section 14: cascade retention. A cascade-root session owns a frame store, retains the
    // source frame on submit, and must not release the FW source until the dependent
    // completes the retention ticket.
    {
        fake_session_source cascade_source;
        fake_session_graph cascade_graph;
        multi_model_session_config cascade_config;
        cascade_config.graph_count_ = 1;
        cascade_config.graphs_[0] =
            vqec_vision_ai_unit_mmsts_make_graph_config(cascade_graph, 41);
        cascade_config.graphs_[0].cascade_root_ = true;
        cascade_config.cadence_.source_fps_numerator_ = 25;
        cascade_config.cadence_.source_fps_denominator_ = 1;
        cascade_config.cadence_.model_count_ = 1;
        cascade_config.cadence_.model_fps_numerators_[0] = 25;
        cascade_config.cadence_.model_fps_denominators_[0] = 1;
        cascade_config.startup_timeout_ns_ = 1000;
        cascade_config.stop_timeout_ns_ = 100000;
        cascade_config.camera_id_ = 2;
        cascade_config.cascade_frames_ = 1;
        cascade_config.cascade_tasks_per_frame_ = 2;
        cascade_config.cascade_max_bytes_ = 1024;
        multi_model_session cascade_session(cascade_source, cascade_config);

        // A cascade-root session without a retention budget is rejected before acquisition.
        {
            multi_model_session_config no_budget = cascade_config;
            no_budget.cascade_max_bytes_ = 0;
            fake_session_source bad_source;
            fake_session_graph bad_graph;
            multi_model_session bad_session(bad_source, no_budget);
            check(bad_session.vqec_vision_ai_appl_mmses_step(0, result, progress).code_ ==
                  status_code::invalid_argument);
            check(bad_source.start_calls_ == 0);
        }

        check(cascade_session.vqec_vision_ai_appl_mmses_step(0, result, progress).code_ ==
              status_code::pending);
        cascade_source.vqec_vision_ai_unit_mmsts_supply_frame(100);
        std::uint64_t now = 1;
        while (cascade_session.vqec_vision_ai_appl_mmses_get_snapshot().session_state_ !=
                   multi_model_session_state::running && now <= 20) {
            (void)cascade_session.vqec_vision_ai_appl_mmses_step(now++, result, progress);
        }
        check(cascade_session.vqec_vision_ai_appl_mmses_get_snapshot().session_state_ ==
              multi_model_session_state::running);
        check(cascade_session.vqec_vision_ai_appl_mmses_has_cascade_store());

        cascade_source.vqec_vision_ai_unit_mmsts_supply_frame(1);
        check(cascade_session.vqec_vision_ai_appl_mmses_step(now++, result, progress).code_ ==
              status_code::ok);
        check(cascade_session.vqec_vision_ai_appl_mmses_get_cascade_bytes() == 16);
        cascade_graph.vqec_vision_ai_unit_mmsts_complete_result();

        const preview_frame_key key{2, 0, 3, 1, 1000};
        raw_frame acquired;
        std::uint64_t ticket = 0;
        cascade_frame_lease_port& lease = cascade_session;
        check(lease.vqec_vision_ai_ports_cflse_acquire(key, acquired, ticket).code_ ==
              status_code::ok);
        check(acquired.owner_ != nullptr);

        // Stop drains the primary result and closes admission, but the acquired secondary
        // ticket keeps the FW source owner alive until its real completion.
        check(cascade_session.vqec_vision_ai_appl_mmses_request_stop(now++).code_ ==
              status_code::ok);
        for (int index = 0; index < 12; ++index) {
            (void)cascade_session.vqec_vision_ai_appl_mmses_step(now++, result, progress);
        }
        check(cascade_session.vqec_vision_ai_appl_mmses_get_snapshot().session_state_ !=
              multi_model_session_state::stopped);
        check(cascade_source.stop_calls_ == 0 &&
              cascade_session.vqec_vision_ai_appl_mmses_get_cascade_bytes() == 16);

        // The drained primary already retired admission. Completion of the acquired
        // dependent ticket releases the final owner and allows source shutdown.
        check(lease.vqec_vision_ai_ports_cflse_complete(ticket).code_ == status_code::ok);
        check(cascade_session.vqec_vision_ai_appl_mmses_get_cascade_bytes() == 0);
        while (cascade_session.vqec_vision_ai_appl_mmses_get_snapshot().session_state_ !=
                   multi_model_session_state::stopped && now <= 200) {
            (void)cascade_session.vqec_vision_ai_appl_mmses_step(now++, result, progress);
        }
        check(cascade_session.vqec_vision_ai_appl_mmses_get_snapshot().session_state_ ==
              multi_model_session_state::stopped);
        check(cascade_source.stop_calls_ == 1);
    }

    std::cout << "multi-model session failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
