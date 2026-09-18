#ifndef VQEC_VISION_AI_QUALCOMM_DSP_BUFFER_CACHE_HPP
#define VQEC_VISION_AI_QUALCOMM_DSP_BUFFER_CACHE_HPP

#include <cstddef>
#include <cstdint>
#include <memory>

#include "vqec/vision/ai/contracts/vqec_vision_status.hpp"

namespace vqec::vision::ai {

struct dsp_buffer_cache_config {
    std::size_t max_entries_{8};
};

// Manages persistent CPU virtual memory and FastRPC cDSP SMMU mappings for
// camera DMA-BUF frames. Caches mappings across acquisition cycles so that
// repeated fastrpc_mmap system calls are avoided on the frame hot path.
class dsp_buffer_cache final {
public:
    explicit dsp_buffer_cache(dsp_buffer_cache_config _config = {});
    ~dsp_buffer_cache() noexcept;

    dsp_buffer_cache(const dsp_buffer_cache&) = delete;
    dsp_buffer_cache& operator=(const dsp_buffer_cache&) = delete;
    dsp_buffer_cache(dsp_buffer_cache&&) noexcept;
    dsp_buffer_cache& operator=(dsp_buffer_cache&&) noexcept;

    // Resolves or establishes CPU and FastRPC mappings for the given DMA-BUF FD and size.
    [[nodiscard]] const std::uint8_t* vqec_vision_ai_qcom_dspbc_map(
        int _fd, std::size_t _size, status& _status);

    // Clears all cached mappings, calling fastrpc_munmap and munmap for each entry.
    void vqec_vision_ai_qcom_dspbc_clear() noexcept;

    [[nodiscard]] std::size_t vqec_vision_ai_qcom_dspbc_entry_count() const noexcept;

private:
    struct implementation;
    std::unique_ptr<implementation> implementation_;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_QUALCOMM_DSP_BUFFER_CACHE_HPP
