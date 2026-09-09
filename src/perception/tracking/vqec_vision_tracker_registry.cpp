#include "vqec_vision_tracker_registry.hpp"

#include <new>

namespace vqec::vision::ai {
namespace {

bool vqec_vision_ai_track_trreg_is_contract(const std::string& _value) noexcept {
    if (_value.empty() || _value.size() > tracker_registry_limits::g_max_contract_bytes) {
        return false;
    }
    for (const unsigned char character : _value) {
        if (!((character >= 'a' && character <= 'z') ||
              (character >= 'A' && character <= 'Z') ||
              (character >= '0' && character <= '9') || character == '_' ||
              character == '-' || character == '.' || character == ':')) {
            return false;
        }
    }
    return true;
}

bool vqec_vision_ai_track_trreg_is_binding_identifier(
    const std::string& _value) noexcept {
    return !_value.empty() &&
        _value.size() <= tracker_registry_limits::g_max_binding_identifier_bytes;
}

}  // namespace

status tracker_registry::vqec_vision_ai_track_trreg_register_factory(
    const std::string& _contract, tracker_factory_port& _factory) {
    if (!vqec_vision_ai_track_trreg_is_contract(_contract)) {
        return {status_code::invalid_argument,
            "tracker contract is empty, too long or contains invalid characters"};
    }
    for (std::size_t index = 0; index < count_; ++index) {
        if (entries_[index].contract_ == _contract) {
            return {status_code::invalid_argument,
                "tracker factory contract is already registered"};
        }
    }
    if (count_ == entries_.size()) {
        return {status_code::resource_exhausted,
            "tracker factory registry capacity reached"};
    }
    try {
        entries_[count_] = {_contract, &_factory};
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted,
            "tracker factory registration allocation failed"};
    }
    ++count_;
    return {};
}

status tracker_registry::vqec_vision_ai_track_trreg_resolve_factory(
    const std::string& _contract, tracker_factory_port*& _factory) const noexcept {
    _factory = nullptr;
    if (!vqec_vision_ai_track_trreg_is_contract(_contract)) {
        return {status_code::invalid_argument,
            "tracker contract is empty, too long or contains invalid characters"};
    }
    for (std::size_t index = 0; index < count_; ++index) {
        if (entries_[index].contract_ == _contract) {
            _factory = entries_[index].factory_;
            return _factory == nullptr ?
                status{status_code::invalid_state,
                    "tracker factory registration is null"} : status{};
        }
    }
    return {status_code::unsupported, "tracker factory contract is not registered"};
}

status tracker_registry::vqec_vision_ai_track_trreg_create_tracker(
    const std::string& _contract, const std::string& _source_id,
    const std::string& _model_id, std::unique_ptr<tracker_port>& _tracker) const {
    if (!vqec_vision_ai_track_trreg_is_binding_identifier(_source_id) ||
        !vqec_vision_ai_track_trreg_is_binding_identifier(_model_id)) {
        return {status_code::invalid_argument,
            "tracker source or model identity is empty or too long"};
    }
    tracker_factory_port* factory = nullptr;
    const auto resolved = vqec_vision_ai_track_trreg_resolve_factory(
        _contract, factory);
    if (resolved.code_ != status_code::ok) {
        return resolved;
    }
    try {
        const auto valid = factory->vqec_vision_ai_track_trfac_validate_activation(
            _source_id, _model_id);
        if (valid.code_ != status_code::ok) {
            return valid;
        }
        std::unique_ptr<tracker_port> candidate;
        const auto created = factory->vqec_vision_ai_track_trfac_create_tracker(
            _source_id, _model_id, candidate);
        if (created.code_ != status_code::ok) {
            return created;
        }
        if (!candidate) {
            return {status_code::invalid_state,
                "tracker factory returned no owner"};
        }
        _tracker = std::move(candidate);
        return {};
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted,
            "tracker creation allocation failed"};
    } catch (...) {
        return {status_code::io_error, "tracker factory raised an exception"};
    }
}

void tracker_registry::vqec_vision_ai_track_trreg_clear() noexcept {
    for (std::size_t index = 0; index < count_; ++index) {
        entries_[index] = {};
    }
    count_ = 0;
}

std::size_t tracker_registry::vqec_vision_ai_track_trreg_get_count() const noexcept {
    return count_;
}

}  // namespace vqec::vision::ai
