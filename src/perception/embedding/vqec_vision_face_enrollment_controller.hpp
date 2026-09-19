#ifndef VQEC_VISION_AI_EMBED_FACE_ENROLLMENT_CONTROLLER_HPP
#define VQEC_VISION_AI_EMBED_FACE_ENROLLMENT_CONTROLLER_HPP

#include "vqec_vision_recognition_session.hpp"
#include "vqec/vision/ai/ports/perception/vqec_vision_face_enrollment.hpp"

namespace vqec::vision::ai {

class face_enrollment_controller final : public face_enrollment_port {
public:
    explicit face_enrollment_controller(recognition_session& _session) noexcept;

    [[nodiscard]] status vqec_vision_ai_ports_fenrl_begin(
        const face_enrollment_begin_request& _request,
        face_enrollment_status& _status) override;
    [[nodiscard]] status vqec_vision_ai_ports_fenrl_begin_image(
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
    [[nodiscard]] status vqec_vision_ai_embed_fenrc_begin_request(
        const face_enrollment_begin_request& _request,
        face_enrollment_status& _status);
    recognition_session& session_;
    face_enrollment_begin_request request_;
    face_enrollment_status status_;
    std::uint64_t record_id_{0};
    std::uint64_t last_accepted_frame_id_{0};
    std::uint64_t last_accepted_pts_ns_{0};
    std::uint64_t accepted_epoch_{0};
    std::uint64_t selected_track_id_{0};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_EMBED_FACE_ENROLLMENT_CONTROLLER_HPP
