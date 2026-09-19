#ifndef VQEC_VISION_AI_PORTS_FACE_GALLERY_STORE_HPP
#define VQEC_VISION_AI_PORTS_FACE_GALLERY_STORE_HPP

#include "vqec/vision/ai/contracts/perception/vqec_vision_face_gallery.hpp"

namespace vqec::vision::ai {

class face_gallery_store_port {
public:
    virtual ~face_gallery_store_port() = default;

    // Serialized cold path. The adapter verifies protected-storage provenance and returns
    // one authenticated/decrypted complete snapshot or fails closed.
    [[nodiscard]] virtual status vqec_vision_ai_ports_fgstr_load(
        const face_gallery_config& _config, face_gallery_snapshot& _snapshot) = 0;

    // Durable atomic CAS. Success means replacement survives restart; a derived-index
    // update is a later step and cannot roll this authoritative revision backward.
    [[nodiscard]] virtual status vqec_vision_ai_ports_fgstr_replace(
        const face_gallery_config& _config, std::uint64_t _expected_revision,
        const face_gallery_snapshot& _replacement) = 0;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_PORTS_FACE_GALLERY_STORE_HPP
