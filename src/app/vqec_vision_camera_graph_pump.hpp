#ifndef VQEC_VISION_AI_APP_CAMERA_GRAPH_PUMP_HPP
#define VQEC_VISION_AI_APP_CAMERA_GRAPH_PUMP_HPP

#include <cstdint>
#include <memory>

#include "vqec_vision_plugin_graph.hpp"
#include "vqec/vision/ai/ports/vqec_vision_raw_source.hpp"

namespace vqec::vision::ai {

struct camera_pump_report {
    bool has_result_{false};
    bool has_submission_{false};
    submission_ticket ticket_;
};

// Composition-only borrowed source/graph, exclusive serialized executor, one acquisition cycle.
// Owners must outlive this pump and drain explicitly. No destructor RPC, worker or frame queue.
class camera_graph_pump {
public:
    camera_graph_pump(raw_source_port& _source, plugin_graph& _graph,
        std::shared_ptr<graph_retention> _retention, std::uint64_t _cycle_id,
        std::uint64_t _job_timeout_ns);
    camera_graph_pump(const camera_graph_pump& _other) = delete;
    camera_graph_pump& operator=(const camera_graph_pump& _other) = delete;
    [[nodiscard]] status vqec_vision_ai_appl_cgpmp_pump_step(
        std::uint64_t _steady_now_ns, tensor_result& _result, camera_pump_report& _report);
    void vqec_vision_ai_appl_cgpmp_begin_stop() noexcept;

private:
    raw_source_port& source_;
    plugin_graph& graph_;
    std::shared_ptr<graph_retention> retention_;
    std::uint64_t cycle_id_{0};
    std::uint64_t job_timeout_ns_{0};
    bool is_armed_{false};
    bool is_stopping_{false};
    bool is_failed_{false};
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_APP_CAMERA_GRAPH_PUMP_HPP
