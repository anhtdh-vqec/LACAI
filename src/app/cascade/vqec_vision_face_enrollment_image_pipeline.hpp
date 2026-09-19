#ifndef VQEC_VISION_AI_APPL_FACE_ENROLLMENT_IMAGE_PIPELINE_HPP
#define VQEC_VISION_AI_APPL_FACE_ENROLLMENT_IMAGE_PIPELINE_HPP

#include "vqec/vision/ai/ports/perception/vqec_vision_face_enrollment.hpp"
#include "vqec/vision/ai/ports/perception/vqec_vision_face_enrollment_image.hpp"
#include "vqec/vision/ai/ports/perception/vqec_vision_image_path_authorizer.hpp"
#include "vqec/vision/ai/ports/perception/vqec_vision_face_image_inference.hpp"

namespace vqec::vision::ai {

struct face_enrollment_image_pipeline_config {
    face_enrollment_port* controller_{nullptr};
    image_path_authorizer_port* path_authorizer_{nullptr};
    face_enrollment_image_source_port* image_source_{nullptr};
    face_image_detector_port* detector_{nullptr};
    face_image_cascade_port* cascade_{nullptr};
    preview_geometry geometry_;
    std::uint64_t source_epoch_{0};
    std::string source_id_;
    std::uint32_t camera_id_{0};
    std::uint32_t channel_id_{0};
};

// One bounded image job. BeginEnrollment returns collecting; the serialized service loop
// calls step and observes completed/failed through the same controller status.
class face_enrollment_image_pipeline final : public face_enrollment_port {
public:
    [[nodiscard]] status vqec_vision_ai_appl_feipl_configure(
        const face_enrollment_image_pipeline_config& _config);
    [[nodiscard]] status vqec_vision_ai_appl_feipl_step(std::uint64_t _steady_now_ns);
    [[nodiscard]] bool vqec_vision_ai_appl_feipl_has_pending() const noexcept;

    [[nodiscard]] status vqec_vision_ai_ports_fenrl_begin(
        const face_enrollment_begin_request& _request,
        face_enrollment_status& _status) override;
    [[nodiscard]] status vqec_vision_ai_ports_fenrl_cancel(
        const std::string& _request_id, face_enrollment_status& _status) override;
    [[nodiscard]] status vqec_vision_ai_ports_fenrl_remove_subject(
        const std::string& _subject_ref, std::uint64_t _expected_gallery_revision,
        std::uint64_t& _new_gallery_revision) override;
    [[nodiscard]] status vqec_vision_ai_ports_fenrl_get_status(
        const std::string& _request_id, face_enrollment_status& _status) const override;
    [[nodiscard]] status vqec_vision_ai_ports_fenrl_get_gallery_status(
        face_gallery_status& _status) const override;
    [[nodiscard]] status vqec_vision_ai_ports_fenrl_fail(
        const std::string& _request_id, status_code _error,
        face_enrollment_status& _status) override;
    [[nodiscard]] status vqec_vision_ai_ports_fenrl_accept_embedding(
        const embedding_result& _embedding, face_enrollment_status& _status) override;
    [[nodiscard]] status vqec_vision_ai_ports_fenrl_accept_batch(
        const std::string& _source_id, const std::vector<embedding_result>& _embeddings,
        std::size_t _eligible_face_count, face_enrollment_status& _status) override;

private:
    [[nodiscard]] status vqec_vision_ai_appl_feipl_fail_pending(status_code _error);
    face_enrollment_image_pipeline_config config_{};
    face_enrollment_begin_request pending_;
    std::uint64_t next_frame_id_{1};
    bool is_configured_{false};
    bool has_pending_{false};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_APPL_FACE_ENROLLMENT_IMAGE_PIPELINE_HPP
