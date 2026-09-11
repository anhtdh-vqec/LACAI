#ifndef VQEC_VISION_AI_FTMGR_FEATURE_STAGE_HPP
#define VQEC_VISION_AI_FTMGR_FEATURE_STAGE_HPP

#include <cstdint>

#include "vqec/vision/ai/ports/vqec_vision_feature_processor.hpp"

namespace vqec::vision::ai {

class feature_stage final {
public:
    feature_stage(feature_processor_port& _processor, feature_processor_config _config);
    feature_stage(const feature_stage& _other) = delete;
    feature_stage& operator=(const feature_stage& _other) = delete;

    [[nodiscard]] status vqec_vision_ai_ftmgr_ftstg_activate();
    [[nodiscard]] status vqec_vision_ai_ftmgr_ftstg_process(
        const observation_batch& _tracked, std::uint64_t _now_monotonic_ns,
        bool _is_source_gap, feature_event_batch& _events);
    [[nodiscard]] bool vqec_vision_ai_ftmgr_ftstg_is_active() const noexcept;
    [[nodiscard]] bool vqec_vision_ai_ftmgr_ftstg_is_faulted() const noexcept;
    // Borrowed activation configuration; valid for the life of the stage. Used by the
    // output boundary to authorize each event against the same identity the stage enforced.
    [[nodiscard]] const feature_processor_config&
    vqec_vision_ai_ftmgr_ftstg_get_config() const noexcept;

private:
    feature_processor_port& processor_;
    feature_processor_config config_;
    std::uint64_t source_epoch_{0};
    std::uint64_t last_now_monotonic_ns_{0};
    bool is_active_{false};
    bool is_faulted_{false};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_FTMGR_FEATURE_STAGE_HPP
