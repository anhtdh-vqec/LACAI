#ifndef VQEC_VISION_AI_APPL_SERVICE_ACTIVATION_RECONCILER_HPP
#define VQEC_VISION_AI_APPL_SERVICE_ACTIVATION_RECONCILER_HPP

#include <cstdint>
#include <memory>
#include <string>

#include "vqec_vision_service_feature_activation.hpp"

namespace vqec::vision::ai {

// Serialized cold-path controller for one live generation. It borrows every generation owner and
// replaces only the feature owner unique_ptr after all candidate validation has completed.
class service_activation_reconciler final {
public:
    [[nodiscard]] status vqec_vision_ai_appl_svacr_configure(
        service_startup_resolution& _startup, const parsed_arguments& _arguments,
        runtime_composition_bundle& _bundle,
        std::unique_ptr<service_feature_activation>& _feature_owner,
        const feature_processor_registry& _feature_registry,
        const std::string& _fallback_attribute_schema,
        output_gate& _output_gate);

    [[nodiscard]] status vqec_vision_ai_appl_svacr_apply_snapshot(
        const runtime_control_snapshot& _runtime, std::uint64_t _steady_now_ns);

    [[nodiscard]] std::uint64_t
    vqec_vision_ai_appl_svacr_get_applied_revision() const noexcept;

private:
    [[nodiscard]] status vqec_vision_ai_appl_svacr_complete_pending();

    service_startup_resolution* startup_{nullptr};
    const parsed_arguments* arguments_{nullptr};
    runtime_composition_bundle* bundle_{nullptr};
    std::unique_ptr<service_feature_activation>* feature_owner_{nullptr};
    const feature_processor_registry* feature_registry_{nullptr};
    const std::string* fallback_attribute_schema_{nullptr};
    output_gate* output_gate_{nullptr};
    std::unique_ptr<service_startup_resolution> pending_startup_;
};

}  // namespace vqec::vision::ai

#endif  // VQEC_VISION_AI_APPL_SERVICE_ACTIVATION_RECONCILER_HPP
