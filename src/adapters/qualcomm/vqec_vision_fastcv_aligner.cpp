#include "vqec_vision_fastcv_aligner.hpp"

#include <fastcv/fastcv.h>

#include <sys/mman.h>
#include <unistd.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <limits>
#include <vector>

#include "vqec/vision/ai/contracts/vqec_vision_color.hpp"

namespace vqec::vision::ai {
namespace {

// The synchronous warp issues one completion ticket.
constexpr std::uint64_t g_align_ticket = 1;

constexpr char g_aligned_tensor_name[] = "aligned_luma";

}  // namespace

fastcv_aligner::fastcv_aligner(fastcv_aligner_config _config) noexcept : config_(_config) {}

status fastcv_aligner::vqec_vision_ai_ports_imaln_probe_capabilities(
    alignment_capabilities& _capabilities) const {
    _capabilities.supports_similarity_ = true;
    _capabilities.max_points_ = image_alignment_limits::g_max_points;
    _capabilities.max_destination_dimension_ =
        image_alignment_limits::g_max_destination_dimension;
    return {};
}

status fastcv_aligner::vqec_vision_ai_ports_imaln_validate_template(
    const alignment_template& _template,
    const alignment_capabilities& _capabilities) const {
    return vqec_vision_ai_core_imaln_require_capability(_capabilities, _template);
}

status fastcv_aligner::vqec_vision_ai_ports_imaln_align(
    const alignment_request& _request, const raw_frame& _source,
    const alignment_template& _template, alignment_result& _result,
    std::uint64_t& _ticket) {
    const auto valid = vqec_vision_ai_core_imaln_validate_request(_request, _template);
    if (valid.code_ != status_code::ok) {
        return valid;
    }
    const auto& descriptor = _source.descriptor_;
    if (_source.native_handle_ < 0 || descriptor.width_ == 0 || descriptor.height_ == 0 ||
        descriptor.view_size_bytes_ == 0 || descriptor.strides_[0] <= 0 ||
        static_cast<std::uint64_t>(descriptor.strides_[0]) < descriptor.width_ ||
        descriptor.offsets_[0] > descriptor.view_size_bytes_) {
        return {status_code::invalid_argument, "alignment source frame is not linear NV12"};
    }
    alignment_transform transform;
    const auto fitted = vqec_vision_ai_core_imaln_compute_similarity(
        _request.landmarks_.points_, _template.reference_points_, transform);
    if (fitted.code_ != status_code::ok) {
        return fitted;
    }
    const double determinant = static_cast<double>(transform.m00_) * transform.m11_ -
        static_cast<double>(transform.m01_) * transform.m10_;
    if (std::fabs(determinant) < 1e-12) {
        return {status_code::unsupported, "alignment transform is singular"};
    }
    const double inv00 = transform.m11_ / determinant;
    const double inv01 = -static_cast<double>(transform.m01_) / determinant;
    const double inv10 = -static_cast<double>(transform.m10_) / determinant;
    const double inv11 = transform.m00_ / determinant;
    const double patch_center_x = _template.destination_width_ / 2.0;
    const double patch_center_y = _template.destination_height_ / 2.0;
    const double offset_x = patch_center_x - transform.m02_;
    const double offset_y = patch_center_y - transform.m12_;
    alignas(16) float position[2] = {
        static_cast<float>(inv00 * offset_x + inv01 * offset_y),
        static_cast<float>(inv10 * offset_x + inv11 * offset_y)};
    alignas(16) float affine[4] = {static_cast<float>(inv00), static_cast<float>(inv01),
        static_cast<float>(inv10), static_cast<float>(inv11)};

    // Map the borrowed FD read-only, page-aligned. This is a CPU read, not zero-copy.
    const long page_size = ::sysconf(_SC_PAGESIZE);
    if (page_size <= 0) {
        return {status_code::io_error, "cannot determine page size"};
    }
    const std::uint64_t aligned_offset =
        (descriptor.memory_offset_bytes_ / static_cast<std::uint64_t>(page_size)) *
        static_cast<std::uint64_t>(page_size);
    const std::uint64_t in_page_offset = descriptor.memory_offset_bytes_ - aligned_offset;
    const std::uint64_t map_length = descriptor.view_size_bytes_ + in_page_offset;
    void* mapping = ::mmap(nullptr, static_cast<std::size_t>(map_length), PROT_READ,
        MAP_PRIVATE, static_cast<int>(_source.native_handle_),
        static_cast<off_t>(aligned_offset));
    if (mapping == MAP_FAILED) {
        return {status_code::io_error, "cannot map the alignment source frame"};
    }
    const auto* base = static_cast<const std::uint8_t*>(mapping);
    const std::size_t luma_offset =
        static_cast<std::size_t>(in_page_offset + descriptor.offsets_[0]);
    const std::size_t patch_pixels = static_cast<std::size_t>(
        _template.destination_width_) * _template.destination_height_;

    alignment_result candidate;
    if (config_.output_rgb_) {
        if ((config_.matrix_ != color_matrix::bt601 &&
                config_.matrix_ != color_matrix::bt709) ||
            (config_.range_ != color_range::limited && config_.range_ != color_range::full) ||
            descriptor.strides_[1] <= 0 ||
            static_cast<std::uint64_t>(descriptor.strides_[1]) < descriptor.width_ ||
            descriptor.offsets_[1] > descriptor.view_size_bytes_) {
            ::munmap(mapping, static_cast<std::size_t>(map_length));
            return {status_code::unsupported, "RGB alignment requires a valid NV12 color policy"};
        }
        const std::size_t rgb_stride = static_cast<std::size_t>(descriptor.width_) * 3U;
        std::vector<std::uint8_t> rgb(rgb_stride * descriptor.height_, 0U);
        const std::size_t chroma_offset =
            static_cast<std::size_t>(in_page_offset + descriptor.offsets_[1]);
        const auto converted = vqec_vision_ai_core_color_convert_nv12_to_rgb(
            base + luma_offset, static_cast<std::uint32_t>(descriptor.strides_[0]),
            base + chroma_offset, static_cast<std::uint32_t>(descriptor.strides_[1]),
            descriptor.width_, descriptor.height_, config_.matrix_, config_.range_,
            channel_order::rgb, rgb.data(), static_cast<std::uint32_t>(rgb_stride));
        if (converted.code_ != status_code::ok) {
            ::munmap(mapping, static_cast<std::size_t>(map_length));
            return converted;
        }
        // Deinterleave into three planar channels, warp each with the verified patch warp,
        // then interleave to the requested order. Correct but not the optimized path.
        std::array<std::vector<std::uint8_t>, 3> planes;
        for (auto& plane : planes) {
            plane.assign(static_cast<std::size_t>(descriptor.width_) * descriptor.height_, 0U);
        }
        for (std::uint32_t row = 0; row < descriptor.height_; ++row) {
            const std::uint8_t* source_row = rgb.data() + static_cast<std::size_t>(row) * rgb_stride;
            for (unsigned channel = 0; channel < 3U; ++channel) {
                std::uint8_t* plane_row = planes[channel].data() +
                    static_cast<std::size_t>(row) * descriptor.width_;
                for (std::uint32_t column = 0; column < descriptor.width_; ++column) {
                    plane_row[column] =
                        source_row[static_cast<std::size_t>(column) * 3U + channel];
                }
            }
        }
        std::array<std::vector<std::uint8_t>, 3> patches;
        for (unsigned channel = 0; channel < 3U; ++channel) {
            patches[channel].assign(patch_pixels, 0U);
            const int warp = fcvTransformAffineu8_v2(planes[channel].data(), descriptor.width_,
                descriptor.height_, descriptor.width_, position, affine,
                patches[channel].data(), _template.destination_width_,
                _template.destination_height_, _template.destination_width_);
            if (warp != 0) {
                ::munmap(mapping, static_cast<std::size_t>(map_length));
                return {status_code::io_error, "FastCV affine warp failed"};
            }
        }
        const bool rgb_order = config_.order_ == channel_order::rgb;
        std::vector<std::uint8_t> packed(patch_pixels * 3U, 0U);
        for (std::size_t index = 0; index < patch_pixels; ++index) {
            packed[index * 3U + 0U] = rgb_order ? patches[0][index] : patches[2][index];
            packed[index * 3U + 1U] = patches[1][index];
            packed[index * 3U + 2U] = rgb_order ? patches[2][index] : patches[0][index];
        }
        candidate.tensor_.spec_.dimensions_ = {1U, _template.destination_height_,
            _template.destination_width_, 3U};
        candidate.tensor_.bytes_ = std::move(packed);
    } else {
        std::vector<std::uint8_t> luma(
            static_cast<std::size_t>(descriptor.width_) * descriptor.height_, 0U);
        for (std::uint32_t row = 0; row < descriptor.height_; ++row) {
            std::memcpy(luma.data() + static_cast<std::size_t>(row) * descriptor.width_,
                base + luma_offset + static_cast<std::size_t>(row) * descriptor.strides_[0],
                descriptor.width_);
        }
        std::vector<std::uint8_t> patch(patch_pixels, 0U);
        const int result = fcvTransformAffineu8_v2(luma.data(), descriptor.width_,
            descriptor.height_, descriptor.width_, position, affine, patch.data(),
            _template.destination_width_, _template.destination_height_,
            _template.destination_width_);
        if (result != 0) {
            ::munmap(mapping, static_cast<std::size_t>(map_length));
            return {status_code::io_error, "FastCV affine warp failed"};
        }
        candidate.tensor_.spec_.dimensions_ = {1U, _template.destination_height_,
            _template.destination_width_, 1U};
        candidate.tensor_.bytes_ = std::move(patch);
    }
    ::munmap(mapping, static_cast<std::size_t>(map_length));
    candidate.tensor_.spec_.name_ = g_aligned_tensor_name;
    candidate.tensor_.spec_.dtype_ = tensor_element_type::uint8;
    candidate.transform_ = transform;
    candidate.transform_.source_width_ = descriptor.width_;
    candidate.transform_.source_height_ = descriptor.height_;
    candidate.transform_.destination_width_ = _template.destination_width_;
    candidate.transform_.destination_height_ = _template.destination_height_;
    candidate.has_transform_ = true;
    _result = std::move(candidate);
    _ticket = g_align_ticket;
    return {};
}

status fastcv_aligner::vqec_vision_ai_ports_imaln_poll_completion(
    std::uint64_t _ticket, bool& _complete) {
    if (_ticket != g_align_ticket) {
        return {status_code::invalid_state, "unknown alignment ticket"};
    }
    _complete = true;
    return {};
}

}  // namespace vqec::vision::ai
