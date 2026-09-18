#include <iostream>
#include <sstream>
#include <string>

#include "vqec_vision_model_package_registry.hpp"

int main() {
    using namespace vqec::vision::ai;
    unsigned failures = 0;
    const auto check = [&failures](bool _condition) {
        if (!_condition) {
            ++failures;
        }
    };
    const std::string document = R"json({
      "schema_version": 1,
      "packages": [
        {"model_id":"face_detector","model_version":"1.0","target_id":"qcs6490",
         "artifact_ref":"face_detector_qnn","package_dir":"/opt/models/fd/package",
         "model_library":"/opt/models/fd/libface_detector.so"},
        {"model_id":"face_embedding","model_version":"1.0","target_id":"qcs6490",
         "artifact_ref":"face_embedding_qnn","package_dir":"/opt/models/fr/package",
         "model_library":"/opt/models/fr/libface_embedding.so"}
      ]
    })json";
    model_package_registry registry;
    std::istringstream stream(document);
    check(vqec_vision_ai_mreg_mprld_load_registry(stream, registry).code_ == status_code::ok);

    model_catalog catalog;
    model_catalog_entry detector;
    detector.model_id_ = "face_detector";
    detector.model_version_ = "1.0";
    detector.target_id_ = "qcs6490";
    detector.artifact_ref_ = "face_detector_qnn";
    model_catalog_entry embedding = detector;
    embedding.model_id_ = "face_embedding";
    embedding.artifact_ref_ = "face_embedding_qnn";
    catalog.models_ = {detector, embedding};
    check(vqec_vision_ai_core_mprgy_validate_registry(registry, catalog).code_ ==
        status_code::ok);
    const auto* found =
        vqec_vision_ai_core_mprgy_find_binding(registry, "face_embedding");
    check(found != nullptr && found->package_dir_ == "/opt/models/fr/package");

    const auto preserved = registry;
    std::istringstream unknown_key(
        R"json({"schema_version":1,"packages":[],"extra":true})json");
    check(vqec_vision_ai_mreg_mprld_load_registry(unknown_key, registry).code_ ==
        status_code::invalid_argument);
    check(registry.bindings_.size() == preserved.bindings_.size());

    auto duplicate = preserved;
    duplicate.bindings_[1].model_id_ = duplicate.bindings_[0].model_id_;
    check(vqec_vision_ai_core_mprgy_validate_registry(duplicate, catalog).code_ ==
        status_code::invalid_argument);
    auto mismatch = preserved;
    mismatch.bindings_[1].artifact_ref_ = "different_artifact";
    check(vqec_vision_ai_core_mprgy_validate_registry(mismatch, catalog).code_ ==
        status_code::invalid_argument);
    auto missing = preserved;
    missing.bindings_.pop_back();
    check(vqec_vision_ai_core_mprgy_validate_registry(missing, catalog).code_ ==
        status_code::invalid_argument);

    std::cout << "model package registry failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
