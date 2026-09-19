#ifndef VQEC_VISION_AI_APPL_SERVICE_RUNTIME_HPP
#define VQEC_VISION_AI_APPL_SERVICE_RUNTIME_HPP

// Runs the complete service control lifecycle. The external entrypoint is deliberately
// separate so process setup remains small and the runtime controller can be characterized.
[[nodiscard]] int vqec_vision_ai_appl_svcmn_run_service(int _argc, char** _argv);

#endif  // VQEC_VISION_AI_APPL_SERVICE_RUNTIME_HPP
