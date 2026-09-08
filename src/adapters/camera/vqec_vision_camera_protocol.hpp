#ifndef VQEC_VISION_AI_CAMERA_PROTOCOL_HPP
#define VQEC_VISION_AI_CAMERA_PROTOCOL_HPP

#include <cstddef>
#include <cstdint>

namespace vqec::vision::ai::camera_protocol {

// Released FW endpoint catalog / Camera1 protocol; not deployment tuning.
inline constexpr char g_bus_name[] = "com.vnpt.camera.Camera";
inline constexpr char g_object_path[] = "/com/vnpt/camera/Camera";
inline constexpr char g_interface_name[] = "com.vnpt.camera.Camera1";
inline constexpr char g_start_stream[] = "StartStream";
inline constexpr char g_stop_stream[] = "StopStream";
inline constexpr char g_get_status[] = "GetStatus";
inline constexpr char g_camera_id_field[] = "camera_id";
inline constexpr char g_channel_id_field[] = "channel_id";
inline constexpr char g_stream_id_field[] = "stream_id";
inline constexpr char g_transport_field[] = "transport";
inline constexpr char g_consumer_id_field[] = "consumer_id";
inline constexpr char g_request_id_field[] = "request_id";
inline constexpr char g_stream_handle_field[] = "stream_handle";
inline constexpr char g_code_field[] = "code";
inline constexpr char g_codec_field[] = "codec";
inline constexpr char g_width_field[] = "width";
inline constexpr char g_height_field[] = "height";
inline constexpr char g_fps_field[] = "fps";
inline constexpr char g_raw_codec[] = "RAW";
inline constexpr char g_ai_stream[] = "third";
inline constexpr char g_fd_transport[] = "dmabuf";
inline constexpr char g_default_channel[] = "0";

// Fixed numeric values from FW common/result.hpp; wire represents them as strings.
inline constexpr std::uint32_t g_code_ok = 0;
inline constexpr std::uint32_t g_code_invalid_argument = 1001;
inline constexpr std::uint32_t g_code_not_found = 1002;
inline constexpr std::uint32_t g_code_invalid_profile = 1003;
inline constexpr std::uint32_t g_code_not_running = 1005;
inline constexpr std::uint32_t g_code_busy = 1006;
inline constexpr std::uint32_t g_code_timeout = 1007;
inline constexpr std::uint32_t g_code_unsupported = 1008;
inline constexpr std::uint32_t g_code_permission_denied = 1009;
inline constexpr std::uint32_t g_code_resource_exhausted = 2004;
inline constexpr std::uint32_t g_code_version_mismatch = 9001;

// AI parser safety budgets, not limits advertised by the FW service.
inline constexpr int g_max_call_timeout_ms = 60000;
inline constexpr std::size_t g_max_fields = 64;
inline constexpr std::size_t g_max_key_bytes = 128;
inline constexpr std::size_t g_max_value_bytes = 4096;
inline constexpr std::size_t g_max_field_bytes = 16384;
inline constexpr std::size_t g_max_reply_wire_bytes = 32768;
inline constexpr std::size_t g_max_identifier_bytes = 128;
inline constexpr std::size_t g_max_handle_bytes = 1024;

}  // namespace vqec::vision::ai::camera_protocol

#endif  // VQEC_VISION_AI_CAMERA_PROTOCOL_HPP
