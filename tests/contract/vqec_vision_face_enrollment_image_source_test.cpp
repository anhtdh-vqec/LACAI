#include <cstdio>
#include <iostream>
#include <unistd.h>

#include <gst/gst.h>

#include "vqec_vision_face_enrollment_image_source.hpp"

int main() {
    using namespace vqec::vision::ai;
    // Owned synthetic JPEG fixture. Width 18 deliberately exercises padded Gst planes.
    constexpr std::uint32_t fixture_width = 18;
    constexpr std::uint32_t fixture_height = 16;
    constexpr std::uint32_t fixture_timeout_ms = 2000;
    constexpr std::uint64_t fixture_bytes = fixture_width * fixture_height * 3 / 2;
    char fixture_path[] = "/tmp/lacai-enrollment-jpeg-XXXXXX";
    const int fixture_fd = mkstemp(fixture_path);
    if (fixture_fd < 0) return 1;
    close(fixture_fd);
    gst_init(nullptr, nullptr);
    // The eSDK QEMU image may omit encoder plugins; the target board test exercises decode.
    if (gst_element_factory_find("videotestsrc") == nullptr ||
        gst_element_factory_find("jpegenc") == nullptr) {
        qcom_face_enrollment_image_source source({"jpegdec", "videoconvert", "videoscale",
                                                  fixture_bytes, fixture_timeout_ms});
        face_enrollment_image image;
        const face_enrollment_image_request invalid{fixture_path, 1, 1, 1, fixture_width + 1,
                                                    fixture_height};
        const auto result = source.vqec_vision_ai_ports_feimg_load(invalid, image);
        std::remove(fixture_path);
        return result.code_ == status_code::invalid_argument ? 0 : 1;
    }
    GError* error = nullptr;
    GstElement* pipeline = gst_parse_launch(
        "videotestsrc num-buffers=1 pattern=black ! "
        "video/x-raw,format=I420,width=18,height=16 ! jpegenc ! filesink name=fixture_sink",
        &error);
    if (error != nullptr || pipeline == nullptr) {
        if (error != nullptr) g_error_free(error);
        if (pipeline != nullptr) gst_object_unref(pipeline);
        std::remove(fixture_path);
        return 2;
    }
    GstElement* sink = gst_bin_get_by_name(GST_BIN(pipeline), "fixture_sink");
    g_object_set(sink, "location", fixture_path, nullptr);
    gst_object_unref(sink);
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    GstBus* bus = gst_element_get_bus(pipeline);
    GstMessage* message = gst_bus_timed_pop_filtered(bus, fixture_timeout_ms * GST_MSECOND,
        static_cast<GstMessageType>(GST_MESSAGE_EOS | GST_MESSAGE_ERROR));
    const bool produced = message != nullptr && GST_MESSAGE_TYPE(message) == GST_MESSAGE_EOS;
    if (message != nullptr) gst_message_unref(message);
    gst_object_unref(bus);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    if (!produced) {
        // Cross SDK QEMU may expose the target registry cache but cannot execute the
        // target plugin scanner. Keep request validation evidence in that environment;
        // native target CI exercises the decode assertions below.
        qcom_face_enrollment_image_source source({"jpegdec", "videoconvert", "videoscale",
                                                  fixture_bytes, fixture_timeout_ms});
        face_enrollment_image image;
        const face_enrollment_image_request invalid{fixture_path, 1, 1, 1, fixture_width + 1,
                                                    fixture_height};
        std::remove(fixture_path);
        return source.vqec_vision_ai_ports_feimg_load(invalid, image).code_ ==
            status_code::invalid_argument ? 0 : 3;
    }
    qcom_face_enrollment_image_source source({"jpegdec", "videoconvert", "videoscale",
                                              fixture_bytes, fixture_timeout_ms});
    face_enrollment_image image;
    const face_enrollment_image_request request{
        fixture_path, 1, 1, 1, fixture_width, fixture_height};
    const auto loaded = source.vqec_vision_ai_ports_feimg_load(request, image);
    std::remove(fixture_path);
    if (loaded.code_ != status_code::ok || !image.nv12_ || image.nv12_->size() != fixture_bytes ||
        image.frame_.descriptor_.offsets_[1] != fixture_width * fixture_height ||
        !image.frame_.owner_) {
        std::cerr << loaded.message_ << '\n';
        return 4;
    }
    // Black JPEG converts to nominal limited-range black; both chroma planes are neutral.
    // JPEG and converter range handling may choose limited or full-range black.
    constexpr std::uint8_t fixture_luma_min = 0;
    constexpr std::uint8_t fixture_luma_max = 32;
    constexpr std::uint8_t fixture_chroma_min = 112;
    constexpr std::uint8_t fixture_chroma_max = 144;
    for (std::size_t index = 0; index < image.nv12_->size(); ++index) {
        const auto value = (*image.nv12_)[index];
        const bool luma = index < fixture_width * fixture_height;
        if (value < (luma ? fixture_luma_min : fixture_chroma_min) ||
            value > (luma ? fixture_luma_max : fixture_chroma_max)) return 5;
    }
    const auto retained = image.nv12_;
    auto invalid = request;
    invalid.width_ += 1;
    if (source.vqec_vision_ai_ports_feimg_load(invalid, image).code_ != status_code::invalid_argument ||
        image.nv12_ != retained) return 6;
    return 0;
}
