#include "vqec/vision/ai/contracts/vqec_vision_face_gallery.hpp"

#include <stdexcept>

namespace vqec::vision::ai {
namespace {

face_gallery_config vqec_vision_ai_unit_fgalt_make_config() {
    return {"front_gallery", "edgeface", "1.0", 4, 2, 4, 2};
}

face_gallery_snapshot vqec_vision_ai_unit_fgalt_make_snapshot() {
    return {face_gallery_limits::g_schema_version, 7, 3, "front_gallery", "edgeface",
        "1.0", 4, 2, {{1, "person_a", {1.0F, 0.0F}},
                          {2, "person_a", {0.0F, 1.0F}}}};
}

void vqec_vision_ai_unit_fgalt_check_valid_and_revision() {
    const auto config = vqec_vision_ai_unit_fgalt_make_config();
    auto snapshot = vqec_vision_ai_unit_fgalt_make_snapshot();
    if (vqec_vision_ai_core_fgalr_validate_snapshot(config, snapshot).code_ !=
            status_code::ok) {
        throw std::runtime_error("valid gallery snapshot was rejected");
    }
    snapshot.revision_ = 8;
    if (vqec_vision_ai_core_fgalr_validate_replacement(config, 7, snapshot).code_ !=
            status_code::ok ||
        vqec_vision_ai_core_fgalr_validate_replacement(config, 8, snapshot).code_ !=
            status_code::invalid_state ||
        vqec_vision_ai_core_fgalr_validate_replacement(config, 9, snapshot).code_ !=
            status_code::invalid_state) {
        throw std::runtime_error("gallery replacement CAS is incorrect");
    }
}

void vqec_vision_ai_unit_fgalt_check_invalid_records() {
    const auto config = vqec_vision_ai_unit_fgalt_make_config();
    auto snapshot = vqec_vision_ai_unit_fgalt_make_snapshot();
    snapshot.templates_[1].record_id_ = snapshot.templates_[0].record_id_;
    if (vqec_vision_ai_core_fgalr_validate_snapshot(config, snapshot).code_ !=
        status_code::invalid_argument) {
        throw std::runtime_error("duplicate gallery record was accepted");
    }
    snapshot = vqec_vision_ai_unit_fgalt_make_snapshot();
    snapshot.templates_.push_back({3, "person_a", {1.0F, 0.0F}});
    snapshot.next_record_id_ = 4;
    if (vqec_vision_ai_core_fgalr_validate_snapshot(config, snapshot).code_ !=
        status_code::invalid_argument) {
        throw std::runtime_error("per-subject template limit was ignored");
    }
    snapshot = vqec_vision_ai_unit_fgalt_make_snapshot();
    snapshot.templates_[0].values_ = {0.5F, 0.0F};
    if (vqec_vision_ai_core_fgalr_validate_snapshot(config, snapshot).code_ !=
        status_code::invalid_argument) {
        throw std::runtime_error("unnormalized gallery vector was accepted");
    }
}

}  // namespace
}  // namespace vqec::vision::ai

int main() {
    using namespace vqec::vision::ai;
    vqec_vision_ai_unit_fgalt_check_valid_and_revision();
    vqec_vision_ai_unit_fgalt_check_invalid_records();
    return 0;
}
