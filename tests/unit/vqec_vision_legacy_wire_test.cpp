#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>

#include "vqec_vision_legacy_wire.hpp"

namespace wire = vqec::vision::ai::legacy_wire_layout;
static_assert(wire::g_header_bytes == 104);
static_assert(wire::g_width_offset == 8 && wire::g_height_offset == 12);
static_assert(wire::g_format_offset == 16 && wire::g_plane_count_offset == 20);
static_assert(wire::g_plane_offsets_offset == 24 && wire::g_plane_strides_offset == 40);
static_assert(wire::g_view_size_offset == 56 && wire::g_memory_offset_offset == 64);
static_assert(wire::g_allocation_size_offset == 72 && wire::g_pts_offset == 80);
static_assert(wire::g_dts_offset == 88 && wire::g_duration_offset == 96);
static_assert(wire::g_plane_field_bytes == 4 && wire::g_nv12_planes == 2);

namespace {

template <typename value_type>
void vqec_vision_ai_unit_lwtst_put_field(
    std::array<std::uint8_t, 104>& _packet, std::size_t _offset, value_type _value) {
    std::memcpy(_packet.data() + _offset, &_value, sizeof(_value));
}

std::array<std::uint8_t, 104> vqec_vision_ai_unit_lwtst_make_packet(
    std::uint32_t _width, std::uint32_t _height, std::uint32_t _stride) {
    std::array<std::uint8_t, 104> packet{};
    // Synthetic enum value tests compatibility binding, not the installed Gst ABI.
    vqec_vision_ai_unit_lwtst_put_field(packet, 0, std::uint64_t{7});
    vqec_vision_ai_unit_lwtst_put_field(packet, 8, _width);
    vqec_vision_ai_unit_lwtst_put_field(packet, 12, _height);
    vqec_vision_ai_unit_lwtst_put_field(packet, 16, std::uint32_t{123});
    vqec_vision_ai_unit_lwtst_put_field(packet, 20, std::uint32_t{2});
    vqec_vision_ai_unit_lwtst_put_field(packet, 28, _stride * _height);
    vqec_vision_ai_unit_lwtst_put_field(packet, 40, static_cast<std::int32_t>(_stride));
    vqec_vision_ai_unit_lwtst_put_field(packet, 44, static_cast<std::int32_t>(_stride));
    const auto size = static_cast<std::uint64_t>(_stride) * _height * 3 / 2;
    vqec_vision_ai_unit_lwtst_put_field(packet, 56, size);
    vqec_vision_ai_unit_lwtst_put_field(packet, 64, std::uint64_t{4096});
    vqec_vision_ai_unit_lwtst_put_field(packet, 72, size + 4096);
    vqec_vision_ai_unit_lwtst_put_field(packet, 80, UINT64_MAX);
    return packet;
}

}  // namespace

int main() {
    using vqec::vision::ai::frame_descriptor;
    using vqec::vision::ai::legacy_frame_limits;
    using vqec::vision::ai::status_code;
    using vqec::vision::ai::vqec_vision_ai_camer_lwire_decode_frame;
    legacy_frame_limits limits;
    limits.nv12_format_value_ = 123;
    limits.max_width_ = 3840;
    limits.max_height_ = 2160;
    limits.max_allocation_bytes_ = 4096ULL * 2160 * 3 / 2 + 4096;
    frame_descriptor output;
    unsigned failures = 0;
    const auto check = [&failures](bool _condition) {
        if (!_condition) {
            ++failures;
        }
    };
    for (const auto width : {854U, 1920U, 2560U, 3840U}) {
        auto packet = vqec_vision_ai_unit_lwtst_make_packet(width, 480, width + 64);
        check(vqec_vision_ai_camer_lwire_decode_frame(
            packet.data(), packet.size(), limits, output).code_ == status_code::ok);
        check(output.width_ == width && output.memory_offset_bytes_ == 4096 &&
              output.pts_ns_ == UINT64_MAX && output.strides_[0] ==
                  static_cast<std::int32_t>(width + 64));
    }
    const auto valid = vqec_vision_ai_unit_lwtst_make_packet(3840, 2160, 4096);
    // Unset policy must fail without publishing metadata.
    legacy_frame_limits unset_limits;
    output.buffer_id_ = 999;
    check(vqec_vision_ai_camer_lwire_decode_frame(
        valid.data(), valid.size(), unset_limits, output).code_ == status_code::invalid_argument);
    check(output.buffer_id_ == 999);
    check(vqec_vision_ai_camer_lwire_decode_frame(
        valid.data(), valid.size(), limits, output).code_ == status_code::ok);
    check(vqec_vision_ai_camer_lwire_decode_frame(
        valid.data(), 103, limits, output).code_ == status_code::protocol_error);
    check(vqec_vision_ai_camer_lwire_decode_frame(
        nullptr, 104, limits, output).code_ == status_code::protocol_error);
    for (const auto offset : {8U, 12U, 16U, 20U, 40U, 44U, 56U, 72U}) {
        auto broken = valid;
        vqec_vision_ai_unit_lwtst_put_field(broken, offset, std::uint32_t{0});
        output.buffer_id_ = 999;
        check(vqec_vision_ai_camer_lwire_decode_frame(
            broken.data(), broken.size(), limits, output).code_ != status_code::ok);
        check(output.buffer_id_ == 999);
    }
    auto broken = valid;
    vqec_vision_ai_unit_lwtst_put_field(broken, 64, UINT64_MAX);
    check(vqec_vision_ai_camer_lwire_decode_frame(
        broken.data(), broken.size(), limits, output).code_ == status_code::protocol_error);
    broken = valid;
    vqec_vision_ai_unit_lwtst_put_field(broken, 28, std::uint32_t{0});
    check(vqec_vision_ai_camer_lwire_decode_frame(
        broken.data(), broken.size(), limits, output).code_ == status_code::protocol_error);
    broken = valid;
    vqec_vision_ai_unit_lwtst_put_field(broken, 8, std::uint32_t{3839});
    check(vqec_vision_ai_camer_lwire_decode_frame(
        broken.data(), broken.size(), limits, output).code_ == status_code::protocol_error);
    broken = valid;
    vqec_vision_ai_unit_lwtst_put_field(broken, 44, std::int32_t{-1});
    check(vqec_vision_ai_camer_lwire_decode_frame(
        broken.data(), broken.size(), limits, output).code_ == status_code::protocol_error);
    limits.max_allocation_bytes_ = 1024;
    check(vqec_vision_ai_camer_lwire_decode_frame(
        valid.data(), valid.size(), limits, output).code_ == status_code::protocol_error);
    std::cout << "legacy wire failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
