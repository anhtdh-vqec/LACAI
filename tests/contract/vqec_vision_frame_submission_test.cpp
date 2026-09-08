#include <cstdlib>
#include <iostream>
#include <memory>

#include <unistd.h>
#include <gst/app/gstappsink.h>

#include "vqec_vision_frame_submission.hpp"

namespace {

struct test_frame {
    int fd_{-1};
    ~test_frame() noexcept {
        if (fd_ >= 0) {
            ::close(fd_);
        }
    }
};

struct test_pipeline {
    GstElement* pipeline_{nullptr};
    GstSample* sample_{nullptr};
    ~test_pipeline() noexcept {
        // Only standard appsrc/appsink; no device reader exists in this fixture.
        if (pipeline_ != nullptr) {
            gst_element_set_state(pipeline_, GST_STATE_NULL);
        }
        if (sample_ != nullptr) {
            gst_sample_unref(sample_);
        }
        if (pipeline_ != nullptr) {
            gst_object_unref(pipeline_);
        }
    }
};

}  // namespace

int main() {
    using namespace vqec::vision::ai;
    gst_init(nullptr, nullptr);
    unsigned failures = 0;
    const auto check = [&](bool _condition) {
        if (!_condition) {
            ++failures;
        }
    };
    auto owner = std::make_shared<test_frame>();
    char path[] = "/tmp/vqec_ai_submission_XXXXXX";
    owner->fd_ = ::mkstemp(path);
    if (owner->fd_ < 0 || ::unlink(path) != 0 || ::ftruncate(owner->fd_, 4096) != 0) {
        return 1;
    }
    frame_descriptor descriptor;
    descriptor.width_ = 32;
    descriptor.height_ = 8;
    descriptor.offsets_ = {16, 576};
    descriptor.strides_ = {64, 64};
    descriptor.view_size_bytes_ = 1024;
    descriptor.memory_offset_bytes_ = 128;
    descriptor.allocation_size_bytes_ = 4096;
    descriptor.session_epoch_ = 7;
    descriptor.pts_ns_ = 100;
    const dmabuf_bridge_profile profile{32, 8, 4096};
    submission_window window;
    check(window.vqec_vision_ai_core_subwn_configure({42, 7, 500, 1000, 1}).code_ ==
          status_code::ok);
    frame_submission job;
    check(job.vqec_vision_ai_qcom_frsub_reset().code_ == status_code::ok);
    check(job.vqec_vision_ai_qcom_frsub_poll_input(window).code_ == status_code::invalid_state);
    test_pipeline fixture;
    fixture.pipeline_ = gst_pipeline_new(nullptr);
    auto* source = gst_element_factory_make("appsrc", nullptr);
    auto* sink = gst_element_factory_make("appsink", nullptr);
    if (fixture.pipeline_ == nullptr || source == nullptr || sink == nullptr) {
        if (source != nullptr) {
            gst_object_unref(source);
        }
        if (sink != nullptr) {
            gst_object_unref(sink);
        }
        return 1;
    }
    gst_bin_add_many(GST_BIN(fixture.pipeline_), source, sink, nullptr);
    g_object_set(source, "is-live", TRUE, "format", GST_FORMAT_TIME, "block", FALSE, nullptr);
    g_object_set(sink, "sync", FALSE, "async", FALSE, "enable-last-sample", FALSE,
                 "max-buffers", 1U, "drop", FALSE, nullptr);
    if (!gst_element_link(source, sink)) {
        return 1;
    }
    const auto push = [&](frame_submission& _job) {
        return vqec_vision_ai_qcom_frsub_push_frame(
            GST_APP_SRC(source), window, descriptor, owner->fd_, owner, profile, 1024, 0, _job);
    };
    g_object_set(source, "block", TRUE, nullptr);
    check(push(job).code_ == status_code::invalid_argument);
    g_object_set(source, "block", FALSE, nullptr);
    descriptor.session_epoch_ = 8;
    check(push(job).code_ == status_code::invalid_argument);
    descriptor.session_epoch_ = 7;
    descriptor.view_size_bytes_ = 1025;
    check(push(job).code_ == status_code::invalid_argument);
    descriptor.view_size_bytes_ = 1024;
    check(window.vqec_vision_ai_core_subwn_get_outstanding() == 0);
    descriptor.offsets_[1] = 16;
    check(push(job).code_ == status_code::invalid_argument);
    check(!job.vqec_vision_ai_qcom_frsub_has_submission());
    check(window.vqec_vision_ai_core_subwn_get_outstanding() == 0);
    descriptor.offsets_[1] = 576;
    ++descriptor.pts_ns_;  // Cancelled reservation intentionally leaves a timestamp gap.
    if (gst_element_set_state(fixture.pipeline_, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
        return 1;
    }
    if (push(job).code_ != status_code::ok) {
        return 1;
    }
    check(job.vqec_vision_ai_qcom_frsub_has_submission());
    check(job.vqec_vision_ai_qcom_frsub_reset().code_ == status_code::pending);
    frame_submission excess;
    ++descriptor.pts_ns_;
    check(push(excess).code_ == status_code::resource_exhausted);
    check(!excess.vqec_vision_ai_qcom_frsub_has_submission());
    fixture.sample_ = gst_app_sink_try_pull_sample(GST_APP_SINK(sink), GST_SECOND);
    if (fixture.sample_ == nullptr) {
        return 1;
    }
    const auto ticket = job.vqec_vision_ai_qcom_frsub_get_ticket();
    check(GST_BUFFER_PTS(gst_sample_get_buffer(fixture.sample_)) == ticket.pipeline_pts_ns_);
    std::weak_ptr<test_frame> lifetime = owner;
    check(owner.use_count() > 1);  // Sample memory, not only this test, retains the frame.
    check(job.vqec_vision_ai_qcom_frsub_poll_input(window).code_ == status_code::pending);
    check(window.vqec_vision_ai_core_subwn_check_deadlines(1000).code_ == status_code::timeout);
    check(window.vqec_vision_ai_core_subwn_get_outstanding() == 1);
    check(!lifetime.expired());
    gst_sample_unref(fixture.sample_);
    fixture.sample_ = nullptr;
    // No device output readers in this fixture. Wrong PTS cannot complete the ledger.
    check(job.vqec_vision_ai_qcom_frsub_complete_result(window, UINT64_MAX).code_ ==
          status_code::protocol_error);
    check(job.vqec_vision_ai_qcom_frsub_complete_result(window, ticket.pipeline_pts_ns_).code_ ==
          status_code::ok);
    check(job.vqec_vision_ai_qcom_frsub_complete_result(window, ticket.pipeline_pts_ns_).code_ ==
          status_code::invalid_state);
    // Synchronous cleanup of a standard synthetic pipeline, never a DMA cancellation recipe.
    gst_element_set_state(fixture.pipeline_, GST_STATE_NULL);
    check(owner.use_count() == 1);
    check(job.vqec_vision_ai_qcom_frsub_poll_input(window).code_ == status_code::ok);
    check(window.vqec_vision_ai_core_subwn_get_outstanding() == 0);
    check(!window.vqec_vision_ai_core_subwn_is_accepting());
    check(job.vqec_vision_ai_qcom_frsub_reset().code_ == status_code::ok);

    // Stopped appsrc returns FLUSHING: committed record must survive the rejected push.
    submission_window failed_window;
    check(failed_window.vqec_vision_ai_core_subwn_configure({43, 7, 0, 1000, 1}).code_ ==
          status_code::ok);
    check(vqec_vision_ai_qcom_frsub_push_frame(
        GST_APP_SRC(source), failed_window, descriptor, owner->fd_, owner, profile,
        1024, 0, job).code_ == status_code::io_error);
    check(job.vqec_vision_ai_qcom_frsub_has_submission());
    check(failed_window.vqec_vision_ai_core_subwn_get_outstanding() == 1);
    owner.reset();
    check(lifetime.expired());
    check(job.vqec_vision_ai_qcom_frsub_poll_input(failed_window).code_ == status_code::ok);
    check(failed_window.vqec_vision_ai_core_subwn_get_outstanding() == 1);
    // This source was stopped before push; no output was ever handed to a consumer.
    check(job.vqec_vision_ai_qcom_frsub_complete_result(failed_window, 0).code_ == status_code::ok);
    check(failed_window.vqec_vision_ai_core_subwn_get_outstanding() == 0);
    check(job.vqec_vision_ai_qcom_frsub_reset().code_ == status_code::ok);
    std::cout << "frame submission failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
