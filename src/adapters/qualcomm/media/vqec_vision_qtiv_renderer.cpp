#include "vqec_vision_qtiv_renderer.hpp"

#include <atomic>
#include <array>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <ctime>
#include <limits>
#include <memory>
#include <vector>

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <gst/allocators/gstdmabuf.h>
#include <gst/gst.h>
#include <gst/video/gstvideometa.h>
#include <gst/video/video.h>

#include "vqec/vision/ai/contracts/vqec_vision_fw_ring_layout.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_tensor_contract.hpp"
#include "vqec_vision_dsp_buffer_cache.hpp"
#include "vqec_vision_dsp_v1_client.hpp"
#include "vqec_vision_dsp_v1_overlay.h"
#include "vqec_vision_rpcmem_pool.hpp"

namespace vqec::vision::ai {
namespace {

constexpr std::size_t g_nv12_plane_count = 2U;
constexpr std::size_t g_nv12_chroma_row_divisor = 2U;
constexpr std::uint32_t g_rgba_alpha_mask = 0xFFU;
constexpr GstClockTime g_encoder_poll_timeout_ns = 0;
constexpr std::uint32_t g_venus_stride_alignment = 128U;
constexpr std::uint32_t g_venus_y_row_alignment = 32U;
constexpr std::uint32_t g_venus_uv_row_alignment = 16U;
constexpr std::uint16_t g_overlay_border_thickness = 2U;
constexpr std::uint16_t g_overlay_font_scale = 2U;

struct renderer_surface_layout {
    std::uint32_t y_stride_{0};
    std::uint32_t uv_offset_{0};
    std::uint32_t uv_stride_{0};
    std::size_t bytes_{0};
};

struct renderer_surface_state {
    std::atomic<bool> busy_{false};
};

std::uint32_t vqec_vision_ai_qcom_qtvr_align_up(std::uint32_t _value,
                                                 std::uint32_t _alignment) {
    return (_value + _alignment - 1U) & ~(_alignment - 1U);
}

renderer_surface_layout vqec_vision_ai_qcom_qtvr_make_surface_layout(
    std::uint32_t _width, std::uint32_t _height) {
    renderer_surface_layout layout;
    layout.y_stride_ = vqec_vision_ai_qcom_qtvr_align_up(
        _width, g_venus_stride_alignment);
    const std::uint32_t y_rows = vqec_vision_ai_qcom_qtvr_align_up(
        _height, g_venus_y_row_alignment);
    const std::uint32_t uv_rows = vqec_vision_ai_qcom_qtvr_align_up(
        _height / g_nv12_chroma_row_divisor, g_venus_uv_row_alignment);
    layout.uv_offset_ = layout.y_stride_ * y_rows;
    layout.uv_stride_ = layout.y_stride_;
    layout.bytes_ = static_cast<std::size_t>(layout.uv_offset_) +
        static_cast<std::size_t>(layout.uv_stride_) * uv_rows;
    return layout;
}

std::uint8_t vqec_vision_ai_qcom_qtvr_clamp_yuv(int _value) {
    if (_value < 0) {
        return 0U;
    }
    if (_value > 255) {
        return 255U;
    }
    return static_cast<std::uint8_t>(_value);
}

void vqec_vision_ai_qcom_qtvr_rgba_to_bt709_limited(
    std::uint32_t _rgba, std::uint8_t& _y, std::uint8_t& _u, std::uint8_t& _v) {
    const int red = static_cast<int>((_rgba >> 24U) & 0xFFU);
    const int green = static_cast<int>((_rgba >> 16U) & 0xFFU);
    const int blue = static_cast<int>((_rgba >> 8U) & 0xFFU);
    _y = vqec_vision_ai_qcom_qtvr_clamp_yuv(
        16 + ((47 * red + 157 * green + 16 * blue + 128) >> 8));
    _u = vqec_vision_ai_qcom_qtvr_clamp_yuv(
        128 + ((-26 * red - 87 * green + 112 * blue + 128) >> 8));
    _v = vqec_vision_ai_qcom_qtvr_clamp_yuv(
        128 + ((112 * red - 102 * green - 10 * blue + 128) >> 8));
}

void vqec_vision_ai_qcom_qtvr_release_surface(gpointer _user_data,
                                               GstMiniObject* _object) {
    (void)_object;
    auto* state = static_cast<renderer_surface_state*>(_user_data);
    if (state != nullptr) {
        state->busy_.store(false, std::memory_order_release);
    }
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

// The released FW ring ABI has a single definition in the contracts header. Adapters must
// not re-declare offsets, sizes or the version; the FW RTSP service is the only reader.
namespace ring_layout = fw_ring_layout;

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
        path_ = ring_layout::vqec_vision_ai_cntr_fwrly_make_shm_path(_ring_id);
        bool created = false;
        fd_ = ::open(path_.c_str(), O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
        if (fd_ < 0 && errno == EEXIST) {
            // Attach to an existing ring instead of clobbering another writer's mapping.
            // Layout/version mismatch fails closed; coordinated replacement is a FW handshake,
            // never an implicit unlink.
            fd_ = ::open(path_.c_str(), O_RDWR | O_CLOEXEC);
        } else if (fd_ >= 0) {
            created = true;
        }
        if (fd_ < 0) {
            return false;
        }
        total_ = static_cast<std::size_t>(ring_layout::g_header_size) +
            static_cast<std::size_t>(ring_layout::g_slot_count) *
                (ring_layout::g_slot_header_size + ring_layout::g_payload_size);
        if (created && ::ftruncate(fd_, static_cast<off_t>(total_)) != 0) {
            close();
            return false;
        }
        mapping_ = ::mmap(nullptr, total_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
        if (mapping_ == MAP_FAILED) {
            mapping_ = nullptr;
            close();
            return false;
        }
        auto* base = static_cast<std::uint8_t*>(mapping_);
        if (created) {
            vqec_vision_ai_qcom_qtvr_store(base, ring_layout::g_h_magic,
                ring_layout::g_magic);
            vqec_vision_ai_qcom_qtvr_store(base, ring_layout::g_h_version,
                ring_layout::g_version);
            vqec_vision_ai_qcom_qtvr_store(base, ring_layout::g_h_header_size,
                ring_layout::g_header_size);
            vqec_vision_ai_qcom_qtvr_store(base, ring_layout::g_h_slot_header_size,
                ring_layout::g_slot_header_size);
            vqec_vision_ai_qcom_qtvr_store(base, ring_layout::g_h_slot_count,
                ring_layout::g_slot_count);
            vqec_vision_ai_qcom_qtvr_store(base, ring_layout::g_h_payload_size,
                ring_layout::g_payload_size);
            vqec_vision_ai_qcom_qtvr_store(base, ring_layout::g_h_write_sequence,
                static_cast<std::uint64_t>(0));
            std::memcpy(base + ring_layout::g_h_ring_id, _ring_id.data(),
                _ring_id.size() < (ring_layout::g_h_ring_id_max_bytes - 1)
                    ? _ring_id.size()
                    : (ring_layout::g_h_ring_id_max_bytes - 1));
            sequence_ = 0;
        } else {
            if (!vqec_vision_ai_qcom_qtvr_validate_attached_header(base)) {
                close();
                return false;
            }
            sequence_ = vqec_vision_ai_qcom_qtvr_read_u64(base,
                ring_layout::g_h_write_sequence);
        }
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
        std::uint32_t _height, std::uint64_t _frame_id, std::uint64_t _timestamp_ns,
        std::uint64_t _media_pts_ns, bool _keyframe) {
        if (mapping_ == nullptr || _data == nullptr || _size == 0 ||
            _size > ring_layout::g_payload_size) {
            return false;
        }
        const std::size_t index = sequence_ % ring_layout::g_slot_count;
        const std::size_t base = static_cast<std::size_t>(ring_layout::g_header_size) +
            index * (ring_layout::g_slot_header_size + ring_layout::g_payload_size);
        auto* slot = static_cast<std::uint8_t*>(mapping_);
        const std::uint32_t write_started =
            static_cast<std::uint32_t>((sequence_ * 2U) + 1U);
        const std::uint32_t write_finished = write_started + 1U;
        vqec_vision_ai_qcom_qtvr_store(slot, base + ring_layout::g_s_seqlock, write_started);
        vqec_vision_ai_qcom_qtvr_store(slot, base + ring_layout::g_s_data_size,
            static_cast<std::uint32_t>(_size));
        vqec_vision_ai_qcom_qtvr_store(slot, base + ring_layout::g_s_width, _width);
        vqec_vision_ai_qcom_qtvr_store(slot, base + ring_layout::g_s_height, _height);
        vqec_vision_ai_qcom_qtvr_store(slot, base + ring_layout::g_s_stride, _width);
        vqec_vision_ai_qcom_qtvr_store(slot, base + ring_layout::g_s_is_keyframe,
            static_cast<std::uint32_t>(_keyframe ? 1U : 0U));
        vqec_vision_ai_qcom_qtvr_store(slot, base + ring_layout::g_s_frame_id,
            _frame_id != 0 ? _frame_id : static_cast<std::uint64_t>(sequence_ + 1U));
        vqec_vision_ai_qcom_qtvr_store(slot, base + ring_layout::g_s_timestamp_ns,
            _timestamp_ns);
        vqec_vision_ai_qcom_qtvr_store(slot, base + ring_layout::g_s_media_pts_ns,
            _media_pts_ns != 0 ? _media_pts_ns : _timestamp_ns);
        vqec_vision_ai_qcom_qtvr_store(slot, base + ring_layout::g_s_sequence, sequence_);
        std::memcpy(slot + base + ring_layout::g_s_codec, "H264", 4);
        std::memcpy(slot + base + ring_layout::g_slot_header_size, _data, _size);
        // Seqlock producer barrier: metadata and payload must be visible before the slot is
        // published. Closing the fd is still not hardware completion.
        std::atomic_thread_fence(std::memory_order_release);
        vqec_vision_ai_qcom_qtvr_store(slot, base + ring_layout::g_s_seqlock, write_finished);
        ++sequence_;
        std::atomic_thread_fence(std::memory_order_release);
        vqec_vision_ai_qcom_qtvr_store(slot, ring_layout::g_h_write_sequence, sequence_);
        return true;
    }

private:
    static std::uint64_t vqec_vision_ai_qcom_qtvr_read_u64(
        const std::uint8_t* _base, std::size_t _offset) {
        std::uint64_t value = 0;
        std::memcpy(&value, _base + _offset, sizeof(value));
        return value;
    }

    static bool vqec_vision_ai_qcom_qtvr_validate_attached_header(
        const std::uint8_t* _base) {
        const auto read_u32 = [](const std::uint8_t* _source, std::size_t _offset) {
            std::uint32_t value = 0;
            std::memcpy(&value, _source + _offset, sizeof(value));
            return value;
        };
        return read_u32(_base, ring_layout::g_h_version) == ring_layout::g_version &&
            read_u32(_base, ring_layout::g_h_header_size) == ring_layout::g_header_size &&
            read_u32(_base, ring_layout::g_h_slot_header_size) ==
                ring_layout::g_slot_header_size &&
            read_u32(_base, ring_layout::g_h_slot_count) == ring_layout::g_slot_count &&
            read_u32(_base, ring_layout::g_h_payload_size) == ring_layout::g_payload_size;
    }

    std::string path_;
    int fd_{-1};
    void* mapping_{nullptr};
    std::size_t total_{0};
    std::uint64_t sequence_{0};
};

}  // namespace

struct qtiv_renderer::implementation {
    GstBuffer* vqec_vision_ai_qcom_qtvr_copy_nv12(
        const raw_frame& _frame, const overlay_batch& _overlay, bool _draw_overlay,
        status& _status);

    qtiv_renderer_config config_;
    GstElement* pipeline_{nullptr};
    GstElement* appsrc_{nullptr};
    GstElement* appsink_{nullptr};
    GstAllocator* dmabuf_allocator_{nullptr};
    rpcmem_pool surface_pool_;
    rpcmem_pool staging_pool_;
    renderer_surface_layout surface_layout_;
    std::vector<std::shared_ptr<renderer_surface_state>> surface_states_;
    std::array<vqec_vision_ai_dsp_v1_overlay_box,
               VQEC_VISION_AI_DSP_V1_OVERLAY_MAX_BOXES> overlay_boxes_{};
    std::array<std::uint8_t, VQEC_VISION_AI_DSP_V1_OVERLAY_MAX_LABEL_BYTES> labels_{};
    std::array<std::uint8_t, VQEC_VISION_AI_DSP_V1_OVERLAY_MAX_DESCRIPTOR_BYTES>
        descriptor_{};
    std::vector<std::shared_ptr<const void>> quarantined_mappings_;
    dsp_buffer_cache cpu_buffer_cache_{dsp_buffer_cache_config{32, false}};
    std::size_t staging_bytes_{0};
    bool staging_quarantined_{false};
    bool has_uncertain_completion_{false};
    fw_ring_writer ring_;
    std::uint64_t written_{0};
    // Monotonic push counter for PTS; it must advance on every push even when the encoder
    // has not produced an access unit yet, otherwise a reused PTS stalls v4l2h264enc.
    std::uint64_t submitted_{0};
    bool demand_gating_enabled_{false};
    bool has_demand_{true};
    std::uint64_t max_observation_age_ns_{500000000ULL};
    std::shared_ptr<dsp_buffer_cache> buffer_cache_{
        std::make_shared<dsp_buffer_cache>(dsp_buffer_cache_config{32, false})};
    std::shared_ptr<dsp_v1_client> dsp_client_;
    bool is_open_{false};
};

qtiv_renderer::qtiv_renderer() : implementation_(std::make_unique<implementation>()) {}

qtiv_renderer::~qtiv_renderer() noexcept {
    vqec_vision_ai_qcom_qtvr_close();
    if (implementation_ != nullptr && implementation_->has_uncertain_completion_) {
        // Process-lifetime quarantine: timeout/close/destruction is not hardware completion.
        // The leaked faulted implementation retains every DSP/encoder-visible resource until
        // BSP process-domain teardown. Reinitialization is forbidden below.
        (void)implementation_.release();
    }
}

status qtiv_renderer::vqec_vision_ai_qcom_qtvr_init(const qtiv_renderer_config& _config) {
    if (implementation_ == nullptr) {
        return {status_code::invalid_state, "qtiv renderer is unavailable"};
    }
    auto& impl = *implementation_;
    if (impl.has_uncertain_completion_) {
        return {status_code::invalid_state,
            "qtiv renderer is quarantined after uncertain hardware completion"};
    }
    if (impl.is_open_) {
        return {status_code::invalid_state, "qtiv renderer is already initialized"};
    }
    if (_config.width_ == 0 || _config.height_ == 0 || _config.fps_ == 0 ||
        _config.bitrate_bps_ == 0 || _config.keyframe_interval_frames_ == 0 ||
        _config.output_surface_count_ == 0U ||
        (_config.box_color_rgba_ & g_rgba_alpha_mask) == 0 ||
        _config.ring_id_.empty() || _config.colorimetry_.empty() ||
        _config.interlace_mode_.empty() || _config.buffer_cache_ == nullptr ||
        _config.dsp_client_ == nullptr ||
        !_config.dsp_client_->vqec_vision_ai_qcom_d1cli_is_open() ||
        _config.colorimetry_ != "bt709" || _config.interlace_mode_ != "progressive") {
        return {status_code::invalid_argument, "invalid qtiv renderer configuration"};
    }
    impl.config_ = _config;
    impl.buffer_cache_ = _config.buffer_cache_;
    impl.dsp_client_ = _config.dsp_client_;
    const auto capabilities = impl.dsp_client_->vqec_vision_ai_qcom_d1cli_capabilities();
    if ((capabilities.operations_mask &
         (1U << (VQEC_VISION_AI_DSP_V1_OVERLAY_COMPOSE - 1U))) == 0U) {
        return {status_code::unsupported, "DSP v1 does not advertise overlay compose"};
    }
    impl.surface_layout_ = vqec_vision_ai_qcom_qtvr_make_surface_layout(
        _config.width_, _config.height_);
    if (impl.surface_layout_.bytes_ == 0U ||
        impl.surface_layout_.bytes_ > VQEC_VISION_AI_DSP_V1_OVERLAY_MAX_SURFACE_BYTES) {
        return {status_code::invalid_argument, "invalid direct-import surface layout"};
    }
    const auto allocated = impl.surface_pool_.vqec_vision_ai_qcom_rpcm_allocate(
        impl.surface_layout_.bytes_, _config.output_surface_count_);
    if (allocated.code_ != status_code::ok) {
        return allocated;
    }
    impl.surface_states_.reserve(_config.output_surface_count_);
    for (std::uint32_t index = 0; index < _config.output_surface_count_; ++index) {
        impl.surface_states_.push_back(std::make_shared<renderer_surface_state>());
    }
    if (!impl.ring_.open(_config.ring_id_)) {
        return {status_code::io_error, "cannot open the FW encoded ring"};
    }
    gst_init(nullptr, nullptr);
    const std::string bitrate = std::to_string(_config.bitrate_bps_);
    const std::string keyframe_interval =
        std::to_string(_config.keyframe_interval_frames_);
    const std::string description =
        "appsrc name=src is-live=true format=time"
        " ! capsfilter name=surfacecaps"
        " ! queue max-size-buffers=2"
        " ! v4l2h264enc capture-io-mode=dmabuf output-io-mode=dmabuf-import"
        " extra-controls=\"controls,video_bitrate=" + bitrate +
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
        return {status_code::unsupported, "cannot build the direct-import encode pipeline"};
    }
    impl.appsrc_ = gst_bin_get_by_name(GST_BIN(impl.pipeline_), "src");
    impl.appsink_ = gst_bin_get_by_name(GST_BIN(impl.pipeline_), "enc");
    GstElement* surface_caps = gst_bin_get_by_name(GST_BIN(impl.pipeline_), "surfacecaps");
    if (impl.appsrc_ == nullptr || impl.appsink_ == nullptr || surface_caps == nullptr) {
        if (surface_caps != nullptr) {
            gst_object_unref(surface_caps);
        }
        return {status_code::unsupported,
            "direct-import pipeline is missing appsrc, surface caps, or appsink"};
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
    gst_object_unref(surface_caps);
    gst_caps_unref(caps);
    impl.dmabuf_allocator_ = gst_dmabuf_allocator_new();
    if (impl.dmabuf_allocator_ == nullptr) {
        return {status_code::resource_exhausted,
            "cannot create the DMA-BUF import allocator"};
    }
    impl.is_open_ = true;
    if (gst_element_set_state(impl.pipeline_, GST_STATE_PLAYING) ==
        GST_STATE_CHANGE_FAILURE) {
        vqec_vision_ai_qcom_qtvr_close();
        return {status_code::io_error, "cannot start the direct-import encode pipeline"};
    }
    return {};
}

GstBuffer* qtiv_renderer::implementation::vqec_vision_ai_qcom_qtvr_copy_nv12(
    const raw_frame& _frame, const overlay_batch& _overlay, bool _draw_overlay,
    status& _status) {
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
        _status = {status_code::invalid_argument, "invalid RAW NV12 descriptor"};
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
            _status = {status_code::invalid_argument, "RAW NV12 plane exceeds its view"};
            return nullptr;
        }
    }
    std::size_t surface_index = surface_states_.size();
    for (std::size_t index = 0; index < surface_states_.size(); ++index) {
        bool expected = false;
        if (surface_states_[index]->busy_.compare_exchange_strong(
                expected, true, std::memory_order_acq_rel)) {
            surface_index = index;
            break;
        }
    }
    if (surface_index == surface_states_.size()) {
        _status = {status_code::resource_exhausted,
            "all direct-import preview surfaces are in flight"};
        return nullptr;
    }
    const auto release_surface = [this, surface_index]() {
        surface_states_[surface_index]->busy_.store(false, std::memory_order_release);
    };
    const auto& surface = surface_pool_.vqec_vision_ai_qcom_rpcm_slot(surface_index);
    if (surface.data_ == nullptr || surface.fd_ < 0 ||
        surface.size_ < surface_layout_.bytes_) {
        release_surface();
        _status = {status_code::invalid_state, "direct-import surface is unavailable"};
        return nullptr;
    }
    const int frame_fd = static_cast<int>(_frame.native_handle_);
    const std::size_t alloc_size = static_cast<std::size_t>(descriptor.allocation_size_bytes_);
    status map_status;
    auto mapped = buffer_cache_->vqec_vision_ai_qcom_dspbc_map(
        frame_fd, alloc_size, map_status);
    bool staged_input = false;
    const std::uint8_t* dsp_input = nullptr;
    std::size_t dsp_input_bytes = static_cast<std::size_t>(descriptor.view_size_bytes_);
    if (mapped.data_ != nullptr && map_status.code_ == status_code::ok) {
        dsp_input = mapped.data_ + descriptor.memory_offset_bytes_;
    } else if (map_status.code_ == status_code::unsupported) {
        if (staging_quarantined_) {
            release_surface();
            _status = {status_code::invalid_state,
                "DSP staging input is quarantined after uncertain completion"};
            return nullptr;
        }
        status cpu_map_status;
        mapped = cpu_buffer_cache_.vqec_vision_ai_qcom_dspbc_map(
            frame_fd, alloc_size, cpu_map_status);
        if (mapped.data_ == nullptr || cpu_map_status.code_ != status_code::ok) {
            release_surface();
            _status = cpu_map_status;
            return nullptr;
        }
        if (staging_pool_.vqec_vision_ai_qcom_rpcm_count() == 0U ||
            staging_bytes_ < dsp_input_bytes) {
            const auto allocated = staging_pool_.vqec_vision_ai_qcom_rpcm_allocate(
                dsp_input_bytes, 1U);
            if (allocated.code_ != status_code::ok) {
                release_surface();
                _status = allocated;
                return nullptr;
            }
            staging_bytes_ = staging_pool_.vqec_vision_ai_qcom_rpcm_slot(0U).size_;
        }
        const auto& staging = staging_pool_.vqec_vision_ai_qcom_rpcm_slot(0U);
        if (staging.data_ == nullptr || staging.size_ < dsp_input_bytes) {
            release_surface();
            _status = {status_code::invalid_state, "DSP staging input is unavailable"};
            return nullptr;
        }
        std::memcpy(staging.data_, mapped.data_ + descriptor.memory_offset_bytes_,
            dsp_input_bytes);
        dsp_input = static_cast<const std::uint8_t*>(staging.data_);
        staged_input = true;
    } else {
        release_surface();
        _status = map_status;
        return nullptr;
    }
    std::uint32_t label_bytes = 0U;
    std::uint32_t box_count = 0U;
    if (_draw_overlay) {
        for (const auto& item : _overlay.boxes_) {
            if (box_count >= overlay_boxes_.size() ||
                item.label_.size() > UINT16_MAX ||
                item.label_.size() > labels_.size() - label_bytes) {
                release_surface();
                _status = {status_code::resource_exhausted,
                    "overlay metadata exceeds negotiated DSP bounds"};
                return nullptr;
            }
            auto& box = overlay_boxes_[box_count];
            box = {};
            box.x = static_cast<std::uint32_t>(item.x_);
            box.y = static_cast<std::uint32_t>(item.y_);
            box.width = static_cast<std::uint32_t>(item.width_);
            box.height = static_cast<std::uint32_t>(item.height_);
            const std::uint32_t rgba =
                item.rgba_ != 0U && item.rgba_ != 0xffffffffU ?
                    item.rgba_ : config_.box_color_rgba_;
            vqec_vision_ai_qcom_qtvr_rgba_to_bt709_limited(
                rgba, box.color_y, box.color_u, box.color_v);
            box.label_offset = label_bytes;
            box.label_bytes = static_cast<std::uint16_t>(item.label_.size());
            if (!item.label_.empty()) {
                std::memcpy(labels_.data() + label_bytes, item.label_.data(), item.label_.size());
                label_bytes += static_cast<std::uint32_t>(item.label_.size());
            }
            ++box_count;
        }
    }
    vqec_vision_ai_dsp_v1_overlay_frame frame{};
    frame.width = descriptor.width_;
    frame.height = descriptor.height_;
    frame.source_y_offset = descriptor.offsets_[0];
    frame.source_y_stride = static_cast<std::uint32_t>(descriptor.strides_[0]);
    frame.source_uv_offset = descriptor.offsets_[1];
    frame.source_uv_stride = static_cast<std::uint32_t>(descriptor.strides_[1]);
    frame.destination_y_offset = 0U;
    frame.destination_y_stride = surface_layout_.y_stride_;
    frame.destination_uv_offset = surface_layout_.uv_offset_;
    frame.destination_uv_stride = surface_layout_.uv_stride_;
    frame.border_thickness = g_overlay_border_thickness;
    frame.font_scale = g_overlay_font_scale;
    std::size_t descriptor_bytes = 0U;
    const auto capabilities = dsp_client_->vqec_vision_ai_qcom_d1cli_capabilities();
    const auto encoded = vqec_vision_ai_qcom_d1ovr_encode_descriptor(
        &capabilities, &frame, overlay_boxes_.data(), box_count, labels_.data(), label_bytes,
        static_cast<std::uint32_t>(descriptor.view_size_bytes_),
        static_cast<std::uint32_t>(surface.size_), descriptor_.data(), descriptor_.size(),
        &descriptor_bytes);
    if (encoded != vqec_vision_ai_dsp_v1_wire_ok) {
        release_surface();
        _status = {status_code::invalid_argument,
            "cannot encode bounded DSP overlay descriptor"};
        return nullptr;
    }
    const auto call = dsp_client_->vqec_vision_ai_qcom_d1cli_execute(
        descriptor_.data(), descriptor_bytes,
        dsp_input, dsp_input_bytes,
        static_cast<std::uint8_t*>(surface.data_), surface.size_);
    if (call.completion_ == dsp_v1_completion::uncertain) {
        has_uncertain_completion_ = true;
        if (staged_input) {
            staging_quarantined_ = true;
        } else {
            quarantined_mappings_.push_back(mapped.owner_);
        }
        _status = call.status_;
        return nullptr;
    }
    if (call.status_.code_ != status_code::ok ||
        call.output_bytes_ < surface_layout_.uv_offset_ +
            surface_layout_.uv_stride_ * (descriptor.height_ / g_nv12_chroma_row_divisor)) {
        release_surface();
        _status = call.status_.code_ != status_code::ok ? call.status_ :
            status{status_code::protocol_error, "DSP overlay returned a short surface"};
        return nullptr;
    }
    const int retained_fd = ::fcntl(surface.fd_, F_DUPFD_CLOEXEC, 0);
    if (retained_fd < 0) {
        release_surface();
        _status = {status_code::io_error, "cannot retain the direct-import surface FD"};
        return nullptr;
    }
    GstMemory* memory = gst_dmabuf_allocator_alloc(
        dmabuf_allocator_, retained_fd, surface.size_);
    GstBuffer* buffer = gst_buffer_new();
    if (memory == nullptr || buffer == nullptr) {
        if (memory != nullptr) {
            gst_memory_unref(memory);
        } else {
            ::close(retained_fd);
        }
        if (buffer != nullptr) {
            gst_buffer_unref(buffer);
        }
        release_surface();
        _status = {status_code::resource_exhausted, "cannot wrap direct-import surface"};
        return nullptr;
    }
    gst_buffer_append_memory(buffer, memory);
    gsize offsets[GST_VIDEO_MAX_PLANES] = {0U, surface_layout_.uv_offset_, 0U, 0U};
    gint strides[GST_VIDEO_MAX_PLANES] = {
        static_cast<gint>(surface_layout_.y_stride_),
        static_cast<gint>(surface_layout_.uv_stride_), 0, 0};
    if (gst_buffer_add_video_meta_full(
            buffer, GST_VIDEO_FRAME_FLAG_NONE, GST_VIDEO_FORMAT_NV12,
            descriptor.width_, descriptor.height_, g_nv12_plane_count, offsets, strides) ==
        nullptr) {
        gst_buffer_unref(buffer);
        release_surface();
        _status = {status_code::resource_exhausted, "cannot attach NV12 import metadata"};
        return nullptr;
    }
    gst_mini_object_weak_ref(GST_MINI_OBJECT(buffer),
        vqec_vision_ai_qcom_qtvr_release_surface, surface_states_[surface_index].get());
    _status = {};
    return buffer;
}

status qtiv_renderer::vqec_vision_ai_qcom_qtvr_render(
    const raw_frame& _frame, const prepared_overlay& _payload) {
    if (implementation_ == nullptr || !implementation_->is_open_) {
        return {status_code::invalid_state, "qtiv renderer is not initialized"};
    }
    auto& impl = *implementation_;
    if (impl.has_uncertain_completion_) {
        return {status_code::invalid_state,
            "qtiv renderer is quarantined after uncertain hardware completion"};
    }
    if (impl.demand_gating_enabled_ && !impl.has_demand_) {
        return {status_code::pending, "no preview demand"};
    }

    struct timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    const std::uint64_t now_ns = static_cast<std::uint64_t>(ts.tv_sec) * 1000000000ULL +
        static_cast<std::uint64_t>(ts.tv_nsec);

    if (_payload.rendered_scopes_.empty() ||
        _payload.rendered_scopes_.size() > output_policy_limits::g_max_rendered_scopes ||
        _payload.overlay_.policy_revision_ == 0) {
        return {status_code::unauthorized,
            "renderer requires a prepared authorized scope"};
    }
    for (const auto& scope : _payload.rendered_scopes_) {
        if (scope.policy_revision_ != _payload.overlay_.policy_revision_ ||
            scope.source_id_.empty() || scope.feature_id_.empty() ||
            scope.attributes_.size() > output_policy_limits::g_max_attributes_per_scope) {
            return {status_code::unauthorized,
                "renderer prepared scope identity is invalid"};
        }
    }
    const auto prepared = vqec_vision_ai_core_pvctr_validate_overlay(
        _payload.overlay_, _payload.overlay_.frame_, _payload.overlay_.geometry_,
        _payload.overlay_.policy_revision_, now_ns, _payload.overlay_.ttl_ns_);
    if (prepared.code_ != status_code::ok) {
        return prepared;
    }

    bool drop_boxes = false;
    if (_payload.overlay_.frame_.source_epoch_ != 0 &&
        _payload.overlay_.frame_.source_epoch_ != _frame.descriptor_.session_epoch_) {
        drop_boxes = true;
    }
    const std::uint64_t max_age = _payload.overlay_.ttl_ns_ != 0 ?
        _payload.overlay_.ttl_ns_ : impl.max_observation_age_ns_;
    if (_payload.overlay_.prepared_monotonic_ns_ != 0 &&
        now_ns > _payload.overlay_.prepared_monotonic_ns_ &&
        now_ns - _payload.overlay_.prepared_monotonic_ns_ > max_age) {
        drop_boxes = true;
    }
    if (_payload.overlay_.frame_.source_pts_ns_ != UINT64_MAX &&
        _frame.descriptor_.pts_ns_ != UINT64_MAX && _frame.descriptor_.pts_ns_ != 0 &&
        _frame.descriptor_.pts_ns_ > _payload.overlay_.frame_.source_pts_ns_ &&
        _frame.descriptor_.pts_ns_ - _payload.overlay_.frame_.source_pts_ns_ > max_age) {
        drop_boxes = true;
    }

    status compose_status;
    GstBuffer* buffer = impl.vqec_vision_ai_qcom_qtvr_copy_nv12(
        _frame, _payload.overlay_, !drop_boxes, compose_status);
    if (buffer == nullptr) {
        return compose_status.code_ != status_code::ok ? compose_status :
            status{status_code::io_error,
                "cannot compose the NV12 frame on the DSP render surface"};
    }
    if (drop_boxes && !_payload.overlay_.boxes_.empty()) {
        static std::uint64_t s_last_renderer_drop_ns = 0;
        if (now_ns - s_last_renderer_drop_ns > 2000000000ULL) {
            std::fprintf(stderr, "qtiv_renderer dropped %zu boxes (epoch/monotonic/pts)\n",
                _payload.overlay_.boxes_.size());
            s_last_renderer_drop_ns = now_ns;
        }
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
            const GstClockTime encoded_pts = GST_BUFFER_PTS(encoded);
            const std::uint64_t timestamp_ns =
                GST_CLOCK_TIME_IS_VALID(encoded_pts) ? encoded_pts : 0U;
            const std::uint64_t frame_pts =
                _frame.descriptor_.pts_ns_ != UINT64_MAX && _frame.descriptor_.pts_ns_ != 0 ?
                    _frame.descriptor_.pts_ns_ : timestamp_ns;
            if (impl.ring_.push(static_cast<const std::uint8_t*>(out_map.data), out_map.size,
                    _frame.descriptor_.width_, _frame.descriptor_.height_,
                    _frame.descriptor_.buffer_id_,
                    frame_pts,
                    frame_pts,
                    keyframe)) {
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

void qtiv_renderer::vqec_vision_ai_qcom_qtvr_set_demand(bool _has_demand) noexcept {
    if (implementation_ != nullptr) {
        implementation_->has_demand_ = _has_demand;
    }
}

bool qtiv_renderer::vqec_vision_ai_qcom_qtvr_has_demand() const noexcept {
    return implementation_ != nullptr && implementation_->has_demand_;
}

void qtiv_renderer::vqec_vision_ai_qcom_qtvr_set_demand_gating(bool _enabled) noexcept {
    if (implementation_ != nullptr) {
        implementation_->demand_gating_enabled_ = _enabled;
    }
}

bool qtiv_renderer::vqec_vision_ai_qcom_qtvr_is_demand_gating_enabled() const noexcept {
    return implementation_ != nullptr && implementation_->demand_gating_enabled_;
}

void qtiv_renderer::vqec_vision_ai_qcom_qtvr_set_max_observation_age(std::uint64_t _max_age_ns) noexcept {
    if (implementation_ != nullptr) {
        implementation_->max_observation_age_ns_ = _max_age_ns;
    }
}

std::uint64_t qtiv_renderer::vqec_vision_ai_qcom_qtvr_get_max_observation_age() const noexcept {
    return implementation_ != nullptr ? implementation_->max_observation_age_ns_ : 0;
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
    if (impl.dmabuf_allocator_ != nullptr) {
        gst_object_unref(impl.dmabuf_allocator_);
        impl.dmabuf_allocator_ = nullptr;
    }
    for (const auto& state : impl.surface_states_) {
        if (state != nullptr && state->busy_.load(std::memory_order_acquire)) {
            impl.has_uncertain_completion_ = true;
            break;
        }
    }
    if (!impl.has_uncertain_completion_) {
        impl.surface_states_.clear();
        impl.surface_pool_.vqec_vision_ai_qcom_rpcm_release();
        impl.staging_pool_.vqec_vision_ai_qcom_rpcm_release();
        impl.staging_bytes_ = 0U;
        impl.quarantined_mappings_.clear();
        impl.dsp_client_.reset();
    }
    impl.ring_.close();
    impl.is_open_ = false;
}

}  // namespace vqec::vision::ai
