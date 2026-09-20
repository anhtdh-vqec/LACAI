#include <vqec/vision/ai/contracts/lifecycle/vqec_vision_activation_delta.hpp>

#include <cstdint>
#include <stdexcept>
#include <string>

namespace vqec::vision::ai {
namespace {

constexpr std::uint64_t g_mebibyte = 1024ULL * 1024ULL;
constexpr std::size_t g_product_app_count = 18;

model_catalog_entry vqec_vision_ai_unit_adtst_make_model(
    const std::string& _id, char _digest_byte) {
    model_catalog_entry model;
    model.model_id_ = _id;
    model.model_version_ = "1.0";
    model.target_id_ = "qcs6490";
    model.artifact_ref_ = _id + ".bin";
    model.artifact_sha256_ = std::string(64, _digest_byte);
    model.output_manifest_ref_ = _id + ".outputs";
    model.decoder_contract_ = _id + ".decoder";
    model.preprocess_contract_ = "nv12.tensor.v1";
    model.graph_name_ = _id + ".graph";
    model.tensor_width_ = 640;
    model.tensor_height_ = 640;
    model.placement_ = image_placement::centre;
    model.inference_fps_numerator_ = 15;
    model.inference_fps_denominator_ = 1;
    model.source_constraints_ = {640, 480, 4096, 2160, 1, 1};
    model.resources_ = {16U * g_mebibyte, 8U * g_mebibyte, 2, 16, false};
    return model;
}

model_catalog vqec_vision_ai_unit_adtst_make_models() {
    model_catalog models;
    models.schema_version_ = model_catalog_limits::g_schema_version;
    models.revision_ = 11;
    models.catalog_id_ = "activation_models";
    models.models_.push_back(vqec_vision_ai_unit_adtst_make_model("shared_a", 'a'));
    models.models_.push_back(vqec_vision_ai_unit_adtst_make_model("shared_b", 'b'));
    return models;
}

deployment_config vqec_vision_ai_unit_adtst_make_deployment() {
    deployment_config deployment;
    deployment.schema_version_ = deployment_limits::g_schema_version;
    deployment.revision_ = 12;
    deployment.model_catalog_ref_ = "activation_models";
    deployment.max_total_resident_bytes_ = 512U * g_mebibyte;
    deployment.max_model_resident_bytes_ = 64U * g_mebibyte;
    source_deployment_config source;
    source.source_id_ = "camera_front";
    source.raw_source_ref_ = "camera_front.raw";
    source.preview_output_ref_ = "camera_front.preview";
    source.profile_ = {1920, 1080, 30, 1};
    source.memory_ = {8U * g_mebibyte, 3, 3, 64U * g_mebibyte, 32U * g_mebibyte};
    source.model_ids_ = {"shared_a", "shared_b"};
    deployment.sources_.push_back(std::move(source));
    return deployment;
}

usecase_catalog vqec_vision_ai_unit_adtst_make_usecases() {
    usecase_catalog usecases;
    usecases.schema_version_ = usecase_activation_limits::g_schema_version;
    usecases.revision_ = 13;
    usecases.catalog_id_ = "activation_usecases";
    usecases.model_catalog_ref_ = "activation_models";
    for (std::size_t index = 0; index < g_product_app_count; ++index) {
        const auto suffix = std::to_string(index + 1U);
        usecases.usecases_.push_back({"app_" + suffix, "1.0",
            {index % 2U == 0 ? "shared_a" : "shared_b"},
            {"feature_" + suffix}});
    }
    return usecases;
}

feature_catalog vqec_vision_ai_unit_adtst_make_features() {
    feature_catalog features;
    features.schema_version_ = feature_catalog_limits::g_schema_version;
    features.revision_ = 14;
    features.catalog_id_ = "activation_features";
    features.model_catalog_ref_ = "activation_models";
    for (std::size_t index = 0; index < g_product_app_count; ++index) {
        const auto suffix = std::to_string(index + 1U);
        feature_catalog_entry feature;
        feature.feature_id_ = "feature_" + suffix;
        feature.feature_version_ = "1.0";
        feature.processor_contract_ = "processor_" + suffix;
        feature.configuration_schema_ = "config_v1";
        feature.model_dependencies_.push_back({"root",
            index % 2U == 0 ? "shared_a" : "shared_b"});
        feature.resources_ = {4096, 4, 4, 4};
        features.features_.push_back(std::move(feature));
    }
    return features;
}

app_runtime_association vqec_vision_ai_unit_adtst_make_association(
    std::size_t _index, bool _desired) {
    const bool uses_a = _index % 2U == 0;
    const std::string model_id = uses_a ? "shared_a" : "shared_b";
    app_runtime_component component;
    component.component_id_ = model_id;
    component.component_version_ = "1.0";
    component.type_ = app_component_type::model;
    component.target_id_ = "qcs6490_qlinux_1_8";
    component.artifact_sha256_ = std::string(64, uses_a ? 'a' : 'b');
    component.artifact_bytes_ = 1024;
    component.semantic_contract_sha256_ = std::string(64, uses_a ? 'c' : 'd');
    component.model_role_ = app_model_role::primary;
    component.immutable_location_ = "/opt/lacai/apps/fixture/" + model_id;

    app_runtime_association association;
    association.app_id_ = "app_" + std::to_string(_index + 1U);
    association.source_id_ = "camera_front";
    association.app_version_ = "1.0";
    association.release_sequence_ = 1;
    association.installed_ = true;
    association.entitled_ = true;
    association.desired_ = _desired;
    association.supported_ = true;
    association.compatible_ = true;
    association.admitted_ = true;
    association.configuration_revision_ = 1;
    association.configuration_sha256_ = std::string(64, 'e');
    association.configuration_schema_id_ = "config_v1";
    association.configuration_payload_ = {'{', '}'};
    association.output_scopes_ = {"event"};
    association.components_.push_back(std::move(component));
    association.entitlement_expires_utc_ns_ = UINT64_MAX - 1U;
    return association;
}

runtime_control_snapshot vqec_vision_ai_unit_adtst_make_runtime(bool _desired) {
    runtime_control_snapshot runtime;
    runtime.schema_version_ = app_lifecycle_limits::g_schema_version;
    runtime.snapshot_revision_ = 1;
    runtime.inventory_revision_ = 1;
    runtime.entitlement_revision_ = 1;
    runtime.desired_revision_ = 1;
    for (std::size_t index = 0; index < g_product_app_count; ++index) {
        runtime.associations_.push_back(
            vqec_vision_ai_unit_adtst_make_association(index, _desired));
    }
    return runtime;
}

app_activation_plan vqec_vision_ai_unit_adtst_build_plan(
    const runtime_control_snapshot& _runtime) {
    app_activation_plan plan;
    const auto built = vqec_vision_ai_core_acdel_build_plan(
        vqec_vision_ai_unit_adtst_make_deployment(),
        vqec_vision_ai_unit_adtst_make_models(),
        vqec_vision_ai_unit_adtst_make_features(),
        vqec_vision_ai_unit_adtst_make_usecases(), _runtime, plan);
    if (built.code_ != status_code::ok) {
        throw std::runtime_error(built.message_);
    }
    return plan;
}

void vqec_vision_ai_unit_adtst_check_eighteen_app_reference_counts() {
    const auto plan = vqec_vision_ai_unit_adtst_build_plan(
        vqec_vision_ai_unit_adtst_make_runtime(true));
    if (plan.source_count_ != 1 || plan.model_dependencies_.size() != 2 ||
        plan.feature_instances_.size() != g_product_app_count ||
        plan.sources_[0].active_model_mask_ != 3U ||
        plan.sources_[0].prepared_model_mask_ != 3U ||
        plan.model_dependencies_[0].consumer_count_ != 9 ||
        plan.model_dependencies_[1].consumer_count_ != 9) {
        throw std::runtime_error("eighteen-app dependency plan is incorrect");
    }
}

void vqec_vision_ai_unit_adtst_check_shared_retain_and_release() {
    auto previous_runtime = vqec_vision_ai_unit_adtst_make_runtime(true);
    const auto previous = vqec_vision_ai_unit_adtst_build_plan(previous_runtime);

    auto retained_runtime = previous_runtime;
    retained_runtime.snapshot_revision_ = 2;
    retained_runtime.desired_revision_ = 2;
    retained_runtime.associations_[0].desired_ = false;
    const auto retained = vqec_vision_ai_unit_adtst_build_plan(retained_runtime);
    app_activation_delta delta;
    const auto retained_delta = vqec_vision_ai_core_acdel_build_delta(
        previous, retained, delta);
    if (retained_delta.code_ != status_code::ok ||
        delta.model_dependencies_.size() != 1 ||
        delta.model_dependencies_[0].kind_ != model_dependency_delta_kind::retain ||
        delta.model_dependencies_[0].previous_consumer_count_ != 9 ||
        delta.model_dependencies_[0].candidate_consumer_count_ != 8 ||
        delta.feature_instances_.size() != 1 ||
        delta.feature_instances_[0].kind_ != feature_instance_delta_kind::remove ||
        delta.requires_capacity_replacement_) {
        throw std::runtime_error("shared dependency retain delta is incorrect");
    }

    auto released_runtime = retained_runtime;
    released_runtime.snapshot_revision_ = 3;
    released_runtime.desired_revision_ = 3;
    for (std::size_t index = 0; index < released_runtime.associations_.size(); index += 2U) {
        released_runtime.associations_[index].desired_ = false;
    }
    const auto released = vqec_vision_ai_unit_adtst_build_plan(released_runtime);
    const auto release_delta = vqec_vision_ai_core_acdel_build_delta(
        retained, released, delta);
    if (release_delta.code_ != status_code::ok ||
        delta.model_dependencies_.size() != 1 ||
        delta.model_dependencies_[0].kind_ != model_dependency_delta_kind::release ||
        delta.model_dependencies_[0].candidate_consumer_count_ != 0 ||
        released.sources_[0].active_model_mask_ != 2U ||
        released.sources_[0].prepared_model_mask_ != 3U) {
        throw std::runtime_error("last-consumer release delta is incorrect");
    }
}

void vqec_vision_ai_unit_adtst_check_configuration_and_capacity_delta() {
    auto previous_runtime = vqec_vision_ai_unit_adtst_make_runtime(true);
    const auto previous = vqec_vision_ai_unit_adtst_build_plan(previous_runtime);
    auto configured_runtime = previous_runtime;
    configured_runtime.snapshot_revision_ = 2;
    configured_runtime.inventory_revision_ = 2;
    configured_runtime.associations_[1].configuration_revision_ = 2;
    configured_runtime.associations_[1].configuration_sha256_ = std::string(64, 'f');
    configured_runtime.associations_[1].configuration_payload_ = {'{', '1', '}'};
    const auto configured = vqec_vision_ai_unit_adtst_build_plan(configured_runtime);
    app_activation_delta delta;
    const auto changed = vqec_vision_ai_core_acdel_build_delta(
        previous, configured, delta);
    if (changed.code_ != status_code::ok || !delta.model_dependencies_.empty() ||
        delta.feature_instances_.size() != 1 ||
        delta.feature_instances_[0].kind_ != feature_instance_delta_kind::replace) {
        throw std::runtime_error("configuration-only delta changed model dependencies");
    }

    auto narrow_runtime = previous_runtime;
    narrow_runtime.associations_.erase(narrow_runtime.associations_.begin() + 1,
        narrow_runtime.associations_.end());
    const auto narrow = vqec_vision_ai_unit_adtst_build_plan(narrow_runtime);
    auto expanded_runtime = narrow_runtime;
    expanded_runtime.snapshot_revision_ = 2;
    expanded_runtime.inventory_revision_ = 2;
    expanded_runtime.associations_.push_back(
        vqec_vision_ai_unit_adtst_make_association(1, true));
    const auto expanded = vqec_vision_ai_unit_adtst_build_plan(expanded_runtime);
    if (vqec_vision_ai_core_acdel_build_delta(narrow, expanded, delta).code_ !=
            status_code::ok || !delta.requires_capacity_replacement_) {
        throw std::runtime_error("new prepared capacity did not require replacement");
    }
}

void vqec_vision_ai_unit_adtst_check_cascade_reference_counts() {
    auto models = vqec_vision_ai_unit_adtst_make_models();
    auto embedding = vqec_vision_ai_unit_adtst_make_model("embedding", '9');
    embedding.role_ = model_role::secondary;
    embedding.depends_on_ = {{"shared_a", "1.0", "qcs6490"}};
    embedding.tensor_width_ = 112;
    embedding.tensor_height_ = 112;
    models.models_.push_back(std::move(embedding));
    auto deployment = vqec_vision_ai_unit_adtst_make_deployment();
    deployment.sources_[0].cascade_ = {2, 4, 4U * g_mebibyte};
    auto runtime = vqec_vision_ai_unit_adtst_make_runtime(true);
    for (const std::size_t association_index : {0U, 2U}) {
        app_runtime_component component;
        component.component_id_ = "embedding";
        component.component_version_ = "1.0";
        component.type_ = app_component_type::model;
        component.target_id_ = "qcs6490";
        component.artifact_sha256_ = std::string(64, '9');
        component.artifact_bytes_ = 1024;
        component.semantic_contract_sha256_ = std::string(64, '8');
        component.model_role_ = app_model_role::secondary;
        component.immutable_location_ = "/opt/lacai/apps/fixture/embedding";
        runtime.associations_[association_index].components_.push_back(
            std::move(component));
    }
    const auto build = [&](const runtime_control_snapshot& _runtime) {
        app_activation_plan plan;
        const auto planned = vqec_vision_ai_core_acdel_build_plan(
            deployment, models, vqec_vision_ai_unit_adtst_make_features(),
            vqec_vision_ai_unit_adtst_make_usecases(), _runtime, plan);
        if (planned.code_ != status_code::ok) {
            throw std::runtime_error(planned.message_);
        }
        return plan;
    };
    const auto previous = build(runtime);
    if (previous.cascade_dependencies_.size() != 1 ||
        previous.cascade_dependencies_[0].consumer_count_ != 2 ||
        previous.cascade_dependencies_[0].root_model_slot_ != 0) {
        throw std::runtime_error("cascade dependency plan is incorrect");
    }
    runtime.snapshot_revision_ = 2;
    runtime.desired_revision_ = 2;
    runtime.associations_[0].desired_ = false;
    const auto retained = build(runtime);
    app_activation_delta delta;
    if (vqec_vision_ai_core_acdel_build_delta(previous, retained, delta).code_ !=
            status_code::ok || delta.cascade_dependencies_.size() != 1 ||
        delta.cascade_dependencies_[0].kind_ != model_dependency_delta_kind::retain ||
        delta.cascade_dependencies_[0].previous_consumer_count_ != 2 ||
        delta.cascade_dependencies_[0].candidate_consumer_count_ != 1) {
        throw std::runtime_error("shared cascade retain delta is incorrect");
    }
    runtime.snapshot_revision_ = 3;
    runtime.desired_revision_ = 3;
    runtime.associations_[2].desired_ = false;
    const auto released = build(runtime);
    if (vqec_vision_ai_core_acdel_build_delta(retained, released, delta).code_ !=
            status_code::ok || delta.cascade_dependencies_.size() != 1 ||
        delta.cascade_dependencies_[0].kind_ != model_dependency_delta_kind::release ||
        delta.cascade_dependencies_[0].candidate_consumer_count_ != 0) {
        throw std::runtime_error("last cascade consumer release is incorrect");
    }
}

void vqec_vision_ai_unit_adtst_check_transactional_rejection() {
    auto runtime = vqec_vision_ai_unit_adtst_make_runtime(true);
    app_activation_plan plan;
    plan.snapshot_revision_ = 77;
    runtime.associations_[2].components_[0].artifact_sha256_ = std::string(64, '9');
    const auto rejected = vqec_vision_ai_core_acdel_build_plan(
        vqec_vision_ai_unit_adtst_make_deployment(),
        vqec_vision_ai_unit_adtst_make_models(),
        vqec_vision_ai_unit_adtst_make_features(),
        vqec_vision_ai_unit_adtst_make_usecases(), runtime, plan);
    if (rejected.code_ != status_code::incompatible_plugin ||
        plan.snapshot_revision_ != 77) {
        throw std::runtime_error("invalid dependency changed the prior plan");
    }

    const auto valid = vqec_vision_ai_unit_adtst_build_plan(
        vqec_vision_ai_unit_adtst_make_runtime(true));
    app_activation_delta delta;
    delta.previous_snapshot_revision_ = 88;
    if (vqec_vision_ai_core_acdel_build_delta(valid, valid, delta).code_ !=
            status_code::invalid_state || delta.previous_snapshot_revision_ != 88) {
        throw std::runtime_error("stale delta changed the prior output");
    }
}

}  // namespace
}  // namespace vqec::vision::ai

int main() {
    try {
        vqec::vision::ai::vqec_vision_ai_unit_adtst_check_eighteen_app_reference_counts();
        vqec::vision::ai::vqec_vision_ai_unit_adtst_check_shared_retain_and_release();
        vqec::vision::ai::vqec_vision_ai_unit_adtst_check_configuration_and_capacity_delta();
        vqec::vision::ai::vqec_vision_ai_unit_adtst_check_cascade_reference_counts();
        vqec::vision::ai::vqec_vision_ai_unit_adtst_check_transactional_rejection();
    } catch (const std::exception&) {
        return 1;
    }
    return 0;
}
