#include "vqec/vision/ai/contracts/vqec_vision_source_binding.hpp"

namespace vqec::vision::ai {
namespace {

bool vqec_vision_ai_core_srcbd_is_reference_valid(const std::string& _reference) noexcept {
    if (_reference.empty() || _reference.size() > 256) {
        return false;
    }
    for (const auto character : _reference) {
        const auto byte = static_cast<unsigned char>(character);
        if (byte < 0x21 || byte > 0x7e) {
            return false;
        }
    }
    return true;
}

}  // namespace

status vqec_vision_ai_core_srcbd_validate_binding(
    const source_binding& _binding, const inference_plan& _plan) {
    const auto plan_status = vqec_vision_ai_core_infpl_validate_plan(_plan);
    if (plan_status.code_ != status_code::ok) {
        return plan_status;
    }
    // Products of two uint32 values fit uint64; compare rates without rounding.
    if (_binding.width_ != _plan.source_width_ || _binding.height_ != _plan.source_height_ ||
        _binding.fps_numerator_ == 0 || _binding.fps_denominator_ == 0 ||
        static_cast<std::uint64_t>(_binding.fps_numerator_) * _plan.fps_denominator_ !=
            static_cast<std::uint64_t>(_plan.fps_numerator_) * _binding.fps_denominator_) {
        return {status_code::invalid_argument, "source geometry or frame rate differs from plan"};
    }
    if (_binding.memory_kind_ != source_memory_kind::dmabuf ||
        _binding.layout_ != source_memory_layout::linear_nv12 ||
        _binding.sync_mode_ != source_sync_mode::implicit_ready) {
        return {status_code::unsupported, "source requires linear NV12 DMA-BUF with implicit sync"};
    }
    if ((_binding.color_profile_ != source_color_profile::bt601_limited &&
         _binding.color_profile_ != source_color_profile::bt709_limited) ||
        (_binding.chroma_site_ != source_chroma_site::mpeg2 &&
         _binding.chroma_site_ != source_chroma_site::jpeg)) {
        return {status_code::unsupported,
                "explicit supported colorimetry and chroma siting needed"};
    }
    if (!vqec_vision_ai_core_srcbd_is_reference_valid(_binding.fw_memory_contract_) ||
        !vqec_vision_ai_core_srcbd_is_reference_valid(_binding.backend_memory_contract_) ||
        !vqec_vision_ai_core_srcbd_is_reference_valid(_binding.preprocess_contract_)) {
        return {status_code::invalid_argument, "missing or invalid deployment evidence reference"};
    }
    return {};
}

}  // namespace vqec::vision::ai
