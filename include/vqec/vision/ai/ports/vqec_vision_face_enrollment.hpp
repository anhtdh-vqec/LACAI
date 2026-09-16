#ifndef VQEC_VISION_AI_PORTS_FACE_ENROLLMENT_HPP
#define VQEC_VISION_AI_PORTS_FACE_ENROLLMENT_HPP

#include <cstddef>
#include <cstdint>
#include <string>

#include "vqec/vision/ai/contracts/vqec_vision_embedding.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_status.hpp"

namespace vqec::vision::ai {

namespace face_enrollment_limits {
inline constexpr std::size_t g_max_request_id_bytes = 128;
inline constexpr std::size_t g_max_source_id_bytes = 128;
inline constexpr std::size_t g_max_image_path_bytes = 4096;
inline constexpr std::size_t g_max_samples_per_request = 32;
}

enum class face_enrollment_state { idle, collecting, completed, cancelled, failed };

struct face_enrollment_begin_request {
    std::string request_id_;
    std::string subject_ref_;
    // FW-authorized local image path. Empty selects live-camera enrollment.
    std::string image_path_;
    std::string source_id_;
    std::uint32_t camera_id_{0};
    std::uint32_t channel_id_{0};
    std::uint64_t target_track_id_{0};
    std::size_t expected_samples_{0};
    std::uint64_t expected_gallery_revision_{0};
};

struct face_enrollment_status {
    std::string request_id_;
    std::string subject_ref_;
    face_enrollment_state state_{face_enrollment_state::idle};
    std::size_t accepted_samples_{0};
    std::size_t expected_samples_{0};
    std::uint64_t gallery_revision_{0};
    status_code last_error_{status_code::ok};
};

class face_enrollment_port {
public:
    virtual ~face_enrollment_port() = default;
    [[nodiscard]] virtual status vqec_vision_ai_ports_fenrl_begin(
        const face_enrollment_begin_request& _request,
        face_enrollment_status& _status) = 0;
    [[nodiscard]] virtual status vqec_vision_ai_ports_fenrl_cancel(
        const std::string& _request_id, face_enrollment_status& _status) = 0;
    [[nodiscard]] virtual status vqec_vision_ai_ports_fenrl_remove_subject(
        const std::string& _subject_ref, std::uint64_t _expected_gallery_revision,
        std::uint64_t& _new_gallery_revision) = 0;
    [[nodiscard]] virtual status vqec_vision_ai_ports_fenrl_get_status(
        const std::string& _request_id, face_enrollment_status& _status) const = 0;
    // Called by the serialized runtime owner after cascade completion. The controller
    // consumes at most one embedding per frame and never retains pixel buffers.
    [[nodiscard]] virtual status vqec_vision_ai_ports_fenrl_accept_embedding(
        const embedding_result& _embedding, face_enrollment_status& _status) = 0;
    [[nodiscard]] virtual status vqec_vision_ai_ports_fenrl_accept_batch(
        const std::string& _source_id, const std::vector<embedding_result>& _embeddings,
        std::size_t _eligible_face_count, face_enrollment_status& _status) = 0;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_PORTS_FACE_ENROLLMENT_HPP
