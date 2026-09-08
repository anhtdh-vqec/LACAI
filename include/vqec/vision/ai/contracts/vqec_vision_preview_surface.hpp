#ifndef VQEC_VISION_AI_CONTRACTS_PREVIEW_SURFACE_HPP
#define VQEC_VISION_AI_CONTRACTS_PREVIEW_SURFACE_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "vqec/vision/ai/contracts/vqec_vision_preview_contract.hpp"

namespace vqec::vision::ai {

// CPU-owned tightly packed NV12 only; no FD import or hardware synchronization.
class writable_preview_surface {
public:
    writable_preview_surface() = default;
    writable_preview_surface(const writable_preview_surface&) = delete;
    writable_preview_surface& operator=(const writable_preview_surface&) = delete;
    writable_preview_surface(writable_preview_surface&&) noexcept = default;
    writable_preview_surface& operator=(writable_preview_surface&&) noexcept = default;

    // Synchronous allocation; failure leaves this object unchanged. Caller budgets total live owners.
    [[nodiscard]] status vqec_vision_ai_core_pvsrf_create(
        preview_geometry _geometry, std::uint64_t _max_bytes);

    // Borrow ends before seal/move/destruction. nullptr/zero when empty or sealed.
    [[nodiscard]] std::uint8_t* vqec_vision_ai_core_pvsrf_borrow_data() noexcept;
    [[nodiscard]] std::size_t vqec_vision_ai_core_pvsrf_size_bytes() const noexcept;

    // All mutable borrows must have ended. Empty returns an empty owner; no allocation/copy.
    // Downstream retains the returned owner until all input reads complete, not just result arrival.
    [[nodiscard]] std::shared_ptr<const std::vector<std::uint8_t>>
    vqec_vision_ai_core_pvsrf_seal() noexcept;

private:
    friend class preview_surface_pool;
    std::shared_ptr<std::vector<std::uint8_t>> storage_;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_CONTRACTS_PREVIEW_SURFACE_HPP
