#include "vqec_vision_tensor_pool.hpp"

#include <new>

#include "vqec/vision/ai/contracts/vqec_vision_tensor_contract.hpp"

namespace vqec::vision::ai {
namespace {

constexpr std::uint8_t g_tensor_pool_poison_byte = 0xDD;

}  // namespace

status tensor_pool::vqec_vision_ai_core_tnpl_configure(const tensor_pool_config& _config) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (capacity_ != 0) {
        return {status_code::invalid_state, "tensor pool is already configured"};
    }
    if (_config.capacity_ == 0 || _config.capacity_ > tensor_pool_limits::g_max_slots ||
        _config.spec_.dtype_ == tensor_element_type::unknown) {
        return {status_code::invalid_argument, "invalid tensor pool configuration"};
    }
    const auto bytes = vqec_vision_ai_core_tnctr_shape_bytes(_config.spec_);
    if (bytes == 0 || bytes > tensor_contract_limits::g_max_output_bytes) {
        return {status_code::invalid_argument,
            "tensor pool spec has no bytes or exceeds the slot limit"};
    }
    try {
        for (std::size_t index = 0; index < _config.capacity_; ++index) {
            slots_[index].spec_ = _config.spec_;
            slots_[index].bytes_.assign(static_cast<std::size_t>(bytes), 0U);
        }
    } catch (const std::bad_alloc&) {
        for (auto& slot : slots_) {
            slot.bytes_.clear();
            slot.bytes_.shrink_to_fit();
        }
        return {status_code::resource_exhausted, "tensor pool preallocation failed"};
    }
    capacity_ = _config.capacity_;
    poison_on_release_ = _config.poison_on_release_;
    return {};
}

bool tensor_pool::vqec_vision_ai_core_tnpl_is_configured() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return capacity_ != 0;
}

status tensor_pool::vqec_vision_ai_core_tnpl_acquire(
    std::size_t& _slot_index, tensor_blob*& _blob) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (capacity_ == 0) {
        return {status_code::invalid_state, "tensor pool is not configured"};
    }
    for (std::size_t index = 0; index < capacity_; ++index) {
        if (!in_use_[index]) {
            in_use_[index] = true;
            ++live_;
            ++acquire_total_;
            if (live_ > high_watermark_) {
                high_watermark_ = live_;
            }
            _slot_index = index;
            _blob = &slots_[index];
            return {};
        }
    }
    ++exhausted_total_;
    return {status_code::resource_exhausted, "tensor pool has no free slot"};
}

status tensor_pool::vqec_vision_ai_core_tnpl_release(std::size_t _slot_index) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (capacity_ == 0) {
        return {status_code::invalid_state, "tensor pool is not configured"};
    }
    if (_slot_index >= capacity_) {
        return {status_code::invalid_argument, "tensor slot is outside the pool"};
    }
    if (!in_use_[_slot_index]) {
        ++double_release_total_;
        return {status_code::invalid_state, "tensor slot is not acquired"};
    }
    if (poison_on_release_) {
        auto& bytes = slots_[_slot_index].bytes_;
        for (std::size_t index = 0; index < bytes.size(); ++index) {
            bytes[index] = g_tensor_pool_poison_byte;
        }
    }
    in_use_[_slot_index] = false;
    --live_;
    ++release_total_;
    return {};
}

status tensor_pool::vqec_vision_ai_core_tnpl_get(
    std::size_t _slot_index, tensor_blob*& _blob) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (capacity_ == 0) {
        return {status_code::invalid_state, "tensor pool is not configured"};
    }
    if (_slot_index >= capacity_ || !in_use_[_slot_index]) {
        return {status_code::invalid_argument, "tensor slot is not acquired"};
    }
    _blob = &slots_[_slot_index];
    return {};
}

tensor_pool_stats tensor_pool::vqec_vision_ai_core_tnpl_get_stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    tensor_pool_stats stats;
    stats.capacity_ = capacity_;
    stats.free_ = capacity_ - live_;
    stats.live_ = live_;
    stats.high_watermark_ = high_watermark_;
    stats.acquire_total_ = acquire_total_;
    stats.release_total_ = release_total_;
    stats.exhausted_total_ = exhausted_total_;
    stats.double_release_total_ = double_release_total_;
    return stats;
}

}  // namespace vqec::vision::ai
