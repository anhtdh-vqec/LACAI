#include <cassert>

#include "vqec_vision_attribute_reader.hpp"

using namespace vqec::vision::ai;

int main() {
    const preview_frame_key frame{2, 1, 4, 10, 20};
    const preview_geometry geometry{640, 360};
    observation_batch batch{
        frame, geometry,
        {{frame, 7, "person", {10, 20, 30, 40, 0xffffffffU, "person"}, 0.9F,
          observation_quality::high,
          {{"human.ppe.helmet", "2", "present", 0.8F,
            observation_quality::medium, 18, 30}}}}};

    const observation_attribute* attribute = nullptr;
    assert(vqec_vision_ai_attr_atrdr_find_current_attribute(
               batch, 7, "human.ppe.helmet", "2", 20, attribute).code_ == status_code::ok);
    assert(attribute != nullptr && attribute->value_ == "present");

    const auto* prior = attribute;
    assert(vqec_vision_ai_attr_atrdr_find_current_attribute(
               batch, 8, "human.ppe.helmet", "2", 20, attribute).code_ ==
           status_code::pending);
    assert(attribute == prior);
    assert(vqec_vision_ai_attr_atrdr_find_current_attribute(
               batch, 7, "human ppe helmet", "2", 20, attribute).code_ ==
           status_code::invalid_argument);
    assert(attribute == prior);
    assert(vqec_vision_ai_attr_atrdr_find_current_attribute(
               batch, 7, "human.ppe.helmet", "2", 30, attribute).code_ ==
           status_code::pending);
    assert(attribute == prior);
    assert(vqec_vision_ai_attr_atrdr_find_current_attribute(
               batch, 7, "human.ppe.helmet", "2", 17, attribute).code_ ==
           status_code::invalid_argument);
    assert(attribute == prior);
    return 0;
}
