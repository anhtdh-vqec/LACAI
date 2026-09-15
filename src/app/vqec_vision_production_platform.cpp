#include "vqec_vision_production_platform.hpp"

#include <algorithm>
#include <fstream>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "vqec_vision_dbus_rpc.hpp"
#include "vqec_vision_fastcv_processor.hpp"
#include "vqec_vision_qnn_engine.hpp"
#include "vqec_vision_qtiv_renderer.hpp"
#include "vqec_vision_qnn_inference_graph.hpp"
#include "vqec_vision_raw_source_resolver.hpp"
#include "vqec_vision_reference_feature.hpp"
#include "vqec_vision_reference_tracker.hpp"
#include "vqec_vision_source_lifecycle.hpp"
#include "vqec_vision_yolov8_decoder.hpp"
#include "vqec_vision_anchor_distance_decoder.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_preview_limits.hpp"
#include "vqec/vision/ai/contracts/vqec_vision_tensor_contract.hpp"

namespace vqec::vision::ai {
namespace {

using nlohmann::json;

inline constexpr char g_labels_key[] = "labels";
inline constexpr char g_labels_ref_key[] = "labels_ref";

// decoder.json primary decoder protocol; see cascade_inference.md.
inline constexpr char g_decoder_kind[] = "kind";
inline constexpr char g_decoder_anchor_distance[] = "anchor_distance";
inline constexpr char g_decoder_yolov8[] = "yolov8";
inline constexpr char g_decoder_decoder_contract[] = "decoder_contract";
inline constexpr char g_decoder_class_id[] = "class_id";
inline constexpr char g_decoder_landmark_schema_id[] = "landmark_schema_id";
inline constexpr char g_decoder_landmark_schema_version[] = "landmark_schema_version";
inline constexpr char g_decoder_landmark_count[] = "landmark_count";
inline constexpr char g_decoder_anchor_offset_cells[] = "anchor_offset_cells";
inline constexpr char g_decoder_confidence_threshold[] = "confidence_threshold";
inline constexpr char g_decoder_iou_threshold[] = "iou_threshold";
inline constexpr char g_decoder_max_candidates[] = "max_candidates";
inline constexpr char g_decoder_stages[] = "stages";
inline constexpr char g_decoder_score_tensor[] = "score_tensor";
inline constexpr char g_decoder_box_tensor[] = "box_tensor";
inline constexpr char g_decoder_landmark_tensor[] = "landmark_tensor";
inline constexpr char g_decoder_stride[] = "stride";
inline constexpr char g_decoder_grid_width[] = "grid_width";
inline constexpr char g_decoder_grid_height[] = "grid_height";
inline constexpr char g_decoder_anchors_per_cell[] = "anchors_per_cell";

struct model_slot_owner {
    std::string model_id_;
    resolved_model_paths paths_;
    model_outputs outputs_;
    std::unique_ptr<qnn_engine> engine_;
    std::unique_ptr<qnn_inference_graph> graph_;
    std::unique_ptr<fastcv_processor> processor_;
    std::unique_ptr<model_decoder_port> decoder_;
};

json vqec_vision_ai_appl_pdplt_load(const std::string& _path) {
    std::ifstream stream(_path);
    if (!stream.is_open()) {
        throw std::runtime_error("cannot open package file: " + _path);
    }
    return json::parse(stream);
}

std::vector<std::string> vqec_vision_ai_appl_pdplt_load_labels(
    const json& _decoder, const std::string& _package_dir,
    std::size_t _class_count) {
    const bool has_inline = _decoder.contains(g_labels_key);
    const bool has_reference = _decoder.contains(g_labels_ref_key);
    if (has_inline && has_reference) {
        throw std::runtime_error("decoder declares both inline and referenced labels");
    }
    std::vector<std::string> labels;
    if (has_inline) {
        labels = _decoder[g_labels_key].get<std::vector<std::string>>();
    } else if (has_reference) {
        const auto reference = _decoder[g_labels_ref_key].get<std::string>();
        if (reference.empty() || reference == "." || reference == ".." ||
            reference.find_first_of("/\\") != std::string::npos) {
            throw std::runtime_error("decoder label reference is not a package filename");
        }
        std::ifstream stream(_package_dir + "/" + reference);
        if (!stream.is_open()) {
            throw std::runtime_error("cannot open decoder label file");
        }
        std::string label;
        while (std::getline(stream, label)) {
            if (!label.empty() && label.back() == '\r') {
                label.pop_back();
            }
            labels.push_back(std::move(label));
        }
    }
    if (!labels.empty() && labels.size() != _class_count) {
        throw std::runtime_error("decoder label count differs from class count");
    }
    for (std::size_t index = 0; index < labels.size(); ++index) {
        const auto& label = labels[index];
        const bool is_printable = std::all_of(label.begin(), label.end(), [](char _value) {
            const auto value = static_cast<unsigned char>(_value);
            return value >= preview_limits::g_min_label_character &&
                value <= preview_limits::g_max_label_character;
        });
        if (label.empty() || label.size() > preview_limits::g_max_label_bytes ||
            !is_printable || std::find(labels.begin(), labels.begin() + index, label) !=
                labels.begin() + index) {
            throw std::runtime_error("decoder label file contains an invalid label");
        }
    }
    return labels;
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


tensor_spec vqec_vision_ai_appl_pdplt_tensor(const json& _entry) {
    tensor_spec spec;
    spec.name_ = _entry.value("name", std::string{});
    if (_entry.contains("dims")) {
        for (const auto& dim : _entry["dims"]) {
            spec.dimensions_.push_back(dim.get<std::uint32_t>());
        }
    }
    spec.dtype_ = vqec_vision_ai_appl_pdplt_dtype(_entry.value("dtype", std::string{}));
    if (_entry.contains("quantization")) {
        const auto& quantization = _entry["quantization"];
        spec.quantization_.is_quantized_ = true;
        spec.quantization_.scale_ = quantization.value("scale", 1.0F);
        spec.quantization_.zero_point_ = quantization.value("zero_point", 0);
    }
    return spec;
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
    std::unique_ptr<qtiv_renderer> renderer_;
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
    if (_config.model_packages_.bindings_.empty() || _config.backend_library_.empty() ||
        _config.system_library_.empty() ||
        _config.tracker_contract_.empty() || _config.event_schema_id_.empty() ||
        _config.preprocess_output_timeout_ns_ == 0 ||
        _config.preprocess_output_timeout_ns_ == UINT64_MAX) {
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
    if (_deployment.sources_.empty()) {
        return {status_code::invalid_argument, "production deployment has no sources"};
    }
    auto& impl = *implementation_;
    if (impl.is_prepared_) {
        return {status_code::invalid_state, "production platform is already prepared"};
    }
    const auto registry_status = vqec_vision_ai_core_mprgy_validate_registry(
        impl.config_.model_packages_, _catalog);
    if (registry_status.code_ != status_code::ok) {
        return registry_status;
    }

    for (const auto& model : _catalog.models_) {
        const source_deployment_config* model_source = nullptr;
        std::uint64_t max_frame_allocation_bytes = 0;
        for (const auto& source : _deployment.sources_) {
            if (std::find(source.model_ids_.begin(), source.model_ids_.end(),
                    model.model_id_) == source.model_ids_.end()) {
                continue;
            }
            if (model_source != nullptr &&
                (model_source->profile_.width_ != source.profile_.width_ ||
                 model_source->profile_.height_ != source.profile_.height_)) {
                return {status_code::unsupported,
                    "shared model decoder requires equal source dimensions"};
            }
            model_source = &source;
            max_frame_allocation_bytes = std::max(max_frame_allocation_bytes,
                source.memory_.max_frame_allocation_bytes_);
        }
        if (model_source == nullptr) {
            continue;
        }

        const auto* binding = vqec_vision_ai_core_mprgy_find_binding(
            impl.config_.model_packages_, model.model_id_);
        if (binding == nullptr) {
            return {status_code::invalid_argument, "model package binding is missing"};
        }
        const json io_manifest =
            vqec_vision_ai_appl_pdplt_load(binding->package_dir_ + "/io_manifest.json");
        const json decoder_json =
            vqec_vision_ai_appl_pdplt_load(binding->package_dir_ + "/decoder.json");
        model_slot_owner owner;
        owner.model_id_ = model.model_id_;
        owner.paths_.model_id_ = model.model_id_;
        owner.paths_.target_id_ = model.target_id_;
        owner.paths_.artifact_ref_ = model.artifact_ref_;
        owner.paths_.model_path_ = binding->model_library_;
        owner.paths_.backend_path_ = impl.config_.backend_library_;
        owner.paths_.system_path_ = impl.config_.system_library_;

        owner.engine_ = std::make_unique<qnn_engine>();
        const auto opened = owner.engine_->vqec_vision_ai_qcom_qneng_open(
            impl.config_.backend_library_, impl.config_.system_library_,
            inference_execution_policy{});
        if (opened.code_ != status_code::ok) {
            return opened;
        }
        // Declared package metadata. The graph validates it against the composed QNN
        // graph during activation; the platform must not prepare the engine here because
        // the graph port owns the prepare/load lifecycle.
        if (!io_manifest.contains("inputs") || io_manifest["inputs"].empty() ||
            !io_manifest.contains("outputs") || io_manifest["outputs"].empty()) {
            return {status_code::unsupported, "model package declares no IO manifest"};
        }
        tensor_spec declared_input = vqec_vision_ai_appl_pdplt_tensor(io_manifest["inputs"][0]);
        std::vector<tensor_spec> outputs;
        for (const auto& entry : io_manifest["outputs"]) {
            outputs.push_back(vqec_vision_ai_appl_pdplt_tensor(entry));
        }
        owner.outputs_.model_id_ = model.model_id_;
        owner.outputs_.model_version_ = model.model_version_;
        owner.outputs_.artifact_sha256_ = model.artifact_sha256_;
        owner.outputs_.decoder_contract_ = model.decoder_contract_;
        for (const auto& output : outputs) {
            owner.outputs_.outputs_.push_back(output);
            owner.outputs_.max_output_bytes_ += vqec_vision_ai_core_tnctr_shape_bytes(output);
        }

        const auto kind = decoder_json.value(g_decoder_kind, std::string{g_decoder_yolov8});
        if (kind == g_decoder_anchor_distance) {
            if (decoder_json.at(g_decoder_decoder_contract).get<std::string>() !=
                    model.decoder_contract_) {
                return {status_code::invalid_argument, "decoder package contract mismatch"};
            }
            anchor_distance_decoder_config config;
            config.source_width_ = model_source->profile_.width_;
            config.source_height_ = model_source->profile_.height_;
            config.tensor_width_ = model.tensor_width_;
            config.tensor_height_ = model.tensor_height_;
            config.placement_ = model.placement_;
            config.class_id_ = decoder_json.at(g_decoder_class_id).get<std::string>();
            config.landmark_schema_id_ = decoder_json.at(g_decoder_landmark_schema_id).get<std::string>();
            config.landmark_schema_version_ = decoder_json.at(g_decoder_landmark_schema_version).get<std::string>();
            config.landmark_count_ = decoder_json.at(g_decoder_landmark_count).get<std::size_t>();
            config.anchor_offset_cells_ = decoder_json.at(g_decoder_anchor_offset_cells).get<float>();
            config.confidence_threshold_ = decoder_json.at(g_decoder_confidence_threshold).get<float>();
            config.iou_threshold_ = decoder_json.at(g_decoder_iou_threshold).get<float>();
            config.max_candidates_ = decoder_json.at(g_decoder_max_candidates).get<std::size_t>();
            const auto& stages = decoder_json.at(g_decoder_stages);
            if (!stages.is_array() || stages.empty() ||
                stages.size() > anchor_distance_decoder_limits::g_max_stages) {
                return {status_code::invalid_argument, "decoder stages are invalid"};
            }
            for (const auto& stage : stages) {
                config.stages_.push_back({
                    stage.at(g_decoder_score_tensor).get<std::string>(),
                    stage.at(g_decoder_box_tensor).get<std::string>(),
                    stage.at(g_decoder_landmark_tensor).get<std::string>(),
                    stage.at(g_decoder_stride).get<std::uint32_t>(),
                    stage.at(g_decoder_grid_width).get<std::uint32_t>(),
                    stage.at(g_decoder_grid_height).get<std::uint32_t>(),
                    stage.at(g_decoder_anchors_per_cell).get<std::uint32_t>()});
            }
            owner.decoder_ = std::make_unique<anchor_distance_decoder>(std::move(config));
        } else if (kind == g_decoder_yolov8) {
            yolov8_decoder_config decoder_config;
            decoder_config.source_width_ = model_source->profile_.width_;
            decoder_config.source_height_ = model_source->profile_.height_;
            decoder_config.tensor_width_ = declared_input.dimensions_.size() == 4 ?
                declared_input.dimensions_[2] : 0;
            decoder_config.tensor_height_ = declared_input.dimensions_.size() == 4 ?
                declared_input.dimensions_[1] : 0;
            decoder_config.placement_ = model.placement_;
            decoder_config.box_tensor_ = decoder_json.value("box_tensor", std::string{"boxes_out"});
            decoder_config.score_tensor_ = decoder_json.value("score_tensor", std::string{"conf_out"});
            decoder_config.class_count_ = decoder_json.value("class_count", std::size_t{1});
            decoder_config.confidence_threshold_ =
                decoder_json.value("confidence_threshold", 0.25F);
            decoder_config.iou_threshold_ = decoder_json.value("iou_threshold", 0.45F);
            decoder_config.class_names_ = vqec_vision_ai_appl_pdplt_load_labels(
                decoder_json, binding->package_dir_, decoder_config.class_count_);
            owner.decoder_ = std::make_unique<yolov8_decoder>(decoder_config);
        } else {
            return {status_code::unsupported, "unknown decoder package kind"};
        }
        const auto decoder_status =
            owner.decoder_->vqec_vision_ai_cntr_mddec_validate(owner.outputs_);
        if (decoder_status.code_ != status_code::ok) {
            return decoder_status;
        }
        owner.graph_ = std::make_unique<qnn_inference_graph>(*owner.engine_);
        owner.processor_ = std::make_unique<fastcv_processor>(fastcv_processor_config{
            max_frame_allocation_bytes,
            impl.config_.preprocess_output_timeout_ns_});
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
    if (!impl.config_.output_ring_id_.empty()) {
        impl.renderer_ = std::make_unique<qtiv_renderer>();
        qtiv_renderer_config renderer_config;
        renderer_config.ring_id_ = impl.config_.output_ring_id_;
        renderer_config.width_ = _deployment.sources_.front().profile_.width_;
        renderer_config.height_ = _deployment.sources_.front().profile_.height_;
        renderer_config.fps_ = _deployment.sources_.front().profile_.fps_numerator_ /
            (_deployment.sources_.front().profile_.fps_denominator_ != 0 ?
                _deployment.sources_.front().profile_.fps_denominator_ : 1U);
        renderer_config.bitrate_bps_ = impl.config_.output_bitrate_bps_;
        renderer_config.keyframe_interval_frames_ =
            impl.config_.output_keyframe_interval_frames_;
        renderer_config.box_color_rgba_ = impl.config_.output_box_color_rgba_;
        renderer_config.output_surface_count_ = impl.config_.output_surface_count_;
        renderer_config.colorimetry_ = impl.config_.output_colorimetry_;
        renderer_config.interlace_mode_ = impl.config_.output_interlace_mode_;
        const auto rendered = impl.renderer_->vqec_vision_ai_qcom_qtvr_init(renderer_config);
        if (rendered.code_ != status_code::ok) {
            return rendered;
        }
    }
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
    std::uint16_t _source_slot, const std::string& _model_id) noexcept {
    (void)_source_slot;
    if (implementation_ == nullptr) {
        return nullptr;
    }
    for (auto& owner : implementation_->models_) {
        if (owner.model_id_ == _model_id) {
            return owner.graph_.get();
        }
    }
    return nullptr;
}

image_processor_port* production_platform::vqec_vision_ai_appl_pdplt_processor(
    std::uint16_t _source_slot, const std::string& _model_id) noexcept {
    (void)_source_slot;
    if (implementation_ == nullptr) {
        return nullptr;
    }
    for (auto& owner : implementation_->models_) {
        if (owner.model_id_ == _model_id) {
            return owner.processor_.get();
        }
    }
    return nullptr;
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

status production_platform::vqec_vision_ai_appl_pdplt_render(
    std::uint16_t _source_slot, const raw_frame& _frame,
    const observation_batch& _observations) {
    (void)_source_slot;
    if (implementation_ == nullptr || !implementation_->is_prepared_) {
        return {status_code::invalid_state, "production platform is not prepared"};
    }
    if (implementation_->renderer_ == nullptr) {
        return {};
    }
    return implementation_->renderer_->vqec_vision_ai_qcom_qtvr_render(_frame, _observations);
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
