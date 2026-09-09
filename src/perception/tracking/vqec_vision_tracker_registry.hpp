#ifndef VQEC_VISION_AI_TRACK_TRACKER_REGISTRY_HPP
#define VQEC_VISION_AI_TRACK_TRACKER_REGISTRY_HPP

#include <array>
#include <cstddef>
#include <memory>
#include <string>

#include "vqec/vision/ai/ports/vqec_vision_tracker.hpp"

namespace vqec::vision::ai {

namespace tracker_registry_limits {
inline constexpr std::size_t g_max_registered_factories = 64;
inline constexpr std::size_t g_max_contract_bytes = 128;
inline constexpr std::size_t g_max_binding_identifier_bytes = 128;
}  // namespace tracker_registry_limits

// Factory implementations are borrowed and must outlive this registry. A factory
// returns a distinct tracker owner for each source/model binding.
class tracker_factory_port {
public:
    virtual ~tracker_factory_port() = default;
    [[nodiscard]] virtual status vqec_vision_ai_track_trfac_validate_activation(
        const std::string& _source_id, const std::string& _model_id) const = 0;
    [[nodiscard]] virtual status vqec_vision_ai_track_trfac_create_tracker(
        const std::string& _source_id, const std::string& _model_id,
        std::unique_ptr<tracker_port>& _tracker) = 0;
};

class tracker_registry final {
public:
    tracker_registry() = default;
    tracker_registry(const tracker_registry& _other) = delete;
    tracker_registry& operator=(const tracker_registry& _other) = delete;

    [[nodiscard]] status vqec_vision_ai_track_trreg_register_factory(
        const std::string& _contract, tracker_factory_port& _factory);
    [[nodiscard]] status vqec_vision_ai_track_trreg_resolve_factory(
        const std::string& _contract, tracker_factory_port*& _factory) const noexcept;
    [[nodiscard]] status vqec_vision_ai_track_trreg_create_tracker(
        const std::string& _contract, const std::string& _source_id,
        const std::string& _model_id,
        std::unique_ptr<tracker_port>& _tracker) const;
    void vqec_vision_ai_track_trreg_clear() noexcept;
    [[nodiscard]] std::size_t
    vqec_vision_ai_track_trreg_get_count() const noexcept;

private:
    struct entry {
        std::string contract_;
        tracker_factory_port* factory_{nullptr};
    };
    std::array<entry, tracker_registry_limits::g_max_registered_factories> entries_{};
    std::size_t count_{0};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_TRACK_TRACKER_REGISTRY_HPP
