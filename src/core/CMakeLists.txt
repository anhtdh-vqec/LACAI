# Neutral core target. Target name is unchanged from the former root definition; only the
# build definition moved next to the module. Requires find_package(Threads) and the
# top-level include directory, both set up before add_subdirectory(src/core).

add_library(vqec_vision_ai_core STATIC
    vqec_vision_tensor_pool.cpp
    vqec_vision_inference_plan.cpp vqec_vision_submission_window.cpp vqec_vision_source_binding.cpp
    vqec_vision_tensor_contract.cpp vqec_vision_output_gate.cpp
    vqec_vision_preview_contract.cpp vqec_vision_preview_surface.cpp
    vqec_vision_encoder_window.cpp vqec_vision_preview_pool.cpp
    vqec_vision_encoded_output.cpp vqec_vision_output_generation.cpp
    vqec_vision_encoder_contract.cpp vqec_vision_observation.cpp
    vqec_vision_deployment_config.cpp vqec_vision_model_catalog.cpp
    vqec_vision_feature_event.cpp vqec_vision_feature_catalog.cpp
    vqec_vision_inference_execution.cpp
    vqec_vision_secondary_inference.cpp)
target_compile_features(vqec_vision_ai_core PUBLIC cxx_std_17)
target_include_directories(vqec_vision_ai_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>)
target_include_directories(vqec_vision_ai_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_link_libraries(vqec_vision_ai_core PUBLIC Threads::Threads)
set_target_properties(vqec_vision_ai_core PROPERTIES CXX_EXTENSIONS OFF)
