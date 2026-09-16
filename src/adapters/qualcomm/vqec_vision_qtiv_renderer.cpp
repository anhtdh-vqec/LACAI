#include "vqec_vision_qtiv_renderer.hpp"

#include <cstring>
#include <limits>

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <gst/allocators/gstqtiallocator.h>
#include <gst/gst.h>
#include <gst/video/gstimagepool.h>
#include <gst/video/gstvideometa.h>
#include <gst/video/video.h>
#include <gst/video/video-utils.h>

#include "vqec/vision/ai/contracts/vqec_vision_tensor_contract.hpp"

namespace vqec::vision::ai {
namespace {

constexpr std::size_t g_nv12_plane_count = 2U;
constexpr std::size_t g_nv12_chroma_row_divisor = 2U;
constexpr std::uint32_t g_rgba_alpha_mask = 0xFFU;
constexpr GstClockTime g_encoder_poll_timeout_ns = 0;

GstBufferPool* vqec_vision_ai_qcom_qtvr_create_output_pool(
    GstCaps* _caps, guint _surface_count) {
    GstVideoInfo info{};
    GstVideoAlignment alignment{};
    if (_caps == nullptr || !gst_video_info_from_caps(&info, _caps) ||
        !gst_video_retrieve_gpu_alignment(&info, &alignment)) {
        return nullptr;
    }
    GstBufferPool* pool = gst_image_buffer_pool_new();
    GstAllocator* allocator = gst_qti_allocator_new(GST_FD_MEMORY_FLAG_KEEP_MAPPED);
    if (pool == nullptr || allocator == nullptr) {
        if (pool != nullptr) {
            gst_object_unref(pool);
        }
        if (allocator != nullptr) {
            gst_object_unref(allocator);
        }
        return nullptr;
    }
    GstStructure* config = gst_buffer_pool_get_config(pool);
    gst_buffer_pool_config_set_allocator(config, allocator, nullptr);
    gst_object_unref(allocator);
    gst_buffer_pool_config_add_option(config, GST_BUFFER_POOL_OPTION_VIDEO_META);
    gst_buffer_pool_config_add_option(config, GST_IMAGE_BUFFER_POOL_OPTION_KEEP_MAPPED);
    gst_buffer_pool_config_add_option(config, GST_BUFFER_POOL_OPTION_VIDEO_ALIGNMENT);
    gst_buffer_pool_config_set_video_alignment(config, &alignment);
    gst_video_info_align(&info, &alignment);
    gst_buffer_pool_config_set_params(
        config, _caps, info.size, _surface_count, _surface_count);
    if (!gst_buffer_pool_set_config(pool, config) || !gst_buffer_pool_set_active(pool, TRUE)) {
        gst_object_unref(pool);
        return nullptr;
    }
    return pool;
}



status vqec_vision_ai_qcom_qtvr_read_pipeline_error(GstElement* _pipeline) {
    GstBus* bus = gst_element_get_bus(_pipeline);
    if (bus == nullptr) {
        return {status_code::invalid_state, "qtiv renderer pipeline has no bus"};
    }
    GstMessage* message = gst_bus_pop_filtered(bus,
        static_cast<GstMessageType>(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));
    gst_object_unref(bus);
    if (message == nullptr) {
        return {};
    }
    status result{status_code::io_error, "qtiv renderer pipeline stopped"};
    if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_ERROR) {
        GError* error = nullptr;
        gchar* debug = nullptr;
        gst_message_parse_error(message, &error, &debug);
        if (error != nullptr && error->message != nullptr) {
            result.message_ = error->message;
        }
        if (debug != nullptr) {
            result.message_ += ": ";
            result.message_ += debug;
        }
        if (error != nullptr) {
            g_error_free(error);
        }
        g_free(debug);
    }
    gst_message_unref(message);
    return result;
}

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
        const std::uint32_t write_started =
            static_cast<std::uint32_t>((sequence_ * 2U) + 1U);
        const std::uint32_t write_finished = write_started + 1U;
        vqec_vision_ai_qcom_qtvr_store(slot, base + 0, write_started);
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
        vqec_vision_ai_qcom_qtvr_store(slot, base + 0, write_finished);
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

class vqec_vision_ai_qcom_qtvr_mmap_guard {
public:
    vqec_vision_ai_qcom_qtvr_mmap_guard(void* _address, std::size_t _size) noexcept
        : address_(_address), size_(_size) {}
    ~vqec_vision_ai_qcom_qtvr_mmap_guard() noexcept {
        if (address_ != nullptr && address_ != MAP_FAILED && size_ != 0) {
            ::munmap(address_, size_);
        }
    }
    vqec_vision_ai_qcom_qtvr_mmap_guard(const vqec_vision_ai_qcom_qtvr_mmap_guard&) = delete;
    vqec_vision_ai_qcom_qtvr_mmap_guard& operator=(
        const vqec_vision_ai_qcom_qtvr_mmap_guard&) = delete;

private:
    void* address_{nullptr};
    std::size_t size_{0};
};

struct qtiv_renderer::implementation {
    GstBuffer* vqec_vision_ai_qcom_qtvr_copy_nv12(const raw_frame& _frame);

    qtiv_renderer_config config_;
    GstElement* pipeline_{nullptr};
    GstElement* appsrc_{nullptr};
    GstElement* appsink_{nullptr};
    GstBufferPool* output_pool_{nullptr};
    fw_ring_writer ring_;
    std::uint64_t written_{0};
    // Monotonic push counter for PTS; it must advance on every push even when the encoder
    // has not produced an access unit yet, otherwise a reused PTS stalls v4l2h264enc.
    std::uint64_t submitted_{0};
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
        _config.bitrate_bps_ == 0 || _config.keyframe_interval_frames_ == 0 ||
        _config.output_surface_count_ == 0 ||
        (_config.box_color_rgba_ & g_rgba_alpha_mask) == 0 ||
        _config.ring_id_.empty() || _config.colorimetry_.empty() ||
        _config.interlace_mode_.empty()) {
        return {status_code::invalid_argument, "invalid qtiv renderer configuration"};
    }
    impl.config_ = _config;
    if (!impl.ring_.open(_config.ring_id_)) {
        return {status_code::io_error, "cannot open the FW encoded ring"};
    }
    gst_init(nullptr, nullptr);
    const std::string bitrate = std::to_string(_config.bitrate_bps_);
    const std::string keyframe_interval =
        std::to_string(_config.keyframe_interval_frames_);
    const std::string description =
        "appsrc name=src is-live=true format=time"
        " ! capsfilter name=surfacecaps ! qtivoverlay"
        " ! v4l2h264enc extra-controls=\"controls,video_bitrate=" + bitrate +
        ",video_gop_size=" + keyframe_interval + "\""
        // Repeat SPS/PPS on every IDR so a late RTSP reader can start from any retained
        // keyframe in the bounded ring.
        " ! h264parse config-interval=-1"
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
    GstElement* surface_caps = gst_bin_get_by_name(GST_BIN(impl.pipeline_), "surfacecaps");
    if (impl.appsrc_ == nullptr || impl.appsink_ == nullptr || surface_caps == nullptr) {
        if (surface_caps != nullptr) {
            gst_object_unref(surface_caps);
        }
        return {status_code::unsupported,
            "qtivoverlay pipeline is missing appsrc, surface caps, or appsink"};
    }
    GstCaps* caps = gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, "NV12",
        "width", G_TYPE_INT, static_cast<int>(_config.width_), "height", G_TYPE_INT,
        static_cast<int>(_config.height_), "framerate", GST_TYPE_FRACTION,
        static_cast<int>(_config.fps_), 1, "colorimetry", G_TYPE_STRING,
        _config.colorimetry_.c_str(), "interlace-mode", G_TYPE_STRING,
        _config.interlace_mode_.c_str(), nullptr);
    g_object_set(G_OBJECT(impl.appsrc_), "caps", caps, nullptr);
    // Fix the renderer surface contract to the complete negotiated camera profile.
    g_object_set(G_OBJECT(surface_caps), "caps", caps, nullptr);
    impl.output_pool_ = vqec_vision_ai_qcom_qtvr_create_output_pool(
        caps, _config.output_surface_count_);
    gst_object_unref(surface_caps);
    gst_caps_unref(caps);
    if (impl.output_pool_ == nullptr) {
        return {status_code::resource_exhausted,
            "cannot create the Qualcomm DMA render pool"};
    }
    impl.is_open_ = true;
    if (gst_element_set_state(impl.pipeline_, GST_STATE_PLAYING) ==
        GST_STATE_CHANGE_FAILURE) {
        vqec_vision_ai_qcom_qtvr_close();
        return {status_code::io_error, "cannot start the qtivoverlay encode pipeline"};
    }
    return {};
}

GstBuffer* qtiv_renderer::implementation::vqec_vision_ai_qcom_qtvr_copy_nv12(
    const raw_frame& _frame) {
    const auto& descriptor = _frame.descriptor_;
    if (_frame.native_handle_ < 0 ||
        _frame.native_handle_ > std::numeric_limits<int>::max() ||
        descriptor.width_ == 0 || descriptor.height_ == 0 ||
        descriptor.width_ % g_nv12_chroma_row_divisor != 0 ||
        descriptor.height_ % g_nv12_chroma_row_divisor != 0 ||
        descriptor.allocation_size_bytes_ == 0 || descriptor.view_size_bytes_ == 0 ||
        descriptor.memory_offset_bytes_ > descriptor.allocation_size_bytes_ ||
        descriptor.view_size_bytes_ >
            descriptor.allocation_size_bytes_ - descriptor.memory_offset_bytes_) {
        return nullptr;
    }
    const std::size_t width = descriptor.width_;
    const std::size_t height = descriptor.height_;
    const std::size_t chroma_rows = height / g_nv12_chroma_row_divisor;
    for (std::size_t plane = 0; plane < g_nv12_plane_count; ++plane) {
        const std::size_t rows = plane == 0 ? height : chroma_rows;
        if (descriptor.strides_[plane] < static_cast<std::int32_t>(width) ||
            descriptor.offsets_[plane] > descriptor.view_size_bytes_ ||
            rows > (descriptor.view_size_bytes_ - descriptor.offsets_[plane]) /
                static_cast<std::size_t>(descriptor.strides_[plane])) {
            return nullptr;
        }
    }
    const int frame_fd = static_cast<int>(_frame.native_handle_);
    const std::size_t alloc_size = static_cast<std::size_t>(descriptor.allocation_size_bytes_);
    void* mapped = ::mmap(nullptr, alloc_size, PROT_READ, MAP_SHARED, frame_fd, 0);
    if (mapped == MAP_FAILED) {
        return nullptr;
    }
    vqec_vision_ai_qcom_qtvr_mmap_guard guard(mapped, alloc_size);
    GstBuffer* buffer = nullptr;
    if (output_pool_ == nullptr ||
        gst_buffer_pool_acquire_buffer(output_pool_, &buffer, nullptr) != GST_FLOW_OK) {
        return nullptr;
    }
    GstMapInfo map{};
    const bool copied = buffer != nullptr && gst_buffer_map(buffer, &map, GST_MAP_WRITE);
    if (copied) {
        const auto* base = static_cast<const std::uint8_t*>(mapped) +
            descriptor.memory_offset_bytes_;
        const GstVideoMeta* output_meta = gst_buffer_get_video_meta(buffer);
        if (output_meta == nullptr || output_meta->n_planes != g_nv12_plane_count) {
            gst_buffer_unmap(buffer, &map);
            gst_buffer_unref(buffer);
            return nullptr;
        }
        for (std::size_t plane = 0; plane < g_nv12_plane_count; ++plane) {
            const std::size_t rows = plane == 0 ? height : chroma_rows;
            const auto* source = base + descriptor.offsets_[plane];
            const std::size_t source_stride =
                static_cast<std::size_t>(descriptor.strides_[plane]);
            const std::size_t destination_offset = output_meta->offset[plane];
            const std::size_t destination_stride =
                static_cast<std::size_t>(output_meta->stride[plane]);
            if (destination_stride < width || destination_offset > map.size ||
                rows > (map.size - destination_offset) / destination_stride) {
                gst_buffer_unmap(buffer, &map);
                gst_buffer_unref(buffer);
                return nullptr;
            }
            if (destination_stride == source_stride && destination_stride == width) {
                std::memcpy(map.data + destination_offset, source, rows * width);
            } else {
                for (std::size_t row = 0; row < rows; ++row) {
                    std::memcpy(map.data + destination_offset + row * destination_stride,
                        source + row * source_stride, width);
                }
            }
        }
        gst_buffer_unmap(buffer, &map);
    }
    if (!copied) {
        if (buffer != nullptr) {
            gst_buffer_unref(buffer);
        }
        return nullptr;
    }
    return buffer;
}

status qtiv_renderer::vqec_vision_ai_qcom_qtvr_render(
    const raw_frame& _frame, const observation_batch& _observations) {
    if (implementation_ == nullptr || !implementation_->is_open_) {
        return {status_code::invalid_state, "qtiv renderer is not initialized"};
    }
    auto& impl = *implementation_;
    GstBuffer* buffer = impl.vqec_vision_ai_qcom_qtvr_copy_nv12(_frame);
    if (buffer == nullptr) {
        return {status_code::io_error,
            "cannot copy the NV12 frame into the Qualcomm render surface"};
    }
    for (const auto& item : _observations.observations_) {
        GstVideoRegionOfInterestMeta* roi = gst_buffer_add_video_region_of_interest_meta(
            buffer, item.box_.label_.c_str(),
            static_cast<guint>(item.box_.x_), static_cast<guint>(item.box_.y_),
            static_cast<guint>(item.box_.width_), static_cast<guint>(item.box_.height_));
        if (roi == nullptr) {
            continue;
        }
        GstStructure* structure = gst_structure_new("ObjectDetection",
            "confidence", G_TYPE_DOUBLE, static_cast<gdouble>(item.confidence_),
            "color", G_TYPE_UINT, impl.config_.box_color_rgba_, nullptr);
        gst_video_region_of_interest_meta_add_param(roi, structure);
    }
    GST_BUFFER_PTS(buffer) = impl.submitted_ * GST_SECOND / impl.config_.fps_;
    GST_BUFFER_DURATION(buffer) = GST_SECOND / impl.config_.fps_;
    ++impl.submitted_;
    const GstFlowReturn pushed = gst_app_src_push_buffer(GST_APP_SRC(impl.appsrc_), buffer);
    if (pushed != GST_FLOW_OK) {
        return {status_code::io_error, "qtiv renderer appsrc rejected the frame"};
    }
    GstSample* sample = nullptr;
    bool any_sample = false;
    status result = {};
    while ((sample = gst_app_sink_try_pull_sample(
                GST_APP_SINK(impl.appsink_), g_encoder_poll_timeout_ns)) != nullptr) {
        any_sample = true;
        GstBuffer* encoded = gst_sample_get_buffer(sample);
        GstMapInfo out_map {};
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
    }
    if (!any_sample) {
        const auto pipeline = vqec_vision_ai_qcom_qtvr_read_pipeline_error(impl.pipeline_);
        if (pipeline.code_ != status_code::ok) {
            return pipeline;
        }
        return {status_code::pending, "encoder produced no access unit"};
    }
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
    if (impl.output_pool_ != nullptr) {
        gst_buffer_pool_set_active(impl.output_pool_, FALSE);
        gst_object_unref(impl.output_pool_);
        impl.output_pool_ = nullptr;
    }
    impl.ring_.close();
    impl.is_open_ = false;
}

}  // namespace vqec::vision::ai
