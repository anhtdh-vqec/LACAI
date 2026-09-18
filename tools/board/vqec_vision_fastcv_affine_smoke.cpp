// Board-only smoke to establish the FastCV affine warp convention used by
// fcvTransformAffineu8_v2: whether the 2x2 affine maps patch coordinates into the source
// (inverse warp) or the source into the patch around `position`, and how the patch is
// centered. It uses a synthetic single-channel image and prints the warped patch; it is not
// a test and never runs in the host/QEMU matrix.

#define _GNU_SOURCE 1
#include <fastcv/fastcv.h>

#include <sys/mman.h>
#include <unistd.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <vector>

#include "vqec_vision_fastcv_aligner.hpp"

namespace {

void vqec_vision_ai_tools_fasmy_print_patch(const char* _label,
    const std::uint8_t* _patch, unsigned _width, unsigned _height, unsigned _stride) {
    std::printf("%s\n", _label);
    for (unsigned y = 0; y < _height; ++y) {
        for (unsigned x = 0; x < _width; ++x) {
            std::printf("%3u ", _patch[y * _stride + x]);
        }
        std::printf("\n");
    }
}

}  // namespace

int main() {
    // src(x, y) = x * 4 so each column is a distinct value; marker at (32, 32).
    constexpr unsigned width = 64;
    constexpr unsigned height = 64;
    constexpr unsigned patch_width = 8;
    constexpr unsigned patch_height = 8;
    std::vector<std::uint8_t> source(width * height);
    for (unsigned y = 0; y < height; ++y) {
        for (unsigned x = 0; x < width; ++x) {
            source[y * width + x] = static_cast<std::uint8_t>(x * 4);
        }
    }
    source[32 * width + 32] = 255;

    alignas(16) float position[2] = {32.0F, 32.0F};
    alignas(16) float identity[4] = {1.0F, 0.0F, 0.0F, 1.0F};
    alignas(16) float rotate_ccw[4] = {0.0F, -1.0F, 1.0F, 0.0F};
    alignas(16) float scale_half[4] = {0.5F, 0.0F, 0.0F, 0.5F};
    std::vector<std::uint8_t> patch(patch_width * patch_height, 0U);

    std::printf("source: src(x,y)=x*4, marker(32,32)=255, patch 8x8 at position(32,32)\n");
    std::printf("src row 32, cols 28..36: ");
    for (unsigned x = 28; x <= 36; ++x) {
        std::printf("%u ", source[32 * width + x]);
    }
    std::printf("\n");

    int result = fcvTransformAffineu8_v2(source.data(), width, height, width, position,
        identity, patch.data(), patch_width, patch_height, patch_width);
    std::printf("identity rc=%d\n", result);
    vqec_vision_ai_tools_fasmy_print_patch(
        "identity patch", patch.data(), patch_width, patch_height, patch_width);

    std::fill(patch.begin(), patch.end(), 0U);
    result = fcvTransformAffineu8_v2(source.data(), width, height, width, position,
        rotate_ccw, patch.data(), patch_width, patch_height, patch_width);
    std::printf("rotate_ccw rc=%d\n", result);
    vqec_vision_ai_tools_fasmy_print_patch(
        "rotate_ccw patch", patch.data(), patch_width, patch_height, patch_width);

    std::fill(patch.begin(), patch.end(), 0U);
    result = fcvTransformAffineu8_v2(source.data(), width, height, width, position,
        scale_half, patch.data(), patch_width, patch_height, patch_width);
    std::printf("scale_half rc=%d\n", result);
    vqec_vision_ai_tools_fasmy_print_patch(
        "scale_half patch", patch.data(), patch_width, patch_height, patch_width);

    // End-to-end adapter call on a synthetic NV12 luma frame in a memfd. The template maps
    // source pixels (28..35)^2 to destination (0..7)^2, i.e. an identity warp centered at
    // source (32, 32); the marker must land at the patch center.
    using namespace vqec::vision::ai;
    const int fd = ::memfd_create("lacai-align", 0);
    if (fd < 0) {
        std::printf("memfd_create failed\n");
        return 1;
    }
    const unsigned nv12_bytes = width * height * 3U / 2U;
    std::vector<std::uint8_t> nv12(nv12_bytes, 128U);
    for (unsigned y = 0; y < height; ++y) {
        for (unsigned x = 0; x < width; ++x) {
            nv12[y * width + x] = static_cast<std::uint8_t>(x * 4);
        }
    }
    nv12[32 * width + 32] = 255;
    if (::write(fd, nv12.data(), nv12_bytes) != static_cast<ssize_t>(nv12_bytes)) {
        std::printf("memfd write failed\n");
        return 1;
    }
    raw_frame frame;
    frame.descriptor_.width_ = width;
    frame.descriptor_.height_ = height;
    frame.descriptor_.offsets_ = {0U, width * height};
    frame.descriptor_.strides_ = {static_cast<std::int32_t>(width),
        static_cast<std::int32_t>(width)};
    frame.descriptor_.view_size_bytes_ = nv12_bytes;
    frame.descriptor_.allocation_size_bytes_ = nv12_bytes;
    frame.descriptor_.memory_offset_bytes_ = 0;
    frame.native_handle_ = fd;
    frame.owner_ = std::make_shared<int>(0);

    alignment_template align_template;
    align_template.schema_id_ = "face.5pt";
    align_template.schema_version_ = "1";
    align_template.destination_width_ = patch_width;
    align_template.destination_height_ = patch_height;
    align_template.reference_points_ = {{0.0F, 0.0F}, {8.0F, 0.0F}, {8.0F, 8.0F}, {0.0F, 8.0F}};
    alignment_request align_request;
    align_request.frame_ = {0U, 0U, 1U, 1U, 1000U};
    align_request.landmarks_.schema_id_ = "face.5pt";
    align_request.landmarks_.schema_version_ = "1";
    align_request.landmarks_.points_ = {
        {28.0F, 28.0F}, {36.0F, 28.0F}, {36.0F, 36.0F}, {28.0F, 36.0F}};

    fastcv_aligner aligner;
    alignment_capabilities capabilities;
    (void)aligner.vqec_vision_ai_ports_imaln_probe_capabilities(capabilities);
    const auto template_status =
        aligner.vqec_vision_ai_ports_imaln_validate_template(align_template, capabilities);
    alignment_result align_result;
    std::uint64_t ticket = 0;
    const auto align_status = aligner.vqec_vision_ai_ports_imaln_align(
        align_request, frame, align_template, align_result, ticket);
    bool complete = false;
    (void)aligner.vqec_vision_ai_ports_imaln_poll_completion(ticket, complete);
    std::printf("aligner template_rc=%d align_rc=%d ticket=%llu bytes=%zu complete=%d\n",
        static_cast<int>(template_status.code_), static_cast<int>(align_status.code_),
        static_cast<unsigned long long>(ticket),
        align_result.tensor_.bytes_.size(), complete ? 1 : 0);
    if (align_status.code_ == status_code::ok &&
        align_result.tensor_.bytes_.size() == patch_width * patch_height) {
        vqec_vision_ai_tools_fasmy_print_patch("adapter patch", align_result.tensor_.bytes_.data(),
            patch_width, patch_height, patch_width);
        std::printf("adapter center marker=%u (expect 255), center=%u\n",
            align_result.tensor_.bytes_[4 * patch_width + 4],
            align_result.tensor_.bytes_[(patch_height / 2) * patch_width + patch_width / 2]);
    }
    // RGB path: a uniform BT.601 limited red NV12 must align to a red RGB patch.
    for (unsigned index = 0; index < width * height; ++index) {
        nv12[index] = 81U;
    }
    for (unsigned index = width * height; index < nv12_bytes; ++index) {
        nv12[index] = (index % 2U == 0U) ? 90U : 240U;
    }
    if (::pwrite(fd, nv12.data(), nv12_bytes, 0) != static_cast<ssize_t>(nv12_bytes)) {
        std::printf("memfd pwrite failed\n");
        return 1;
    }
    fastcv_aligner_config rgb_config;
    rgb_config.output_rgb_ = true;
    rgb_config.matrix_ = color_matrix::bt601;
    rgb_config.range_ = color_range::limited;
    rgb_config.order_ = channel_order::rgb;
    fastcv_aligner rgb_aligner(rgb_config);
    alignment_result rgb_result;
    std::uint64_t rgb_ticket = 0;
    const auto rgb_status = rgb_aligner.vqec_vision_ai_ports_imaln_align(
        align_request, frame, align_template, rgb_result, rgb_ticket);
    std::printf("aligner rgb_rc=%d bytes=%zu\n", static_cast<int>(rgb_status.code_),
        rgb_result.tensor_.bytes_.size());
    if (rgb_status.code_ == status_code::ok &&
        rgb_result.tensor_.bytes_.size() == patch_width * patch_height * 3U) {
        const unsigned center = (patch_height / 2U) * patch_width + patch_width / 2U;
        const std::uint8_t* pixel = rgb_result.tensor_.bytes_.data() + center * 3U;
        std::printf("aligner rgb center=%u,%u,%u (expect ~254,0,0)\n", pixel[0], pixel[1],
            pixel[2]);
    }

    ::close(fd);
    return 0;
}
