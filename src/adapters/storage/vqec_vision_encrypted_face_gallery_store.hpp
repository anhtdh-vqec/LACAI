#ifndef VQEC_VISION_AI_STOR_ENCRYPTED_FACE_GALLERY_STORE_HPP
#define VQEC_VISION_AI_STOR_ENCRYPTED_FACE_GALLERY_STORE_HPP

#include <cstddef>
#include <cstdint>
#include <string>

#include "vqec/vision/ai/ports/vqec_vision_face_gallery_store.hpp"

namespace vqec::vision::ai {

struct encrypted_face_gallery_store_config {
    std::string directory_path_;
    std::string gallery_file_name_;
    std::string key_file_name_;
    std::string lock_file_name_;
    std::uint32_t expected_owner_uid_{0};
    std::size_t max_serialized_bytes_{0};
};

// AI-owned protected gallery. The file is authenticated and encrypted by this adapter;
// Zvec remains a disposable derived index. A future TEE/keystore provider can replace
// key-file creation without changing the neutral face_gallery_store_port.
class encrypted_face_gallery_store final : public face_gallery_store_port {
public:
    explicit encrypted_face_gallery_store(encrypted_face_gallery_store_config _config);
    ~encrypted_face_gallery_store() override = default;

    encrypted_face_gallery_store(const encrypted_face_gallery_store&) = delete;
    encrypted_face_gallery_store& operator=(const encrypted_face_gallery_store&) = delete;

    [[nodiscard]] status vqec_vision_ai_ports_fgstr_load(
        const face_gallery_config& _config, face_gallery_snapshot& _snapshot) override;
    [[nodiscard]] status vqec_vision_ai_ports_fgstr_replace(
        const face_gallery_config& _config, std::uint64_t _expected_revision,
        const face_gallery_snapshot& _replacement) override;

private:
    encrypted_face_gallery_store_config config_;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_STOR_ENCRYPTED_FACE_GALLERY_STORE_HPP
