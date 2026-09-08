#include "vqec/vision/ai/contracts/vqec_vision_preview_surface.hpp"

#include "vqec/vision/ai/contracts/vqec_vision_preview_limits.hpp"

#include <limits>
#include <new>
#include <utility>

namespace vqec::vision::ai {

status writable_preview_surface::vqec_vision_ai_core_pvsrf_create(
    preview_geometry _geometry, std::uint64_t _max_bytes) {
    if (storage_) {
        return {status_code::invalid_state, "preview surface is already writable"};
    }
    if (_geometry.width_ == 0 || _geometry.height_ == 0 ||
        _geometry.width_ > preview_limits::g_max_dimension_pixels || _geometry.height_ > preview_limits::g_max_dimension_pixels ||
        _geometry.width_ % 2 != 0 || _geometry.height_ % 2 != 0 ||
        _max_bytes == 0 || _max_bytes > preview_limits::g_max_surface_bytes) {
        return {status_code::invalid_argument, "invalid CPU preview surface geometry or budget"};
    }
    const std::uint64_t pixels = static_cast<std::uint64_t>(_geometry.width_) * _geometry.height_;
    const std::uint64_t bytes = pixels + pixels / 2;
    if (bytes > _max_bytes || bytes > std::numeric_limits<std::size_t>::max()) {
        return {status_code::resource_exhausted, "CPU preview surface exceeds byte budget"};
    }
    try {
        storage_ = std::make_shared<std::vector<std::uint8_t>>(static_cast<std::size_t>(bytes));
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted, "CPU preview surface allocation failed"};
    }
    return {};
}

std::uint8_t* writable_preview_surface::vqec_vision_ai_core_pvsrf_borrow_data() noexcept {
    return storage_ ? storage_->data() : nullptr;
}

std::size_t writable_preview_surface::vqec_vision_ai_core_pvsrf_size_bytes() const noexcept {
    return storage_ ? storage_->size() : 0;
}

std::shared_ptr<const std::vector<std::uint8_t>>
writable_preview_surface::vqec_vision_ai_core_pvsrf_seal() noexcept {
    return std::move(storage_);
}

}  // namespace vqec::vision::ai
