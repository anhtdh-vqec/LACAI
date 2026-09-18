#ifndef VQEC_VISION_AI_QUALCOMM_DSP_BUFFER_CACHE_HPP
#define VQEC_VISION_AI_QUALCOMM_DSP_BUFFER_CACHE_HPP

#include <cstddef>
#include <cstdint>
#include <memory>

#include "vqec/vision/ai/contracts/vqec_vision_status.hpp"

namespace vqec::vision::ai {

struct dsp_buffer_cache_config {
    std::size_t max_entries_{32};
    bool enable_fastrpc_{true};
};

// Pins mappings, not camera acquisition/cache readiness. Keep owner_ through
// the last synchronous access and retain the RAW owner separately.
struct dsp_buffer_mapping {
    const std::uint8_t* data_{nullptr};
    std::size_t size_{0};
    std::shared_ptr<const void> owner_;
};

// Manages persistent CPU virtual memory and FastRPC buffer registrations for
// camera DMA-BUF frames. Caches registrations across acquisition cycles so
// that the generated QAIC pointer-marshalling path can reuse the FD association.
class dsp_buffer_cache final {
public:
    explicit dsp_buffer_cache(dsp_buffer_cache_config _config = {});
    ~dsp_buffer_cache() noexcept;

    dsp_buffer_cache(const dsp_buffer_cache&) = delete;
    dsp_buffer_cache& operator=(const dsp_buffer_cache&) = delete;
    dsp_buffer_cache(dsp_buffer_cache&&) noexcept;
    dsp_buffer_cache& operator=(dsp_buffer_cache&&) noexcept;

    // Resolves or establishes CPU mapping and FastRPC registration for the DMA-BUF.
    [[nodiscard]] dsp_buffer_mapping vqec_vision_ai_qcom_dspbc_map(
        int _fd, std::size_t _size, status& _status);

    // Retires mappings. Active leases remain valid and count against capacity.
    void vqec_vision_ai_qcom_dspbc_clear() noexcept;

    [[nodiscard]] std::size_t vqec_vision_ai_qcom_dspbc_entry_count() const noexcept;

private:
    struct implementation;
    std::unique_ptr<implementation> implementation_;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_QUALCOMM_DSP_BUFFER_CACHE_HPP
