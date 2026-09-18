// Device-free preprocess conformance suite for the CPU reference image processor. It is the
// golden oracle an accelerator implementation must match: solid colour, gradient, letterbox
// padding, strides/offsets, aspect ratios, channel order, normalization, quantization,
// clipping and invalid-input rejection. Explicit checks, not assert(), so it runs in Release.

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iostream>
#include <memory>
#include <vector>

#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#include "vqec_vision_reference_processor.hpp"

using namespace vqec::vision::ai;

namespace {

struct nv12_fixture {
    int fd_{-1};
    std::uint64_t allocation_bytes_{0};
    std::shared_ptr<int> owner_;
};

// Builds a memfd-backed NV12 plane set. y_at returns the luma; chroma is constant.
nv12_fixture make_nv12(std::uint32_t _width, std::uint32_t _height, std::int32_t _y_stride,
    std::int32_t _uv_stride, std::uint64_t _y_offset, std::uint64_t _uv_offset,
    std::uint8_t _u, std::uint8_t _v,
    const std::function<std::uint8_t(std::uint32_t, std::uint32_t)>& _y_at) {
    nv12_fixture fixture;
    const std::uint64_t y_end =
        _y_offset + static_cast<std::uint64_t>(_height) * _y_stride;
    const std::uint64_t uv_end =
        _uv_offset + static_cast<std::uint64_t>(_height / 2) * _uv_stride;
    fixture.allocation_bytes_ = (y_end > uv_end ? y_end : uv_end) + 4096U;
    fixture.fd_ = ::memfd_create("vqec_vision_ai_pconf", MFD_CLOEXEC);
    fixture.owner_ = std::make_shared<int>(0);
    if (fixture.fd_ < 0 ||
        ::ftruncate(fixture.fd_, static_cast<off_t>(fixture.allocation_bytes_)) != 0) {
        return fixture;
    }
    void* base = ::mmap(nullptr, fixture.allocation_bytes_, PROT_READ | PROT_WRITE,
                        MAP_SHARED, fixture.fd_, 0);
    if (base == MAP_FAILED) {
        fixture.fd_ = -1;
        return fixture;
    }
    auto* bytes = static_cast<std::uint8_t*>(base);
    std::memset(bytes, 0, fixture.allocation_bytes_);
    for (std::uint32_t y = 0; y < _height; ++y) {
        for (std::uint32_t x = 0; x < _width; ++x) {
            bytes[_y_offset + static_cast<std::uint64_t>(y) * _y_stride + x] = _y_at(x, y);
        }
    }
    for (std::uint32_t y = 0; y < _height / 2; ++y) {
        for (std::uint32_t x = 0; x < _width / 2; ++x) {
            bytes[_uv_offset + static_cast<std::uint64_t>(y) * _uv_stride + x * 2] = _u;
            bytes[_uv_offset + static_cast<std::uint64_t>(y) * _uv_stride + x * 2 + 1] = _v;
        }
    }
    ::munmap(base, fixture.allocation_bytes_);
    return fixture;
}

raw_frame make_frame(const nv12_fixture& _fixture, std::uint32_t _width,
    std::uint32_t _height, std::int32_t _y_stride, std::int32_t _uv_stride,
    std::uint64_t _y_offset, std::uint64_t _uv_offset) {
    raw_frame frame;
    frame.descriptor_.width_ = _width;
    frame.descriptor_.height_ = _height;
    frame.descriptor_.offsets_ = {static_cast<std::uint32_t>(_y_offset),
        static_cast<std::uint32_t>(_uv_offset)};
    frame.descriptor_.strides_ = {_y_stride, _uv_stride};
    frame.descriptor_.view_size_bytes_ = _fixture.allocation_bytes_ - 4096U + 1U;
    frame.descriptor_.memory_offset_bytes_ = 0;
    frame.descriptor_.allocation_size_bytes_ = _fixture.allocation_bytes_;
    frame.native_handle_ = _fixture.fd_;
    frame.owner_ = _fixture.owner_;
    return frame;
}

inference_plan make_plan(std::uint32_t _sw, std::uint32_t _sh, std::uint32_t _tw,
    std::uint32_t _th, image_placement _placement, channel_order _order) {
    inference_plan plan;
    plan.source_width_ = _sw;
    plan.source_height_ = _sh;
    plan.fps_numerator_ = 25;
    plan.fps_denominator_ = 1;
    plan.tensor_width_ = _tw;
    plan.tensor_height_ = _th;
    plan.input_type_ = tensor_element_type::float32;
    plan.channel_order_ = _order;
    plan.placement_ = _placement;
    plan.mean_ = {0.0, 0.0, 0.0};
    plan.sigma_ = {1.0, 1.0, 1.0};
    plan.model_path_ = "/opt/vqec/models/model.bin";
    plan.backend_path_ = "/usr/lib/libQnnHtp.so";
    plan.system_path_ = "/usr/lib/libQnnSystem.so";
    plan.input_queue_bytes_ = 4096;
    plan.output_queue_buffers_ = 1;
    return plan;
}

tensor_spec make_target(std::uint32_t _w, std::uint32_t _h, tensor_element_type _dtype) {
    tensor_spec spec;
    spec.name_ = "input";
    spec.dimensions_ = {1, _h, _w, 3};
    spec.dtype_ = _dtype;
    return spec;
}

float float_at(const tensor_blob& _blob, std::size_t _index) {
    float value = 0.0F;
    std::memcpy(&value, _blob.bytes_.data() + _index * sizeof(float), sizeof(value));
    return value;
}

bool preprocess_ok(reference_image_processor& _processor, const raw_frame& _frame,
    const inference_plan& _plan, const tensor_spec& _target, tensor_blob& _blob) {
    std::vector<tensor_blob> outputs;
    if (_processor.vqec_vision_ai_ports_imgpr_preprocess(
            _frame, _plan, _target, outputs).code_ != status_code::ok ||
        outputs.size() != 1) {
        return false;
    }
    _blob = std::move(outputs[0]);
    return true;
}

}  // namespace

int main() {
    unsigned failures = 0;
    const auto check = [&failures](bool _condition) {
        if (!_condition) {
            ++failures;
        }
    };
    reference_image_processor processor;

    // Solid gray Y=128, neutral chroma -> BT.601 limited 1.164*(128-16)=130.368 -> 130.
    {
        const auto fixture = make_nv12(16, 16, 16, 16, 0, 256, 128, 128,
            [](std::uint32_t, std::uint32_t) { return std::uint8_t{128}; });
        check(fixture.fd_ >= 0);
        const auto frame = make_frame(fixture, 16, 16, 16, 16, 0, 256);
        const auto plan = make_plan(16, 16, 8, 8, image_placement::centre, channel_order::rgb);
        tensor_blob blob;
        check(preprocess_ok(processor, frame, plan, make_target(8, 8, tensor_element_type::float32), blob));
        check(blob.bytes_.size() == 8U * 8U * 3U * 4U);
        check(float_at(blob, 0) == 130.0F);
        check(float_at(blob, 1) == 130.0F && float_at(blob, 2) == 130.0F);
        ::close(fixture.fd_);
    }

    // A caller that keeps one output vector per binding reuses its storage instead of
    // reallocating: the backing pointer must be unchanged after preprocessing.
    {
        const auto fixture = make_nv12(16, 16, 16, 16, 0, 256, 128, 128,
            [](std::uint32_t, std::uint32_t) { return std::uint8_t{128}; });
        const auto frame = make_frame(fixture, 16, 16, 16, 16, 0, 256);
        const auto plan = make_plan(16, 16, 8, 8, image_placement::centre, channel_order::rgb);
        const auto target = make_target(8, 8, tensor_element_type::float32);
        std::vector<tensor_blob> outputs(1);
        outputs[0].spec_ = target;
        outputs[0].bytes_.assign(8U * 8U * 3U * 4U, 0U);
        const auto* storage_before = outputs[0].bytes_.data();
        check(processor.vqec_vision_ai_ports_imgpr_preprocess(
                  frame, plan, target, outputs).code_ == status_code::ok);
        check(outputs.size() == 1 && outputs[0].bytes_.data() == storage_before);
        check(float_at(outputs[0], 0) == 130.0F);
        ::close(fixture.fd_);
    }

    // Horizontal gradient: luma increases left to right and is sampled monotonically.
    {
        const auto fixture = make_nv12(16, 16, 16, 16, 0, 256, 128, 128,
            [](std::uint32_t _x, std::uint32_t) {
                return static_cast<std::uint8_t>(16 + _x * 8);
            });
        const auto frame = make_frame(fixture, 16, 16, 16, 16, 0, 256);
        const auto plan = make_plan(16, 16, 8, 8, image_placement::centre, channel_order::rgb);
        tensor_blob blob;
        check(preprocess_ok(processor, frame, plan, make_target(8, 8, tensor_element_type::float32), blob));
        float previous = -1.0F;
        bool monotonic = true;
        for (std::uint32_t x = 0; x < 8; ++x) {
            const float value = float_at(blob, (static_cast<std::size_t>(x) * 3U));
            monotonic = monotonic && value > previous;
            previous = value;
        }
        check(monotonic);
        ::close(fixture.fd_);
    }

    // Wide source letterboxes top/bottom; the pad rows are black (no content sampled).
    {
        const auto fixture = make_nv12(16, 8, 16, 16, 0, 128, 128, 128,
            [](std::uint32_t, std::uint32_t) { return std::uint8_t{235}; });
        const auto frame = make_frame(fixture, 16, 8, 16, 16, 0, 128);
        const auto plan = make_plan(16, 8, 8, 8, image_placement::centre, channel_order::rgb);
        tensor_blob blob;
        check(preprocess_ok(processor, frame, plan, make_target(8, 8, tensor_element_type::float32), blob));
        check(float_at(blob, 0) == 0.0F);  // top pad row
        check(float_at(blob, static_cast<std::size_t>(3U * 8U * 3U)) > 250.0F);  // content
        ::close(fixture.fd_);
    }

    // Tall source letterboxes left/right.
    {
        const auto fixture = make_nv12(8, 16, 8, 8, 0, 64, 128, 128,
            [](std::uint32_t, std::uint32_t) { return std::uint8_t{235}; });
        const auto frame = make_frame(fixture, 8, 16, 8, 8, 0, 64);
        const auto plan = make_plan(8, 16, 8, 8, image_placement::centre, channel_order::rgb);
        tensor_blob blob;
        check(preprocess_ok(processor, frame, plan, make_target(8, 8, tensor_element_type::float32), blob));
        check(float_at(blob, 0) == 0.0F);  // left pad column of row 0
        check(float_at(blob, static_cast<std::size_t>(2U * 3U)) > 250.0F);  // content column
        ::close(fixture.fd_);
    }

    // Non-default stride and non-zero plane offsets are honoured.
    {
        const auto fixture = make_nv12(16, 16, 24, 24, 64, 64 + 24 * 16, 128, 128,
            [](std::uint32_t, std::uint32_t) { return std::uint8_t{128}; });
        const auto frame = make_frame(fixture, 16, 16, 24, 24, 64, 64 + 24 * 16);
        const auto plan = make_plan(16, 16, 8, 8, image_placement::centre, channel_order::rgb);
        tensor_blob blob;
        check(preprocess_ok(processor, frame, plan, make_target(8, 8, tensor_element_type::float32), blob));
        check(float_at(blob, 0) == 130.0F);
        ::close(fixture.fd_);
    }

    // Channel order: a saturated chroma pixel yields R>B for RGB and B>R for BGR.
    {
        const auto fixture = make_nv12(8, 8, 8, 8, 0, 64, 90, 200,
            [](std::uint32_t _x, std::uint32_t _y) {
                return (_x == 2 && _y == 2) ? std::uint8_t{200} : std::uint8_t{16};
            });
        const auto frame = make_frame(fixture, 8, 8, 8, 8, 0, 64);
        const auto pixel_base = static_cast<std::size_t>((2U * 8U + 2U) * 3U);
        tensor_blob rgb;
        check(preprocess_ok(processor, frame,
            make_plan(8, 8, 8, 8, image_placement::centre, channel_order::rgb),
            make_target(8, 8, tensor_element_type::float32), rgb));
        check(float_at(rgb, pixel_base) > float_at(rgb, pixel_base + 2U));
        tensor_blob bgr;
        check(preprocess_ok(processor, frame,
            make_plan(8, 8, 8, 8, image_placement::centre, channel_order::bgr),
            make_target(8, 8, tensor_element_type::float32), bgr));
        check(float_at(bgr, pixel_base + 2U) > float_at(bgr, pixel_base));
        ::close(fixture.fd_);
    }

    // Normalization mean/sigma scales the pixel value.
    {
        const auto fixture = make_nv12(8, 8, 8, 8, 0, 64, 128, 128,
            [](std::uint32_t, std::uint32_t) { return std::uint8_t{128}; });
        const auto frame = make_frame(fixture, 8, 8, 8, 8, 0, 64);
        auto plan = make_plan(8, 8, 8, 8, image_placement::centre, channel_order::rgb);
        plan.mean_ = {30.0, 30.0, 30.0};
        plan.sigma_ = {2.0, 2.0, 2.0};
        tensor_blob blob;
        check(preprocess_ok(processor, frame, plan, make_target(8, 8, tensor_element_type::float32), blob));
        check(float_at(blob, 0) == (130.0F - 30.0F) * 2.0F);
        ::close(fixture.fd_);
    }

    // uint8 quantization: real/scale + zero_point.
    {
        const auto fixture = make_nv12(8, 8, 8, 8, 0, 64, 128, 128,
            [](std::uint32_t, std::uint32_t) { return std::uint8_t{128}; });
        const auto frame = make_frame(fixture, 8, 8, 8, 8, 0, 64);
        const auto plan = make_plan(8, 8, 8, 8, image_placement::centre, channel_order::rgb);
        auto target = make_target(8, 8, tensor_element_type::uint8);
        target.quantization_ = {true, 0.5F, 10};
        tensor_blob blob;
        check(preprocess_ok(processor, frame, plan, target, blob));
        // 130.368 / 0.5 + 10 = 270.7 -> clamp to 255 as real/scale before cast? The
        // reference quantizes then clamps into the dtype range.
        check(blob.bytes_[0] == 255U);
        ::close(fixture.fd_);
    }

    // int8 quantization keeps signed values in range.
    {
        const auto fixture = make_nv12(8, 8, 8, 8, 0, 64, 128, 128,
            [](std::uint32_t, std::uint32_t) { return std::uint8_t{64}; });
        const auto frame = make_frame(fixture, 8, 8, 8, 8, 0, 64);
        const auto plan = make_plan(8, 8, 8, 8, image_placement::centre, channel_order::rgb);
        auto target = make_target(8, 8, tensor_element_type::int8);
        target.quantization_ = {true, 2.0F, 0};
        tensor_blob blob;
        check(preprocess_ok(processor, frame, plan, target, blob));
        // Y=64 -> 1.164*(64-16)=55.872; /2 = 27.936 -> 27.
        check(static_cast<std::int8_t>(blob.bytes_[0]) == 27);
        ::close(fixture.fd_);
    }

    // Clipping: white stays <=255 and black stays >=0.
    {
        auto white_fixture = make_nv12(8, 8, 8, 8, 0, 64, 128, 128,
            [](std::uint32_t, std::uint32_t) { return std::uint8_t{235}; });
        auto white_frame = make_frame(white_fixture, 8, 8, 8, 8, 0, 64);
        const auto plan = make_plan(8, 8, 8, 8, image_placement::centre, channel_order::rgb);
        tensor_blob white;
        check(preprocess_ok(processor, white_frame, plan, make_target(8, 8, tensor_element_type::float32), white));
        check(float_at(white, 0) > 250.0F && float_at(white, 0) <= 255.0F);
        ::close(white_fixture.fd_);

        auto black_fixture = make_nv12(8, 8, 8, 8, 0, 64, 128, 128,
            [](std::uint32_t, std::uint32_t) { return std::uint8_t{16}; });
        auto black_frame = make_frame(black_fixture, 8, 8, 8, 8, 0, 64);
        tensor_blob black;
        check(preprocess_ok(processor, black_frame, plan, make_target(8, 8, tensor_element_type::float32), black));
        check(float_at(black, 0) == 0.0F);
        ::close(black_fixture.fd_);
    }

    // Invalid inputs are rejected, not silently accepted.
    {
        const auto fixture = make_nv12(16, 16, 16, 16, 0, 256, 128, 128,
            [](std::uint32_t, std::uint32_t) { return std::uint8_t{128}; });
        const auto plan = make_plan(16, 16, 8, 8, image_placement::centre, channel_order::rgb);
        const auto target = make_target(8, 8, tensor_element_type::float32);

        auto odd = make_frame(fixture, 17, 16, 16, 16, 0, 256);
        check(processor.vqec_vision_ai_ports_imgpr_validate(odd, plan, target).code_ ==
              status_code::invalid_argument);
        auto short_stride = make_frame(fixture, 16, 16, 8, 16, 0, 256);
        check(processor.vqec_vision_ai_ports_imgpr_validate(
                  short_stride, plan, target).code_ == status_code::invalid_argument);
        auto no_fd = make_frame(fixture, 16, 16, 16, 16, 0, 256);
        no_fd.native_handle_ = -1;
        check(processor.vqec_vision_ai_ports_imgpr_validate(no_fd, plan, target).code_ ==
              status_code::unsupported);
        check(processor.vqec_vision_ai_ports_imgpr_validate(
                  make_frame(fixture, 16, 16, 16, 16, 0, 256), plan,
                  make_target(8, 8, tensor_element_type::unknown)).code_ ==
              status_code::unsupported);
        auto bad_rank = target;
        bad_rank.dimensions_ = {1, 3, 8, 8};
        check(processor.vqec_vision_ai_ports_imgpr_validate(
                  make_frame(fixture, 16, 16, 16, 16, 0, 256), plan, bad_rank).code_ ==
              status_code::unsupported);
        ::close(fixture.fd_);
    }

    // Spec-driven colorimetry, pad and normalization (Gate 0). Color matrix must be data: a
    // BT.601 source converted with BT.709 coefficients (or vice versa) changes the result.
    {
        preprocess_spec spec;
        spec.source_format_ = source_pixel_format::nv12;
        spec.matrix_ = color_matrix::bt601;
        spec.range_ = color_range::limited;
        spec.resize_ = resize_mode::letterbox;
        spec.interpolation_ = interpolation_mode::nearest;
        spec.placement_ = image_placement::centre;
        spec.pad_value_ = {0.0F, 0.0F, 0.0F};
        spec.channels_ = channel_order::rgb;
        spec.normalization_ = normalization_formula::offset_scale;
        spec.offset_ = {0.0F, 0.0F, 0.0F};
        spec.scale_ = {1.0F / 255.0F, 1.0F / 255.0F, 1.0F / 255.0F};
        spec.coordinates_ = coordinate_convention::tensor_pixels_xywh;

        const auto chroma_fixture = make_nv12(8, 8, 8, 8, 0, 64, 90, 200,
            [](std::uint32_t _x, std::uint32_t _y) {
                return (_x == 2 && _y == 2) ? std::uint8_t{200} : std::uint8_t{16};
            });
        const auto chroma_frame = make_frame(chroma_fixture, 8, 8, 8, 8, 0, 64);
        const auto pixel_base = static_cast<std::size_t>((2U * 8U + 2U) * 3U);
        auto plan601 = make_plan(8, 8, 8, 8, image_placement::centre, channel_order::rgb);
        plan601.preprocess_ = spec;
        auto plan709 = plan601;
        plan709.preprocess_.matrix_ = color_matrix::bt709;
        tensor_blob b601;
        tensor_blob b709;
        check(preprocess_ok(processor, chroma_frame, plan601,
            make_target(8, 8, tensor_element_type::float32), b601));
        check(preprocess_ok(processor, chroma_frame, plan709,
            make_target(8, 8, tensor_element_type::float32), b709));
        // Compare the blue channel: red saturates under both matrices for this pixel.
        check(float_at(b601, pixel_base + 2U) != float_at(b709, pixel_base + 2U));

        // Pad value is data (normalization none => stored value equals the pad value).
        auto pad_spec = spec;
        pad_spec.resize_ = resize_mode::letterbox;
        pad_spec.normalization_ = normalization_formula::none;
        pad_spec.pad_value_ = {114.0F, 114.0F, 114.0F};
        const auto wide_fixture = make_nv12(16, 8, 16, 16, 0, 128, 128, 128,
            [](std::uint32_t, std::uint32_t) { return std::uint8_t{235}; });
        const auto wide_frame = make_frame(wide_fixture, 16, 8, 16, 16, 0, 128);
        auto pad_plan = make_plan(16, 8, 8, 8, image_placement::centre, channel_order::rgb);
        pad_plan.preprocess_ = pad_spec;
        tensor_blob padded;
        check(preprocess_ok(processor, wide_frame, pad_plan,
            make_target(8, 8, tensor_element_type::float32), padded));
        check(float_at(padded, 0) == 114.0F);

        // offset_scale formula on a neutral gray pixel: (130 - 0) * (1/255).
        const auto gray_fixture = make_nv12(8, 8, 8, 8, 0, 64, 128, 128,
            [](std::uint32_t, std::uint32_t) { return std::uint8_t{128}; });
        const auto gray_frame = make_frame(gray_fixture, 8, 8, 8, 8, 0, 64);
        auto gray_plan = make_plan(8, 8, 8, 8, image_placement::centre, channel_order::rgb);
        gray_plan.preprocess_ = spec;
        tensor_blob normalized;
        check(preprocess_ok(processor, gray_frame, gray_plan,
            make_target(8, 8, tensor_element_type::float32), normalized));
        check(std::abs(float_at(normalized, 0) - (130.0F / 255.0F)) < 0.001F);

        ::close(chroma_fixture.fd_);
        ::close(wide_fixture.fd_);
        ::close(gray_fixture.fd_);
    }

    std::cout << "preprocess conformance failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
