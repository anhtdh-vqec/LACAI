#include <cassert>
#include <cstdint>

#include "vqec_vision_service_execution_loop.hpp"

using namespace vqec::vision::ai;

int main() {
    std::uint32_t phase = 0;
    bool initialized = false;
    std::uint32_t selected = 0;
    for (std::uint32_t frame = 0; frame < 60; ++frame) {
        if (vqec_vision_ai_appl_svxlp_select_preview(
                30, 15, phase, initialized)) {
            ++selected;
        }
    }
    assert(selected == 31);

    phase = 0;
    initialized = false;
    selected = 0;
    for (std::uint32_t frame = 0; frame < 60; ++frame) {
        if (vqec_vision_ai_appl_svxlp_select_preview(
                30, 25, phase, initialized)) {
            ++selected;
        }
    }
    assert(selected == 51);

    phase = 0;
    initialized = false;
    assert(vqec_vision_ai_appl_svxlp_select_preview(
        30, 30, phase, initialized));
    assert(!initialized);

    assert(vqec_vision_ai_appl_svxlp_is_source_replacement(
        status_code::invalid_state, true, status_code::source_lost));
    assert(!vqec_vision_ai_appl_svxlp_is_source_replacement(
        status_code::invalid_state, true, status_code::timeout));
    assert(!vqec_vision_ai_appl_svxlp_is_source_replacement(
        status_code::source_lost, false, status_code::source_lost));

    service_execution_context incomplete;
    service_execution_result result;
    const auto rejected =
        vqec_vision_ai_appl_svxlp_run(incomplete, result);
    assert(rejected.code_ == status_code::invalid_argument);
    return 0;
}
