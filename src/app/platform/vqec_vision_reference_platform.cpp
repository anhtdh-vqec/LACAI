#include "vqec_vision_reference_platform.hpp"

#include <utility>

#include "vqec_vision_fixture_detector.hpp"

namespace vqec::vision::ai {
namespace {

// Real device-free IoU tracker: associates detections into stable track ids instead of
// the fake platform's fresh-id-per-detection behaviour.
class reference_tracker_factory final : public tracker_factory_port {
public:
    explicit reference_tracker_factory(reference_tracker_config _config) noexcept
        : config_(_config) {}

    [[nodiscard]] status vqec_vision_ai_track_trfac_validate_activation(
        const std::string& _source_id, const std::string& _model_id) const override {
        (void)_model_id;
        return !_source_id.empty() ? status{} :
            status{status_code::unsupported, "reference tracker requires a source"};
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

// Real device-free zone feature: emits bounded ROI/dwell/line/count events instead of
// the fake platform's one-snapshot-per-track behaviour.
class reference_feature_factory final : public feature_processor_factory_port {
public:
    explicit reference_feature_factory(reference_feature_params _params)
        : params_(std::move(_params)) {}

    [[nodiscard]] status vqec_vision_ai_ports_ftfac_validate_configuration(
        const feature_catalog_entry& _feature, const feature_processor_config& _processor_config,
        const feature_configuration& _configuration) const override {
        (void)_feature;
        (void)_processor_config;
        (void)_configuration;
        if (params_.event_schema_id_.empty() || params_.event_schema_version_.empty()) {
            return {status_code::invalid_argument, "reference feature requires an event schema"};
        }
        if (params_.mode_ != reference_feature_mode::line_crossing &&
            (params_.zone_.width_ <= 0.0F || params_.zone_.height_ <= 0.0F)) {
            return {status_code::invalid_argument, "reference feature zone must have an extent"};
        }
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

struct reference_platform::implementation {
    reference_platform_config config_;
    fixture_detector decoder_;
    reference_tracker_factory tracker_factory_{reference_tracker_config{}};
    reference_feature_factory feature_factory_{reference_feature_params{}};
    bool is_configured_{false};
};

reference_platform::reference_platform()
    : implementation_(std::make_unique<implementation>()) {}

reference_platform::~reference_platform() noexcept = default;

status reference_platform::vqec_vision_ai_appl_rplat_configure(
    const reference_platform_config& _config) {
    if (implementation_ == nullptr) {
        return {status_code::invalid_state, "reference platform implementation is unavailable"};
    }
    auto& impl = *implementation_;
    if (impl.is_configured_) {
        return {status_code::invalid_state, "reference platform is already configured"};
    }
    if (_config.source_width_ == 0 || _config.source_height_ == 0 ||
        _config.tracker_contract_.empty() ||
        _config.feature_.event_schema_id_.empty() ||
        _config.feature_.event_schema_version_.empty()) {
        return {status_code::invalid_argument, "invalid reference platform configuration"};
    }
    reference_platform_config config = _config;
    if (config.feature_.mode_ != reference_feature_mode::line_crossing &&
        (config.feature_.zone_.width_ <= 0.0F || config.feature_.zone_.height_ <= 0.0F)) {
        config.feature_.zone_ = {0.0F, 0.0F,
            static_cast<float>(config.source_width_), static_cast<float>(config.source_height_), 0U,
            {}};
    }
    impl.decoder_ = fixture_detector(
        fixture_detector_config{config.source_width_, config.source_height_});
    impl.tracker_factory_ = reference_tracker_factory{config.tracker_};
    impl.feature_factory_ = reference_feature_factory{config.feature_};
    impl.config_ = std::move(config);
    impl.is_configured_ = true;
    return {};
}

status reference_platform::vqec_vision_ai_appl_rplat_register_decoders(
    const model_catalog& _catalog, model_decoder_registry& _decoders) {
    if (implementation_ == nullptr || !implementation_->is_configured_) {
        return {status_code::invalid_state, "reference platform is not configured"};
    }
    for (const auto& model : _catalog.models_) {
        const auto registered = _decoders.vqec_vision_ai_detec_mdreg_register_decoder(
            model.decoder_contract_, implementation_->decoder_);
        if (registered.code_ != status_code::ok) {
            return registered;
        }
    }
    return {};
}

status reference_platform::vqec_vision_ai_appl_rplat_register_tracker(
    tracker_registry& _trackers) {
    if (implementation_ == nullptr || !implementation_->is_configured_) {
        return {status_code::invalid_state, "reference platform is not configured"};
    }
    return _trackers.vqec_vision_ai_track_trreg_register_factory(
        implementation_->config_.tracker_contract_, implementation_->tracker_factory_);
}

status reference_platform::vqec_vision_ai_appl_rplat_register_features(
    const feature_catalog& _features, feature_processor_registry& _registry) {
    if (implementation_ == nullptr || !implementation_->is_configured_) {
        return {status_code::invalid_state, "reference platform is not configured"};
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

const std::string& reference_platform::vqec_vision_ai_appl_rplat_get_tracker_contract()
    const noexcept {
    static const std::string empty;
    return implementation_ != nullptr ? implementation_->config_.tracker_contract_ : empty;
}

const std::string& reference_platform::vqec_vision_ai_appl_rplat_get_attribute_schema_id()
    const noexcept {
    static const std::string empty;
    return implementation_ != nullptr ? implementation_->config_.feature_.event_schema_id_ : empty;
}

const reference_platform_config& reference_platform::vqec_vision_ai_appl_rplat_get_config()
    const noexcept {
    static const reference_platform_config empty;
    return implementation_ != nullptr ? implementation_->config_ : empty;
}

}  // namespace vqec::vision::ai
