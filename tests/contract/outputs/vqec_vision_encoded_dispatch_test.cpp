#include <iostream>
#include <stdexcept>

#include "vqec_vision_encoded_dispatch.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_output_generation.hpp"

namespace vqec::vision::ai {

class test_encoded_sink final : public encoded_sink {
public:
    encoded_sink_demand demand_{3, 1};
    unsigned queries_{0};
    unsigned writes_{0};
    bool throw_after_write_{false};
    status write_status_;
    output_gate* revoke_on_query_{nullptr};

    status vqec_vision_ai_cntr_encsk_query_demand(encoded_sink_demand& _demand) override {
        ++queries_;
        _demand = demand_;
        if (revoke_on_query_ != nullptr) {
            revoke_on_query_->vqec_vision_ai_core_otgat_invalidate();
        }
        return {};
    }

    status vqec_vision_ai_cntr_encsk_write(
        const h264_access_unit_view& _unit, std::uint64_t _expected_generation) override {
        if (_expected_generation != demand_.mapping_generation_ || _unit.payload_.size_ != 4) {
            return {status_code::invalid_state, "fake sink mismatch"};
        }
        ++writes_;
        if (throw_after_write_) {
            throw std::runtime_error("synthetic ambiguous ring write");
        }
        return write_status_;
    }
};

class test_event_backend final : public encoder_backend {
public:
    encoder_event next_;
    bool has_event_{false};
    unsigned polls_{0};
    status vqec_vision_ai_cntr_encbk_submit(const encoder_input& _input) override {
        (void)_input;
        return {status_code::unsupported, "poll-only fixture"};
    }
    status vqec_vision_ai_cntr_encbk_poll(encoder_event& _event) override {
        ++polls_;
        if (!has_event_) {
            return {status_code::pending, "no fixture event"};
        }
        _event = next_;
        has_event_ = false;
        return {};
    }
    status vqec_vision_ai_cntr_encbk_begin_drain() override {
        return {status_code::pending, "fixture drain not implemented"};
    }
};

}  // namespace vqec::vision::ai

int main() {
    using namespace vqec::vision::ai;
    unsigned failures = 0;
    const auto check = [&](bool _condition) {
        if (!_condition) {
            ++failures;
        }
    };
    const preview_frame_key frame{0, 0, 1, 42, 10};
    const preview_geometry geometry{4, 2};
    const std::uint8_t bytes[]{0, 0, 1, 0x65};
    const h264_access_unit_view view{frame, geometry, true, {bytes, sizeof(bytes)}, {}, {}};
    std::shared_ptr<const owned_h264_output> output;
    check(owned_h264_output::vqec_vision_ai_core_encot_copy_output(
        view, frame, geometry, output).code_ == status_code::ok);
    if (!output) {
        return 1;
    }
    output_gate gate;
    const output_policy policy{1, 0, 1000, {{"cam0", "person_tracking", {"human.clothing"}}}};
    check(gate.vqec_vision_ai_core_otgat_apply_policy(policy, 0).code_ == status_code::ok);
    encoded_dispatch_context context{frame, geometry, "cam0", 3, 10, 100,
        {{1, "cam0", "person_tracking", {"human.clothing"}}}};
    test_encoded_sink sink;
    const auto dispatch = [&]() {
        return vqec_vision_ai_outpt_encdp_dispatch(*output, context, 20, gate, sink);
    };
    check(dispatch().code_ == status_code::ok && sink.writes_ == 1);
    sink.demand_.active_consumers_ = 0;
    check(dispatch().code_ == status_code::pending && sink.writes_ == 1);
    sink.demand_.active_consumers_ = 1;
    sink.demand_.mapping_generation_ = 4;
    check(dispatch().code_ == status_code::invalid_state && sink.writes_ == 1);
    sink.demand_.mapping_generation_ = 3;
    context.created_monotonic_ns_ = 21;
    check(dispatch().code_ == status_code::invalid_argument);
    context.created_monotonic_ns_ = 0;
    context.max_age_ns_ = 19;
    check(dispatch().code_ == status_code::timeout && sink.writes_ == 1);
    context.max_age_ns_ = 100;
    const auto queries = sink.queries_;
    context.rendered_scopes_[0].attributes_.push_back("human.face_embedding");
    check(dispatch().code_ == status_code::unauthorized && sink.queries_ == queries);
    context.rendered_scopes_[0].attributes_.pop_back();
    context.rendered_scopes_[0].source_id_ = "cam1";
    check(dispatch().code_ == status_code::unauthorized && sink.writes_ == 1);
    context.rendered_scopes_[0].source_id_ = "cam0";
    context.rendered_scopes_.push_back({1, "cam0", "blacklist", {}});
    check(dispatch().code_ == status_code::unauthorized && sink.writes_ == 1);
    context.rendered_scopes_.pop_back();
    sink.write_status_ = {status_code::io_error, "fake write failure"};
    check(dispatch().code_ == status_code::io_error && sink.writes_ == 2); // No implicit retry.
    sink.write_status_ = {};
    sink.revoke_on_query_ = &gate;  // Fault injection; real calls must be non-reentrant.
    check(dispatch().code_ == status_code::unauthorized && sink.writes_ == 2);
    sink.revoke_on_query_ = nullptr;
    check(dispatch().code_ == status_code::unauthorized && sink.writes_ == 2);
    // Replacement instances must share one runtime issuer, not SDK-local counters.
    output_generation issuer;
    std::uint64_t old_generation = 0;
    std::uint64_t new_generation = 0;
    check(issuer.vqec_vision_ai_core_otgen_issue(old_generation).code_ == status_code::ok);
    check(issuer.vqec_vision_ai_core_otgen_issue(new_generation).code_ == status_code::ok);
    check(old_generation != new_generation);
    output_gate replacement_gate;
    check(replacement_gate.vqec_vision_ai_core_otgat_apply_policy(policy, 0).code_ ==
          status_code::ok);
    test_encoded_sink replacement_sink;
    replacement_sink.demand_ = {new_generation, 1};
    auto old_context = context;
    old_context.mapping_generation_ = old_generation;
    check(vqec_vision_ai_outpt_encdp_dispatch(
        *output, old_context, 20, replacement_gate, replacement_sink).code_ ==
        status_code::invalid_state);
    check(replacement_sink.queries_ == 1 && replacement_sink.writes_ == 0);
    // Separate fixture represents a newly produced AU, not retagging the old queued AU.
    auto next_frame = frame;
    ++next_frame.frame_id_;
    ++next_frame.source_pts_ns_;
    auto next_view = view;
    next_view.frame_ = next_frame;
    std::shared_ptr<const owned_h264_output> next_output;
    check(owned_h264_output::vqec_vision_ai_core_encot_copy_output(
        next_view, next_frame, geometry, next_output).code_ == status_code::ok);
    if (!next_output) {
        return 1;
    }
    auto next_context = old_context;
    next_context.expected_frame_ = next_frame;
    next_context.mapping_generation_ = new_generation;
    check(vqec_vision_ai_outpt_encdp_dispatch(
        *next_output, next_context, 20, replacement_gate, replacement_sink).code_ ==
        status_code::ok);
    check(replacement_sink.writes_ == 1);
    // A successful new write must not make an old binding valid again.
    check(vqec_vision_ai_outpt_encdp_dispatch(
        *output, old_context, 20, replacement_gate, replacement_sink).code_ ==
        status_code::invalid_state);
    check(replacement_sink.writes_ == 1);
    encoder_window event_window;
    encoder_window_config event_config;
    event_config.submission_ = {4, frame.source_epoch_, 0, 100, 1};
    event_config.geometry_ = geometry;
    event_config.max_input_bytes_ = 12;
    check(event_window.vqec_vision_ai_core_encwn_configure(event_config).code_ == status_code::ok);
    submission_ticket event_ticket;
    check(event_window.vqec_vision_ai_core_encwn_reserve(
        frame, true, 0, event_ticket).code_ == status_code::ok);
    check(event_window.vqec_vision_ai_core_encwn_commit(event_ticket.token_).code_ == status_code::ok);
    output_gate event_gate;
    check(event_gate.vqec_vision_ai_core_otgat_apply_policy(policy, 0).code_ == status_code::ok);
    test_encoded_sink event_sink;
    event_sink.demand_.active_consumers_ = 0;
    encoder_event result_event{encoder_event_kind::output_ready, event_ticket.token_, output, {}};
    status delivery{status_code::io_error, "unchanged sentinel"};
    check(vqec_vision_ai_outpt_encdp_handle_event(result_event, event_window, context, 20,
        event_gate, event_sink, delivery).code_ == status_code::ok);
    check(delivery.code_ == status_code::pending && event_sink.writes_ == 0);
    check(event_window.vqec_vision_ai_core_encwn_outstanding() == 1);
    const auto event_queries = event_sink.queries_;
    check(vqec_vision_ai_outpt_encdp_handle_event(result_event, event_window, context, 20,
        event_gate, event_sink, delivery).code_ == status_code::invalid_state);
    check(event_sink.queries_ == event_queries && delivery.code_ == status_code::pending);
    result_event.kind_ = encoder_event_kind::input_complete;
    result_event.output_.reset();
    check(vqec_vision_ai_outpt_encdp_handle_event(result_event, event_window, context, 20,
        event_gate, event_sink, delivery).code_ == status_code::ok);
    check(event_window.vqec_vision_ai_core_encwn_outstanding() == 0);
    check(event_sink.queries_ == event_queries && delivery.code_ == status_code::pending);
    // Exercise complete event handling for each terminal delivery outcome.
    for (const auto delivery_code : {status_code::ok, status_code::unauthorized,
                                    status_code::io_error}) {
        encoder_window delivery_window;
        check(delivery_window.vqec_vision_ai_core_encwn_configure(event_config).code_ ==
            status_code::ok);
        submission_ticket delivery_ticket;
        check(delivery_window.vqec_vision_ai_core_encwn_reserve(
            frame, true, 0, delivery_ticket).code_ == status_code::ok);
        check(delivery_window.vqec_vision_ai_core_encwn_commit(delivery_ticket.token_).code_ ==
            status_code::ok);
        output_gate delivery_gate;
        check(delivery_gate.vqec_vision_ai_core_otgat_apply_policy(policy, 0).code_ ==
            status_code::ok);
        test_encoded_sink delivery_sink;
        if (delivery_code == status_code::unauthorized) {
            delivery_gate.vqec_vision_ai_core_otgat_invalidate();
        } else if (delivery_code == status_code::io_error) {
            delivery_sink.write_status_ = {status_code::io_error, "synthetic write failure"};
        }
        encoder_event delivery_event{
            encoder_event_kind::output_ready, delivery_ticket.token_, output, {}};
        status delivery_outcome;
        check(vqec_vision_ai_outpt_encdp_handle_event(delivery_event, delivery_window, context,
            20, delivery_gate, delivery_sink, delivery_outcome).code_ == status_code::ok);
        check(delivery_outcome.code_ == delivery_code);
        const unsigned expected_writes = delivery_code == status_code::unauthorized ? 0U : 1U;
        check(delivery_sink.writes_ == expected_writes);
        check(delivery_window.vqec_vision_ai_core_encwn_outstanding() == 1 &&
            delivery_window.vqec_vision_ai_core_encwn_reserved_bytes() == 12);
        check(delivery_window.vqec_vision_ai_core_encwn_validate_event(delivery_event).code_ ==
            status_code::invalid_state);
        delivery_event.kind_ = encoder_event_kind::input_complete;
        delivery_event.output_.reset();
        check(vqec_vision_ai_outpt_encdp_handle_event(delivery_event, delivery_window, context,
            20, delivery_gate, delivery_sink, delivery_outcome).code_ == status_code::ok);
        check(delivery_window.vqec_vision_ai_core_encwn_outstanding() == 0 &&
            delivery_window.vqec_vision_ai_core_encwn_reserved_bytes() == 0);
        check(delivery_sink.writes_ == expected_writes);
    }
    encoder_window ambiguous_window;
    check(ambiguous_window.vqec_vision_ai_core_encwn_configure(event_config).code_ == status_code::ok);
    submission_ticket ambiguous_ticket;
    check(ambiguous_window.vqec_vision_ai_core_encwn_reserve(
        frame, true, 0, ambiguous_ticket).code_ == status_code::ok);
    check(ambiguous_window.vqec_vision_ai_core_encwn_commit(ambiguous_ticket.token_).code_ ==
        status_code::ok);
    test_encoded_sink ambiguous_sink;
    ambiguous_sink.throw_after_write_ = true;
    encoder_event ambiguous_event{
        encoder_event_kind::output_ready, ambiguous_ticket.token_, output, {}};
    status ambiguous_delivery{status_code::pending, "unchanged"};
    bool write_threw = false;
    try {
        const auto unexpected = vqec_vision_ai_outpt_encdp_handle_event(
            ambiguous_event, ambiguous_window, context, 20, event_gate,
            ambiguous_sink, ambiguous_delivery);
        (void)unexpected;
    } catch (const std::runtime_error&) {
        write_threw = true;
    }
    check(write_threw && ambiguous_sink.writes_ == 1);
    check(ambiguous_delivery.code_ == status_code::pending);
    check(ambiguous_window.vqec_vision_ai_core_encwn_outstanding() == 1 &&
        ambiguous_window.vqec_vision_ai_core_encwn_reserved_bytes() == 12);
    submission_ticket stopped_ticket;
    check(ambiguous_window.vqec_vision_ai_core_encwn_reserve(
        next_frame, true, 21, stopped_ticket).code_ == status_code::invalid_state);
    // Recovery explicitly discards the known result, without another sink call.
    check(ambiguous_window.vqec_vision_ai_core_encwn_apply_event(ambiguous_event).code_ ==
        status_code::ok);
    ambiguous_event.kind_ = encoder_event_kind::input_complete;
    ambiguous_event.output_.reset();
    check(ambiguous_window.vqec_vision_ai_core_encwn_apply_event(ambiguous_event).code_ ==
        status_code::ok);
    check(ambiguous_sink.writes_ == 1 && ambiguous_window.vqec_vision_ai_core_encwn_outstanding() == 0);
    encoder_window poll_window;
    check(poll_window.vqec_vision_ai_core_encwn_configure(event_config).code_ == status_code::ok);
    submission_ticket poll_ticket;
    check(poll_window.vqec_vision_ai_core_encwn_reserve(frame, true, 0, poll_ticket).code_ ==
        status_code::ok);
    check(poll_window.vqec_vision_ai_core_encwn_commit(poll_ticket.token_).code_ == status_code::ok);
    test_event_backend poll_backend;
    encoder_event polled;
    check(vqec_vision_ai_outpt_encdp_poll_event(poll_backend, poll_window, polled).code_ ==
        status_code::pending);
    poll_backend.next_ = {encoder_event_kind::input_complete, poll_ticket.token_, {}, {}};
    poll_backend.has_event_ = true;
    check(vqec_vision_ai_outpt_encdp_poll_event(poll_backend, poll_window, polled).code_ ==
        status_code::ok);
    check(poll_window.vqec_vision_ai_core_encwn_outstanding() == 1);
    const auto polls_before_overwrite = poll_backend.polls_;
    check(vqec_vision_ai_outpt_encdp_poll_event(poll_backend, poll_window, polled).code_ ==
        status_code::invalid_state);
    check(poll_backend.polls_ == polls_before_overwrite);
    check(poll_window.vqec_vision_ai_core_encwn_apply_event(polled).code_ == status_code::ok);
    polled = {};
    poll_backend.next_.kind_ = encoder_event_kind::output_dropped;
    poll_backend.has_event_ = true;
    check(vqec_vision_ai_outpt_encdp_poll_event(poll_backend, poll_window, polled).code_ ==
        status_code::ok);
    check(poll_window.vqec_vision_ai_core_encwn_apply_event(polled).code_ == status_code::ok);
    check(poll_window.vqec_vision_ai_core_encwn_outstanding() == 0);
    std::cout << "encoded dispatch failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
