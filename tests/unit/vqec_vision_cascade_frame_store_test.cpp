#include <iostream>
#include <memory>
#include "vqec_vision_cascade_frame_store.hpp"

int main() {
    using namespace vqec::vision::ai;
    unsigned failures = 0;
    const auto check = [&failures](bool _ok) { if (!_ok) { ++failures; } };
    // Independent boundary fixture: one frame, two secondary tasks, sixteen bytes.
    cascade_frame_store store(1, 2, 16);
    raw_frame frame;
    frame.native_handle_ = 1;
    frame.owner_ = std::make_shared<int>(0);
    std::weak_ptr<const void> lifetime = frame.owner_;
    frame.descriptor_.allocation_size_bytes_ = 16;
    frame.descriptor_.session_epoch_ = 1;
    frame.descriptor_.buffer_id_ = 2;
    frame.descriptor_.pts_ns_ = 3;
    preview_frame_key key{0, 0, 1, 2, 3};
    check(store.vqec_vision_ai_sched_cfstr_retain(key, frame).code_ == status_code::ok);
    check(store.vqec_vision_ai_sched_cfstr_retain(key, frame).code_ == status_code::invalid_state);
    raw_frame task;
    std::uint64_t ticket = 0;
    auto wrong_epoch = key;
    ++wrong_epoch.source_epoch_;
    check(store.vqec_vision_ai_sched_cfstr_acquire(wrong_epoch, task, ticket).code_ ==
        status_code::invalid_state);
    check(store.vqec_vision_ai_sched_cfstr_acquire(key, task, ticket).code_ == status_code::ok);
    const auto first_ticket = ticket;
    raw_frame second;
    check(store.vqec_vision_ai_sched_cfstr_acquire(key, second, ticket).code_ == status_code::ok);
    std::uint64_t rejected_ticket = 0;
    raw_frame rejected;
    check(store.vqec_vision_ai_sched_cfstr_acquire(key, rejected, rejected_ticket).code_ ==
        status_code::resource_exhausted);
    check(store.vqec_vision_ai_sched_cfstr_retire(key).code_ == status_code::ok);
    check(store.vqec_vision_ai_sched_cfstr_bytes() == 16);
    check(store.vqec_vision_ai_sched_cfstr_acquire(key, rejected, rejected_ticket).code_ ==
        status_code::invalid_state);
    frame.owner_.reset();
    task.owner_.reset(); // simulated real completion of the first task
    check(store.vqec_vision_ai_sched_cfstr_complete(first_ticket).code_ == status_code::ok);
    check(store.vqec_vision_ai_sched_cfstr_complete(first_ticket).code_ == status_code::invalid_state);
    check(!lifetime.expired() && store.vqec_vision_ai_sched_cfstr_bytes() == 16);
    second.owner_.reset(); // simulated real completion of the final task
    check(store.vqec_vision_ai_sched_cfstr_complete(ticket).code_ == status_code::ok);
    check(lifetime.expired() && store.vqec_vision_ai_sched_cfstr_bytes() == 0);

    // A completion ticket is not portable between store instances.
    {
        cascade_frame_store first(1, 1, 16);
        cascade_frame_store replacement(1, 1, 16);
        raw_frame owned;
        owned.native_handle_ = 1;
        owned.owner_ = std::make_shared<int>(0);
        owned.descriptor_.allocation_size_bytes_ = 16;
        owned.descriptor_.session_epoch_ = 1;
        owned.descriptor_.buffer_id_ = 2;
        owned.descriptor_.pts_ns_ = 3;
        check(first.vqec_vision_ai_sched_cfstr_retain(key, owned).code_ == status_code::ok);
        raw_frame acquired;
        std::uint64_t first_ticket = 0;
        check(first.vqec_vision_ai_sched_cfstr_acquire(key, acquired, first_ticket).code_ ==
            status_code::ok);
        check(replacement.vqec_vision_ai_sched_cfstr_retain(key, owned).code_ == status_code::ok);
        check(replacement.vqec_vision_ai_sched_cfstr_complete(first_ticket).code_ ==
            status_code::invalid_state);
        check(first.vqec_vision_ai_sched_cfstr_complete(first_ticket).code_ == status_code::ok);
    }

    // A zero budget fails closed instead of silently admitting no tasks.
    {
        cascade_frame_store empty(0, 0, 0);
        check(empty.vqec_vision_ai_sched_cfstr_retain(key, frame).code_ ==
            status_code::invalid_state);
        raw_frame none;
        std::uint64_t none_ticket = 0;
        check(empty.vqec_vision_ai_sched_cfstr_acquire(key, none, none_ticket).code_ ==
            status_code::invalid_state);
    }

    std::cout << "cascade frame store failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
