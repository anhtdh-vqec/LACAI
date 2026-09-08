#include "vqec/vision/ai/contracts/vqec_vision_preview_pool.hpp"

#include "vqec/vision/ai/contracts/vqec_vision_preview_limits.hpp"

#include <new>
#include <utility>

namespace vqec::vision::ai {

status preview_surface_pool::vqec_vision_ai_core_pvpol_configure(
    preview_geometry _geometry, unsigned _capacity, std::uint64_t _max_total_bytes) {
    if (configured_) {
        return {status_code::invalid_state, "preview pool already configured"};
    }
    if (_capacity == 0 || _capacity > slots_.size() || _geometry.width_ == 0 ||
        _geometry.height_ == 0 || _geometry.width_ > preview_limits::g_max_dimension_pixels || _geometry.height_ > preview_limits::g_max_dimension_pixels ||
        _geometry.width_ % 2 != 0 || _geometry.height_ % 2 != 0 ||
        _max_total_bytes == 0 || _max_total_bytes > preview_limits::g_max_pool_bytes) {
        return {status_code::invalid_argument, "invalid preview pool configuration"};
    }
    const auto pixels = static_cast<std::uint64_t>(_geometry.width_) * _geometry.height_;
    const auto bytes = pixels + pixels / 2;
    if (bytes > preview_limits::g_max_surface_bytes || bytes * _capacity > _max_total_bytes) {
        return {status_code::resource_exhausted, "preview pool exceeds pixel budget"};
    }
    decltype(slots_) pending;
    try {
        for (unsigned index = 0; index < _capacity; ++index) {
            pending[index] = std::make_shared<slot>();
            pending[index]->bytes_.resize(static_cast<std::size_t>(bytes));
        }
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted, "preview pool allocation failed"};
    }
    slots_ = std::move(pending);
    allocated_bytes_ = bytes * _capacity;
    geometry_ = _geometry;
    configured_ = true;
    return {};
}

status preview_surface_pool::vqec_vision_ai_core_pvpol_acquire(writable_preview_surface& _writer) {
    if (!configured_ || _writer.storage_) {
        return {status_code::invalid_state, "preview pool unavailable or writer occupied"};
    }
    for (const auto& slot_owner : slots_) {
        if (!slot_owner || slot_owner->busy_.exchange(true, std::memory_order_acquire)) {
            continue;
        }
        try {
            // Separate control block per lease: old weak readers cannot resurrect reused pixels.
            std::shared_ptr<std::vector<std::uint8_t>> lease(
                &slot_owner->bytes_, [lease_slot = slot_owner](
                    std::vector<std::uint8_t>* _bytes) mutable noexcept {
                    (void)_bytes;
                    lease_slot->busy_.store(false, std::memory_order_release);
                    // Weak observers may retain the old control block, not the pixel allocation.
                    lease_slot.reset();
                });
            _writer.storage_ = std::move(lease);
        } catch (const std::bad_alloc&) {
            // shared_ptr invokes the supplied deleter if control-block allocation fails.
            return {status_code::resource_exhausted, "preview pool lease allocation failed"};
        }
        return {};
    }
    return {status_code::resource_exhausted, "preview pool has no free surface"};
}

unsigned preview_surface_pool::vqec_vision_ai_core_pvpol_available() const noexcept {
    unsigned available = 0;
    for (const auto& slot_owner : slots_) {
        if (slot_owner && !slot_owner->busy_.load(std::memory_order_acquire)) {
            ++available;
        }
    }
    return available;
}

std::uint64_t preview_surface_pool::vqec_vision_ai_core_pvpol_allocated_bytes() const noexcept {
    return allocated_bytes_;
}

preview_geometry preview_surface_pool::vqec_vision_ai_core_pvpol_geometry() const noexcept {
    return geometry_;
}

}  // namespace vqec::vision::ai
