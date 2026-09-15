// Board-only smoke to establish the FastCV affine warp convention used by
// fcvTransformAffineu8_v2: whether the 2x2 affine maps patch coordinates into the source
// (inverse warp) or the source into the patch around `position`, and how the patch is
// centered. It uses a synthetic single-channel image and prints the warped patch; it is not
// a test and never runs in the host/QEMU matrix.

#include <fastcv/fastcv.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <vector>

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
    return 0;
}
