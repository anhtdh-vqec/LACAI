#include "vqec_vision_model_decoder_registry.hpp"

#include <new>

namespace vqec::vision::ai {

status model_decoder_registry::vqec_vision_ai_detec_mdreg_register_decoder(
    const std::string& _contract, model_decoder_port& _decoder) {
    if (_contract.empty() || _contract.size() > model_decoder_limits::g_max_contract_bytes) {
        return {status_code::invalid_argument, "decoder contract is empty or too long"};
    }
    for (std::size_t index = 0; index < count_; ++index) {
        if (entries_[index].contract_ == _contract) {
            return {status_code::invalid_argument, "decoder contract is already registered"};
        }
    }
    if (count_ == entries_.size()) {
        return {status_code::resource_exhausted, "decoder registry capacity reached"};
    }
    entries_[count_] = {_contract, &_decoder};
    ++count_;
    return {};
}

status model_decoder_registry::vqec_vision_ai_detec_mdreg_resolve_decoder(
    const std::string& _contract, model_decoder_port*& _decoder) const noexcept {
    _decoder = nullptr;
    if (_contract.empty() || _contract.size() > model_decoder_limits::g_max_contract_bytes) {
        return {status_code::invalid_argument, "decoder contract is empty or too long"};
    }
    for (std::size_t index = 0; index < count_; ++index) {
        if (entries_[index].contract_ == _contract) {
            _decoder = entries_[index].decoder_;
            return _decoder == nullptr ?
                status{status_code::invalid_state, "decoder registration is null"} : status{};
        }
    }
    return {status_code::unsupported, "decoder contract is not registered"};
}

status model_decoder_registry::vqec_vision_ai_detec_mdreg_validate_model_outputs(
    const model_catalog_entry& _model, const model_outputs& _outputs) const {
    if (_model.model_id_.empty() || _model.model_version_.empty() ||
        _model.decoder_contract_.empty() || _outputs.model_id_ != _model.model_id_ ||
        _outputs.model_version_ != _model.model_version_ ||
        _outputs.decoder_contract_ != _model.decoder_contract_) {
        return {status_code::invalid_argument, "model output identity differs from catalog"};
    }
    model_decoder_port* decoder = nullptr;
    const auto resolved = vqec_vision_ai_detec_mdreg_resolve_decoder(
        _model.decoder_contract_, decoder);
    if (resolved.code_ != status_code::ok) {
        return resolved;
    }
    try {
        return decoder->vqec_vision_ai_cntr_mddec_validate(_outputs);
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted, "decoder validation allocation failed"};
    } catch (...) {
        return {status_code::io_error, "decoder validation raised an exception"};
    }
}

void model_decoder_registry::vqec_vision_ai_detec_mdreg_clear() noexcept {
    for (std::size_t index = 0; index < count_; ++index) {
        entries_[index] = {};
    }
    count_ = 0;
}

std::size_t model_decoder_registry::vqec_vision_ai_detec_mdreg_get_count() const noexcept {
    return count_;
}

}  // namespace vqec::vision::ai
