#include "vqec_vision_reference_ring_sink.hpp"

namespace vqec::vision::ai {

reference_ring_sink::reference_ring_sink(reference_ring_sink_config _config)
    : config_(_config) {}

status reference_ring_sink::vqec_vision_ai_cntr_encsk_query_demand(
    encoded_sink_demand& _demand) {
    if (config_.mapping_generation_ == 0) {
        return {status_code::invalid_argument, "reference ring has no ready generation"};
    }
    encoded_sink_demand demand;
    demand.mapping_generation_ = config_.mapping_generation_;
    demand.active_consumers_ = consumers_;
    _demand = demand;
    return {};
}

status reference_ring_sink::vqec_vision_ai_cntr_encsk_write(
    const h264_access_unit_view& _unit, std::uint64_t _expected_generation) {
    if (_expected_generation == 0 ||
        _expected_generation != config_.mapping_generation_) {
        ++rejected_;
        return {status_code::invalid_argument, "reference ring mapping generation mismatch"};
    }
    if (_unit.payload_.data_ == nullptr || _unit.payload_.size_ == 0 ||
        _unit.payload_.size_ > config_.max_access_unit_bytes_ ||
        _unit.geometry_.width_ == 0 || _unit.geometry_.height_ == 0) {
        ++rejected_;
        return {status_code::invalid_argument, "invalid access unit for the reference ring"};
    }
    if (config_.capacity_ == 0 || depth_ >= config_.capacity_) {
        ++rejected_;
        return {status_code::resource_exhausted, "reference ring is full; backpressure"};
    }
    ++depth_;
    ++written_;
    last_bytes_ = _unit.payload_.size_;
    return {};
}

void reference_ring_sink::vqec_vision_ai_refer_rring_set_active_consumers(
    unsigned _consumers) noexcept {
    consumers_ = _consumers;
}

void reference_ring_sink::vqec_vision_ai_refer_rring_consume_one() noexcept {
    if (depth_ > 0) {
        --depth_;
    }
}

std::size_t reference_ring_sink::vqec_vision_ai_refer_rring_get_depth() const noexcept {
    return depth_;
}

std::size_t reference_ring_sink::vqec_vision_ai_refer_rring_get_capacity() const noexcept {
    return config_.capacity_;
}

std::uint64_t reference_ring_sink::vqec_vision_ai_refer_rring_get_written() const noexcept {
    return written_;
}

std::uint64_t reference_ring_sink::vqec_vision_ai_refer_rring_get_rejected() const noexcept {
    return rejected_;
}

std::size_t reference_ring_sink::vqec_vision_ai_refer_rring_get_last_bytes() const noexcept {
    return last_bytes_;
}

}  // namespace vqec::vision::ai
