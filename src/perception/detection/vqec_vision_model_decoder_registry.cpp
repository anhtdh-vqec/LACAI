#include "vqec_vision_model_decoder_registry.hpp"

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
