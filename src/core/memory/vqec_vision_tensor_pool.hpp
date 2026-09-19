#ifndef VQEC_VISION_AI_CORE_TENSOR_POOL_HPP
#define VQEC_VISION_AI_CORE_TENSOR_POOL_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>

#include "vqec/vision/ai/contracts/base/vqec_vision_status.hpp"
#include "vqec/vision/ai/contracts/inference/vqec_vision_tensor_result.hpp"

namespace vqec::vision::ai {

namespace tensor_pool_limits {
inline constexpr std::size_t g_max_slots = 16;
}  // namespace tensor_pool_limits

// Fixed-capacity, preallocated tensor storage so the steady-state path reuses bytes instead
// of resizing a vector per frame. The spec is immutable after configure; acquire/release are
// bounded and double-release is detected. The caller serializes use of an acquired slot.
struct tensor_pool_config {
    std::size_t capacity_{0};
    tensor_spec spec_;
    // Debug aid: overwrite released bytes with a poison pattern to expose use-after-release.
    bool poison_on_release_{false};
};

struct tensor_pool_stats {
    std::size_t capacity_{0};
    std::size_t free_{0};
    std::size_t live_{0};
    std::size_t high_watermark_{0};
    std::uint64_t acquire_total_{0};
    std::uint64_t release_total_{0};
    std::uint64_t exhausted_total_{0};
    std::uint64_t double_release_total_{0};
};

class tensor_pool {
public:
    tensor_pool() = default;
    tensor_pool(const tensor_pool& _other) = delete;
    tensor_pool& operator=(const tensor_pool& _other) = delete;

    [[nodiscard]] status vqec_vision_ai_core_tnpl_configure(
        const tensor_pool_config& _config);
    [[nodiscard]] bool vqec_vision_ai_core_tnpl_is_configured() const;
    // Returns a borrowed pointer to preallocated bytes. The pointer stays valid until
    // release; the caller must not resize it.
    [[nodiscard]] status vqec_vision_ai_core_tnpl_acquire(
        std::size_t& _slot_index, tensor_blob*& _blob);
    [[nodiscard]] status vqec_vision_ai_core_tnpl_release(std::size_t _slot_index);
    // Read access for a still-acquired slot; used by a downstream decoder.
    [[nodiscard]] status vqec_vision_ai_core_tnpl_get(
        std::size_t _slot_index, tensor_blob*& _blob);
    [[nodiscard]] tensor_pool_stats vqec_vision_ai_core_tnpl_get_stats() const;

private:
    mutable std::mutex mutex_;
    std::array<tensor_blob, tensor_pool_limits::g_max_slots> slots_{};
    std::array<bool, tensor_pool_limits::g_max_slots> in_use_{};
    std::size_t capacity_{0};
    std::size_t live_{0};
    std::size_t high_watermark_{0};
    std::uint64_t acquire_total_{0};
    std::uint64_t release_total_{0};
    std::uint64_t exhausted_total_{0};
    std::uint64_t double_release_total_{0};
    bool poison_on_release_{false};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_CORE_TENSOR_POOL_HPP
