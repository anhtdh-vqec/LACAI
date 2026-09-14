# Application orchestration targets and the service executable. Target names are
# unchanged from the former root definitions.

add_library(vqec_vision_ai_camera_graph_pump STATIC
    vqec_vision_camera_graph_pump.cpp vqec_vision_camera_session.cpp)
target_include_directories(vqec_vision_ai_camera_graph_pump PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_link_libraries(vqec_vision_ai_camera_graph_pump PUBLIC vqec_vision_ai_core)
set_target_properties(vqec_vision_ai_camera_graph_pump PROPERTIES CXX_EXTENSIONS OFF)

add_library(vqec_vision_ai_source_supervisor STATIC
    vqec_vision_multi_source_supervisor.cpp)
target_include_directories(vqec_vision_ai_source_supervisor PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_link_libraries(vqec_vision_ai_source_supervisor
    PUBLIC vqec_vision_ai_core vqec_vision_ai_source_session_worker)
set_target_properties(vqec_vision_ai_source_supervisor PROPERTIES CXX_EXTENSIONS OFF)

add_library(vqec_vision_ai_source_session_worker STATIC
    vqec_vision_source_session_worker.cpp)
target_include_directories(vqec_vision_ai_source_session_worker PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_link_libraries(vqec_vision_ai_source_session_worker
    PUBLIC vqec_vision_ai_core Threads::Threads)
set_target_properties(vqec_vision_ai_source_session_worker PROPERTIES CXX_EXTENSIONS OFF)

add_library(vqec_vision_ai_multi_model_pump STATIC
    vqec_vision_multi_model_pump.cpp)
target_include_directories(vqec_vision_ai_multi_model_pump PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_link_libraries(vqec_vision_ai_multi_model_pump
    PUBLIC vqec_vision_ai_core vqec_vision_ai_scheduler)
set_target_properties(vqec_vision_ai_multi_model_pump PROPERTIES CXX_EXTENSIONS OFF)

add_library(vqec_vision_ai_multi_model_session STATIC
    vqec_vision_multi_model_session.cpp)
target_include_directories(vqec_vision_ai_multi_model_session PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_link_libraries(vqec_vision_ai_multi_model_session
    PUBLIC vqec_vision_ai_multi_model_pump)
set_target_properties(vqec_vision_ai_multi_model_session PROPERTIES CXX_EXTENSIONS OFF)

add_library(vqec_vision_ai_encoder_preparation STATIC vqec_vision_encoder_preparation.cpp)
target_include_directories(vqec_vision_ai_encoder_preparation PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_link_libraries(vqec_vision_ai_encoder_preparation PUBLIC vqec_vision_ai_core)
set_target_properties(vqec_vision_ai_encoder_preparation PROPERTIES CXX_EXTENSIONS OFF)
add_library(vqec_vision_ai_application_composition STATIC
    vqec_vision_application_composition.cpp)
target_include_directories(vqec_vision_ai_application_composition PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_link_libraries(vqec_vision_ai_application_composition PUBLIC vqec_vision_ai_source_supervisor)
set_target_properties(vqec_vision_ai_application_composition PROPERTIES CXX_EXTENSIONS OFF)
add_library(vqec_vision_ai_perception_result_stage STATIC
    vqec_vision_perception_result_stage.cpp
    vqec_vision_perception_stage_factory.cpp)
target_include_directories(vqec_vision_ai_perception_result_stage PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/perception/detection>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/perception/tracking>)
target_link_libraries(vqec_vision_ai_perception_result_stage
    PUBLIC vqec_vision_ai_detection vqec_vision_ai_tracking)
set_target_properties(vqec_vision_ai_perception_result_stage PROPERTIES CXX_EXTENSIONS OFF)
add_library(vqec_vision_ai_multi_model_result_router STATIC
    vqec_vision_multi_model_result_router.cpp
    vqec_vision_source_perception_factory.cpp)
target_include_directories(vqec_vision_ai_multi_model_result_router PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_link_libraries(vqec_vision_ai_multi_model_result_router
    PUBLIC vqec_vision_ai_multi_model_pump vqec_vision_ai_perception_result_stage)
set_target_properties(vqec_vision_ai_multi_model_result_router PROPERTIES CXX_EXTENSIONS OFF)
add_library(vqec_vision_ai_runtime_composition_factory STATIC
    vqec_vision_runtime_composition_factory.cpp)
target_include_directories(vqec_vision_ai_runtime_composition_factory PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/runtime/admission>)
target_link_libraries(vqec_vision_ai_runtime_composition_factory PUBLIC
    vqec_vision_ai_admission vqec_vision_ai_application_composition
    vqec_vision_ai_multi_model_session vqec_vision_ai_multi_model_result_router
    vqec_vision_ai_multi_model_feature_pipeline vqec_vision_ai_runtime_executor)
set_target_properties(vqec_vision_ai_runtime_composition_factory
    PROPERTIES CXX_EXTENSIONS OFF)
add_library(vqec_vision_ai_feature_fanout STATIC
    vqec_vision_feature_fanout.cpp)
target_include_directories(vqec_vision_ai_feature_fanout PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/runtime/feature_manager>)
target_link_libraries(vqec_vision_ai_feature_fanout PUBLIC vqec_vision_ai_feature_runtime)
set_target_properties(vqec_vision_ai_feature_fanout PROPERTIES CXX_EXTENSIONS OFF)
add_library(vqec_vision_ai_multi_model_feature_pipeline STATIC
    vqec_vision_multi_model_feature_pipeline.cpp)
target_include_directories(vqec_vision_ai_multi_model_feature_pipeline PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_link_libraries(vqec_vision_ai_multi_model_feature_pipeline
    PUBLIC vqec_vision_ai_multi_model_result_router vqec_vision_ai_feature_fanout)
set_target_properties(vqec_vision_ai_multi_model_feature_pipeline PROPERTIES CXX_EXTENSIONS OFF)
add_library(vqec_vision_ai_runtime_executor STATIC
    vqec_vision_runtime_executor.cpp)
target_include_directories(vqec_vision_ai_runtime_executor PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/outputs>)
target_link_libraries(vqec_vision_ai_runtime_executor
    PUBLIC vqec_vision_ai_application_composition
    vqec_vision_ai_multi_model_feature_pipeline
    vqec_vision_ai_feature_event_dispatch)
set_target_properties(vqec_vision_ai_runtime_executor PROPERTIES CXX_EXTENSIONS OFF)

if(TARGET vqec_vision_ai_qnn_engine AND TARGET vqec_vision_ai_reference AND
   TARGET vqec_vision_ai_detection)
    add_executable(vqec_vision_ai_model_runner
        ${CMAKE_SOURCE_DIR}/tools/vqec_vision_model_runner.cpp)
    target_link_libraries(vqec_vision_ai_model_runner PRIVATE
        vqec_vision_ai_qnn_engine vqec_vision_ai_reference vqec_vision_ai_detection
        nlohmann_json::nlohmann_json)
    set_target_properties(vqec_vision_ai_model_runner PROPERTIES CXX_EXTENSIONS OFF)
endif()

# Board-only camera + inference + RTSP service. Needs the QNN engine, the reference
# preprocessor, the detection decoder and the Qualcomm GStreamer plugins.
if(VQEC_VISION_AI_ENABLE_QUALCOMM AND TARGET vqec_vision_ai_qnn_engine AND
   TARGET vqec_vision_ai_reference AND TARGET vqec_vision_ai_detection)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(camera_gstreamer REQUIRED IMPORTED_TARGET
        gstreamer-1.0 gstreamer-app-1.0 gstreamer-video-1.0)
    add_executable(vqec_vision_ai_camera_service
        ${CMAKE_SOURCE_DIR}/tools/vqec_vision_camera_service.cpp)
    target_link_libraries(vqec_vision_ai_camera_service PRIVATE
        vqec_vision_ai_qnn_engine vqec_vision_ai_reference vqec_vision_ai_detection
        nlohmann_json::nlohmann_json PkgConfig::camera_gstreamer)
    set_target_properties(vqec_vision_ai_camera_service PROPERTIES CXX_EXTENSIONS OFF)
endif()

# Required external service executable. Built only when the JSON loaders and the
# device-free reference backend are available; otherwise there is no runnable harness.
if(TARGET vqec_vision_ai_reference AND TARGET vqec_vision_ai_deployment_config AND
   TARGET vqec_vision_ai_model_catalog_config AND TARGET vqec_vision_ai_feature_catalog_config)
    add_executable(vqec_ai_vision_applications
        vqec_vision_service_main.cpp
        vqec_vision_fake_platform.cpp
        vqec_vision_reference_platform.cpp
        vqec_vision_fixture_detector.cpp)
    target_link_libraries(vqec_ai_vision_applications PRIVATE
        vqec_vision_ai_runtime_composition_factory
        vqec_vision_ai_reference
        vqec_vision_ai_feature_fanout
        vqec_vision_ai_feature_runtime
        vqec_vision_ai_deployment_config
        vqec_vision_ai_model_catalog_config
        vqec_vision_ai_feature_catalog_config)
    set_target_properties(vqec_ai_vision_applications PROPERTIES CXX_EXTENSIONS OFF)
endif()
