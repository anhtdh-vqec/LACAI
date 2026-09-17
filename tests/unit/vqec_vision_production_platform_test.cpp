#include <cassert>
#include <memory>
#include <string>

#include "vqec_vision_production_platform.hpp"

using namespace vqec::vision::ai;

namespace {

production_platform_config vqec_vision_ai_unit_pdptst_make_valid_config() {
    production_platform_config config;
    model_package_binding binding;
    binding.model_id_ = "person_detector";
    binding.package_dir_ = "/tmp/fake_package";
    binding.model_library_ = "/tmp/fake_model.so";
    config.model_packages_.bindings_.push_back(std::move(binding));
    config.backend_library_ = "/usr/lib/libQnnHtp.so";
    config.system_library_ = "/usr/lib/libQnnSystem.so";
    config.model_root_ = "/tmp";
    config.max_artifact_bytes_ = production_platform_limits::g_default_max_artifact_bytes;
    config.execution_policy_.mode_ = inference_execution_mode::synchronous;
    config.execution_policy_.profile_ = inference_perf_profile::balanced;
    config.socket_dir_ = "/run/camera_ai";
    config.producer_uid_ = 1000;
    config.nv12_format_value_ = 23;
    config.preprocess_output_timeout_ns_ = 1000000000ULL;
    config.tracker_contract_ = "portable.iou.tracker.v1";
    config.event_schema_id_ = "reference.zone";
    config.event_schema_version_ = "1";
    config.consumer_id_prefix_ = "lacai_ai";
    return config;
}

feature_catalog vqec_vision_ai_unit_pdptst_make_feature_catalog() {
    feature_catalog catalog;
    catalog.schema_version_ = 1;
    catalog.revision_ = 1;
    catalog.catalog_id_ = "test_features";

    feature_catalog_entry counting;
    counting.feature_id_ = "counting";
    counting.feature_version_ = "1.0";
    counting.processor_contract_ = "counting.processor.v1";
    catalog.features_.push_back(std::move(counting));

    feature_catalog_entry intrusion;
    intrusion.feature_id_ = "intrusion";
    intrusion.feature_version_ = "1.0";
    intrusion.processor_contract_ = "intrusion.processor.v1";
    catalog.features_.push_back(std::move(intrusion));

    feature_catalog_entry reference_zone;
    reference_zone.feature_id_ = "reference_zone";
    reference_zone.feature_version_ = "1.0";
    reference_zone.processor_contract_ = "reference.zone.processor.v1";
    catalog.features_.push_back(std::move(reference_zone));

    return catalog;
}

}  // namespace

int main() {
    production_platform platform;

    // 1. Unprepared platform returns nullptr and invalid_state
    assert(platform.vqec_vision_ai_appl_pdplt_source(0) == nullptr);
    assert(platform.vqec_vision_ai_appl_pdplt_graph(0, "person_detector") == nullptr);
    assert(platform.vqec_vision_ai_appl_pdplt_graph(1, "person_detector") == nullptr);
    assert(platform.vqec_vision_ai_appl_pdplt_processor(0, "person_detector") == nullptr);
    assert(platform.vqec_vision_ai_appl_pdplt_outputs("person_detector") == nullptr);

    const auto feature_cat = vqec_vision_ai_unit_pdptst_make_feature_catalog();
    feature_processor_registry feat_registry;
    assert(platform.vqec_vision_ai_appl_pdplt_register_features(feature_cat, feat_registry).code_ ==
           status_code::invalid_state);

    tracker_registry trk_registry;
    assert(platform.vqec_vision_ai_appl_pdplt_register_tracker(trk_registry).code_ ==
           status_code::invalid_state);

    // 2. Configuration validation
    production_platform_config invalid_config;
    assert(platform.vqec_vision_ai_appl_pdplt_configure(invalid_config).code_ ==
           status_code::invalid_argument);

    auto bad_root = vqec_vision_ai_unit_pdptst_make_valid_config();
    bad_root.model_root_ = "";
    production_platform platform_bad_root;
    assert(platform_bad_root.vqec_vision_ai_appl_pdplt_configure(bad_root).code_ ==
           status_code::invalid_argument);

    auto bad_bytes = vqec_vision_ai_unit_pdptst_make_valid_config();
    bad_bytes.max_artifact_bytes_ = 0;
    production_platform platform_bad_bytes;
    assert(platform_bad_bytes.vqec_vision_ai_appl_pdplt_configure(bad_bytes).code_ ==
           status_code::invalid_argument);

    auto excess_bytes = vqec_vision_ai_unit_pdptst_make_valid_config();
    excess_bytes.max_artifact_bytes_ =
        production_platform_limits::g_max_artifact_bytes_ceiling + 1;
    production_platform platform_excess_bytes;
    assert(platform_excess_bytes.vqec_vision_ai_appl_pdplt_configure(excess_bytes).code_ ==
           status_code::invalid_argument);

    auto valid_config = vqec_vision_ai_unit_pdptst_make_valid_config();
    assert(platform.vqec_vision_ai_appl_pdplt_configure(valid_config).code_ == status_code::ok);

    auto reference_tracker_config = vqec_vision_ai_unit_pdptst_make_valid_config();
    reference_tracker_config.tracker_contract_ = "reference.tracker.v1";
    production_platform reference_tracker_platform;
    assert(reference_tracker_platform.vqec_vision_ai_appl_pdplt_configure(
               reference_tracker_config).code_ == status_code::invalid_argument);

    // Reconfiguration is rejected
    assert(platform.vqec_vision_ai_appl_pdplt_configure(valid_config).code_ ==
           status_code::invalid_state);

    // 3. Prepare requires sources
    deployment_config empty_deployment;
    model_catalog empty_catalog;
    assert(platform.vqec_vision_ai_appl_pdplt_prepare(empty_deployment, empty_catalog).code_ ==
           status_code::invalid_argument);

    // 4. Source slot isolation on non-existent slots returns nullptr cleanly
    assert(platform.vqec_vision_ai_appl_pdplt_graph(0, "non_existent") == nullptr);
    assert(platform.vqec_vision_ai_appl_pdplt_graph(1, "non_existent") == nullptr);
    assert(platform.vqec_vision_ai_appl_pdplt_processor(0, "non_existent") == nullptr);

    return 0;
}
