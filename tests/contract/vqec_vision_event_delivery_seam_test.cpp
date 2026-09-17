#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>

#include "vqec_vision_event_delivery_seam.hpp"

namespace vqec::vision::ai {
namespace {

feature_event vqec_vision_ai_ctest_edsct_make_event(
    const std::string& _source_id, const std::string& _feature_id,
    std::uint64_t _occurred_at_ns) {
    const preview_frame_key frame{1, 0, 1, 100, _occurred_at_ns};
    feature_event event;
    event.frame_ = frame;
    event.source_id_ = _source_id;
    event.feature_id_ = _feature_id;
    event.event_id_ = "evt_1";
    event.event_schema_id_ = "security.occupancy";
    event.event_schema_version_ = "1";
    event.kind_ = feature_event_kind::snapshot;
    event.occurred_at_ns_ = _occurred_at_ns;
    event.config_revision_ = 1;
    feature_event_field field;
    field.schema_id_ = "occupancy";
    field.schema_version_ = "1";
    field.value_ = "42";
    field.confidence_ = 1.0F;
    field.quality_ = observation_quality::high;
    event.fields_.push_back(field);
    return event;
}

void vqec_vision_ai_ctest_edsct_test_acceptance_and_pending() {
    event_delivery_seam_config config;
    config.max_queued_events_ = 10;
    event_delivery_seam seam(config);

    assert(!seam.vqec_vision_ai_outpt_evdsm_is_stopping());
    assert(seam.vqec_vision_ai_outpt_evdsm_get_pending_count() == 0);

    const auto event1 = vqec_vision_ai_ctest_edsct_make_event("cam0", "counting", 1000);
    const auto res1 = seam.vqec_vision_ai_ports_fesnk_deliver_event(event1);
    assert(res1.code_ == status_code::ok);

    const auto event2 = vqec_vision_ai_ctest_edsct_make_event("cam0", "counting", 2000);
    const auto res2 = seam.vqec_vision_ai_ports_fesnk_deliver_event(event2);
    assert(res2.code_ == status_code::ok);

    const auto& metrics = seam.vqec_vision_ai_outpt_evdsm_get_metrics();
    assert(metrics.events_accepted_ == 2);
    assert(metrics.events_pending_ == 2);
    assert(metrics.events_drained_ == 0);
    assert(metrics.events_dropped_ == 0);
    assert(metrics.events_rejected_ == 0);
    assert(metrics.oldest_pending_ns_ == 1000);
    assert(seam.vqec_vision_ai_outpt_evdsm_get_pending_count() == 2);

    const auto* peek0 = seam.vqec_vision_ai_outpt_evdsm_peek(0);
    assert(peek0 != nullptr);
    assert(peek0->occurred_at_ns_ == 1000);
    assert(peek0->fields_.size() == 1 && peek0->fields_[0].value_ == "42");

    const auto* peek1 = seam.vqec_vision_ai_outpt_evdsm_peek(1);
    assert(peek1 != nullptr);
    assert(peek1->occurred_at_ns_ == 2000);

    assert(seam.vqec_vision_ai_outpt_evdsm_peek(2) == nullptr);
}

void vqec_vision_ai_ctest_edsct_test_bounded_capacity() {
    event_delivery_seam_config config;
    config.max_queued_events_ = 2;
    event_delivery_seam seam(config);

    assert(seam.vqec_vision_ai_ports_fesnk_deliver_event(
        vqec_vision_ai_ctest_edsct_make_event("cam0", "counting", 100)).code_ == status_code::ok);
    assert(seam.vqec_vision_ai_ports_fesnk_deliver_event(
        vqec_vision_ai_ctest_edsct_make_event("cam0", "counting", 200)).code_ == status_code::ok);

    // 3rd event exceeds max_queued_events_ -> fail-closed with resource_exhausted
    const auto res_overflow = seam.vqec_vision_ai_ports_fesnk_deliver_event(
        vqec_vision_ai_ctest_edsct_make_event("cam0", "counting", 300));
    assert(res_overflow.code_ == status_code::resource_exhausted);

    const auto& metrics = seam.vqec_vision_ai_outpt_evdsm_get_metrics();
    assert(metrics.events_accepted_ == 2);
    assert(metrics.events_dropped_ == 1);
    assert(metrics.events_pending_ == 2);
}

void vqec_vision_ai_ctest_edsct_test_stop_rejection() {
    event_delivery_seam_config config;
    config.max_queued_events_ = 5;
    event_delivery_seam seam(config);

    assert(seam.vqec_vision_ai_ports_fesnk_deliver_event(
        vqec_vision_ai_ctest_edsct_make_event("cam0", "counting", 100)).code_ == status_code::ok);

    seam.vqec_vision_ai_outpt_evdsm_request_stop();
    assert(seam.vqec_vision_ai_outpt_evdsm_is_stopping());

    const auto res_stopped = seam.vqec_vision_ai_ports_fesnk_deliver_event(
        vqec_vision_ai_ctest_edsct_make_event("cam0", "counting", 200));
    assert(res_stopped.code_ == status_code::invalid_state);

    const auto& metrics = seam.vqec_vision_ai_outpt_evdsm_get_metrics();
    assert(metrics.events_accepted_ == 1);
    assert(metrics.events_rejected_ == 1);
    assert(metrics.events_pending_ == 1);
}

void vqec_vision_ai_ctest_edsct_test_drain() {
    event_delivery_seam_config config;
    config.max_queued_events_ = 5;
    event_delivery_seam seam(config);

    assert(seam.vqec_vision_ai_ports_fesnk_deliver_event(
        vqec_vision_ai_ctest_edsct_make_event("cam0", "counting", 100)).code_ == status_code::ok);
    assert(seam.vqec_vision_ai_ports_fesnk_deliver_event(
        vqec_vision_ai_ctest_edsct_make_event("cam0", "counting", 200)).code_ == status_code::ok);
    assert(seam.vqec_vision_ai_ports_fesnk_deliver_event(
        vqec_vision_ai_ctest_edsct_make_event("cam0", "counting", 300)).code_ == status_code::ok);

    // Drain partial (1 event)
    assert(seam.vqec_vision_ai_outpt_evdsm_drain(1).code_ == status_code::ok);
    assert(seam.vqec_vision_ai_outpt_evdsm_get_pending_count() == 2);
    assert(seam.vqec_vision_ai_outpt_evdsm_get_metrics().events_drained_ == 1);
    assert(seam.vqec_vision_ai_outpt_evdsm_get_metrics().oldest_pending_ns_ == 200);

    // Drain remaining
    assert(seam.vqec_vision_ai_outpt_evdsm_drain().code_ == status_code::ok);
    assert(seam.vqec_vision_ai_outpt_evdsm_get_pending_count() == 0);
    assert(seam.vqec_vision_ai_outpt_evdsm_get_metrics().events_drained_ == 3);
    assert(seam.vqec_vision_ai_outpt_evdsm_get_metrics().oldest_pending_ns_ == 0);
}

}  // namespace
}  // namespace vqec::vision::ai

int main() {
    try {
        vqec::vision::ai::vqec_vision_ai_ctest_edsct_test_acceptance_and_pending();
        vqec::vision::ai::vqec_vision_ai_ctest_edsct_test_bounded_capacity();
        vqec::vision::ai::vqec_vision_ai_ctest_edsct_test_stop_rejection();
        vqec::vision::ai::vqec_vision_ai_ctest_edsct_test_drain();
        std::cout << "vqec_vision_event_delivery_seam_test: all tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << "Test failed with exception: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
