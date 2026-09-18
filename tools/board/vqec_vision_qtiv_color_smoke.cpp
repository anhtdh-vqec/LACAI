// Board-only smoke for the QtIV color-conversion offload: opens the FastCV/GLES NV12->RGB
// pipeline and converts a synthetic red frame; it prints the center RGB for inspection.

#include <cstdint>
#include <cstdio>
#include <vector>

#include "vqec_vision_qtiv_color.hpp"

using namespace vqec::vision::ai;

namespace {

void vqec_vision_ai_tools_qtcsm_fill_red(std::vector<std::uint8_t>& _nv12,
    unsigned _width, unsigned _height) {
    for (unsigned index = 0; index < _width * _height; ++index) {
        _nv12[index] = 81U;
    }
    for (unsigned index = _width * _height; index < _nv12.size(); ++index) {
        _nv12[index] = (index % 2U == 0U) ? 90U : 240U;
    }
}

}  // namespace

int main(int _argc, char** _argv) {
    const qtiv_color_engine engine = _argc > 1 && _argv[1][0] == 'g' ?
        qtiv_color_engine::gles : qtiv_color_engine::fcv;
    constexpr unsigned width = 64;
    constexpr unsigned height = 64;
    std::vector<std::uint8_t> nv12(width * height * 3U / 2U, 0U);
    vqec_vision_ai_tools_qtcsm_fill_red(nv12, width, height);

    qtiv_color_converter converter;
    const auto opened = converter.vqec_vision_ai_qcom_qtcol_open(
        engine, width, height, color_matrix::bt601, 2000000000ULL);
    std::printf("qtiv color open rc=%d\n", static_cast<int>(opened.code_));
    if (opened.code_ != status_code::ok) {
        return 1;
    }
    std::vector<std::uint8_t> rgb;
    const auto converted =
        converter.vqec_vision_ai_qcom_qtcol_convert(nv12.data(), rgb);
    std::printf("qtiv color convert rc=%d bytes=%zu msg=%s\n",
        static_cast<int>(converted.code_), rgb.size(), converted.message_.c_str());
    if (converted.code_ == status_code::ok && rgb.size() == width * height * 3U) {
        const std::size_t center = (height / 2U) * width + width / 2U;
        const std::uint8_t* pixel = rgb.data() + center * 3U;
        std::printf("qtiv color center=%u,%u,%u (expect ~254,0,0 for BT.601 red)\n", pixel[0],
            pixel[1], pixel[2]);
    }
    converter.vqec_vision_ai_qcom_qtcol_close();
    return 0;
}
