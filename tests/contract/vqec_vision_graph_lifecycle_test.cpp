#include <array>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>

#include <unistd.h>

#include "vqec_vision_plugin_graph.hpp"

namespace {

using namespace vqec::vision::ai;

void vqec_vision_ai_ctest_gltst_require(bool _condition) {
    if (!_condition) {
        throw std::runtime_error("graph lifecycle assertion failed");
    }
}

template <typename predicate>
void vqec_vision_ai_ctest_gltst_wait(predicate _predicate) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (!_predicate()) {
        vqec_vision_ai_ctest_gltst_require(std::chrono::steady_clock::now() < deadline);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

struct test_frame {
    int fd_{-1};
    ~test_frame() noexcept {
        if (fd_ >= 0) {
            ::close(fd_);
        }
    }
};

void vqec_vision_ai_ctest_gltst_start(plugin_graph& _graph) {
    inference_plan plan;
    plan.source_width_ = 32;
    plan.source_height_ = 8;
    plan.fps_numerator_ = 25;
    plan.fps_denominator_ = 1;
    plan.tensor_width_ = 8;
    plan.tensor_height_ = 8;
    plan.placement_ = image_placement::centre;
    plan.model_path_ = "/fixture/model.bin";
    plan.backend_path_ = "/fixture/backend.so";
    plan.system_path_ = "/fixture/system.so";
    plan.input_queue_bytes_ = 1024;
    plan.output_queue_buffers_ = 2;
    vqec_vision_ai_ctest_gltst_require(
        _graph.vqec_vision_ai_qcom_plgr_configure_fixture(plan).code_ == status_code::ok);
    const auto loading = _graph.vqec_vision_ai_qcom_plgr_load_model();
    vqec_vision_ai_ctest_gltst_require(
        loading.code_ == status_code::ok || loading.code_ == status_code::pending);
    vqec_vision_ai_ctest_gltst_wait([&]() {
        const auto state = _graph.vqec_vision_ai_qcom_plgr_poll_state();
        (void)state;
        return _graph.vqec_vision_ai_qcom_plgr_get_state() == plugin_graph_state::ready;
    });
    source_binding binding;
    binding.width_ = 32;
    binding.height_ = 8;
    binding.fps_numerator_ = 25;
    binding.fps_denominator_ = 1;
    binding.memory_kind_ = source_memory_kind::dmabuf;
    binding.layout_ = source_memory_layout::linear_nv12;
    binding.sync_mode_ = source_sync_mode::implicit_ready;
    binding.color_profile_ = source_color_profile::bt709_limited;
    binding.chroma_site_ = source_chroma_site::mpeg2;
    binding.fw_memory_contract_ = "fixture:fw";
    binding.backend_memory_contract_ = "fixture:backend";
    binding.preprocess_contract_ = "fixture:golden";
    vqec_vision_ai_ctest_gltst_require(
        _graph.vqec_vision_ai_qcom_plgr_start_stream({{"bytes", {256}}}, 1024).code_ ==
            status_code::invalid_state);
    vqec_vision_ai_ctest_gltst_require(
        _graph.vqec_vision_ai_qcom_plgr_bind_source(binding).code_ == status_code::ok);
    const auto started = _graph.vqec_vision_ai_qcom_plgr_start_stream({{"bytes", {256}}}, 1024);
    vqec_vision_ai_ctest_gltst_require(
        started.code_ == status_code::ok || started.code_ == status_code::pending);
    vqec_vision_ai_ctest_gltst_wait([&]() {
        const auto state = _graph.vqec_vision_ai_qcom_plgr_poll_state();
        (void)state;
        return _graph.vqec_vision_ai_qcom_plgr_get_state() == plugin_graph_state::playing;
    });
}

void vqec_vision_ai_ctest_gltst_unload(plugin_graph& _graph) {
    const auto unloaded = _graph.vqec_vision_ai_qcom_plgr_unload_model();
    vqec_vision_ai_ctest_gltst_require(
        unloaded.code_ == status_code::ok || unloaded.code_ == status_code::pending);
    vqec_vision_ai_ctest_gltst_wait([&]() {
        const auto state = _graph.vqec_vision_ai_qcom_plgr_poll_state();
        (void)state;
        return _graph.vqec_vision_ai_qcom_plgr_get_state() == plugin_graph_state::configured;
    });
}

void vqec_vision_ai_ctest_gltst_drain(plugin_graph& _graph) {
    const auto drain = _graph.vqec_vision_ai_qcom_plgr_request_drain();
    vqec_vision_ai_ctest_gltst_require(
        drain.code_ == status_code::ok || drain.code_ == status_code::pending);
    vqec_vision_ai_ctest_gltst_wait([&]() {
        const auto state = _graph.vqec_vision_ai_qcom_plgr_poll_state();
        (void)state;
        return _graph.vqec_vision_ai_qcom_plgr_get_state() == plugin_graph_state::drained;
    });
    vqec_vision_ai_ctest_gltst_unload(_graph);
}

void vqec_vision_ai_ctest_gltst_check_lifecycle() {
    auto domain = std::make_shared<graph_retention>();
    auto owner = std::make_shared<test_frame>();
    char path[] = "/tmp/vqec_ai_graph_XXXXXX";
    owner->fd_ = ::mkstemp(path);
    vqec_vision_ai_ctest_gltst_require(owner->fd_ >= 0);
    const auto unlinked = ::unlink(path);
    vqec_vision_ai_ctest_gltst_require(unlinked == 0 && ::ftruncate(owner->fd_, 4096) == 0);
    frame_descriptor frame;
    frame.width_ = 32;
    frame.height_ = 8;
    frame.offsets_ = {16, 576};
    frame.strides_ = {64, 64};
    frame.view_size_bytes_ = 1024;
    frame.memory_offset_bytes_ = 128;
    frame.allocation_size_bytes_ = 4096;
    frame.session_epoch_ = 7;
    frame.pts_ns_ = 100;
    plugin_graph healthy;
    vqec_vision_ai_ctest_gltst_start(healthy);
    vqec_vision_ai_ctest_gltst_require(
        healthy.vqec_vision_ai_qcom_plgr_arm_submission(1, 7, 1000, domain).code_ ==
            status_code::ok);
    submission_ticket ticket;
    vqec_vision_ai_ctest_gltst_require(healthy.vqec_vision_ai_qcom_plgr_submit_frame(
        frame, owner->fd_, owner, 0, ticket).code_ == status_code::ok);
    vqec_vision_ai_ctest_gltst_require(
        healthy.vqec_vision_ai_qcom_plgr_unload_model().code_ == status_code::pending);
    tensor_result result;
    vqec_vision_ai_ctest_gltst_wait([&]() {
        const auto output = healthy.vqec_vision_ai_qcom_plgr_poll_result(0, result);
        vqec_vision_ai_ctest_gltst_require(
            output.code_ == status_code::ok || output.code_ == status_code::pending);
        return output.code_ == status_code::ok;
    });
    vqec_vision_ai_ctest_gltst_require(result.tensors_.size() == 1);
    vqec_vision_ai_ctest_gltst_wait([&]() {
        const auto jobs = healthy.vqec_vision_ai_qcom_plgr_poll_jobs(0);
        return jobs.code_ == status_code::ok;
    });
    vqec_vision_ai_ctest_gltst_drain(healthy);

    // Timeout plus wrapper destruction must preserve the pipeline and unconsumed sample.
    {
        plugin_graph fault;
        vqec_vision_ai_ctest_gltst_start(fault);
        vqec_vision_ai_ctest_gltst_require(
            fault.vqec_vision_ai_qcom_plgr_arm_submission(2, 7, 1000, domain).code_ ==
                status_code::ok);
        ticket = {};
        vqec_vision_ai_ctest_gltst_require(fault.vqec_vision_ai_qcom_plgr_submit_frame(
            frame, owner->fd_, owner, 0, ticket).code_ == status_code::ok);
        vqec_vision_ai_ctest_gltst_require(
            fault.vqec_vision_ai_qcom_plgr_poll_jobs(1000).code_ == status_code::timeout);
        vqec_vision_ai_ctest_gltst_require(
            fault.vqec_vision_ai_qcom_plgr_unload_model().code_ == status_code::pending);
    }
    vqec_vision_ai_ctest_gltst_require(domain->vqec_vision_ai_qcom_plgr_get_retained_count() == 1);
    plugin_graph restored;
    vqec_vision_ai_ctest_gltst_require(
        domain->vqec_vision_ai_qcom_plgr_restore_graph(0, restored, domain).code_ ==
            status_code::ok);
    result.pipeline_pts_ns_ = 777;
    vqec_vision_ai_ctest_gltst_wait([&]() {
        const auto output = restored.vqec_vision_ai_qcom_plgr_poll_result(1000, result);
        vqec_vision_ai_ctest_gltst_require(output.code_ != status_code::ok);
        return restored.vqec_vision_ai_qcom_plgr_get_outstanding() == 0;
    });
    vqec_vision_ai_ctest_gltst_require(result.pipeline_pts_ns_ == 777);
    vqec_vision_ai_ctest_gltst_wait([&]() {
        const auto jobs = restored.vqec_vision_ai_qcom_plgr_poll_jobs(1000);
        (void)jobs;
        return restored.vqec_vision_ai_qcom_plgr_get_outstanding() == 0;
    });
    vqec_vision_ai_ctest_gltst_unload(restored);

    // Four retained graphs prevent a fifth arm. No camera job needed to test the quota.
    for (unsigned index = 0; index < 4; ++index) {
        plugin_graph retained;
        vqec_vision_ai_ctest_gltst_start(retained);
        vqec_vision_ai_ctest_gltst_require(retained.vqec_vision_ai_qcom_plgr_arm_submission(
            10 + index, 7, 1000, domain).code_ == status_code::ok);
    }
    vqec_vision_ai_ctest_gltst_require(domain->vqec_vision_ai_qcom_plgr_get_retained_count() == 4);
    plugin_graph rejected;
    vqec_vision_ai_ctest_gltst_start(rejected);
    vqec_vision_ai_ctest_gltst_require(rejected.vqec_vision_ai_qcom_plgr_arm_submission(
        20, 7, 1000, domain).code_ == status_code::resource_exhausted);
    vqec_vision_ai_ctest_gltst_drain(rejected);
    for (unsigned index = 0; index < 4; ++index) {
        plugin_graph recovered;
        vqec_vision_ai_ctest_gltst_require(domain->vqec_vision_ai_qcom_plgr_restore_graph(
            index, recovered, domain).code_ == status_code::ok);
        vqec_vision_ai_ctest_gltst_drain(recovered);
    }
    vqec_vision_ai_ctest_gltst_require(domain->vqec_vision_ai_qcom_plgr_get_retained_count() == 0);
}

}  // namespace

int main() {
    try {
        vqec_vision_ai_ctest_gltst_check_lifecycle();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "synthetic graph lifecycle checks passed; no Qualcomm execution\n";
    return 0;
}
