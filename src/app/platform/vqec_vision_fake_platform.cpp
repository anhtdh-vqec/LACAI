#include "vqec_vision_fake_platform.hpp"

#include <utility>

#include "vqec_vision_fixture_detector.hpp"

namespace vqec::vision::ai {
namespace {

// Development fake tracker: assigns a fresh monotonic id per detection.
class fake_platform_tracker final : public tracker_port {
public:
    [[nodiscard]] status vqec_vision_ai_ports_trker_validate_activation() const override {
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_trker_update_tracks(
        const observation_batch& _detections, std::uint64_t _now_monotonic_ns,
        bool _is_source_gap, observation_batch& _tracked) override {
        (void)_now_monotonic_ns;
        (void)_is_source_gap;
        observation_batch candidate = _detections;
        for (auto& item : candidate.observations_) {
            item.track_id_ = ++assigned_;
        }
        _tracked = std::move(candidate);
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_trker_reset_epoch(
        std::uint64_t _source_epoch) override {
        (void)_source_epoch;
        assigned_ = 0;
        return {};
    }

    std::uint64_t assigned_{0};
};

class fake_platform_tracker_factory final : public tracker_factory_port {
public:
    [[nodiscard]] status vqec_vision_ai_track_trfac_validate_activation(
        const std::string& _source_id, const std::string& _model_id) const override {
        (void)_model_id;
        return !_source_id.empty() ? status{} :
            status{status_code::unsupported, "fake tracker requires a source"};
    }
    [[nodiscard]] status vqec_vision_ai_track_trfac_create_tracker(
        const std::string& _source_id, const std::string& _model_id,
        std::unique_ptr<tracker_port>& _tracker) override {
        (void)_source_id;
        (void)_model_id;
        _tracker = std::make_unique<fake_platform_tracker>();
        return {};
    }
};

// Development fake feature: one snapshot event per tracked observation.
class fake_platform_feature final : public feature_processor_port {
public:
    explicit fake_platform_feature(fake_platform_config _config)
        : platform_config_(std::move(_config)) {}
    [[nodiscard]] status vqec_vision_ai_ports_ftpro_validate_activation(
        const feature_processor_config& _config) const override {
        config_ = _config;
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_ftpro_reset_epoch(
        std::uint64_t _source_epoch) override {
        (void)_source_epoch;
        next_event_id_ = 0;
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_ftpro_process_observations(
        const observation_batch& _tracked, std::uint64_t _now_monotonic_ns,
        bool _is_source_gap, feature_event_batch& _events) override {
        (void)_now_monotonic_ns;
        (void)_is_source_gap;
        feature_event_batch candidate;
        candidate.frame_ = _tracked.frame_;
        candidate.geometry_ = _tracked.geometry_;
        for (const auto& item : _tracked.observations_) {
            if (item.track_id_ == 0) {
                continue;
            }
            feature_event event;
            event.frame_ = _tracked.frame_;
            event.source_id_ = config_.source_id_;
            event.feature_id_ = config_.feature_id_;
            event.event_id_ = "fake_" + std::to_string(++next_event_id_);
            event.event_schema_id_ = platform_config_.event_schema_id_;
            event.event_schema_version_ = platform_config_.event_schema_version_;
            event.kind_ = feature_event_kind::snapshot;
            event.occurred_at_ns_ = _tracked.frame_.source_pts_ns_;
            event.episode_begin_ns_ = event.occurred_at_ns_;
            event.config_revision_ = config_.config_revision_;
            event.track_ids_.push_back(item.track_id_);
            feature_event_field field;
            field.schema_id_ = platform_config_.attribute_schema_id_;
            field.schema_version_ = platform_config_.attribute_schema_version_;
            field.value_ = "present";
            field.confidence_ = 0.5F;
            field.quality_ = observation_quality::low;
            event.fields_.push_back(std::move(field));
            candidate.events_.push_back(std::move(event));
        }
        _events = std::move(candidate);
        return {};
    }

    fake_platform_config platform_config_;
    mutable feature_processor_config config_;
    std::uint64_t next_event_id_{0};
};

class fake_platform_feature_factory final : public feature_processor_factory_port {
public:
    fake_platform_feature_factory() = default;
    explicit fake_platform_feature_factory(fake_platform_config _config)
        : platform_config_(std::move(_config)) {}
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
        _processor = std::make_unique<fake_platform_feature>(platform_config_);
        return {};
    }

    fake_platform_config platform_config_;
};

}  // namespace

struct fake_platform::implementation {
    fake_platform_config config_;
    fixture_detector decoder_;
    fake_platform_tracker_factory tracker_factory_;
    fake_platform_feature_factory feature_factory_;
    bool is_configured_{false};
};

fake_platform::fake_platform() : implementation_(std::make_unique<implementation>()) {}

fake_platform::~fake_platform() noexcept = default;

status fake_platform::vqec_vision_ai_appl_fkplt_configure(
    const fake_platform_config& _config) {
    if (implementation_ == nullptr) {
        return {status_code::invalid_state, "fake platform implementation is unavailable"};
    }
    auto& impl = *implementation_;
    if (impl.is_configured_) {
        return {status_code::invalid_state, "fake platform is already configured"};
    }
    if (_config.source_width_ == 0 || _config.source_height_ == 0 ||
        _config.tracker_contract_.empty() || _config.event_schema_id_.empty() ||
        _config.event_schema_version_.empty() || _config.attribute_schema_id_.empty() ||
        _config.attribute_schema_version_.empty()) {
        return {status_code::invalid_argument, "invalid fake platform configuration"};
    }
    impl.config_ = _config;
    impl.decoder_ = fixture_detector(
        fixture_detector_config{_config.source_width_, _config.source_height_});
    impl.feature_factory_ = fake_platform_feature_factory{_config};
    impl.is_configured_ = true;
    return {};
}

status fake_platform::vqec_vision_ai_appl_fkplt_register_decoders(
    const model_catalog& _catalog, model_decoder_registry& _decoders) {
    if (implementation_ == nullptr || !implementation_->is_configured_) {
        return {status_code::invalid_state, "fake platform is not configured"};
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

status fake_platform::vqec_vision_ai_appl_fkplt_register_tracker(
    tracker_registry& _trackers) {
    if (implementation_ == nullptr || !implementation_->is_configured_) {
        return {status_code::invalid_state, "fake platform is not configured"};
    }
    return _trackers.vqec_vision_ai_track_trreg_register_factory(
        implementation_->config_.tracker_contract_, implementation_->tracker_factory_);
}

status fake_platform::vqec_vision_ai_appl_fkplt_register_features(
    const feature_catalog& _features, feature_processor_registry& _registry) {
    if (implementation_ == nullptr || !implementation_->is_configured_) {
        return {status_code::invalid_state, "fake platform is not configured"};
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

const std::string& fake_platform::vqec_vision_ai_appl_fkplt_get_tracker_contract()
    const noexcept {
    static const std::string empty;
    return implementation_ != nullptr ? implementation_->config_.tracker_contract_ : empty;
}

const fake_platform_config& fake_platform::vqec_vision_ai_appl_fkplt_get_config()
    const noexcept {
    static const fake_platform_config empty;
    return implementation_ != nullptr ? implementation_->config_ : empty;
}

}  // namespace vqec::vision::ai
