#include <iostream>
#include <string>

#include "vqec/vision/ai/contracts/media/vqec_vision_fw_ring_layout.hpp"

namespace ring = vqec::vision::ai::fw_ring_layout;

// Independent guard: these literals mirror the released FW reader, not the AI writer code.
// A mismatch must fail the build so a silent ring ABI drift cannot ship.
static_assert(ring::g_version == 5);
static_assert(ring::g_slot_count == 16);
static_assert(ring::g_payload_size == 1048576U);
static_assert(ring::g_header_size == 4096);
static_assert(ring::g_slot_header_size == 1232);
static_assert(ring::g_h_write_sequence == 32);
static_assert(ring::g_h_ring_id == 64);
static_assert(ring::g_s_seqlock == 0);
static_assert(ring::g_s_data_size == 8);
static_assert(ring::g_s_sequence == 104);
static_assert(ring::g_s_codec == 176);
static_assert(ring::g_s_h264_sps == 208);
static_assert(ring::g_s_h264_pps == 720);

int main() {
    unsigned failures = 0;
    const auto check = [&failures](bool _condition, const char* _what) {
        if (!_condition) {
            ++failures;
            std::cerr << "failed: " << _what << '\n';
        }
    };
    check(ring::vqec_vision_ai_cntr_fwrly_make_shm_path("encoded_ai_detect0_cam0_ch0") ==
            "/dev/shm/camera_ai_encoded_ai_detect0_cam0_ch0",
        "plain ring id");
    check(ring::vqec_vision_ai_cntr_fwrly_make_shm_path("a/b c:d") ==
            "/dev/shm/camera_ai_a_b_c_d",
        "sanitize separators like the FW reader");
    check(ring::vqec_vision_ai_cntr_fwrly_make_shm_path("keep._-") ==
            "/dev/shm/camera_ai_keep._-",
        "keep the FW-accepted character set");
    check(ring::vqec_vision_ai_cntr_fwrly_make_shm_path("") == "/dev/shm/camera_ai_",
        "empty ring id");
    std::cout << "fw ring layout failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
