# Scheduler and execution-worker targets. Target names are unchanged from the former root
# definitions; only the build definition moved next to the module.

add_library(vqec_vision_ai_scheduler STATIC
    vqec_vision_model_cadence.cpp)
target_include_directories(vqec_vision_ai_scheduler PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_link_libraries(vqec_vision_ai_scheduler PUBLIC vqec_vision_ai_core)
set_target_properties(vqec_vision_ai_scheduler PROPERTIES CXX_EXTENSIONS OFF)

add_library(vqec_vision_ai_inference_worker STATIC
    vqec_vision_inference_worker.cpp)
target_include_directories(vqec_vision_ai_inference_worker PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_link_libraries(vqec_vision_ai_inference_worker
    PUBLIC vqec_vision_ai_core vqec_vision_ai_scheduler Threads::Threads)
set_target_properties(vqec_vision_ai_inference_worker PROPERTIES CXX_EXTENSIONS OFF)
