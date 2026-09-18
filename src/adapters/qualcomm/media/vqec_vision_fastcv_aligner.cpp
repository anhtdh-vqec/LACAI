#include "vqec_vision_fastcv_aligner.hpp"
#include "vqec_vision_dsp_buffer_cache.hpp"

#include <fastcv/fastcv.h>

#include <sys/mman.h>
#include <unistd.h>

#include <array>
#include <algorithm>
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
constexpr double g_roi_interpolation_margin_pixels = 2.0;
constexpr std::uint32_t g_nv12_coordinate_alignment_pixels = 8U;

// True when a plane with the given offset/stride/width/height lies entirely inside the
// valid view, using overflow-safe arithmetic.
bool vqec_vision_ai_qcom_fcaln_plane_fits(std::uint64_t _offset, std::int32_t _stride,
    std::uint32_t _width, std::uint32_t _height, std::uint64_t _view_bytes) noexcept {
    if (_stride <= 0 || static_cast<std::uint64_t>(_stride) < _width || _height == 0 ||
        _offset > _view_bytes) {
        return false;
    }
    const std::uint64_t stride = static_cast<std::uint64_t>(_stride);
    const std::uint64_t last_row = static_cast<std::uint64_t>(_height) - 1U;
    if (last_row != 0 &&
        stride > (std::numeric_limits<std::uint64_t>::max() - _width) / last_row) {
        return false;
    }
    const std::uint64_t span = last_row * stride + _width;
    if (_offset > std::numeric_limits<std::uint64_t>::max() - span) {
        return false;
    }
    return _offset + span <= _view_bytes;
}

// FastCV rejects affine inputs smaller than the destination patch even when the sampled
// footprint itself is smaller (for example a close face). Preserve the ROI optimization
// while expanding and shifting the crop to that documented backend precondition.
void vqec_vision_ai_qcom_fcaln_make_roi_bounds(double _sample_min, double _sample_max,
    std::uint32_t _minimum_extent, std::uint32_t _source_extent,
    std::uint32_t& _begin, std::uint32_t& _end) noexcept {
    const double sampled_extent = std::max(0.0, _sample_max - _sample_min) +
        2.0 * g_roi_interpolation_margin_pixels;
    const double required_extent = std::max(
        sampled_extent,
        static_cast<double>(_minimum_extent) + 2.0 * g_roi_interpolation_margin_pixels);
    auto extent = static_cast<std::uint32_t>(std::ceil(required_extent));
    extent = (extent + g_nv12_coordinate_alignment_pixels - 1U) &
        ~(g_nv12_coordinate_alignment_pixels - 1U);
    extent = std::min(extent, _source_extent);
    const double centre = (_sample_min + _sample_max) / 2.0;
    const double unclamped_begin = centre - static_cast<double>(extent) / 2.0;
    const double maximum_begin = static_cast<double>(_source_extent - extent);
    auto begin = static_cast<std::uint32_t>(
        std::max(0.0, std::min(std::floor(unclamped_begin), maximum_begin)));
    begin &= ~(g_nv12_coordinate_alignment_pixels - 1U);
    if (begin > _source_extent - extent) {
        begin = (_source_extent - extent) &
            ~(g_nv12_coordinate_alignment_pixels - 1U);
    }
    _begin = begin;
    _end = begin + extent;
}

// A direct bounded fallback keeps FR available when the FastCV binary rejects an otherwise
// valid small ROI while another FastCV-backed graph is active. FastCV remains the first
// path; this samples only the aligned patch and never converts the full source frame.
void vqec_vision_ai_qcom_fcaln_warp_bilinear_fallback(const std::uint8_t* _source,
    std::uint32_t _source_width, std::uint32_t _source_height,
    std::uint32_t _source_stride, const float (&_position)[2],
    const float (&_affine)[4], std::uint8_t* _destination,
    std::uint32_t _destination_width, std::uint32_t _destination_height,
    std::uint32_t _destination_stride) noexcept {
    constexpr std::uint8_t g_border_fill_value = 0U;
    const float destination_centre_x = static_cast<float>(_destination_width) / 2.0F;
    const float destination_centre_y = static_cast<float>(_destination_height) / 2.0F;
    for (std::uint32_t row = 0; row < _destination_height; ++row) {
        for (std::uint32_t column = 0; column < _destination_width; ++column) {
            const float offset_x = static_cast<float>(column) - destination_centre_x;
            const float offset_y = static_cast<float>(row) - destination_centre_y;
            const float source_x = _position[0] +
                _affine[0] * offset_x + _affine[1] * offset_y;
            const float source_y = _position[1] +
                _affine[2] * offset_x + _affine[3] * offset_y;
            auto& output = _destination[
                static_cast<std::size_t>(row) * _destination_stride + column];
            if (source_x < 0.0F || source_y < 0.0F ||
                source_x >= static_cast<float>(_source_width - 1U) ||
                source_y >= static_cast<float>(_source_height - 1U)) {
                output = g_border_fill_value;
                continue;
            }
            const auto x0 = static_cast<std::uint32_t>(std::floor(source_x));
            const auto y0 = static_cast<std::uint32_t>(std::floor(source_y));
            const auto x1 = x0 + 1U;
            const auto y1 = y0 + 1U;
            const float x_fraction = source_x - static_cast<float>(x0);
            const float y_fraction = source_y - static_cast<float>(y0);
            const auto top_left = static_cast<float>(
                _source[static_cast<std::size_t>(y0) * _source_stride + x0]);
            const auto top_right = static_cast<float>(
                _source[static_cast<std::size_t>(y0) * _source_stride + x1]);
            const auto bottom_left = static_cast<float>(
                _source[static_cast<std::size_t>(y1) * _source_stride + x0]);
            const auto bottom_right = static_cast<float>(
                _source[static_cast<std::size_t>(y1) * _source_stride + x1]);
            const float top = top_left + (top_right - top_left) * x_fraction;
            const float bottom = bottom_left + (bottom_right - bottom_left) * x_fraction;
            const float interpolated = top + (bottom - top) * y_fraction;
            output = static_cast<std::uint8_t>(std::lround(
                std::max(0.0F, std::min(255.0F, interpolated))));
        }
    }
}

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
        descriptor.view_size_bytes_ == 0 || descriptor.height_ % 2 != 0 ||
        !vqec_vision_ai_qcom_fcaln_plane_fits(descriptor.offsets_[0],
            descriptor.strides_[0], descriptor.width_, descriptor.height_,
            descriptor.view_size_bytes_)) {
        return {status_code::invalid_argument,
            "alignment source luma plane is out of bounds"};
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

    const std::uint8_t* base = nullptr;
    void* mapping = nullptr;
    std::uint64_t map_length = 0;
    std::uint64_t in_page_offset = 0;
    dsp_buffer_mapping cached_mapping;

    if (config_.buffer_cache_ != nullptr) {
        status map_status;
        cached_mapping = config_.buffer_cache_->vqec_vision_ai_qcom_dspbc_map(
            static_cast<int>(_source.native_handle_),
            static_cast<std::size_t>(descriptor.allocation_size_bytes_), map_status);
        if (cached_mapping.data_ == nullptr || map_status.code_ != status_code::ok) {
            return map_status;
        }
        base = cached_mapping.data_ + descriptor.memory_offset_bytes_;
    } else {
        // Map the borrowed FD read-only, page-aligned. This is a CPU read, not zero-copy.
        const long page_size = ::sysconf(_SC_PAGESIZE);
        if (page_size <= 0) {
            return {status_code::io_error, "cannot determine page size"};
        }
        const std::uint64_t aligned_offset =
            (descriptor.memory_offset_bytes_ / static_cast<std::uint64_t>(page_size)) *
            static_cast<std::uint64_t>(page_size);
        in_page_offset = descriptor.memory_offset_bytes_ - aligned_offset;
        if (descriptor.view_size_bytes_ >
            std::numeric_limits<std::uint64_t>::max() - in_page_offset) {
            return {status_code::invalid_argument, "alignment source view length overflows"};
        }
        map_length = descriptor.view_size_bytes_ + in_page_offset;
        mapping = ::mmap(nullptr, static_cast<std::size_t>(map_length), PROT_READ,
            MAP_PRIVATE, static_cast<int>(_source.native_handle_),
            static_cast<off_t>(aligned_offset));
        if (mapping == MAP_FAILED) {
            return {status_code::io_error, "cannot map the alignment source frame"};
        }
        base = static_cast<const std::uint8_t*>(mapping);
    }
    const std::size_t luma_offset =
        static_cast<std::size_t>(in_page_offset + descriptor.offsets_[0]);
    const std::size_t patch_pixels = static_cast<std::size_t>(
        _template.destination_width_) * _template.destination_height_;

    // Crop the exact source region the aligned patch samples (plus a small interpolation
    // margin) so conversion and warp cost scale with the face, not the frame. Coordinates
    // are snapped to even values for NV12 chroma.
    const double patch_width = _template.destination_width_;
    const double patch_height = _template.destination_height_;
    double min_x = std::numeric_limits<double>::max();
    double min_y = std::numeric_limits<double>::max();
    double max_x = std::numeric_limits<double>::lowest();
    double max_y = std::numeric_limits<double>::lowest();
    const double corners[4][2] = {{0.0, 0.0}, {patch_width, 0.0},
        {patch_width, patch_height}, {0.0, patch_height}};
    for (const auto& corner : corners) {
        const double dx = corner[0] - patch_center_x;
        const double dy = corner[1] - patch_center_y;
        const double source_x = position[0] + inv00 * dx + inv01 * dy;
        const double source_y = position[1] + inv10 * dx + inv11 * dy;
        min_x = std::min(min_x, source_x);
        min_y = std::min(min_y, source_y);
        max_x = std::max(max_x, source_x);
        max_y = std::max(max_y, source_y);
    }
    std::uint32_t roi_x0 = 0;
    std::uint32_t roi_y0 = 0;
    std::uint32_t roi_x1 = 0;
    std::uint32_t roi_y1 = 0;
    vqec_vision_ai_qcom_fcaln_make_roi_bounds(min_x, max_x,
        _template.destination_width_, descriptor.width_, roi_x0, roi_x1);
    vqec_vision_ai_qcom_fcaln_make_roi_bounds(min_y, max_y,
        _template.destination_height_, descriptor.height_, roi_y0, roi_y1);
    if (roi_x1 <= roi_x0 || roi_y1 <= roi_y0) {
        if (mapping != nullptr) {
            ::munmap(mapping, static_cast<std::size_t>(map_length));
        }
        return {status_code::invalid_argument, "alignment source region is empty"};
    }
    const std::uint32_t roi_width = roi_x1 - roi_x0;
    const std::uint32_t roi_height = roi_y1 - roi_y0;
    alignas(16) float roi_position[2] = {position[0] - static_cast<float>(roi_x0),
        position[1] - static_cast<float>(roi_y0)};

    alignment_result candidate;
    if (config_.output_rgb_) {
        if ((config_.matrix_ != color_matrix::bt601 &&
                config_.matrix_ != color_matrix::bt709) ||
            (config_.range_ != color_range::limited && config_.range_ != color_range::full) ||
            !vqec_vision_ai_qcom_fcaln_plane_fits(descriptor.offsets_[1],
                descriptor.strides_[1], descriptor.width_, descriptor.height_ / 2U,
                descriptor.view_size_bytes_)) {
            if (mapping != nullptr) {
                ::munmap(mapping, static_cast<std::size_t>(map_length));
            }
            return {status_code::unsupported, "RGB alignment requires a valid NV12 color policy"};
        }
        const std::uint32_t fastcv_stride = (roi_width * 3U + 7U) & ~7U;
        const std::size_t rgb_size = static_cast<std::size_t>(fastcv_stride) * roi_height;
        if (rgb_scratch_.size() != rgb_size) {
            rgb_scratch_.resize(rgb_size);
        }
        const std::size_t chroma_offset =
            static_cast<std::size_t>(in_page_offset + descriptor.offsets_[1]);
        const std::uint8_t* y_roi = base + luma_offset +
            static_cast<std::size_t>(roi_y0) * descriptor.strides_[0] + roi_x0;
        const std::uint8_t* uv_roi = base + chroma_offset +
            static_cast<std::size_t>(roi_y0 / 2U) * descriptor.strides_[1] + roi_x0;

        fcvColorYCbCr420PseudoPlanarToRGB888u8(
            y_roi, uv_roi, roi_width, roi_height,
            static_cast<std::uint32_t>(descriptor.strides_[0]),
            static_cast<std::uint32_t>(descriptor.strides_[1]),
            rgb_scratch_.data(), fastcv_stride);
        // Deinterleave into three planar channels, warp each with the verified patch warp,
        // then interleave to the requested order.
        const std::size_t plane_size = static_cast<std::size_t>(roi_width) * roi_height;
        for (auto& plane : planes_scratch_) {
            if (plane.size() != plane_size) {
                plane.resize(plane_size);
            }
        }
        for (std::uint32_t row = 0; row < roi_height; ++row) {
            const std::uint8_t* source_row = rgb_scratch_.data() + static_cast<std::size_t>(row) * fastcv_stride;
            for (unsigned channel = 0; channel < 3U; ++channel) {
                std::uint8_t* plane_row = planes_scratch_[channel].data() +
                    static_cast<std::size_t>(row) * roi_width;
                for (std::uint32_t column = 0; column < roi_width; ++column) {
                    plane_row[column] =
                        source_row[static_cast<std::size_t>(column) * 3U + channel];
                }
            }
        }
        for (unsigned channel = 0; channel < 3U; ++channel) {
            if (patches_scratch_[channel].size() != patch_pixels) {
                patches_scratch_[channel].resize(patch_pixels);
            }
            const int warp = fcvTransformAffineu8_v2(planes_scratch_[channel].data(), roi_width,
                roi_height, roi_width, roi_position, affine, patches_scratch_[channel].data(),
                _template.destination_width_, _template.destination_height_,
                _template.destination_width_);
            if (warp != 0) {
                vqec_vision_ai_qcom_fcaln_warp_bilinear_fallback(
                    planes_scratch_[channel].data(), roi_width, roi_height, roi_width,
                    roi_position, affine, patches_scratch_[channel].data(),
                    _template.destination_width_, _template.destination_height_,
                    _template.destination_width_);
            }
        }
        const bool rgb_order = config_.order_ == channel_order::rgb;
        std::vector<std::uint8_t> packed(patch_pixels * 3U);
        for (std::size_t index = 0; index < patch_pixels; ++index) {
            packed[index * 3U + 0U] = rgb_order ? patches_scratch_[0][index] : patches_scratch_[2][index];
            packed[index * 3U + 1U] = patches_scratch_[1][index];
            packed[index * 3U + 2U] = rgb_order ? patches_scratch_[2][index] : patches_scratch_[0][index];
        }
        candidate.tensor_.spec_.dimensions_ = {1U, _template.destination_height_,
            _template.destination_width_, 3U};
        candidate.tensor_.bytes_ = std::move(packed);
    } else {
        const std::size_t plane_size = static_cast<std::size_t>(roi_width) * roi_height;
        if (luma_scratch_.size() != plane_size) {
            luma_scratch_.resize(plane_size);
        }
        for (std::uint32_t row = 0; row < roi_height; ++row) {
            std::memcpy(luma_scratch_.data() + static_cast<std::size_t>(row) * roi_width,
                base + luma_offset +
                    static_cast<std::size_t>(roi_y0 + row) * descriptor.strides_[0] + roi_x0,
                roi_width);
        }
        std::vector<std::uint8_t> patch(patch_pixels);
        const int result = fcvTransformAffineu8_v2(luma_scratch_.data(), roi_width, roi_height,
            roi_width, roi_position, affine, patch.data(), _template.destination_width_,
            _template.destination_height_, _template.destination_width_);
        if (result != 0) {
            vqec_vision_ai_qcom_fcaln_warp_bilinear_fallback(
                luma_scratch_.data(), roi_width, roi_height, roi_width, roi_position, affine,
                patch.data(), _template.destination_width_, _template.destination_height_,
                _template.destination_width_);
        }
        candidate.tensor_.spec_.dimensions_ = {1U, _template.destination_height_,
            _template.destination_width_, 1U};
        candidate.tensor_.bytes_ = std::move(patch);
    }
    if (mapping != nullptr) {
        ::munmap(mapping, static_cast<std::size_t>(map_length));
    }
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
