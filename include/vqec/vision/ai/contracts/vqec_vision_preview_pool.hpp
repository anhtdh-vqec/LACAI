#ifndef VQEC_VISION_AI_CONTRACTS_PREVIEW_POOL_HPP
#define VQEC_VISION_AI_CONTRACTS_PREVIEW_POOL_HPP

#include <array>
#include <atomic>
#include <memory>

#include "vqec/vision/ai/contracts/vqec_vision_preview_surface.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_preview_limits.hpp"

namespace vqec::vision::ai {

// CPU pixel pool only. Configure/acquire serialize; final owner release may run anywhere.
class preview_surface_pool {
public:
    preview_surface_pool() = default;
    preview_surface_pool(const preview_surface_pool&) = delete;
    preview_surface_pool& operator=(const preview_surface_pool&) = delete;

    // One-shot synchronous preallocation; failure leaves the pool unconfigured.
    [[nodiscard]] status vqec_vision_ai_core_pvpol_configure(
        preview_geometry _geometry, unsigned _capacity, std::uint64_t _max_total_bytes);
    // Requires empty writer; pixels are not cleared. No waiting/fallback if all slots busy.
    [[nodiscard]] status vqec_vision_ai_core_pvpol_acquire(writable_preview_surface& _writer);
    // Diagnostic snapshot, not a reservation. Concurrent releases may increase availability.
    [[nodiscard]] unsigned vqec_vision_ai_core_pvpol_available() const noexcept;
    [[nodiscard]] std::uint64_t vqec_vision_ai_core_pvpol_allocated_bytes() const noexcept;
    [[nodiscard]] preview_geometry vqec_vision_ai_core_pvpol_geometry() const noexcept;

private:
    struct slot {
        std::vector<std::uint8_t> bytes_;
        std::atomic<bool> busy_{false};
    };
    std::array<std::shared_ptr<slot>, preview_limits::g_max_surface_slots> slots_{};
    std::uint64_t allocated_bytes_{0};
    bool configured_{false};
    preview_geometry geometry_;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_CONTRACTS_PREVIEW_POOL_HPP
