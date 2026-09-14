// Board-side C++ encoder that turns raw FW frames into the released encoded ring.
//
//   FW RAW socket -> LACAI frame_source (legacy wire) -> GStreamer H264 encode
//     -> released FW SharedMemoryFrameRingBuffer layout -> mock RTSP reader
//
// This is the C++ counterpart of the Python ring bridge used while wiring LACAI's encoded
// output. It uses LACAI's own camera adapter and reproduces the released ring layout; it is
// a board integration tool, not a registered test.

#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <gst/gst.h>

#include "vqec_vision_frame_source.hpp"

using namespace vqec::vision::ai;

namespace {

namespace ring_layout {
inline constexpr std::uint32_t g_version = 5;
inline constexpr std::uint32_t g_slot_count = 16;
inline constexpr std::uint32_t g_payload_size = 1U << 20;
inline constexpr std::size_t g_header_size = 4096;
inline constexpr std::size_t g_slot_header_size = 1232;
inline constexpr std::size_t g_h_write_sequence = 32;
inline constexpr std::size_t g_h_ring_id = 64;
inline constexpr std::size_t g_s_seqlock = 0;
inline constexpr std::size_t g_s_data_size = 8;
inline constexpr std::size_t g_s_width = 20;
inline constexpr std::size_t g_s_height = 24;
inline constexpr std::size_t g_s_stride = 28;
inline constexpr std::size_t g_s_is_keyframe = 40;
inline constexpr std::size_t g_s_frame_id = 56;
inline constexpr std::size_t g_s_sequence = 104;
inline constexpr std::size_t g_s_codec = 176;
inline constexpr std::uint32_t g_magic = 0x4C414341U;
}  // namespace ring_layout

template <typename T>
void vqec_vision_ai_tools_rgwr_store(std::uint8_t* _base, std::size_t _offset, T _value) {
    std::memcpy(_base + _offset, &_value, sizeof(T));
}

class ring_writer {
public:
    ~ring_writer() { close(); }

    bool open(const std::string& _ring_id) {
        path_ = "/dev/shm/camera_ai_" + _ring_id;
        ::unlink(path_.c_str());
        fd_ = ::open(path_.c_str(), O_RDWR | O_CREAT, 0600);
        if (fd_ < 0) {
            return false;
        }
        total_ = ring_layout::g_header_size +
            static_cast<std::size_t>(ring_layout::g_slot_count) *
                (ring_layout::g_slot_header_size + ring_layout::g_payload_size);
        if (::ftruncate(fd_, static_cast<off_t>(total_)) != 0) {
            return false;
        }
        mapping_ = ::mmap(nullptr, total_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
        if (mapping_ == MAP_FAILED) {
            mapping_ = nullptr;
            return false;
        }
        auto* base = static_cast<std::uint8_t*>(mapping_);
        vqec_vision_ai_tools_rgwr_store(base, 0, ring_layout::g_magic);
        vqec_vision_ai_tools_rgwr_store(base, 4, ring_layout::g_version);
        vqec_vision_ai_tools_rgwr_store(base, 8, static_cast<std::uint32_t>(ring_layout::g_header_size));
        vqec_vision_ai_tools_rgwr_store(base, 12, static_cast<std::uint32_t>(ring_layout::g_slot_header_size));
        vqec_vision_ai_tools_rgwr_store(base, 16, ring_layout::g_slot_count);
        vqec_vision_ai_tools_rgwr_store(base, 20, ring_layout::g_payload_size);
        const std::uint64_t zero = 0;
        vqec_vision_ai_tools_rgwr_store(base, ring_layout::g_h_write_sequence, zero);
        std::memcpy(base + ring_layout::g_h_ring_id, _ring_id.data(),
            _ring_id.size() < 63 ? _ring_id.size() : 63);
        return true;
    }

    void close() {
        if (mapping_ != nullptr) {
            ::munmap(mapping_, total_);
            mapping_ = nullptr;
        }
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
    }

    void push(const std::uint8_t* _data, std::size_t _size, std::uint32_t _width,
        std::uint32_t _height, bool _keyframe) {
        if (mapping_ == nullptr || _size == 0 || _size > ring_layout::g_payload_size) {
            return;
        }
        const std::size_t index = sequence_ % ring_layout::g_slot_count;
        const std::size_t base = ring_layout::g_header_size +
            index * (ring_layout::g_slot_header_size + ring_layout::g_payload_size);
        auto* slot = static_cast<std::uint8_t*>(mapping_);
        vqec_vision_ai_tools_rgwr_store(slot, base + ring_layout::g_s_seqlock,
            static_cast<std::uint32_t>(1));
        vqec_vision_ai_tools_rgwr_store(slot, base + ring_layout::g_s_data_size,
            static_cast<std::uint32_t>(_size));
        vqec_vision_ai_tools_rgwr_store(slot, base + ring_layout::g_s_width, _width);
        vqec_vision_ai_tools_rgwr_store(slot, base + ring_layout::g_s_height, _height);
        vqec_vision_ai_tools_rgwr_store(slot, base + ring_layout::g_s_stride, _width);
        vqec_vision_ai_tools_rgwr_store(slot, base + ring_layout::g_s_is_keyframe,
            static_cast<std::uint32_t>(_keyframe ? 1 : 0));
        vqec_vision_ai_tools_rgwr_store(slot, base + ring_layout::g_s_frame_id, sequence_ + 1);
        vqec_vision_ai_tools_rgwr_store(slot, base + ring_layout::g_s_sequence, sequence_);
        std::memcpy(slot + base + ring_layout::g_s_codec, "H264", 4);
        std::memcpy(slot + base + ring_layout::g_slot_header_size, _data, _size);
        vqec_vision_ai_tools_rgwr_store(slot, base + ring_layout::g_s_seqlock,
            static_cast<std::uint32_t>(0));
        ++sequence_;
        vqec_vision_ai_tools_rgwr_store(slot, ring_layout::g_h_write_sequence, sequence_);
    }

private:
    std::string path_;
    int fd_{-1};
    void* mapping_{nullptr};
    std::size_t total_{0};
    std::uint64_t sequence_{0};
};

volatile std::sig_atomic_t g_stop = 0;
void vqec_vision_ai_tools_rgwr_on_signal(int) { g_stop = 1; }

}  // namespace

int main(int _argc, char** _argv) {
    std::string socket_path;
    std::string ring_id = "encoded_ai_detect0_cam0_ch0";
    std::uint32_t producer_uid = 0;
    std::uint32_t nv12_format = 23;
    std::uint32_t width = 1280;
    std::uint32_t height = 720;
    std::uint32_t fps = 30;
    std::uint64_t iterations = 0;
    for (int index = 1; index < _argc; ++index) {
        const std::string option = _argv[index];
        const bool has_value = index + 1 < _argc;
        if (!has_value) {
            std::fprintf(stderr, "missing value for %s\n", option.c_str());
            return 2;
        }
        const std::string value = _argv[++index];
        if (option == "--socket") socket_path = value;
        else if (option == "--ring-id") ring_id = value;
        else if (option == "--producer-uid") producer_uid = static_cast<std::uint32_t>(std::strtoul(value.c_str(), nullptr, 10));
        else if (option == "--nv12-format") nv12_format = static_cast<std::uint32_t>(std::strtoul(value.c_str(), nullptr, 10));
        else if (option == "--width") width = static_cast<std::uint32_t>(std::strtoul(value.c_str(), nullptr, 10));
        else if (option == "--height") height = static_cast<std::uint32_t>(std::strtoul(value.c_str(), nullptr, 10));
        else if (option == "--fps") fps = static_cast<std::uint32_t>(std::strtoul(value.c_str(), nullptr, 10));
        else if (option == "--iterations") iterations = std::strtoull(value.c_str(), nullptr, 10);
        else { std::fprintf(stderr, "unknown option: %s\n", option.c_str()); return 2; }
    }
    if (socket_path.empty()) {
        std::fprintf(stderr,
            "usage: vqec_vision_ring_writer --socket PATH [--ring-id ID] [--width W] "
            "[--height H] [--fps F] [--producer-uid N] [--nv12-format 23] [--iterations N]\n");
        return 2;
    }
    const std::uint64_t max_allocation =
        static_cast<std::uint64_t>(width) * height * 3U / 2U;
    std::signal(SIGINT, vqec_vision_ai_tools_rgwr_on_signal);

    gst_init(&_argc, &_argv);
    const std::string pipeline_desc =
        "appsrc name=src is-live=true format=time"
        " ! queue ! videoconvert ! video/x-raw,format=NV12"
        " ! v4l2h264enc ! h264parse config-interval=1"
        " ! appsink name=enc max-buffers=2 drop=true sync=false";
    GError* error = nullptr;
    GstElement* pipeline = gst_parse_launch(pipeline_desc.c_str(), &error);
    if (pipeline == nullptr || error != nullptr) {
        std::fprintf(stderr, "encode pipeline error: %s\n",
            error != nullptr ? error->message : "unknown");
        return 1;
    }
    GstElement* appsrc = gst_bin_get_by_name(GST_BIN(pipeline), "src");
    GstElement* appsink = gst_bin_get_by_name(GST_BIN(pipeline), "enc");
    GstCaps* caps = gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, "NV12",
        "width", G_TYPE_INT, static_cast<int>(width), "height", G_TYPE_INT,
        static_cast<int>(height), "framerate", GST_TYPE_FRACTION, static_cast<int>(fps), 1,
        nullptr);
    g_object_set(G_OBJECT(appsrc), "caps", caps, nullptr);
    gst_caps_unref(caps);
    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    ring_writer writer;
    if (!writer.open(ring_id)) {
        std::fprintf(stderr, "cannot open ring %s\n", ring_id.c_str());
        return 1;
    }

    camera_source_config source_config;
    source_config.socket_path_ = socket_path;
    source_config.producer_uid_ = producer_uid;
    source_config.limits_.nv12_format_value_ = nv12_format;
    source_config.limits_.max_width_ = width;
    source_config.limits_.max_height_ = height;
    source_config.limits_.max_allocation_bytes_ = max_allocation;
    frame_source source;
    const auto connected = source.vqec_vision_ai_camer_frsrc_connect(source_config);
    if (connected.code_ != status_code::ok) {
        std::fprintf(stderr, "camera connect failed: %s\n", connected.message_.c_str());
        return 1;
    }
    std::printf("ring writer: socket=%s ring=%s\n", socket_path.c_str(), ring_id.c_str());

    std::vector<std::uint8_t> frame_buffer;
    std::uint64_t frames = 0;
    std::uint64_t written = 0;
    while (!g_stop && (iterations == 0 || frames < iterations)) {
        std::shared_ptr<const received_frame> frame;
        if (source.vqec_vision_ai_camer_frsrc_receive(frame, 3000).code_ != status_code::ok ||
            frame == nullptr) {
            std::fprintf(stderr, "camera receive failed\n");
            break;
        }
        const auto& descriptor = frame->vqec_vision_ai_camer_frsrc_get_descriptor();
        const std::size_t size = static_cast<std::size_t>(descriptor.view_size_bytes_);
        frame_buffer.resize(size);
        const int fd = frame->vqec_vision_ai_camer_frsrc_get_fd();
        if (::lseek(fd, 0, SEEK_SET) < 0 ||
            ::read(fd, frame_buffer.data(), size) != static_cast<ssize_t>(size)) {
            break;
        }
        GstBuffer* buffer = gst_buffer_new_allocate(nullptr, size, nullptr);
        GstMapInfo map {};
        if (gst_buffer_map(buffer, &map, GST_MAP_WRITE)) {
            std::memcpy(map.data, frame_buffer.data(), size);
            gst_buffer_unmap(buffer, &map);
            GST_BUFFER_PTS(buffer) = frames * 1000000000ULL / fps;
            GST_BUFFER_DURATION(buffer) = 1000000000ULL / fps;
            gst_app_src_push_buffer(GST_APP_SRC(appsrc), buffer);
        } else {
            gst_buffer_unref(buffer);
        }
        GstSample* sample = gst_app_sink_try_pull_sample(GST_APP_SINK(appsink), GST_SECOND);
        if (sample != nullptr) {
            GstBuffer* encoded = gst_sample_get_buffer(sample);
            GstMapInfo out_map {};
            if (encoded != nullptr && gst_buffer_map(encoded, &out_map, GST_MAP_READ)) {
                const bool keyframe = (GST_BUFFER_FLAGS(encoded) & GST_BUFFER_FLAG_DELTA_UNIT) == 0;
                writer.push(static_cast<const std::uint8_t*>(out_map.data), out_map.size,
                    descriptor.width_, descriptor.height_, keyframe);
                gst_buffer_unmap(encoded, &out_map);
                ++written;
            }
            gst_sample_unref(sample);
        }
        ++frames;
        if (frames % 60U == 0U) {
            std::printf("frames=%llu written=%llu\n",
                static_cast<unsigned long long>(frames),
                static_cast<unsigned long long>(written));
            std::fflush(stdout);
        }
    }
    source.vqec_vision_ai_camer_frsrc_disconnect();
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(appsrc);
    gst_object_unref(appsink);
    gst_object_unref(pipeline);
    writer.close();
    std::printf("ring writer stopping: frames=%llu written=%llu\n",
        static_cast<unsigned long long>(frames), static_cast<unsigned long long>(written));
    return written == 0 ? 1 : 0;
}
