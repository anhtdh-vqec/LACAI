#include <chrono>
#include <cstdint>
#include <stdexcept>

#include "vqec_vision_cascade_graph_session.hpp"
#include "vqec_vision_application_composition.hpp"
#include "vqec_vision_cascade_coordinator.hpp"
#include "vqec_vision_reference_graph.hpp"
#include "vqec_vision_runtime_executor.hpp"
#include "vqec_vision_service_cascade_runtime.hpp"

using namespace vqec::vision::ai;

namespace {

constexpr std::uint64_t g_startup_timeout_ns = 1000;
constexpr std::uint64_t g_stop_timeout_ns = 1000;
constexpr std::uint64_t g_gated_startup_timeout_ns = 1000000000ULL;

class vqec_vision_ai_unit_cgsts_lease final : public cascade_frame_lease_port {
public:
    [[nodiscard]] status vqec_vision_ai_ports_cflse_acquire(
        const preview_frame_key&, raw_frame&, std::uint64_t&) override {
        return {status_code::invalid_state, "activation fixture does not acquire frames"};
    }
    [[nodiscard]] status vqec_vision_ai_ports_cflse_retire(
        const preview_frame_key&) override {
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_cflse_complete(std::uint64_t) override {
        return {};
    }
};

class vqec_vision_ai_unit_cgsts_aligner final : public image_alignment_port {
public:
    [[nodiscard]] status vqec_vision_ai_ports_imaln_probe_capabilities(
        alignment_capabilities& _capabilities) const override {
        _capabilities.supports_similarity_ = true;
        _capabilities.max_points_ = image_alignment_limits::g_max_points;
        _capabilities.max_destination_dimension_ = 112;
        return {};
    }
    [[nodiscard]] status vqec_vision_ai_ports_imaln_validate_template(
        const alignment_template& _template,
        const alignment_capabilities& _capabilities) const override {
        return vqec_vision_ai_core_imaln_require_capability(
            _capabilities, _template);
    }
    [[nodiscard]] status vqec_vision_ai_ports_imaln_align(
        const alignment_request&, const raw_frame&, const alignment_template&,
        alignment_result&, std::uint64_t&) override {
        return {status_code::invalid_state, "activation fixture does not align frames"};
    }
    [[nodiscard]] status vqec_vision_ai_ports_imaln_poll_completion(
        std::uint64_t, bool& _is_complete) override {
        _is_complete = false;
        return {};
    }
};

cascade_graph_session_config vqec_vision_ai_unit_cgsts_make_config(
    inference_graph_port& _graph) {
    cascade_graph_session_config config;
    config.graph_ = &_graph;
    config.plan_.source_width_ = 1920;
    config.plan_.source_height_ = 1080;
    config.plan_.fps_numerator_ = 25;
    config.plan_.fps_denominator_ = 1;
    config.plan_.tensor_width_ = 112;
    config.plan_.tensor_height_ = 112;
    config.plan_.placement_ = image_placement::stretch;
    config.plan_.model_path_ = "/fixture/embedding.bin";
    config.plan_.backend_path_ = "/fixture/backend.so";
    config.plan_.system_path_ = "/fixture/system.so";
    config.plan_.input_queue_bytes_ = 1920U * 1080U * 3U / 2U;
    config.plan_.output_queue_buffers_ = 1;
    config.binding_.width_ = config.plan_.source_width_;
    config.binding_.height_ = config.plan_.source_height_;
    config.binding_.fps_numerator_ = config.plan_.fps_numerator_;
    config.binding_.fps_denominator_ = config.plan_.fps_denominator_;
    config.binding_.memory_kind_ = source_memory_kind::dmabuf;
    config.binding_.layout_ = source_memory_layout::linear_nv12;
    config.binding_.sync_mode_ = source_sync_mode::implicit_ready;
    config.binding_.color_profile_ = source_color_profile::bt709_limited;
    config.binding_.chroma_site_ = source_chroma_site::mpeg2;
    config.binding_.fw_memory_contract_ = "fixture:fw";
    config.binding_.backend_memory_contract_ = "fixture:backend";
    config.binding_.preprocess_contract_ = "fixture:edgeface";
    config.outputs_ = {
        {"embedding", {1, 512}, tensor_element_type::float32, {}}};
    config.max_output_bytes_ = 512U * sizeof(float);
    config.startup_timeout_ns_ = g_startup_timeout_ns;
    config.stop_timeout_ns_ = g_stop_timeout_ns;
    return config;
}

void vqec_vision_ai_unit_cgsts_require(bool _condition, const char* _message) {
    if (!_condition) {
        throw std::runtime_error(_message);
    }
}

void vqec_vision_ai_unit_cgsts_check_lifecycle() {
    reference_inference_graph graph;
    cascade_graph_session session(vqec_vision_ai_unit_cgsts_make_config(graph));
    for (std::uint64_t now_ns = 0; now_ns < 5; ++now_ns) {
        const auto stepped = session.vqec_vision_ai_appl_cgses_step(now_ns);
        vqec_vision_ai_unit_cgsts_require(
            stepped.code_ == status_code::ok || stepped.code_ == status_code::pending,
            "cascade graph startup failed");
    }
    vqec_vision_ai_unit_cgsts_require(
        session.vqec_vision_ai_appl_cgses_get_state() ==
            cascade_graph_session_state::running &&
        graph.vqec_vision_ai_ports_infgr_get_state() == inference_graph_state::running,
        "cascade graph did not reach running");

    vqec_vision_ai_unit_cgsts_require(
        session.vqec_vision_ai_appl_cgses_request_stop(5).code_ == status_code::ok,
        "cascade graph stop request failed");
    for (std::uint64_t now_ns = 6; now_ns < 9; ++now_ns) {
        const auto stepped = session.vqec_vision_ai_appl_cgses_step(now_ns);
        vqec_vision_ai_unit_cgsts_require(
            stepped.code_ == status_code::ok || stepped.code_ == status_code::pending,
            "cascade graph stop failed");
    }
    vqec_vision_ai_unit_cgsts_require(
        session.vqec_vision_ai_appl_cgses_get_state() ==
            cascade_graph_session_state::stopped &&
        graph.vqec_vision_ai_ports_infgr_get_state() == inference_graph_state::configured,
        "cascade graph did not drain and unload");

    vqec_vision_ai_unit_cgsts_require(
        session.vqec_vision_ai_appl_cgses_request_active(true, 9).code_ ==
            status_code::pending,
        "cascade graph restart request failed");
    for (std::uint64_t now_ns = 10; now_ns < 14; ++now_ns) {
        const auto stepped = session.vqec_vision_ai_appl_cgses_step(now_ns);
        vqec_vision_ai_unit_cgsts_require(
            stepped.code_ == status_code::ok || stepped.code_ == status_code::pending,
            "cascade graph restart failed");
    }
    vqec_vision_ai_unit_cgsts_require(
        session.vqec_vision_ai_appl_cgses_get_state() ==
            cascade_graph_session_state::running &&
        graph.vqec_vision_ai_ports_infgr_get_state() == inference_graph_state::running,
        "cascade graph did not restart in place");
}

void vqec_vision_ai_unit_cgsts_check_guards() {
    reference_inference_graph graph;
    auto invalid_config = vqec_vision_ai_unit_cgsts_make_config(graph);
    invalid_config.stop_timeout_ns_ = 0;
    cascade_graph_session invalid(std::move(invalid_config));
    vqec_vision_ai_unit_cgsts_require(
        invalid.vqec_vision_ai_appl_cgses_step(0).code_ == status_code::invalid_argument &&
            !invalid.vqec_vision_ai_appl_cgses_is_recovery_required(),
        "invalid activation did not fault closed");

    reference_inference_graph timeout_graph;
    cascade_graph_session timeout(
        vqec_vision_ai_unit_cgsts_make_config(timeout_graph));
    vqec_vision_ai_unit_cgsts_require(
        timeout.vqec_vision_ai_appl_cgses_step(10).code_ == status_code::pending,
        "timeout fixture did not begin activation");
    vqec_vision_ai_unit_cgsts_require(
        timeout.vqec_vision_ai_appl_cgses_step(10 + g_startup_timeout_ns).code_ ==
                status_code::timeout &&
            !timeout.vqec_vision_ai_appl_cgses_is_recovery_required(),
        "startup deadline did not fault the graph session");

    reference_inference_graph monotonic_graph;
    cascade_graph_session monotonic(
        vqec_vision_ai_unit_cgsts_make_config(monotonic_graph));
    (void)monotonic.vqec_vision_ai_appl_cgses_step(10);
    vqec_vision_ai_unit_cgsts_require(
        monotonic.vqec_vision_ai_appl_cgses_step(9).code_ ==
            status_code::invalid_argument,
        "decreasing steady time was accepted");
}

void vqec_vision_ai_unit_cgsts_check_source_gate() {
    reference_inference_graph graph;
    std::array<service_cascade_owner, deployment_limits::g_max_sources> owners;
    auto gated_config = vqec_vision_ai_unit_cgsts_make_config(graph);
    gated_config.startup_timeout_ns_ = g_gated_startup_timeout_ns;
    gated_config.stop_timeout_ns_ = g_gated_startup_timeout_ns;
    owners[0].graph_session_ = std::make_unique<cascade_graph_session>(
        std::move(gated_config));
    std::array<bool, deployment_limits::g_max_sources> source_ready{};

    vqec_vision_ai_unit_cgsts_require(
        vqec_vision_ai_appl_svcsc_start_ready_graphs(owners, source_ready).code_ ==
                status_code::ok &&
            owners[0].graph_session_->vqec_vision_ai_appl_cgses_get_state() ==
                cascade_graph_session_state::idle &&
            graph.vqec_vision_ai_ports_infgr_get_state() == inference_graph_state::empty,
        "cascade graph crossed a closed first-frame gate");

    source_ready[0] = true;
    vqec_vision_ai_unit_cgsts_require(
        vqec_vision_ai_appl_svcsc_start_ready_graphs(owners, source_ready).code_ ==
            status_code::ok,
        "ready cascade graph startup failed");
    vqec_vision_ai_unit_cgsts_require(
        owners[0].graph_session_->vqec_vision_ai_appl_cgses_get_state() ==
            cascade_graph_session_state::running,
        "cascade session did not run after the first-frame gate opened");
    vqec_vision_ai_unit_cgsts_require(
        graph.vqec_vision_ai_ports_infgr_get_state() == inference_graph_state::running,
        "cascade backend did not run after the first-frame gate opened");
    vqec_vision_ai_unit_cgsts_require(
        vqec_vision_ai_appl_svcsc_stop_graphs(owners).code_ == status_code::ok,
        "gated cascade graph did not stop");
}

void vqec_vision_ai_unit_cgsts_check_reference_count_lifecycle() {
    reference_inference_graph graph;
    model_catalog_entry model;
    model.model_id_ = "embedding";
    model.model_version_ = "1.0";
    model.target_id_ = "qcs6490";
    std::array<service_cascade_owner, deployment_limits::g_max_sources> owners;
    auto owner_config = vqec_vision_ai_unit_cgsts_make_config(graph);
    owner_config.startup_timeout_ns_ = g_gated_startup_timeout_ns;
    owner_config.stop_timeout_ns_ = g_gated_startup_timeout_ns;
    owners[0].graph_session_ = std::make_unique<cascade_graph_session>(
        std::move(owner_config));
    owners[0].model_ = &model;
    owners[0].root_model_slot_ = 0;

    application_composition composition(1, 1, 1);
    std::array<multi_model_feature_pipeline*, deployment_limits::g_max_sources>
        pipelines{};
    std::array<std::uint16_t, deployment_limits::g_max_sources> root_slots{};
    root_slots.fill(g_invalid_model_slot);
    root_slots[0] = 0;
    std::array<std::uint32_t, deployment_limits::g_max_sources> camera_ids{};
    std::array<std::uint32_t, deployment_limits::g_max_sources> channel_ids{};
    runtime_executor executor(
        composition, pipelines, root_slots, camera_ids, channel_ids, 1);
    vqec_vision_ai_unit_cgsts_lease lease;
    vqec_vision_ai_unit_cgsts_aligner aligner;
    cascade_coordinator coordinator;
    cascade_coordinator_config coordinator_config;
    coordinator_config.aligner_ = &aligner;
    coordinator_config.lease_ = &lease;
    coordinator_config.template_.schema_id_ = "five_point_landmarks";
    coordinator_config.template_.schema_version_ = "1.0";
    coordinator_config.template_.destination_width_ = 112;
    coordinator_config.template_.destination_height_ = 112;
    coordinator_config.template_.reference_points_ = {
        {38.0F, 52.0F}, {74.0F, 52.0F}, {56.0F, 72.0F},
        {42.0F, 92.0F}, {71.0F, 92.0F}};
    coordinator_config.max_tasks_per_frame_ = 1;
    vqec_vision_ai_unit_cgsts_require(
        coordinator.vqec_vision_ai_appl_cscrd_configure(
            coordinator_config).code_ == status_code::ok &&
        executor.vqec_vision_ai_appl_rtexe_bind_cascade(
            0, 0, coordinator, 1).code_ == status_code::ok,
        "cascade activation fixture binding failed");

    app_activation_plan plan;
    plan.source_count_ = 1;
    cascade_dependency_reference dependency;
    dependency.source_id_ = "camera_front";
    dependency.model_id_ = model.model_id_;
    dependency.model_version_ = model.model_version_;
    dependency.target_id_ = model.target_id_;
    dependency.source_slot_ = 0;
    dependency.root_model_slot_ = 0;
    dependency.consumer_count_ = 2;
    dependency.consumer_app_ids_ = {"app_a", "app_b"};
    plan.cascade_dependencies_.push_back(dependency);
    vqec_vision_ai_unit_cgsts_require(
        vqec_vision_ai_appl_svcsc_initialize_activation(
            owners, plan, executor, 0).code_ == status_code::ok,
        "cascade reference initialization failed");
    std::array<bool, deployment_limits::g_max_sources> source_ready{};
    source_ready[0] = true;
    vqec_vision_ai_unit_cgsts_require(
        vqec_vision_ai_appl_svcsc_start_ready_graphs(
            owners, source_ready).code_ == status_code::ok &&
        vqec_vision_ai_appl_svcsc_sync_executor(
            owners, executor).code_ == status_code::ok &&
        vqec_vision_ai_appl_svcsc_is_activation_complete(owners),
        "shared cascade dependency did not activate");

    const auto current_time = []() noexcept {
        return static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());
    };
    plan.cascade_dependencies_[0].consumer_count_ = 1;
    plan.cascade_dependencies_[0].consumer_app_ids_ = {"app_b"};
    vqec_vision_ai_unit_cgsts_require(
        vqec_vision_ai_appl_svcsc_validate_activation(
            owners, plan, current_time()).code_ == status_code::ok &&
        vqec_vision_ai_appl_svcsc_request_activation(
            owners, plan, executor, current_time()).code_ == status_code::ok &&
        owners[0].graph_session_->vqec_vision_ai_appl_cgses_get_state() ==
            cascade_graph_session_state::running,
        "two-to-one cascade transition changed graph lifecycle");

    plan.cascade_dependencies_[0].consumer_count_ = 0;
    plan.cascade_dependencies_[0].consumer_app_ids_.clear();
    vqec_vision_ai_unit_cgsts_require(
        vqec_vision_ai_appl_svcsc_validate_activation(
            owners, plan, current_time()).code_ == status_code::ok &&
        vqec_vision_ai_appl_svcsc_request_activation(
            owners, plan, executor, current_time()).code_ == status_code::ok &&
        vqec_vision_ai_appl_svcsc_start_ready_graphs(
            owners, source_ready).code_ == status_code::ok &&
        vqec_vision_ai_appl_svcsc_sync_executor(
            owners, executor).code_ == status_code::ok &&
        owners[0].graph_session_->vqec_vision_ai_appl_cgses_get_state() ==
            cascade_graph_session_state::stopped &&
        graph.vqec_vision_ai_ports_infgr_get_state() ==
            inference_graph_state::configured,
        "last cascade consumer did not drain and unload the graph");

    plan.cascade_dependencies_[0].consumer_count_ = 1;
    plan.cascade_dependencies_[0].consumer_app_ids_ = {"app_a"};
    vqec_vision_ai_unit_cgsts_require(
        vqec_vision_ai_appl_svcsc_validate_activation(
            owners, plan, current_time()).code_ == status_code::ok &&
        vqec_vision_ai_appl_svcsc_request_activation(
            owners, plan, executor, current_time()).code_ == status_code::ok &&
        vqec_vision_ai_appl_svcsc_start_ready_graphs(
            owners, source_ready).code_ == status_code::ok &&
        vqec_vision_ai_appl_svcsc_sync_executor(
            owners, executor).code_ == status_code::ok &&
        vqec_vision_ai_appl_svcsc_is_activation_complete(owners),
        "zero-to-one cascade transition did not restart the prepared graph");
    vqec_vision_ai_unit_cgsts_require(
        vqec_vision_ai_appl_svcsc_stop_graphs(owners).code_ == status_code::ok,
        "cascade reference-count fixture did not stop");
}

}  // namespace

int main() {
    vqec_vision_ai_unit_cgsts_check_lifecycle();
    vqec_vision_ai_unit_cgsts_check_guards();
    vqec_vision_ai_unit_cgsts_check_source_gate();
    vqec_vision_ai_unit_cgsts_check_reference_count_lifecycle();
    return 0;
}
