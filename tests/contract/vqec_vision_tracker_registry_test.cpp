#include <cassert>
#include <cstdint>
#include <memory>
#include <string>

#include "vqec_vision_tracker_registry.hpp"

using namespace vqec::vision::ai;

namespace {

class vqec_vision_ai_ctest_trrct_tracker final : public tracker_port {
public:
    [[nodiscard]] status vqec_vision_ai_ports_trker_validate_activation() const override {
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_trker_update_tracks(
        const observation_batch& _detections, std::uint64_t _now_monotonic_ns,
        bool _is_source_gap, observation_batch& _tracked) override {
        (void)_now_monotonic_ns;
        (void)_is_source_gap;
        _tracked = _detections;
        for (auto& observation : _tracked.observations_) {
            observation.track_id_ = 1;
        }
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_trker_reset_epoch(
        std::uint64_t _source_epoch) override {
        (void)_source_epoch;
        return {};
    }
};

class vqec_vision_ai_ctest_trrct_factory final : public tracker_factory_port {
public:
    [[nodiscard]] status vqec_vision_ai_track_trfac_validate_activation(
        const std::string& _source_id, const std::string& _model_id) const override {
        return _source_id == "source.front" && _model_id == "person_detector" ? status{} :
            status{status_code::unsupported, "fixture only supports the front model"};
    }
    [[nodiscard]] status vqec_vision_ai_track_trfac_create_tracker(
        const std::string& _source_id, const std::string& _model_id,
        std::unique_ptr<tracker_port>& _tracker) override {
        (void)_source_id;
        (void)_model_id;
        _tracker = std::make_unique<vqec_vision_ai_ctest_trrct_tracker>();
        return {};
    }
};

}  // namespace

int main() {
    tracker_registry registry;
    vqec_vision_ai_ctest_trrct_factory factory;
    assert(registry.vqec_vision_ai_track_trreg_register_factory(
               "bytetrack.v1", factory).code_ == status_code::ok);
    assert(registry.vqec_vision_ai_track_trreg_register_factory(
               "bytetrack.v1", factory).code_ == status_code::invalid_argument);
    tracker_factory_port* resolved = nullptr;
    assert(registry.vqec_vision_ai_track_trreg_resolve_factory(
               "missing.v1", resolved).code_ == status_code::unsupported);
    assert(resolved == nullptr);

    std::unique_ptr<tracker_port> tracker;
    assert(registry.vqec_vision_ai_track_trreg_create_tracker(
               "bytetrack.v1", "source.front", "person_detector", tracker).code_ ==
           status_code::ok);
    assert(tracker != nullptr);
    auto* previous = tracker.get();
    assert(registry.vqec_vision_ai_track_trreg_create_tracker(
               "bytetrack.v1", "source.rear", "person_detector", tracker).code_ ==
           status_code::unsupported);
    assert(tracker.get() == previous);
    assert(registry.vqec_vision_ai_track_trreg_create_tracker(
               "bad contract", "source.front", "person_detector", tracker).code_ ==
           status_code::invalid_argument);
    assert(registry.vqec_vision_ai_track_trreg_create_tracker(
               "bytetrack.v1", "", "person_detector", tracker).code_ ==
           status_code::invalid_argument);
    registry.vqec_vision_ai_track_trreg_clear();
    assert(registry.vqec_vision_ai_track_trreg_get_count() == 0U);
    return 0;
}
