#include <cassert>

#include "vqec_vision_service_shutdown.hpp"

using namespace vqec::vision::ai;

int main() {
    assert(vqec_vision_ai_appl_svshd_decide_exit(
        true, true, true, 1, true, status_code::ok, false, 1) == 0);
    assert(vqec_vision_ai_appl_svshd_decide_exit(
        true, true, true, 0, true, status_code::ok, false, 1) == 1);
    assert(vqec_vision_ai_appl_svshd_decide_exit(
        true, true, true, 1, false, status_code::ok, false, 1) == 1);
    assert(vqec_vision_ai_appl_svshd_decide_exit(
        true, true, true, 1, true, status_code::invalid_state, false, 1) == 1);
    assert(vqec_vision_ai_appl_svshd_decide_exit(
        true, true, true, 1, true, status_code::ok, true, 1) == 4);
    assert(vqec_vision_ai_appl_svshd_decide_exit(
        false, true, true, 1, true, status_code::ok, false, 1) == 5);
    assert(vqec_vision_ai_appl_svshd_decide_exit(
        true, false, true, 1, true, status_code::ok, false, 1) == 5);
    assert(vqec_vision_ai_appl_svshd_decide_exit(
        true, true, false, 1, true, status_code::ok, false, 1) == 5);
    return 0;
}
