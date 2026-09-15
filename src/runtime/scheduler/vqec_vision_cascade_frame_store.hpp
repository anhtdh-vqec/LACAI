#ifndef VQEC_VISION_AI_SCHED_CASCADE_FRAME_STORE_HPP
#define VQEC_VISION_AI_SCHED_CASCADE_FRAME_STORE_HPP

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>
#include "vqec/vision/ai/contracts/vqec_vision_preview_contract.hpp"
#include "vqec/vision/ai/ports/vqec_vision_raw_source.hpp"

namespace vqec::vision::ai {

// Serial owner. Acquired frame copies MUST survive every submitted hardware read.
// Retire closes admission; complete is called only after actual task completion.
//
// A completion ticket carries a store domain in its high bits. Tickets are therefore not
// portable between store instances: a stale callback from a retired store cannot release a
// task in a replacement store even if the low counter matches.
class cascade_frame_store {
public:
    cascade_frame_store(const cascade_frame_store&) = delete;
    cascade_frame_store& operator=(const cascade_frame_store&) = delete;
    // _domain 0 asks for a process-unique domain; a nonzero value is used as given (masked
    // to the domain field). A zero frame, task or byte budget makes the store unusable and
    // every retain fails closed instead of silently admitting nothing.
    cascade_frame_store(std::size_t _frames, std::size_t _tasks_per_frame,
        std::uint64_t _max_bytes, std::uint32_t _domain = 0)
        : slots_(vqec_vision_ai_sched_cfstr_valid_budget(_frames, _tasks_per_frame, _max_bytes) ?
              _frames : 0),
          max_bytes_(_max_bytes),
          is_valid_(vqec_vision_ai_sched_cfstr_valid_budget(
              _frames, _tasks_per_frame, _max_bytes)) {
        domain_ = _domain != 0 ? (_domain & g_domain_mask) :
            (vqec_vision_ai_sched_cfstr_next_domain() & g_domain_mask);
        if (domain_ == 0) {
            domain_ = 1;
        }
        for (auto& entry : slots_) {
            entry.tickets_.resize(_tasks_per_frame);
        }
    }
    [[nodiscard]] status vqec_vision_ai_sched_cfstr_retain(
        const preview_frame_key& _key, const raw_frame& _frame) {
        if (!is_valid_) {
            return {status_code::invalid_state, "cascade frame store has no admitted budget"};
        }
        if (!_frame.owner_ || _frame.native_handle_ < 0 ||
            _frame.descriptor_.allocation_size_bytes_ == 0 ||
            _key.source_epoch_ != _frame.descriptor_.session_epoch_ ||
            _key.frame_id_ != _frame.descriptor_.buffer_id_ ||
            _key.source_pts_ns_ != _frame.descriptor_.pts_ns_) {
            return {status_code::invalid_argument, "invalid cascade frame identity or owner"};
        }
        slot* available = nullptr;
        for (auto& entry : slots_) {
            if (entry.occupied_ && vqec_vision_ai_sched_cfstr_equal(entry.key_, _key)) {
                return {status_code::invalid_state, "duplicate retained frame"};
            }
            if (!entry.occupied_ && !entry.tickets_.empty()) {
                available = &entry;
            }
        }
        if (!available || _frame.descriptor_.allocation_size_bytes_ > max_bytes_ - bytes_) {
            return {status_code::resource_exhausted, "cascade frame budget exhausted"};
        }
        available->key_ = _key;
        available->frame_ = _frame;
        available->occupied_ = true;
        available->admits_tasks_ = true;
        bytes_ += _frame.descriptor_.allocation_size_bytes_;
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_sched_cfstr_acquire(
        const preview_frame_key& _key, raw_frame& _frame, std::uint64_t& _ticket) {
        if (!is_valid_) {
            return {status_code::invalid_state, "cascade frame store has no admitted budget"};
        }
        for (auto& entry : slots_) {
            if (!entry.occupied_ || !entry.admits_tasks_ ||
                !vqec_vision_ai_sched_cfstr_equal(entry.key_, _key)) {
                continue;
            }
            auto free = std::find(entry.tickets_.begin(), entry.tickets_.end(), 0);
            if (free == entry.tickets_.end() || next_counter_ > g_counter_mask) {
                return {status_code::resource_exhausted, "cascade task budget exhausted"};
            }
            const std::uint64_t ticket =
                (static_cast<std::uint64_t>(domain_) << g_domain_shift) | next_counter_;
            ++next_counter_;
            *free = ticket;
            _ticket = ticket;
            _frame = entry.frame_;
            return {};
        }
        return {status_code::invalid_state, "exact cascade frame unavailable"};
    }
    [[nodiscard]] status vqec_vision_ai_sched_cfstr_retire(const preview_frame_key& _key) {
        if (!is_valid_) {
            return {status_code::invalid_state, "cascade frame store has no admitted budget"};
        }
        for (auto& entry : slots_) {
            if (entry.occupied_ && vqec_vision_ai_sched_cfstr_equal(entry.key_, _key)) {
                entry.admits_tasks_ = false;
                vqec_vision_ai_sched_cfstr_reclaim(entry);
                return {};
            }
        }
        return {status_code::invalid_state, "frame is not retained"};
    }
    [[nodiscard]] status vqec_vision_ai_sched_cfstr_complete(std::uint64_t _ticket) {
        // Reject a ticket minted by another (or an older) store instance.
        if (!is_valid_ || _ticket == 0 ||
            (_ticket >> g_domain_shift) != domain_) {
            return {status_code::invalid_state, "completion ticket is not outstanding"};
        }
        for (auto& entry : slots_) {
            auto found = std::find(entry.tickets_.begin(), entry.tickets_.end(), _ticket);
            if (entry.occupied_ && found != entry.tickets_.end()) {
                *found = 0;
                vqec_vision_ai_sched_cfstr_reclaim(entry);
                return {};
            }
        }
        return {status_code::invalid_state, "completion ticket is not outstanding"};
    }
    [[nodiscard]] std::uint64_t vqec_vision_ai_sched_cfstr_bytes() const noexcept {
        return bytes_;
    }
private:
    struct slot {
        preview_frame_key key_;
        raw_frame frame_;
        std::vector<std::uint64_t> tickets_;
        bool occupied_{false};
        bool admits_tasks_{false};
    };
    static constexpr unsigned g_domain_shift = 48;
    static constexpr std::uint64_t g_counter_mask = (1ULL << g_domain_shift) - 1ULL;
    static constexpr std::uint32_t g_domain_mask = 0xFFFFU;

    static bool vqec_vision_ai_sched_cfstr_valid_budget(
        std::size_t _frames, std::size_t _tasks_per_frame, std::uint64_t _max_bytes) noexcept {
        return _frames != 0 && _tasks_per_frame != 0 && _max_bytes != 0;
    }
    static std::uint32_t vqec_vision_ai_sched_cfstr_next_domain() noexcept {
        static std::atomic<std::uint32_t> counter{0};
        return counter.fetch_add(1, std::memory_order_relaxed) + 1U;
    }
    static bool vqec_vision_ai_sched_cfstr_equal(
        const preview_frame_key& _left, const preview_frame_key& _right) noexcept {
        return _left.camera_id_ == _right.camera_id_ &&
            _left.channel_id_ == _right.channel_id_ &&
            _left.source_epoch_ == _right.source_epoch_ &&
            _left.frame_id_ == _right.frame_id_ &&
            _left.source_pts_ns_ == _right.source_pts_ns_;
    }
    void vqec_vision_ai_sched_cfstr_reclaim(slot& _slot) {
        if (!_slot.admits_tasks_ && std::all_of(_slot.tickets_.begin(), _slot.tickets_.end(),
                [](std::uint64_t _ticket) { return _ticket == 0; })) {
            bytes_ -= _slot.frame_.descriptor_.allocation_size_bytes_;
            _slot.frame_ = {};
            _slot.occupied_ = false;
        }
    }
    std::vector<slot> slots_;
    std::uint64_t max_bytes_{0};
    std::uint64_t bytes_{0};
    std::uint64_t next_counter_{1};
    std::uint32_t domain_{0};
    bool is_valid_{false};
};
}  // namespace vqec::vision::ai
#endif
