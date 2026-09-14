#include "vqec_vision_production_platform.hpp"

#include <fstream>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "vqec_vision_dbus_rpc.hpp"
#include "vqec_vision_qnn_engine.hpp"
#include "vqec_vision_qnn_inference_graph.hpp"
#include "vqec_vision_raw_source_resolver.hpp"
#include "vqec_vision_reference_feature.hpp"
#include "vqec_vision_reference_tracker.hpp"
#include "vqec_vision_source_lifecycle.hpp"
#include "vqec_vision_yolov8_decoder.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_tensor_contract.hpp"

namespace vqec::vision::ai {
namespace {

using nlohmann::json;

struct model_slot_owner {
    std::string model_id_;
    resolved_model_paths paths_;
    model_outputs outputs_;
    std::unique_ptr<qnn_engine> engine_;
    std::unique_ptr<qnn_inference_graph> graph_;
    std::unique_ptr<yolov8_decoder> decoder_;
};

json vqec_vision_ai_appl_pdplt_load(const std::string& _path) {
    std::ifstream stream(_path);
    if (!stream.is_open()) {
        throw std::runtime_error("cannot open package file: " + _path);
    }
    return json::parse(stream);
}

tensor_element_type vqec_vision_ai_appl_pdplt_dtype(const std::string& _name) {
    if (_name == "uint16") return tensor_element_type::uint16;
    if (_name == "int8") return tensor_element_type::int8;
    if (_name == "uint8") return tensor_element_type::uint8;
    if (_name == "int16") return tensor_element_type::int16;
    if (_name == "int32") return tensor_element_type::int32;
    if (_name == "float32") return tensor_element_type::float32;
    return tensor_element_type::unknown;
}

class platform_tracker_factory final : public tracker_factory_port {
public:
    explicit platform_tracker_factory(reference_tracker_config _config) noexcept
        : config_(_config) {}
    [[nodiscard]] status vqec_vision_ai_track_trfac_validate_activation(
        const std::string& _source_id, const std::string& _model_id) const override {
        (void)_model_id;
        return !_source_id.empty() ? status{} :
            status{status_code::unsupported, "tracker requires a source"};
    }
    [[nodiscard]] status vqec_vision_ai_track_trfac_create_tracker(
        const std::string& _source_id, const std::string& _model_id,
        std::unique_ptr<tracker_port>& _tracker) override {
        (void)_source_id;
        (void)_model_id;
        _tracker = std::make_unique<reference_tracker>(config_);
        return {};
    }

private:
    reference_tracker_config config_;
};

class platform_feature_factory final : public feature_processor_factory_port {
public:
    explicit platform_feature_factory(reference_feature_params _params)
        : params_(std::move(_params)) {}
    [[nodiscard]] status vqec_vision_ai_ports_ftfac_validate_configuration(
        const feature_catalog_entry& _feature, const feature_processor_config& _processor_config,
        const feature_configuration& _configuration) const override {
        (void)_feature;
        (void)_processor_config;
        (void)_configuration;
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_ftfac_create_processor(
        const feature_catalog_entry& _feature, const feature_processor_config& _processor_config,
        const feature_configuration& _configuration,
        std::unique_ptr<feature_processor_port>& _processor) override {
        (void)_feature;
        (void)_processor_config;
        (void)_configuration;
        _processor = std::make_unique<reference_zone_feature>(params_);
        return {};
    }

private:
    reference_feature_params params_;
};

}  // namespace

struct production_platform::implementation {
    production_platform_config config_;
    bool is_configured_{false};
    bool is_prepared_{false};
    std::shared_ptr<dbus_rpc> rpc_;
    std::vector<model_slot_owner> models_;
    std::vector<std::unique_ptr<source_lifecycle>> sources_;
    platform_feature_factory feature_factory_{reference_feature_params{}};
    std::unique_ptr<platform_tracker_factory> tracker_factory_;
};

production_platform::production_platform()
    : implementation_(std::make_unique<implementation>()) {}

production_platform::~production_platform() noexcept = default;

status production_platform::vqec_vision_ai_appl_pdplt_configure(
    const production_platform_config& _config) {
    if (implementation_ == nullptr) {
        return {status_code::invalid_state, "production platform is unavailable"};
    }
    auto& impl = *implementation_;
    if (impl.is_configured_) {
        return {status_code::invalid_state, "production platform is already configured"};
    }
    if (_config.package_dir_.empty() || _config.model_library_.empty() ||
        _config.backend_library_.empty() || _config.system_library_.empty() ||
        _config.tracker_contract_.empty() || _config.event_schema_id_.empty()) {
        return {status_code::invalid_argument, "invalid production platform configuration"};
    }
    impl.config_ = _config;
    impl.is_configured_ = true;
    return {};
}

status production_platform::vqec_vision_ai_appl_pdplt_prepare(
    const deployment_config& _deployment, const model_catalog& _catalog) {
    if (implementation_ == nullptr || !implementation_->is_configured_) {
        return {status_code::invalid_state, "production platform is not configured"};
    }
    auto& impl = *implementation_;
    if (impl.is_prepared_) {
        return {status_code::invalid_state, "production platform is already prepared"};
    }
    const json io_manifest =
        vqec_vision_ai_appl_pdplt_load(impl.config_.package_dir_ + "/io_manifest.json");
    const json decoder_json =
        vqec_vision_ai_appl_pdplt_load(impl.config_.package_dir_ + "/decoder.json");

    for (const auto& model : _catalog.models_) {
        model_slot_owner owner;
        owner.model_id_ = model.model_id_;
        owner.paths_.model_id_ = model.model_id_;
        owner.paths_.target_id_ = model.target_id_;
        owner.paths_.artifact_ref_ = model.artifact_ref_;
        owner.paths_.model_path_ = impl.config_.model_library_;
        owner.paths_.backend_path_ = impl.config_.backend_library_;
        owner.paths_.system_path_ = impl.config_.system_library_;

        owner.engine_ = std::make_unique<qnn_engine>();
        const auto opened = owner.engine_->vqec_vision_ai_qcom_qneng_open(
            impl.config_.backend_library_, impl.config_.system_library_,
            inference_execution_policy{});
        if (opened.code_ != status_code::ok) {
            return opened;
        }
        const auto prepared = owner.engine_->vqec_vision_ai_qcom_qneng_prepare(
            impl.config_.model_library_);
        if (prepared.code_ != status_code::ok) {
            return prepared;
        }
        std::vector<tensor_spec> inputs;
        std::vector<tensor_spec> outputs;
        const auto tensors =
            owner.engine_->vqec_vision_ai_qcom_qneng_get_tensors(inputs, outputs);
        if (tensors.code_ != status_code::ok || inputs.size() != 1) {
            return {status_code::unsupported, "production platform needs one graph input"};
        }

        owner.outputs_.model_id_ = model.model_id_;
        owner.outputs_.model_version_ = model.model_version_;
        owner.outputs_.artifact_sha256_ = model.artifact_sha256_;
        owner.outputs_.decoder_contract_ = model.decoder_contract_;
        for (const auto& output : outputs) {
            owner.outputs_.outputs_.push_back(output);
            owner.outputs_.max_output_bytes_ += vqec_vision_ai_core_tnctr_shape_bytes(output);
        }

        yolov8_decoder_config decoder_config;
        decoder_config.source_width_ = _deployment.sources_.front().profile_.width_;
        decoder_config.source_height_ = _deployment.sources_.front().profile_.height_;
        decoder_config.tensor_width_ = inputs[0].dimensions_.size() == 4 ?
            inputs[0].dimensions_[2] : 0;
        decoder_config.tensor_height_ = inputs[0].dimensions_.size() == 4 ?
            inputs[0].dimensions_[1] : 0;
        decoder_config.placement_ = image_placement::centre;
        decoder_config.box_tensor_ = decoder_json.value("box_tensor", std::string{"boxes_out"});
        decoder_config.score_tensor_ = decoder_json.value("score_tensor", std::string{"conf_out"});
        decoder_config.class_count_ = decoder_json.value("class_count", std::size_t{1});
        decoder_config.confidence_threshold_ =
            decoder_json.value("confidence_threshold", 0.25F);
        decoder_config.iou_threshold_ = decoder_json.value("iou_threshold", 0.45F);
        if (decoder_json.contains("labels")) {
            decoder_config.class_names_ = decoder_json["labels"].get<std::vector<std::string>>();
        }
        owner.decoder_ = std::make_unique<yolov8_decoder>(decoder_config);
        owner.graph_ = std::make_unique<qnn_inference_graph>(*owner.engine_);
        if (io_manifest.contains("inputs") && io_manifest["inputs"].is_array() &&
            !io_manifest["inputs"].empty()) {
            const auto& declared = io_manifest["inputs"][0];
            if (declared.value("name", std::string{}) != inputs[0].name_ ||
                vqec_vision_ai_appl_pdplt_dtype(declared.value("dtype", std::string{})) !=
                    inputs[0].dtype_) {
                return {status_code::unsupported,
                    "declared model input differs from the composed graph"};
            }
        }
        impl.models_.push_back(std::move(owner));
    }

    impl.rpc_ = std::make_shared<dbus_rpc>();
    const auto bus = impl.rpc_->vqec_vision_ai_camer_dbrpc_open(false);
    if (bus.code_ != status_code::ok) {
        return bus;
    }
    for (std::uint16_t slot = 0; slot < _deployment.sources_.size(); ++slot) {
        const auto& source = _deployment.sources_[slot];
        raw_source_route route;
        const auto routed = vqec_vision_ai_camer_rsrsv_make_legacy_route(
            source, impl.config_.socket_dir_, impl.config_.producer_uid_,
            impl.config_.nv12_format_value_, route);
        if (routed.code_ != status_code::ok) {
            return routed;
        }
        raw_source_cycle_identity identity;
        identity.consumer_id_ = impl.config_.consumer_id_prefix_ + std::to_string(slot);
        identity.start_request_id_ = identity.consumer_id_ + "_start";
        identity.stop_request_id_ = identity.consumer_id_ + "_stop";
        camera_lifecycle_config lifecycle_config;
        const auto composed = vqec_vision_ai_camer_rsrsv_compose_lifecycle(
            source, route, identity, lifecycle_config);
        if (composed.code_ != status_code::ok) {
            return composed;
        }
        impl.sources_.push_back(
            std::make_unique<source_lifecycle>(impl.rpc_, lifecycle_config));
    }

    impl.tracker_factory_ = std::make_unique<platform_tracker_factory>(
        reference_tracker_config{});
    reference_feature_params feature_params;
    feature_params.zone_ = {0.0F, 0.0F,
        static_cast<float>(_deployment.sources_.front().profile_.width_),
        static_cast<float>(_deployment.sources_.front().profile_.height_), 0U, {}};
    feature_params.event_schema_id_ = impl.config_.event_schema_id_;
    feature_params.event_schema_version_ = impl.config_.event_schema_version_;
    impl.feature_factory_ = platform_feature_factory{feature_params};
    impl.is_prepared_ = true;
    return {};
}

status production_platform::vqec_vision_ai_appl_pdplt_register_decoders(
    const model_catalog& _catalog, model_decoder_registry& _decoders) const {
    if (implementation_ == nullptr || !implementation_->is_prepared_) {
        return {status_code::invalid_state, "production platform is not prepared"};
    }
    for (const auto& model : _catalog.models_) {
        bool registered = false;
        for (const auto& owner : implementation_->models_) {
            if (owner.model_id_ != model.model_id_ || owner.decoder_ == nullptr) {
                continue;
            }
            const auto result = _decoders.vqec_vision_ai_detec_mdreg_register_decoder(
                model.decoder_contract_, *owner.decoder_);
            if (result.code_ != status_code::ok) {
                return result;
            }
            registered = true;
        }
        if (!registered) {
            return {status_code::unsupported, "no prepared decoder for catalog model"};
        }
    }
    return {};
}

status production_platform::vqec_vision_ai_appl_pdplt_register_tracker(
    tracker_registry& _trackers) const {
    if (implementation_ == nullptr || !implementation_->is_prepared_ ||
        implementation_->tracker_factory_ == nullptr) {
        return {status_code::invalid_state, "production platform is not prepared"};
    }
    return _trackers.vqec_vision_ai_track_trreg_register_factory(
        implementation_->config_.tracker_contract_, *implementation_->tracker_factory_);
}

status production_platform::vqec_vision_ai_appl_pdplt_register_features(
    const feature_catalog& _features, feature_processor_registry& _registry) const {
    if (implementation_ == nullptr || !implementation_->is_prepared_) {
        return {status_code::invalid_state, "production platform is not prepared"};
    }
    for (const auto& feature : _features.features_) {
        const auto registered = _registry.vqec_vision_ai_ftmgr_ftreg_register_factory(
            feature.processor_contract_, implementation_->feature_factory_);
        if (registered.code_ != status_code::ok) {
            return registered;
        }
    }
    return {};
}

const std::string& production_platform::vqec_vision_ai_appl_pdplt_get_tracker_contract()
    const noexcept {
    static const std::string empty;
    return implementation_ != nullptr ? implementation_->config_.tracker_contract_ : empty;
}

const std::string& production_platform::vqec_vision_ai_appl_pdplt_get_attribute_schema_id()
    const noexcept {
    static const std::string empty;
    return implementation_ != nullptr ? implementation_->config_.event_schema_id_ : empty;
}

raw_source_port* production_platform::vqec_vision_ai_appl_pdplt_source(
    std::uint16_t _source_slot) noexcept {
    if (implementation_ == nullptr || _source_slot >= implementation_->sources_.size()) {
        return nullptr;
    }
    return implementation_->sources_[_source_slot].get();
}

inference_graph_port* production_platform::vqec_vision_ai_appl_pdplt_graph(
    std::uint16_t _source_slot, std::uint16_t _model_slot) noexcept {
    (void)_source_slot;
    if (implementation_ == nullptr || _model_slot >= implementation_->models_.size()) {
        return nullptr;
    }
    return implementation_->models_[_model_slot].graph_.get();
}

const model_outputs* production_platform::vqec_vision_ai_appl_pdplt_outputs(
    const std::string& _model_id) const noexcept {
    if (implementation_ == nullptr) {
        return nullptr;
    }
    for (const auto& owner : implementation_->models_) {
        if (owner.model_id_ == _model_id) {
            return &owner.outputs_;
        }
    }
    return nullptr;
}

resolved_model_paths production_platform::vqec_vision_ai_appl_pdplt_paths(
    const std::string& _model_id) const noexcept {
    if (implementation_ != nullptr) {
        for (const auto& owner : implementation_->models_) {
            if (owner.model_id_ == _model_id) {
                return owner.paths_;
            }
        }
    }
    return {};
}

}  // namespace vqec::vision::ai
