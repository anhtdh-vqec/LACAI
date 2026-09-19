#include <cassert>
#include <string>

#include "vqec_vision_service_feature_registry.hpp"

namespace {

using namespace vqec::vision::ai;

feature_catalog_entry vqec_vision_ai_unit_sfrtst_feature(
    const std::string& _id, const std::string& _processor) {
    feature_catalog_entry feature;
    feature.feature_id_ = _id;
    feature.feature_version_ = "1.0.0";
    feature.processor_contract_ = _processor;
    feature.configuration_schema_ = _id + ".configuration";
    feature.input_mode_ = feature_input_mode::single_model;
    feature.model_dependencies_.push_back({"detections", "model"});
    feature.resources_ = {1024, 8, 8, 16};
    return feature;
}

void vqec_vision_ai_unit_sfrtst_test_partition_and_lifetime() {
    feature_catalog catalog;
    catalog.schema_version_ = 1;
    catalog.revision_ = 3;
    catalog.catalog_id_ = "features";
    catalog.model_catalog_ref_ = "models";
    auto fire = vqec_vision_ai_unit_sfrtst_feature(
        "fire_smoke_alarm", "fire_smoke_alarm");
    fire.configuration_schema_ = "security.fire_smoke.configuration";
    catalog.features_.push_back(fire);
    catalog.features_.push_back(
        vqec_vision_ai_unit_sfrtst_feature("another_feature", "another_processor"));

    feature_processor_registry registry;
    service_feature_registry owner;
    feature_catalog residual;
    assert(owner.vqec_vision_ai_appl_sfreg_register_compiled(
               catalog, registry, residual)
               .code_ == status_code::ok);
    assert(registry.vqec_vision_ai_ftmgr_ftreg_get_count() == 1);
    assert(residual.features_.size() == 1);
    assert(residual.features_[0].processor_contract_ == "another_processor");
    feature_processor_factory_port* factory = nullptr;
    assert(registry.vqec_vision_ai_ftmgr_ftreg_resolve_factory(
               "fire_smoke_alarm", factory)
               .code_ == status_code::ok);
    assert(factory != nullptr);
}

}  // namespace

int main() {
    vqec_vision_ai_unit_sfrtst_test_partition_and_lifetime();
    return 0;
}
