#include <cstdint>
#include <stdexcept>

#include "vqec_vision_cascade_graph_session.hpp"
#include "vqec_vision_reference_graph.hpp"

using namespace vqec::vision::ai;

namespace {

constexpr std::uint64_t g_startup_timeout_ns = 1000;
constexpr std::uint64_t g_stop_timeout_ns = 1000;

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

}  // namespace

int main() {
    vqec_vision_ai_unit_cgsts_check_lifecycle();
    vqec_vision_ai_unit_cgsts_check_guards();
    return 0;
}
