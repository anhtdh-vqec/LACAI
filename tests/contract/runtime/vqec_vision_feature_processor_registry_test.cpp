#include <cassert>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "vqec_vision_feature_processor_registry.hpp"

using namespace vqec::vision::ai;

namespace {

class fake_processor final : public feature_processor_port {
public:
    fake_processor(feature_processor_config _config, std::vector<std::uint8_t> _payload)
        : config_(std::move(_config)), payload_(std::move(_payload)) {}

    [[nodiscard]] status vqec_vision_ai_ports_ftpro_validate_activation(
        const feature_processor_config& _config) const override {
        return _config.config_revision_ == config_.config_revision_ ? status{} :
            status{status_code::invalid_argument, "revision mismatch"};
    }

    [[nodiscard]] status vqec_vision_ai_ports_ftpro_reset_epoch(
        std::uint64_t _source_epoch) override {
        (void)_source_epoch;
        return {};
    }

    [[nodiscard]] status vqec_vision_ai_ports_ftpro_process_observations(
        const observation_batch& _tracked, std::uint64_t _now_monotonic_ns,
        bool _is_source_gap, feature_event_batch& _events) override {
        (void)_tracked;
        (void)_now_monotonic_ns;
        (void)_is_source_gap;
        (void)_events;
        return {};
    }

    feature_processor_config config_;
    std::vector<std::uint8_t> payload_;
};

class fake_factory final : public feature_processor_factory_port {
public:
    [[nodiscard]] status vqec_vision_ai_ports_ftfac_validate_configuration(
        const feature_catalog_entry& _feature,
        const feature_processor_config& _processor_config,
        const feature_configuration& _configuration) const override {
        ++validate_count_;
        if (_feature.processor_contract_ != "counting.processor.v1" ||
            _processor_config.feature_id_ != "counting" ||
            _configuration.payload_ != std::vector<std::uint8_t>{1, 2, 3}) {
            return {status_code::invalid_argument, "unsupported fixture configuration"};
        }
        return {};
    }

    [[nodiscard]] status vqec_vision_ai_ports_ftfac_create_processor(
        const feature_catalog_entry& _feature,
        const feature_processor_config& _processor_config,
        const feature_configuration& _configuration,
        std::unique_ptr<feature_processor_port>& _processor) override {
        (void)_feature;
        ++create_count_;
        if (returns_null_) {
            return {};
        }
        _processor = std::make_unique<fake_processor>(
            _processor_config, _configuration.payload_);
        return {};
    }

    mutable unsigned validate_count_{0};
    unsigned create_count_{0};
    bool returns_null_{false};
};

feature_catalog_entry vqec_vision_ai_ctest_fprct_make_feature() {
    feature_catalog_entry feature;
    feature.feature_id_ = "counting";
    feature.feature_version_ = "1.0";
    feature.processor_contract_ = "counting.processor.v1";
    feature.configuration_schema_ = "counting.configuration.v1";
    feature.model_dependencies_.push_back({"person_tracks", "person_detector"});
    feature.resources_ = {1024, 4, 8, 8};
    return feature;
}

}  // namespace

int main() {
    feature_processor_registry registry;
    fake_factory factory;
    assert(registry.vqec_vision_ai_ftmgr_ftreg_get_count() == 0U);
    assert(registry.vqec_vision_ai_ftmgr_ftreg_register_factory(
               "counting processor", factory).code_ == status_code::invalid_argument);
    assert(registry.vqec_vision_ai_ftmgr_ftreg_register_factory(
               "counting.processor.v1", factory).code_ == status_code::ok);
    assert(registry.vqec_vision_ai_ftmgr_ftreg_register_factory(
               "counting.processor.v1", factory).code_ == status_code::invalid_argument);

    feature_processor_factory_port* resolved = nullptr;
    assert(registry.vqec_vision_ai_ftmgr_ftreg_resolve_factory(
               "counting.processor.v1", resolved).code_ == status_code::ok);
    assert(resolved == &factory);
    assert(registry.vqec_vision_ai_ftmgr_ftreg_resolve_factory(
               "missing.processor.v1", resolved).code_ == status_code::unsupported);
    assert(resolved == nullptr);

    const auto feature = vqec_vision_ai_ctest_fprct_make_feature();
    feature_configuration configuration{
        "counting.configuration.v1", 7, {1, 2, 3}};
    std::unique_ptr<feature_processor_port> processor;
    assert(registry.vqec_vision_ai_ftmgr_ftreg_create_processor(
               feature, "source.front", configuration, processor).code_ == status_code::ok);
    const auto* created = dynamic_cast<const fake_processor*>(processor.get());
    assert(created != nullptr && created->config_.source_id_ == "source.front" &&
           created->config_.config_revision_ == 7 &&
           created->payload_ == configuration.payload_ && factory.validate_count_ == 1U &&
           factory.create_count_ == 1U);

    auto wrong_schema = configuration;
    wrong_schema.schema_id_ = "other.configuration.v1";
    auto* const previous = processor.get();
    assert(registry.vqec_vision_ai_ftmgr_ftreg_create_processor(
               feature, "source.front", wrong_schema, processor).code_ ==
           status_code::invalid_argument);
    assert(processor.get() == previous && factory.validate_count_ == 1U);

    factory.returns_null_ = true;
    assert(registry.vqec_vision_ai_ftmgr_ftreg_create_processor(
               feature, "source.front", configuration, processor).code_ ==
           status_code::invalid_state);
    assert(processor.get() == previous && factory.create_count_ == 2U);

    auto oversized = configuration;
    oversized.payload_.resize(feature_catalog_limits::g_max_configuration_bytes + 1U);
    assert(registry.vqec_vision_ai_ftmgr_ftreg_create_processor(
               feature, "source.front", oversized, processor).code_ ==
           status_code::resource_exhausted);
    assert(processor.get() == previous);

    registry.vqec_vision_ai_ftmgr_ftreg_clear();
    assert(registry.vqec_vision_ai_ftmgr_ftreg_get_count() == 0U);
    return 0;
}
