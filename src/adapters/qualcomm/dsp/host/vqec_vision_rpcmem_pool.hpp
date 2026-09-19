#ifndef VQEC_VISION_AI_QUALCOMM_RPCMEM_POOL_HPP
#define VQEC_VISION_AI_QUALCOMM_RPCMEM_POOL_HPP

#include <cstddef>
#include <cstdint>
#include <vector>

#include "vqec/vision/ai/contracts/base/vqec_vision_status.hpp"

namespace vqec::vision::ai {

struct rpcmem_buffer {
    void* data_{nullptr};
    int fd_{-1};
    std::size_t size_{0};
};

class rpcmem_pool final {
public:
    rpcmem_pool() = default;
    ~rpcmem_pool();

    rpcmem_pool(const rpcmem_pool&) = delete;
    rpcmem_pool& operator=(const rpcmem_pool&) = delete;
    rpcmem_pool(rpcmem_pool&&) noexcept;
    rpcmem_pool& operator=(rpcmem_pool&&) noexcept;

    [[nodiscard]] status vqec_vision_ai_qcom_rpcm_allocate(
        std::size_t _size, std::size_t _count);

    [[nodiscard]] status vqec_vision_ai_qcom_rpcm_allocate(
        const std::vector<std::size_t>& _sizes);

    [[nodiscard]] const rpcmem_buffer& vqec_vision_ai_qcom_rpcm_slot(
        std::size_t _index) const;

    [[nodiscard]] std::size_t vqec_vision_ai_qcom_rpcm_count() const noexcept;

    void vqec_vision_ai_qcom_rpcm_release() noexcept;

private:
    std::vector<rpcmem_buffer> slots_;
};

}  // namespace vqec::vision::ai

extern "C" {
void* vqec_vision_ai_qcom_rpcm_alloc(std::size_t _size, int* _fd);
void vqec_vision_ai_qcom_rpcm_free(void* _ptr, int _fd, std::size_t _size);
}

#endif  // VQEC_VISION_AI_QUALCOMM_RPCMEM_POOL_HPP
