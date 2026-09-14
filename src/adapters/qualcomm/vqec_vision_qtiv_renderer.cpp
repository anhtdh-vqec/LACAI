#include "vqec_vision_qtiv_renderer.hpp"

#include <cstring>
#include <vector>

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <gst/gst.h>
#include <gst/video/gstvideometa.h>
#include <gst/video/video.h>

#include "vqec/vision/ai/contracts/vqec_vision_tensor_contract.hpp"

namespace vqec::vision::ai {
namespace {

namespace ring_layout {
inline constexpr std::uint32_t g_version = 5;
inline constexpr std::uint32_t g_slot_count = 16;
inline constexpr std::uint32_t g_payload_size = 1U << 20;
inline constexpr std::size_t g_header_size = 4096;
inline constexpr std::size_t g_slot_header_size = 1232;
inline constexpr std::size_t g_h_write_sequence = 32;
inline constexpr std::size_t g_h_ring_id = 64;
inline constexpr std::uint32_t g_magic = 0x4C414341U;
}  // namespace ring_layout

template <typename T>
void vqec_vision_ai_qcom_qtvr_store(std::uint8_t* _base, std::size_t _offset, T _value) {
    std::memcpy(_base + _offset, &_value, sizeof(T));
}

// Minimal writer for the released FW ring layout. The AI side owns production; the FW RTSP
// service is the only reader. This does not link the FW SDK.
class fw_ring_writer {
public:
    ~fw_ring_writer() { close(); }

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
        vqec_vision_ai_qcom_qtvr_store(base, 0, ring_layout::g_magic);
        vqec_vision_ai_qcom_qtvr_store(base, 4, ring_layout::g_version);
        vqec_vision_ai_qcom_qtvr_store(base, 8,
            static_cast<std::uint32_t>(ring_layout::g_header_size));
        vqec_vision_ai_qcom_qtvr_store(base, 12,
            static_cast<std::uint32_t>(ring_layout::g_slot_header_size));
        vqec_vision_ai_qcom_qtvr_store(base, 16, ring_layout::g_slot_count);
        vqec_vision_ai_qcom_qtvr_store(base, 20, ring_layout::g_payload_size);
        vqec_vision_ai_qcom_qtvr_store(base, ring_layout::g_h_write_sequence,
            static_cast<std::uint64_t>(0));
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

    bool push(const std::uint8_t* _data, std::size_t _size, std::uint32_t _width,
        std::uint32_t _height, bool _keyframe) {
        if (mapping_ == nullptr || _size == 0 || _size > ring_layout::g_payload_size) {
            return false;
        }
        const std::size_t index = sequence_ % ring_layout::g_slot_count;
        const std::size_t base = ring_layout::g_header_size +
            index * (ring_layout::g_slot_header_size + ring_layout::g_payload_size);
        auto* slot = static_cast<std::uint8_t*>(mapping_);
        vqec_vision_ai_qcom_qtvr_store(slot, base + 0, static_cast<std::uint32_t>(1));
        vqec_vision_ai_qcom_qtvr_store(slot, base + 8, static_cast<std::uint32_t>(_size));
        vqec_vision_ai_qcom_qtvr_store(slot, base + 20, _width);
        vqec_vision_ai_qcom_qtvr_store(slot, base + 24, _height);
        vqec_vision_ai_qcom_qtvr_store(slot, base + 28, _width);
        vqec_vision_ai_qcom_qtvr_store(slot, base + 40,
            static_cast<std::uint32_t>(_keyframe ? 1 : 0));
        vqec_vision_ai_qcom_qtvr_store(slot, base + 56, sequence_ + 1);
        vqec_vision_ai_qcom_qtvr_store(slot, base + 104, sequence_);
        std::memcpy(slot + base + 176, "H264", 4);
        std::memcpy(slot + base + ring_layout::g_slot_header_size, _data, _size);
        vqec_vision_ai_qcom_qtvr_store(slot, base + 0, static_cast<std::uint32_t>(0));
        ++sequence_;
        vqec_vision_ai_qcom_qtvr_store(slot, ring_layout::g_h_write_sequence, sequence_);
        return true;
    }

private:
    std::string path_;
    int fd_{-1};
    void* mapping_{nullptr};
    std::size_t total_{0};
    std::uint64_t sequence_{0};
};

}  // namespace

struct qtiv_renderer::implementation {
    qtiv_renderer_config config_;
    GstElement* pipeline_{nullptr};
    GstElement* appsrc_{nullptr};
    GstElement* appsink_{nullptr};
    fw_ring_writer ring_;
    std::vector<std::uint8_t> frame_buffer_;
    std::uint64_t written_{0};
    bool is_open_{false};
};

qtiv_renderer::qtiv_renderer() : implementation_(std::make_unique<implementation>()) {}

qtiv_renderer::~qtiv_renderer() noexcept { vqec_vision_ai_qcom_qtvr_close(); }

status qtiv_renderer::vqec_vision_ai_qcom_qtvr_init(const qtiv_renderer_config& _config) {
    if (implementation_ == nullptr) {
        return {status_code::invalid_state, "qtiv renderer is unavailable"};
    }
    auto& impl = *implementation_;
    if (impl.is_open_) {
        return {status_code::invalid_state, "qtiv renderer is already initialized"};
    }
    if (_config.width_ == 0 || _config.height_ == 0 || _config.fps_ == 0 ||
        _config.ring_id_.empty()) {
        return {status_code::invalid_argument, "invalid qtiv renderer configuration"};
    }
    impl.config_ = _config;
    if (!impl.ring_.open(_config.ring_id_)) {
        return {status_code::io_error, "cannot open the FW encoded ring"};
    }
    gst_init(nullptr, nullptr);
    const std::string bitrate = std::to_string(
        _config.bitrate_bps_ != 0 ? _config.bitrate_bps_ : 2000000U);
    const std::string description =
        "appsrc name=src is-live=true format=time"
        " ! queue ! qtivoverlay"
        " ! videoconvert ! video/x-raw,format=NV12"
        " ! v4l2h264enc extra-controls=\"controls,video_bitrate=" + bitrate + "\""
        " ! h264parse config-interval=1"
        " ! appsink name=enc max-buffers=2 drop=true sync=false";
    GError* error = nullptr;
    impl.pipeline_ = gst_parse_launch(description.c_str(), &error);
    if (impl.pipeline_ == nullptr || error != nullptr) {
        if (error != nullptr) {
            g_error_free(error);
        }
        return {status_code::unsupported, "cannot build the qtivoverlay encode pipeline"};
    }
    impl.appsrc_ = gst_bin_get_by_name(GST_BIN(impl.pipeline_), "src");
    impl.appsink_ = gst_bin_get_by_name(GST_BIN(impl.pipeline_), "enc");
    if (impl.appsrc_ == nullptr || impl.appsink_ == nullptr) {
        return {status_code::unsupported, "qtivoverlay pipeline is missing appsrc/appsink"};
    }
    GstCaps* caps = gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, "NV12",
        "width", G_TYPE_INT, static_cast<int>(_config.width_), "height", G_TYPE_INT,
        static_cast<int>(_config.height_), "framerate", GST_TYPE_FRACTION,
        static_cast<int>(_config.fps_), 1, nullptr);
    g_object_set(G_OBJECT(impl.appsrc_), "caps", caps, nullptr);
    gst_caps_unref(caps);
    gst_element_set_state(impl.pipeline_, GST_STATE_PLAYING);
    impl.is_open_ = true;
    return {};
}

status qtiv_renderer::vqec_vision_ai_qcom_qtvr_render(
    const raw_frame& _frame, const observation_batch& _observations) {
    if (implementation_ == nullptr || !implementation_->is_open_) {
        return {status_code::invalid_state, "qtiv renderer is not initialized"};
    }
    auto& impl = *implementation_;
    const std::size_t size = static_cast<std::size_t>(_frame.descriptor_.view_size_bytes_);
    if (size == 0 || _frame.native_handle_ < 0) {
        return {status_code::invalid_argument, "qtiv renderer needs a mapped NV12 frame"};
    }
    impl.frame_buffer_.resize(size);
    const int fd = static_cast<int>(_frame.native_handle_);
    if (::lseek(fd, 0, SEEK_SET) < 0 ||
        ::read(fd, impl.frame_buffer_.data(), size) != static_cast<ssize_t>(size)) {
        return {status_code::io_error, "cannot read the NV12 frame for rendering"};
    }
    GstBuffer* buffer = gst_buffer_new_allocate(nullptr, size, nullptr);
    GstMapInfo map {};
    if (buffer == nullptr || !gst_buffer_map(buffer, &map, GST_MAP_WRITE)) {
        if (buffer != nullptr) {
            gst_buffer_unref(buffer);
        }
        return {status_code::resource_exhausted, "cannot map the render buffer"};
    }
    std::memcpy(map.data, impl.frame_buffer_.data(), size);
    gst_buffer_unmap(buffer, &map);
    for (const auto& item : _observations.observations_) {
        GstVideoRegionOfInterestMeta* roi = gst_buffer_add_video_region_of_interest_meta(
            buffer, item.class_id_.c_str(),
            static_cast<guint>(item.box_.x_), static_cast<guint>(item.box_.y_),
            static_cast<guint>(item.box_.width_), static_cast<guint>(item.box_.height_));
        if (roi == nullptr) {
            continue;
        }
        GstStructure* structure = gst_structure_new("ObjectDetection",
            "confidence", G_TYPE_DOUBLE, static_cast<gdouble>(item.confidence_),
            "color", G_TYPE_UINT, impl.config_.box_color_argb_, nullptr);
        gst_video_region_of_interest_meta_add_param(roi, structure);
    }
    GST_BUFFER_PTS(buffer) = impl.written_ * 1000000000ULL / impl.config_.fps_;
    GST_BUFFER_DURATION(buffer) = 1000000000ULL / impl.config_.fps_;
    gst_app_src_push_buffer(GST_APP_SRC(impl.appsrc_), buffer);
    GstSample* sample = gst_app_sink_try_pull_sample(GST_APP_SINK(impl.appsink_), GST_SECOND);
    if (sample == nullptr) {
        return {status_code::pending, "encoder produced no access unit"};
    }
    GstBuffer* encoded = gst_sample_get_buffer(sample);
    GstMapInfo out_map {};
    status result = {};
    if (encoded != nullptr && gst_buffer_map(encoded, &out_map, GST_MAP_READ)) {
        const bool keyframe = (GST_BUFFER_FLAGS(encoded) & GST_BUFFER_FLAG_DELTA_UNIT) == 0;
        if (impl.ring_.push(static_cast<const std::uint8_t*>(out_map.data), out_map.size,
                _frame.descriptor_.width_, _frame.descriptor_.height_, keyframe)) {
            ++impl.written_;
        } else {
            result = {status_code::io_error, "cannot write the encoded ring slot"};
        }
        gst_buffer_unmap(encoded, &out_map);
    } else {
        result = {status_code::io_error, "cannot map the encoded access unit"};
    }
    gst_sample_unref(sample);
    return result;
}

std::uint64_t qtiv_renderer::vqec_vision_ai_qcom_qtvr_get_written() const noexcept {
    return implementation_ != nullptr ? implementation_->written_ : 0;
}

void qtiv_renderer::vqec_vision_ai_qcom_qtvr_close() noexcept {
    if (implementation_ == nullptr) {
        return;
    }
    auto& impl = *implementation_;
    if (impl.pipeline_ != nullptr) {
        gst_element_set_state(impl.pipeline_, GST_STATE_NULL);
    }
    if (impl.appsrc_ != nullptr) {
        gst_object_unref(impl.appsrc_);
        impl.appsrc_ = nullptr;
    }
    if (impl.appsink_ != nullptr) {
        gst_object_unref(impl.appsink_);
        impl.appsink_ = nullptr;
    }
    if (impl.pipeline_ != nullptr) {
        gst_object_unref(impl.pipeline_);
        impl.pipeline_ = nullptr;
    }
    impl.ring_.close();
    impl.is_open_ = false;
}

}  // namespace vqec::vision::ai
