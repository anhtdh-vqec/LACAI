#include "vqec_vision_reference_sink.hpp"

#include <new>

namespace vqec::vision::ai {

status reference_event_sink::vqec_vision_ai_ports_fesnk_deliver_event(
    const feature_event& _event) {
    try {
        last_ = _event;
    } catch (const std::bad_alloc&) {
        return {status_code::resource_exhausted, "reference event sink copy failed"};
    }
    ++delivered_;
    return {};
}

std::uint64_t reference_event_sink::
vqec_vision_ai_refer_rfsnk_get_delivered() const noexcept {
    return delivered_;
}

const feature_event& reference_event_sink::
vqec_vision_ai_refer_rfsnk_get_last() const noexcept {
    return last_;
}

}  // namespace vqec::vision::ai
