# Neutral core target. Target name is unchanged from the former root definition; only the
# build definition moved next to the module. Requires find_package(Threads) and the
# top-level include directory, both set up before add_subdirectory(src/core).

add_library(vqec_vision_ai_core STATIC
    memory/vqec_vision_tensor_pool.cpp
    inference/vqec_vision_inference_plan.cpp
    inference/vqec_vision_submission_window.cpp
    inference/vqec_vision_source_binding.cpp
    inference/vqec_vision_tensor_contract.cpp
    inference/vqec_vision_inference_execution.cpp
    inference/vqec_vision_preprocess_spec.cpp
    inference/vqec_vision_model_io_manifest.cpp
    inference/vqec_vision_model_package.cpp
    inference/vqec_vision_model_package_registry.cpp
    media/vqec_vision_preview_contract.cpp
    media/vqec_vision_preview_surface.cpp
    media/vqec_vision_encoder_window.cpp
    media/vqec_vision_preview_pool.cpp
    media/vqec_vision_encoded_output.cpp
    media/vqec_vision_encoder_contract.cpp
    media/vqec_vision_image_alignment.cpp
    media/vqec_vision_color.cpp
    perception/vqec_vision_observation.cpp
    perception/vqec_vision_embedding.cpp
    perception/vqec_vision_face_gallery.cpp
    features/vqec_vision_usecase_activation.cpp
    features/vqec_vision_feature_event.cpp
    features/vqec_vision_feature_catalog.cpp
    output/vqec_vision_output_gate.cpp
    output/vqec_vision_output_generation.cpp
    output/vqec_vision_metadata_query.cpp
    output/vqec_vision_spatiotemporal_metadata.cpp
    output/vqec_vision_trajectory_codec.cpp
    output/vqec_vision_evidence_transport.cpp
    configuration/vqec_vision_deployment_config.cpp
    configuration/vqec_vision_app_lifecycle.cpp
    configuration/vqec_vision_model_catalog.cpp)
target_compile_features(vqec_vision_ai_core PUBLIC cxx_std_17)
target_include_directories(vqec_vision_ai_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>)
target_include_directories(vqec_vision_ai_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/memory>)
target_link_libraries(vqec_vision_ai_core PUBLIC Threads::Threads)
set_target_properties(vqec_vision_ai_core PROPERTIES CXX_EXTENSIONS OFF)
