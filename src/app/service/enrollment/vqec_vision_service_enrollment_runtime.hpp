#ifndef VQEC_VISION_AI_APPL_SERVICE_ENROLLMENT_RUNTIME_HPP
#define VQEC_VISION_AI_APPL_SERVICE_ENROLLMENT_RUNTIME_HPP

#include <cstdint>
#include <memory>

#include "vqec/vision/ai/contracts/base/vqec_vision_status.hpp"
#include "vqec/vision/ai/contracts/inference/vqec_vision_model_catalog.hpp"
#include "vqec/vision/ai/contracts/lifecycle/vqec_vision_deployment_config.hpp"
#include "vqec/vision/ai/ports/perception/vqec_vision_face_enrollment.hpp"

namespace vqec::vision::ai {

struct parsed_arguments;
class production_platform;
class recognition_session;

// Owns the optional file-enrollment control plane and its dedicated detector/embedding
// graph lifecycles. Construction and D-Bus publication never start Qualcomm graphs.
class service_enrollment_runtime final {
public:
    service_enrollment_runtime();
    ~service_enrollment_runtime();
    service_enrollment_runtime(const service_enrollment_runtime& _other) = delete;
    service_enrollment_runtime& operator=(
        const service_enrollment_runtime& _other) = delete;

    [[nodiscard]] status vqec_vision_ai_appl_svenr_configure(
        const parsed_arguments& _arguments, recognition_session& _recognition,
        production_platform& _platform, std::uint16_t _source_slot,
        const source_deployment_config& _source, const model_catalog& _catalog,
        const model_catalog_entry& _embedding_model, std::uint64_t _cycle_id);
    [[nodiscard]] status vqec_vision_ai_appl_svenr_poll(std::uint64_t _steady_now_ns);
    [[nodiscard]] status vqec_vision_ai_appl_svenr_stop();
    [[nodiscard]] face_enrollment_port*
    vqec_vision_ai_appl_svenr_get_port() noexcept;

private:
    struct implementation;
    std::unique_ptr<implementation> implementation_;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_APPL_SERVICE_ENROLLMENT_RUNTIME_HPP
