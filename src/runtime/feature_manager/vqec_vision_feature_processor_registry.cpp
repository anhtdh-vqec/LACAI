#include "vqec_vision_feature_processor_registry.hpp"

#include <new>
#include <utility>

#include "vqec/vision/ai/contracts/vqec_vision_identifier.hpp"

namespace vqec::vision::ai {
namespace {

bool vqec_vision_ai_ftmgr_ftreg_is_contract(const std::string& _value) noexcept {
    return vqec_vision_ai_cntr_ident_is_valid(
        _value, feature_catalog_limits::g_max_identifier_bytes);
}

}  // namespace

status feature_processor_registry::vqec_vision_ai_ftmgr_ftreg_register_factory(
    const std::string& _processor_contract,
    feature_processor_factory_port& _factory) {
    if (!vqec_vision_ai_ftmgr_ftreg_is_contract(_processor_contract)) {
        return {status_code::invalid_argument,
            "feature processor contract is empty or too long"};
    }
    for (std::size_t index = 0; index < count_; ++index) {
        if (entries_[index].processor_contract_ == _processor_contract) {
            return {status_code::invalid_argument,
                "feature processor contract is already registered"};
        }
    }
    if (count_ == entries_.size()) {
        return {status_code::resource_exhausted,
            "feature processor registry capacity reached"};
    }
    try {
        entries_[count_] = {_processor_contract, &_factory};
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted,
            "feature processor registration allocation failed"};
    }
    ++count_;
    return {};
}

status feature_processor_registry::vqec_vision_ai_ftmgr_ftreg_resolve_factory(
    const std::string& _processor_contract,
    feature_processor_factory_port*& _factory) const noexcept {
    _factory = nullptr;
    if (!vqec_vision_ai_ftmgr_ftreg_is_contract(_processor_contract)) {
        return {status_code::invalid_argument,
            "feature processor contract is empty or too long"};
    }
    for (std::size_t index = 0; index < count_; ++index) {
        if (entries_[index].processor_contract_ == _processor_contract) {
            _factory = entries_[index].factory_;
            return _factory == nullptr ?
                status{status_code::invalid_state,
                    "feature processor factory registration is null"} : status{};
        }
    }
    return {status_code::unsupported,
        "feature processor contract is not registered"};
}

status feature_processor_registry::vqec_vision_ai_ftmgr_ftreg_create_processor(
    const feature_catalog_entry& _feature, const std::string& _source_id,
    const feature_configuration& _configuration,
    std::unique_ptr<feature_processor_port>& _processor) const {
    const auto valid_configuration =
        vqec_vision_ai_core_ftcat_validate_configuration(_feature, _configuration);
    if (valid_configuration.code_ != status_code::ok) {
        return valid_configuration;
    }
    feature_processor_config processor_config;
    const auto composed = vqec_vision_ai_core_ftcat_compose_processor_config(
        _feature, _source_id, _configuration.revision_, processor_config);
    if (composed.code_ != status_code::ok) {
        return composed;
    }
    feature_processor_factory_port* factory = nullptr;
    const auto resolved = vqec_vision_ai_ftmgr_ftreg_resolve_factory(
        _feature.processor_contract_, factory);
    if (resolved.code_ != status_code::ok) {
        return resolved;
    }
    try {
        const auto valid = factory->vqec_vision_ai_ports_ftfac_validate_configuration(
            _feature, processor_config, _configuration);
        if (valid.code_ != status_code::ok) {
            return valid;
        }
        std::unique_ptr<feature_processor_port> candidate;
        const auto created = factory->vqec_vision_ai_ports_ftfac_create_processor(
            _feature, processor_config, _configuration, candidate);
        if (created.code_ != status_code::ok) {
            return created;
        }
        if (!candidate) {
            return {status_code::invalid_state,
                "feature processor factory returned no owner"};
        }
        _processor = std::move(candidate);
        return {};
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted,
            "feature processor creation allocation failed"};
    } catch (...) {
        return {status_code::io_error,
            "feature processor factory raised an exception"};
    }
}

void feature_processor_registry::vqec_vision_ai_ftmgr_ftreg_clear() noexcept {
    for (std::size_t index = 0; index < count_; ++index) {
        entries_[index] = {};
    }
    count_ = 0;
}

std::size_t feature_processor_registry::
vqec_vision_ai_ftmgr_ftreg_get_count() const noexcept {
    return count_;
}

}  // namespace vqec::vision::ai
