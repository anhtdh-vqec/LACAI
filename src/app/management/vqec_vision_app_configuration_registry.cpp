#include "vqec_vision_app_configuration_registry.hpp"

#include <new>

#include "vqec/vision/ai/contracts/vqec_vision_identifier.hpp"

namespace vqec::vision::ai {

status app_configuration_registry::vqec_vision_ai_appl_apcrg_register(
    const std::string& _app_id, const std::string& _schema_id,
    const std::string& _processor_contract,
    app_configuration_validator_port& _validator) {
    if (!vqec_vision_ai_cntr_ident_is_valid(
            _app_id, app_lifecycle_limits::g_max_identifier_bytes) ||
        !vqec_vision_ai_cntr_ident_is_valid(
            _schema_id, app_lifecycle_limits::g_max_identifier_bytes) ||
        !vqec_vision_ai_cntr_ident_is_valid(
            _processor_contract, app_lifecycle_limits::g_max_identifier_bytes)) {
        return {status_code::invalid_argument, "invalid app configuration registration"};
    }
    for (std::size_t index = 0; index < count_; ++index) {
        if (entries_[index].app_id_ == _app_id ||
            entries_[index].schema_id_ == _schema_id ||
            entries_[index].processor_contract_ == _processor_contract) {
            return {status_code::invalid_argument,
                "duplicate app configuration registration"};
        }
    }
    if (count_ == entries_.size()) {
        return {status_code::resource_exhausted,
            "app configuration registry capacity reached"};
    }
    try {
        entries_[count_] = {_app_id, _schema_id, _processor_contract, &_validator};
        ++count_;
        return {};
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted,
            "app configuration registration allocation failed"};
    }
}

status app_configuration_registry::vqec_vision_ai_appl_apcrg_validate(
    const std::string& _app_id, const std::string& _schema_id,
    std::uint64_t _revision, const std::vector<std::uint8_t>& _payload) const {
    for (std::size_t index = 0; index < count_; ++index) {
        const auto& entry = entries_[index];
        if (entry.app_id_ == _app_id && entry.schema_id_ == _schema_id) {
            if (entry.validator_ == nullptr) {
                return {status_code::invalid_state,
                    "app configuration validator is unavailable"};
            }
            return entry.validator_->vqec_vision_ai_ports_apcfg_validate(
                _schema_id, _revision, _payload);
        }
    }
    return {status_code::unsupported, "app configuration schema is not registered"};
}

bool app_configuration_registry::vqec_vision_ai_appl_apcrg_supports_manifest(
    const usecase_app_manifest& _manifest) const noexcept {
    for (const auto& feature : _manifest.features_) {
        bool found = false;
        for (std::size_t index = 0; index < count_; ++index) {
            const auto& entry = entries_[index];
            if (entry.app_id_ == _manifest.app_id_ &&
                entry.schema_id_ == feature.configuration_schema_id_ &&
                entry.processor_contract_ == feature.processor_contract_) {
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }
    return !_manifest.features_.empty();
}

}  // namespace vqec::vision::ai

