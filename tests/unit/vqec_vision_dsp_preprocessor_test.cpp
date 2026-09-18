#include <sys/mman.h>
#include <unistd.h>

#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <vector>

#include "vqec_vision_dsp_buffer_cache.hpp"
#include "vqec_vision_dsp_preprocessor.hpp"
#include "vqec_vision_dsp_session.hpp"

namespace vqec::vision::ai {
namespace {

int vqec_vision_ai_unit_dsppt_test_geom() {
    // 1920x1080 -> 640x640 YOLOv8 (center padded, pad 114)
    const auto geom_yolo = dsp_preprocessor::vqec_vision_ai_qcom_dsppr_compute_geom(
        dsp_preprocessor_kind::yolov8, 1920, 1080, 1920, 1920 * 1080, 1920, 640);
    if (geom_yolo[0] != 1920 || geom_yolo[1] != 1080 || geom_yolo[5] != 640 || geom_yolo[6] != 640) {
        std::cerr << "YOLOv8 geom basic dimensions failed\n";
        return 1;
    }
    if (geom_yolo[11] != 114) {
        std::cerr << "YOLOv8 geom pad value failed, expected 114, got " << geom_yolo[11] << "\n";
        return 1;
    }
    // 1080 * (640 / 1920) = 360; (640 - 360) / 2 = 140
    if (geom_yolo[9] != 640 || geom_yolo[10] != 360 || geom_yolo[7] != 0 || geom_yolo[8] != 140) {
        std::cerr << "YOLOv8 geom letterbox coordinates failed: dst_w=" << geom_yolo[9]
                  << " dst_h=" << geom_yolo[10] << " dst_x=" << geom_yolo[7]
                  << " dst_y=" << geom_yolo[8] << "\n";
        return 1;
    }

    // 1920x1080 -> 640x640 SCRFD (top-left aligned, pad 0)
    const auto geom_scrfd = dsp_preprocessor::vqec_vision_ai_qcom_dsppr_compute_geom(
        dsp_preprocessor_kind::scrfd, 1920, 1080, 1920, 1920 * 1080, 1920, 640);
    if (geom_scrfd[11] != 0) {
        std::cerr << "SCRFD geom pad value failed, expected 0, got " << geom_scrfd[11] << "\n";
        return 1;
    }
    if (geom_scrfd[7] != 0 || geom_scrfd[8] != 0 || geom_scrfd[9] != 640 || geom_scrfd[10] != 360) {
        std::cerr << "SCRFD geom coordinates failed: dst_w=" << geom_scrfd[9]
                  << " dst_h=" << geom_scrfd[10] << " dst_x=" << geom_scrfd[7]
                  << " dst_y=" << geom_scrfd[8] << "\n";
        return 1;
    }
    return 0;
}

int vqec_vision_ai_unit_dsppt_test_buffer_cache() {
    dsp_buffer_cache cache(dsp_buffer_cache_config{4, false});
    const int fd = ::memfd_create("test_dma_cache", 0);
    if (fd < 0) {
        std::cerr << "memfd_create failed\n";
        return 1;
    }
    const std::size_t test_size = 4096;
    if (::ftruncate(fd, static_cast<off_t>(test_size)) != 0) {
        ::close(fd);
        std::cerr << "ftruncate failed\n";
        return 1;
    }

    status s1;
    auto p1 = cache.vqec_vision_ai_qcom_dspbc_map(fd, test_size, s1);
    if (p1.data_ == nullptr || s1.code_ != status_code::ok) {
        ::close(fd);
        std::cerr << "first map failed: " << s1.message_ << "\n";
        return 1;
    }
    if (cache.vqec_vision_ai_qcom_dspbc_entry_count() != 1) {
        ::close(fd);
        std::cerr << "entry count expected 1, got "
                  << cache.vqec_vision_ai_qcom_dspbc_entry_count() << "\n";
        return 1;
    }

    status s2;
    auto p2 = cache.vqec_vision_ai_qcom_dspbc_map(fd, test_size, s2);
    if (p2.data_ != p1.data_ || s2.code_ != status_code::ok) {
        ::close(fd);
        std::cerr << "cache hit returned different pointer or failed\n";
        return 1;
    }

    p1 = {};
    p2 = {};
    cache.vqec_vision_ai_qcom_dspbc_clear();
    if (cache.vqec_vision_ai_qcom_dspbc_entry_count() != 0) {
        ::close(fd);
        std::cerr << "clear did not empty cache\n";
        return 1;
    }

    ::close(fd);
    return 0;
}

int vqec_vision_ai_unit_dsppt_test_preprocessor() {
    auto session = std::make_shared<dsp_session>();
    auto cache = std::make_shared<dsp_buffer_cache>(dsp_buffer_cache_config{4, false});

    dsp_preprocessor_config cfg;
    cfg.kind_ = dsp_preprocessor_kind::yolov8;
    cfg.session_ = session;
    cfg.buffer_cache_ = cache;

    dsp_preprocessor preprocessor(cfg);

    const int fd = ::memfd_create("test_frame", 0);
    if (fd < 0) {
        std::cerr << "memfd_create failed\n";
        return 1;
    }
    const std::uint32_t width = 64;
    const std::uint32_t height = 32;
    const std::size_t frame_bytes = static_cast<std::size_t>(width * height + width * (height / 2));
    if (::ftruncate(fd, static_cast<off_t>(frame_bytes)) != 0) {
        ::close(fd);
        std::cerr << "ftruncate failed\n";
        return 1;
    }

    // Fill with gray NV12
    void* map_ptr = ::mmap(nullptr, frame_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (map_ptr != MAP_FAILED) {
        std::memset(map_ptr, 128, frame_bytes);
        ::munmap(map_ptr, frame_bytes);
    }

    raw_frame frame;
    frame.native_handle_ = fd;
    frame.descriptor_.width_ = width;
    frame.descriptor_.height_ = height;
    frame.descriptor_.strides_[0] = static_cast<std::int32_t>(width);
    frame.descriptor_.offsets_[1] = width * height;
    frame.descriptor_.strides_[1] = static_cast<std::int32_t>(width);
    frame.descriptor_.allocation_size_bytes_ = frame_bytes;

    inference_plan plan;
    tensor_spec target;
    target.dtype_ = tensor_element_type::uint16;
    target.layout_ = tensor_layout::nhwc;
    target.dimensions_ = {1, 32, 32, 3};

    const auto valid = preprocessor.vqec_vision_ai_ports_imgpr_validate(frame, plan, target);
    if (valid.code_ != status_code::ok) {
        ::close(fd);
        std::cerr << "validate failed: " << valid.message_ << "\n";
        return 1;
    }

    std::vector<tensor_blob> outputs;
    const auto prep = preprocessor.vqec_vision_ai_ports_imgpr_preprocess(frame, plan, target, outputs);
    if (prep.code_ != status_code::ok) {
        ::close(fd);
        std::cerr << "preprocess failed: " << prep.message_ << "\n";
        return 1;
    }

    if (outputs.size() != 1) {
        ::close(fd);
        std::cerr << "outputs size expected 1, got " << outputs.size() << "\n";
        return 1;
    }
    const std::size_t expected_bytes = 32 * 32 * 3 * sizeof(std::uint16_t);
    if (outputs[0].bytes_.size() != expected_bytes) {
        ::close(fd);
        std::cerr << "output byte count expected " << expected_bytes
                  << ", got " << outputs[0].bytes_.size() << "\n";
        return 1;
    }

    // Verify top pad band (pad = 114, scaled u16 = 114 * 257 = 29298)
    const auto* out_u16 = reinterpret_cast<const std::uint16_t*>(outputs[0].bytes_.data());
    const std::uint16_t expected_pad = 114 * 257;
    if (out_u16[0] != expected_pad) {
        ::close(fd);
        std::cerr << "expected pad value " << expected_pad << ", got " << out_u16[0] << "\n";
        return 1;
    }

    ::close(fd);
    return 0;
}

}  // namespace
}  // namespace vqec::vision::ai

int main() {
    if (vqec::vision::ai::vqec_vision_ai_unit_dsppt_test_geom() != 0) {
        return 1;
    }
    if (vqec::vision::ai::vqec_vision_ai_unit_dsppt_test_buffer_cache() != 0) {
        return 1;
    }
    if (vqec::vision::ai::vqec_vision_ai_unit_dsppt_test_preprocessor() != 0) {
        return 1;
    }
    std::cout << "dsp_preprocessor_test PASSED\n";
    return 0;
}
