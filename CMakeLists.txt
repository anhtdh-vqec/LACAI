cmake_minimum_required(VERSION 3.16)
project(vqec_vision_ai VERSION 0.1.0 LANGUAGES CXX)

option(VQEC_VISION_AI_ENABLE_FW_RING "Enable wrapper for externally supplied FW ring SDK" OFF)
set(VQEC_VISION_AI_FW_RING_TARGET "camera_ai_common" CACHE STRING "Existing version-pinned FW SDK target")

option(VQEC_VISION_AI_ENABLE_QUALCOMM "Enable the private GStreamer Qualcomm adapter" OFF)
option(VQEC_VISION_AI_ENABLE_CAMERA "Enable Linux legacy camera FD receiver" OFF)
option(VQEC_VISION_AI_ENABLE_CAMERA_DBUS "Enable Camera Service GIO D-Bus transport" OFF)
option(VQEC_VISION_AI_ENABLE_GST_FRAME_BRIDGE "Enable Linux FD to GstMemory bridge" OFF)
option(VQEC_VISION_AI_ENABLE_MODEL_MANIFEST "Enable bounded JSON output manifest loading" OFF)
option(VQEC_VISION_AI_ENABLE_MODEL_CATALOG "Enable bounded JSON model catalog loading" OFF)
option(VQEC_VISION_AI_ENABLE_DEPLOYMENT_CONFIG "Enable bounded JSON deployment configuration loading" OFF)
option(VQEC_VISION_AI_ENABLE_FEATURE_CATALOG "Enable bounded JSON feature catalog loading" OFF)
option(VQEC_VISION_AI_BUILD_MANIFEST_CHECK "Build read-only model metadata checker" OFF)
option(VQEC_VISION_AI_ENABLE_ARTIFACT_DIGEST "Enable bounded artifact SHA-256 comparison" OFF)
option(VQEC_VISION_AI_WERROR "Treat project warnings as errors" ON)

if(VQEC_VISION_AI_BUILD_MANIFEST_CHECK AND NOT VQEC_VISION_AI_ENABLE_MODEL_MANIFEST)
    message(FATAL_ERROR "Manifest checker requires VQEC_VISION_AI_ENABLE_MODEL_MANIFEST")
endif()

if(VQEC_VISION_AI_ENABLE_MODEL_MANIFEST OR VQEC_VISION_AI_ENABLE_MODEL_CATALOG OR
   VQEC_VISION_AI_ENABLE_DEPLOYMENT_CONFIG OR VQEC_VISION_AI_ENABLE_FEATURE_CATALOG)
    # Prefer an installed package; otherwise use the pinned in-tree single header.
    # The eSDK AArch64 sysroot does not currently provide nlohmann_json 3.12.0.
    find_package(nlohmann_json 3.12.0 EXACT CONFIG QUIET)
    if(NOT nlohmann_json_FOUND)
        add_library(vqec_vision_ai_nlohmann_json_vendored INTERFACE)
        # SYSTEM keeps -Werror focused on project code, not the vendored header.
        target_include_directories(vqec_vision_ai_nlohmann_json_vendored SYSTEM INTERFACE
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/nlohmann/include")
        add_library(nlohmann_json::nlohmann_json ALIAS
            vqec_vision_ai_nlohmann_json_vendored)
        message(STATUS
            "nlohmann_json package not found; using vendored 3.12.0 single header")
    endif()
endif()

add_library(vqec_vision_ai_core STATIC
    src/core/vqec_vision_inference_plan.cpp src/core/vqec_vision_submission_window.cpp src/core/vqec_vision_source_binding.cpp
    src/core/vqec_vision_tensor_contract.cpp src/core/vqec_vision_output_gate.cpp
    src/core/vqec_vision_preview_contract.cpp src/core/vqec_vision_preview_surface.cpp
    src/core/vqec_vision_encoder_window.cpp src/core/vqec_vision_preview_pool.cpp
    src/core/vqec_vision_encoded_output.cpp src/core/vqec_vision_output_generation.cpp
    src/core/vqec_vision_encoder_contract.cpp src/core/vqec_vision_observation.cpp
    src/core/vqec_vision_deployment_config.cpp src/core/vqec_vision_model_catalog.cpp
    src/core/vqec_vision_feature_event.cpp src/core/vqec_vision_feature_catalog.cpp)
target_compile_features(vqec_vision_ai_core PUBLIC cxx_std_17)
target_include_directories(vqec_vision_ai_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
set_target_properties(vqec_vision_ai_core PROPERTIES CXX_EXTENSIONS OFF)

add_library(vqec_vision_ai_detection STATIC
    src/perception/detection/vqec_vision_model_decode_stage.cpp
    src/perception/detection/vqec_vision_model_decoder_registry.cpp
    src/perception/detection/vqec_vision_tensor_reader.cpp)
target_include_directories(vqec_vision_ai_detection PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/perception/detection>)
target_link_libraries(vqec_vision_ai_detection PUBLIC vqec_vision_ai_core)
set_target_properties(vqec_vision_ai_detection PROPERTIES CXX_EXTENSIONS OFF)

add_library(vqec_vision_ai_tracking STATIC
    src/perception/tracking/vqec_vision_tracking_stage.cpp
    src/perception/tracking/vqec_vision_tracker_registry.cpp)
target_include_directories(vqec_vision_ai_tracking PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/perception/tracking>)
target_link_libraries(vqec_vision_ai_tracking PUBLIC vqec_vision_ai_core)
set_target_properties(vqec_vision_ai_tracking PROPERTIES CXX_EXTENSIONS OFF)

add_library(vqec_vision_ai_attributes STATIC
    src/perception/attributes/vqec_vision_attribute_reader.cpp)
target_include_directories(vqec_vision_ai_attributes PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/perception/attributes>)
target_link_libraries(vqec_vision_ai_attributes PUBLIC vqec_vision_ai_core)
set_target_properties(vqec_vision_ai_attributes PROPERTIES CXX_EXTENSIONS OFF)

add_library(vqec_vision_ai_feature_runtime STATIC
    src/runtime/feature_manager/vqec_vision_feature_stage.cpp
    src/runtime/feature_manager/vqec_vision_feature_processor_registry.cpp
    src/runtime/feature_manager/vqec_vision_feature_activation_manager.cpp)
target_include_directories(vqec_vision_ai_feature_runtime PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/feature_manager>)
target_link_libraries(vqec_vision_ai_feature_runtime PUBLIC vqec_vision_ai_core)
set_target_properties(vqec_vision_ai_feature_runtime PROPERTIES CXX_EXTENSIONS OFF)

if(VQEC_VISION_AI_ENABLE_MODEL_CATALOG)
    add_library(vqec_vision_ai_model_catalog_config STATIC
        src/runtime/model_registry/vqec_vision_model_catalog.cpp)
    target_include_directories(vqec_vision_ai_model_catalog_config PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/model_registry>)
    target_link_libraries(vqec_vision_ai_model_catalog_config
        PUBLIC vqec_vision_ai_core PRIVATE nlohmann_json::nlohmann_json)
    set_target_properties(vqec_vision_ai_model_catalog_config PROPERTIES CXX_EXTENSIONS OFF)
endif()

if(VQEC_VISION_AI_ENABLE_ARTIFACT_DIGEST)
    find_package(OpenSSL 3.0 REQUIRED COMPONENTS Crypto)
    add_library(vqec_vision_ai_artifact_digest STATIC src/runtime/model_registry/vqec_vision_artifact_digest.cpp)
    target_include_directories(vqec_vision_ai_artifact_digest PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/model_registry>)
    target_link_libraries(vqec_vision_ai_artifact_digest
        PUBLIC vqec_vision_ai_core PRIVATE OpenSSL::Crypto)
    set_target_properties(vqec_vision_ai_artifact_digest PROPERTIES CXX_EXTENSIONS OFF)
endif()

if(VQEC_VISION_AI_ENABLE_MODEL_MANIFEST)
    add_library(vqec_vision_ai_model_manifest STATIC
        src/runtime/model_registry/vqec_vision_output_manifest.cpp)
    target_include_directories(vqec_vision_ai_model_manifest PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/model_registry>)
    target_link_libraries(vqec_vision_ai_model_manifest
        PUBLIC vqec_vision_ai_core PRIVATE nlohmann_json::nlohmann_json)
    set_target_properties(vqec_vision_ai_model_manifest PROPERTIES CXX_EXTENSIONS OFF)
    if(VQEC_VISION_AI_BUILD_MANIFEST_CHECK)
        add_executable(vqec_vision_ai_manifest_check tools/vqec_vision_manifest_check.cpp)
        target_link_libraries(vqec_vision_ai_manifest_check PRIVATE vqec_vision_ai_model_manifest)
        set_target_properties(vqec_vision_ai_manifest_check PROPERTIES CXX_EXTENSIONS OFF)
    endif()
endif()

if(VQEC_VISION_AI_ENABLE_DEPLOYMENT_CONFIG)
    add_library(vqec_vision_ai_deployment_config STATIC
        src/runtime/lifecycle/vqec_vision_deployment_config.cpp)
    target_include_directories(vqec_vision_ai_deployment_config PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/lifecycle>)
    target_link_libraries(vqec_vision_ai_deployment_config
        PUBLIC vqec_vision_ai_core PRIVATE nlohmann_json::nlohmann_json)
    set_target_properties(vqec_vision_ai_deployment_config PROPERTIES CXX_EXTENSIONS OFF)
endif()

if(VQEC_VISION_AI_ENABLE_FEATURE_CATALOG)
    add_library(vqec_vision_ai_feature_catalog_config STATIC
        src/runtime/feature_manager/vqec_vision_feature_catalog.cpp)
    target_include_directories(vqec_vision_ai_feature_catalog_config PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/feature_manager>)
    target_link_libraries(vqec_vision_ai_feature_catalog_config
        PUBLIC vqec_vision_ai_feature_runtime PRIVATE nlohmann_json::nlohmann_json)
    set_target_properties(vqec_vision_ai_feature_catalog_config PROPERTIES CXX_EXTENSIONS OFF)
endif()

add_library(vqec_vision_ai_camera_wire STATIC src/adapters/camera/vqec_vision_legacy_wire.cpp)
target_link_libraries(vqec_vision_ai_camera_wire PUBLIC vqec_vision_ai_core)
target_include_directories(vqec_vision_ai_camera_wire PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/adapters/camera>)
set_target_properties(vqec_vision_ai_camera_wire PROPERTIES CXX_EXTENSIONS OFF)

add_library(vqec_vision_ai_camera_control STATIC src/adapters/camera/vqec_vision_camera_control.cpp)
target_link_libraries(vqec_vision_ai_camera_control PUBLIC vqec_vision_ai_core)
target_include_directories(vqec_vision_ai_camera_control PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/adapters/camera>)
set_target_properties(vqec_vision_ai_camera_control PROPERTIES CXX_EXTENSIONS OFF)

if(VQEC_VISION_AI_ENABLE_CAMERA_DBUS)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(camera_gio REQUIRED IMPORTED_TARGET gio-2.0)
    add_library(vqec_vision_ai_camera_dbus STATIC src/adapters/camera/vqec_vision_dbus_rpc.cpp)
    target_link_libraries(vqec_vision_ai_camera_dbus
        PUBLIC vqec_vision_ai_camera_control PRIVATE PkgConfig::camera_gio)
    set_target_properties(vqec_vision_ai_camera_dbus PROPERTIES CXX_EXTENSIONS OFF)
endif()

if(VQEC_VISION_AI_ENABLE_CAMERA)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
        message(FATAL_ERROR "Camera FD receiver requires Linux")
    endif()
    add_library(vqec_vision_ai_camera STATIC
        src/adapters/camera/vqec_vision_frame_source.cpp
        src/adapters/camera/vqec_vision_source_lifecycle.cpp
        src/adapters/camera/vqec_vision_raw_source_resolver.cpp)
    target_link_libraries(vqec_vision_ai_camera
        PUBLIC vqec_vision_ai_camera_wire vqec_vision_ai_camera_control)
    target_compile_definitions(vqec_vision_ai_camera PRIVATE _GNU_SOURCE)
    set_target_properties(vqec_vision_ai_camera PROPERTIES CXX_EXTENSIONS OFF)
endif()

if(VQEC_VISION_AI_ENABLE_GST_FRAME_BRIDGE OR VQEC_VISION_AI_ENABLE_QUALCOMM)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
        message(FATAL_ERROR "GStreamer DMA-BUF bridge requires Linux")
    endif()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(frame_gstreamer REQUIRED IMPORTED_TARGET
        gstreamer-1.0 gstreamer-video-1.0 gstreamer-allocators-1.0 gstreamer-app-1.0)
    add_library(vqec_vision_ai_gst_frame_bridge STATIC
        src/adapters/qualcomm/vqec_vision_dmabuf_bridge.cpp src/adapters/qualcomm/vqec_vision_tensor_output.cpp
        src/adapters/qualcomm/vqec_vision_frame_submission.cpp)
    target_include_directories(vqec_vision_ai_gst_frame_bridge PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/adapters/qualcomm>)
    target_link_libraries(vqec_vision_ai_gst_frame_bridge
        PUBLIC vqec_vision_ai_core PkgConfig::frame_gstreamer)
    set_target_properties(vqec_vision_ai_gst_frame_bridge PROPERTIES CXX_EXTENSIONS OFF)
endif()

if(VQEC_VISION_AI_ENABLE_QUALCOMM)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(gstreamer REQUIRED IMPORTED_TARGET gstreamer-1.0 gstreamer-app-1.0)
    add_library(vqec_vision_ai_qualcomm STATIC
        src/adapters/qualcomm/vqec_vision_plugin_graph.cpp
        src/adapters/qualcomm/vqec_vision_inference_graph.cpp)
    target_compile_features(vqec_vision_ai_qualcomm PUBLIC cxx_std_17)
    target_include_directories(vqec_vision_ai_qualcomm PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/adapters/qualcomm>)
    target_link_libraries(vqec_vision_ai_qualcomm
        PUBLIC vqec_vision_ai_core PRIVATE PkgConfig::gstreamer vqec_vision_ai_gst_frame_bridge)
    set_target_properties(vqec_vision_ai_qualcomm PROPERTIES CXX_EXTENSIONS OFF)
endif()

if(UNIX)
    add_library(vqec_vision_ai_reference STATIC
        src/adapters/reference/vqec_vision_reference_source.cpp
        src/adapters/reference/vqec_vision_reference_graph.cpp
        src/adapters/reference/vqec_vision_reference_sink.cpp)
    target_include_directories(vqec_vision_ai_reference PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/adapters/reference>)
    target_link_libraries(vqec_vision_ai_reference PUBLIC vqec_vision_ai_core)
    set_target_properties(vqec_vision_ai_reference PROPERTIES CXX_EXTENSIONS OFF)
endif()

add_library(vqec_vision_ai_camera_graph_pump STATIC
    src/app/vqec_vision_camera_graph_pump.cpp src/app/vqec_vision_camera_session.cpp)
target_include_directories(vqec_vision_ai_camera_graph_pump PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/app>)
target_link_libraries(vqec_vision_ai_camera_graph_pump PUBLIC vqec_vision_ai_core)
set_target_properties(vqec_vision_ai_camera_graph_pump PROPERTIES CXX_EXTENSIONS OFF)

add_library(vqec_vision_ai_source_supervisor STATIC
    src/app/vqec_vision_multi_source_supervisor.cpp)
target_include_directories(vqec_vision_ai_source_supervisor PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/app>)
target_link_libraries(vqec_vision_ai_source_supervisor PUBLIC vqec_vision_ai_core)
set_target_properties(vqec_vision_ai_source_supervisor PROPERTIES CXX_EXTENSIONS OFF)

add_library(vqec_vision_ai_scheduler STATIC
    src/runtime/scheduler/vqec_vision_model_cadence.cpp)
target_include_directories(vqec_vision_ai_scheduler PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/scheduler>)
target_link_libraries(vqec_vision_ai_scheduler PUBLIC vqec_vision_ai_core)
set_target_properties(vqec_vision_ai_scheduler PROPERTIES CXX_EXTENSIONS OFF)

add_library(vqec_vision_ai_multi_model_pump STATIC
    src/app/vqec_vision_multi_model_pump.cpp)
target_include_directories(vqec_vision_ai_multi_model_pump PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/app>)
target_link_libraries(vqec_vision_ai_multi_model_pump
    PUBLIC vqec_vision_ai_core vqec_vision_ai_scheduler)
set_target_properties(vqec_vision_ai_multi_model_pump PROPERTIES CXX_EXTENSIONS OFF)

add_library(vqec_vision_ai_multi_model_session STATIC
    src/app/vqec_vision_multi_model_session.cpp)
target_include_directories(vqec_vision_ai_multi_model_session PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/app>)
target_link_libraries(vqec_vision_ai_multi_model_session
    PUBLIC vqec_vision_ai_multi_model_pump)
set_target_properties(vqec_vision_ai_multi_model_session PROPERTIES CXX_EXTENSIONS OFF)

include(CTest)
if(VQEC_VISION_AI_ENABLE_FW_RING)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
        message(FATAL_ERROR "Released FW ring SDK requires Linux")
    endif()
    if(NOT TARGET ${VQEC_VISION_AI_FW_RING_TARGET})
        message(FATAL_ERROR "Provide the pinned FW SDK target before add_subdirectory(ai_applications)")
    endif()
    add_library(vqec_vision_ai_fw_ring STATIC src/adapters/fw_output/vqec_vision_ring_sink.cpp)
    target_include_directories(vqec_vision_ai_fw_ring PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/adapters/fw_output>)
    target_link_libraries(vqec_vision_ai_fw_ring PUBLIC vqec_vision_ai_core
        PRIVATE ${VQEC_VISION_AI_FW_RING_TARGET})
    set_target_properties(vqec_vision_ai_fw_ring PROPERTIES CXX_EXTENSIONS OFF)
endif()
add_library(vqec_vision_ai_encoded_dispatch STATIC src/outputs/vqec_vision_encoded_dispatch.cpp)
target_include_directories(vqec_vision_ai_encoded_dispatch PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/outputs>)
target_link_libraries(vqec_vision_ai_encoded_dispatch PUBLIC vqec_vision_ai_core)
set_target_properties(vqec_vision_ai_encoded_dispatch PROPERTIES CXX_EXTENSIONS OFF)
add_library(vqec_vision_ai_feature_event_dispatch STATIC
    src/outputs/vqec_vision_feature_event_dispatch.cpp)
target_include_directories(vqec_vision_ai_feature_event_dispatch PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/outputs>)
target_link_libraries(vqec_vision_ai_feature_event_dispatch PUBLIC vqec_vision_ai_core)
set_target_properties(vqec_vision_ai_feature_event_dispatch PROPERTIES CXX_EXTENSIONS OFF)
add_library(vqec_vision_ai_overlay_preparation STATIC
    src/outputs/vqec_vision_overlay_preparation.cpp)
target_include_directories(vqec_vision_ai_overlay_preparation PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/outputs>)
target_link_libraries(vqec_vision_ai_overlay_preparation PUBLIC vqec_vision_ai_core)
set_target_properties(vqec_vision_ai_overlay_preparation PROPERTIES CXX_EXTENSIONS OFF)
add_library(vqec_vision_ai_admission STATIC
    src/runtime/admission/vqec_vision_activation_snapshot.cpp)
target_include_directories(vqec_vision_ai_admission PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/admission>)
target_link_libraries(vqec_vision_ai_admission PUBLIC vqec_vision_ai_core)
set_target_properties(vqec_vision_ai_admission PROPERTIES CXX_EXTENSIONS OFF)
add_library(vqec_vision_ai_encoder_preparation STATIC src/app/vqec_vision_encoder_preparation.cpp)
target_include_directories(vqec_vision_ai_encoder_preparation PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/app>)
target_link_libraries(vqec_vision_ai_encoder_preparation PUBLIC vqec_vision_ai_core)
set_target_properties(vqec_vision_ai_encoder_preparation PROPERTIES CXX_EXTENSIONS OFF)
add_library(vqec_vision_ai_application_composition STATIC
    src/app/vqec_vision_application_composition.cpp)
target_include_directories(vqec_vision_ai_application_composition PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/app>)
target_link_libraries(vqec_vision_ai_application_composition PUBLIC vqec_vision_ai_source_supervisor)
set_target_properties(vqec_vision_ai_application_composition PROPERTIES CXX_EXTENSIONS OFF)
add_library(vqec_vision_ai_perception_result_stage STATIC
    src/app/vqec_vision_perception_result_stage.cpp
    src/app/vqec_vision_perception_stage_factory.cpp)
target_include_directories(vqec_vision_ai_perception_result_stage PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/app>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/perception/detection>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/perception/tracking>)
target_link_libraries(vqec_vision_ai_perception_result_stage
    PUBLIC vqec_vision_ai_detection vqec_vision_ai_tracking)
set_target_properties(vqec_vision_ai_perception_result_stage PROPERTIES CXX_EXTENSIONS OFF)
add_library(vqec_vision_ai_multi_model_result_router STATIC
    src/app/vqec_vision_multi_model_result_router.cpp
    src/app/vqec_vision_source_perception_factory.cpp)
target_include_directories(vqec_vision_ai_multi_model_result_router PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/app>)
target_link_libraries(vqec_vision_ai_multi_model_result_router
    PUBLIC vqec_vision_ai_multi_model_pump vqec_vision_ai_perception_result_stage)
set_target_properties(vqec_vision_ai_multi_model_result_router PROPERTIES CXX_EXTENSIONS OFF)
add_library(vqec_vision_ai_runtime_composition_factory STATIC
    src/app/vqec_vision_runtime_composition_factory.cpp)
target_include_directories(vqec_vision_ai_runtime_composition_factory PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/app>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/admission>)
target_link_libraries(vqec_vision_ai_runtime_composition_factory PUBLIC
    vqec_vision_ai_admission vqec_vision_ai_application_composition
    vqec_vision_ai_multi_model_session vqec_vision_ai_multi_model_result_router
    vqec_vision_ai_multi_model_feature_pipeline vqec_vision_ai_runtime_executor)
set_target_properties(vqec_vision_ai_runtime_composition_factory
    PROPERTIES CXX_EXTENSIONS OFF)
add_library(vqec_vision_ai_feature_fanout STATIC
    src/app/vqec_vision_feature_fanout.cpp)
target_include_directories(vqec_vision_ai_feature_fanout PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/app>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/feature_manager>)
target_link_libraries(vqec_vision_ai_feature_fanout PUBLIC vqec_vision_ai_feature_runtime)
set_target_properties(vqec_vision_ai_feature_fanout PROPERTIES CXX_EXTENSIONS OFF)
add_library(vqec_vision_ai_multi_model_feature_pipeline STATIC
    src/app/vqec_vision_multi_model_feature_pipeline.cpp)
target_include_directories(vqec_vision_ai_multi_model_feature_pipeline PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/app>)
target_link_libraries(vqec_vision_ai_multi_model_feature_pipeline
    PUBLIC vqec_vision_ai_multi_model_result_router vqec_vision_ai_feature_fanout)
set_target_properties(vqec_vision_ai_multi_model_feature_pipeline PROPERTIES CXX_EXTENSIONS OFF)
add_library(vqec_vision_ai_runtime_executor STATIC
    src/app/vqec_vision_runtime_executor.cpp)
target_include_directories(vqec_vision_ai_runtime_executor PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/app>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/outputs>)
target_link_libraries(vqec_vision_ai_runtime_executor
    PUBLIC vqec_vision_ai_application_composition
    vqec_vision_ai_multi_model_feature_pipeline
    vqec_vision_ai_feature_event_dispatch)
set_target_properties(vqec_vision_ai_runtime_executor PROPERTIES CXX_EXTENSIONS OFF)

# Required external service executable. Built only when the JSON loaders and the
# device-free reference backend are available; otherwise there is no runnable harness.
if(TARGET vqec_vision_ai_reference AND TARGET vqec_vision_ai_deployment_config AND
   TARGET vqec_vision_ai_model_catalog_config AND TARGET vqec_vision_ai_feature_catalog_config)
    add_executable(vqec_ai_vision_applications src/app/vqec_vision_service_main.cpp)
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

if(BUILD_TESTING)
    add_executable(vqec_vision_ai_feature_processor_registry_test
        tests/contract/vqec_vision_feature_processor_registry_test.cpp)
    target_link_libraries(vqec_vision_ai_feature_processor_registry_test
        PRIVATE vqec_vision_ai_feature_runtime)
    set_target_properties(vqec_vision_ai_feature_processor_registry_test
        PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME feature_processor_registry_contract
        COMMAND vqec_vision_ai_feature_processor_registry_test)
    add_executable(vqec_vision_ai_feature_activation_manager_test
        tests/contract/vqec_vision_feature_activation_manager_test.cpp)
    target_link_libraries(vqec_vision_ai_feature_activation_manager_test
        PRIVATE vqec_vision_ai_feature_runtime)
    set_target_properties(vqec_vision_ai_feature_activation_manager_test
        PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME feature_activation_manager_contract
        COMMAND vqec_vision_ai_feature_activation_manager_test)
    add_executable(vqec_vision_ai_tracker_registry_test
        tests/contract/vqec_vision_tracker_registry_test.cpp)
    target_link_libraries(vqec_vision_ai_tracker_registry_test
        PRIVATE vqec_vision_ai_tracking)
    set_target_properties(vqec_vision_ai_tracker_registry_test
        PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME tracker_registry_contract
        COMMAND vqec_vision_ai_tracker_registry_test)
    if(TARGET vqec_vision_ai_feature_catalog_config)
        add_executable(vqec_vision_ai_feature_catalog_loader_test
            tests/unit/vqec_vision_feature_catalog_loader_test.cpp)
        target_link_libraries(vqec_vision_ai_feature_catalog_loader_test
            PRIVATE vqec_vision_ai_feature_catalog_config)
        set_target_properties(vqec_vision_ai_feature_catalog_loader_test
            PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME feature_catalog_loading
            COMMAND vqec_vision_ai_feature_catalog_loader_test)
    endif()
    add_executable(vqec_vision_ai_feature_catalog_test
        tests/unit/vqec_vision_feature_catalog_test.cpp)
    target_link_libraries(vqec_vision_ai_feature_catalog_test PRIVATE vqec_vision_ai_core)
    set_target_properties(vqec_vision_ai_feature_catalog_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME feature_catalog_contract COMMAND vqec_vision_ai_feature_catalog_test)
    add_executable(vqec_vision_ai_multi_model_feature_pipeline_test
        tests/contract/vqec_vision_multi_model_feature_pipeline_test.cpp)
    target_link_libraries(vqec_vision_ai_multi_model_feature_pipeline_test
        PRIVATE vqec_vision_ai_multi_model_feature_pipeline)
    set_target_properties(vqec_vision_ai_multi_model_feature_pipeline_test
        PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME multi_model_feature_pipeline
        COMMAND vqec_vision_ai_multi_model_feature_pipeline_test)
    add_executable(vqec_vision_ai_multi_model_result_router_test
        tests/contract/vqec_vision_multi_model_result_router_test.cpp)
    target_link_libraries(vqec_vision_ai_multi_model_result_router_test
        PRIVATE vqec_vision_ai_multi_model_result_router)
    set_target_properties(vqec_vision_ai_multi_model_result_router_test
        PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME multi_model_result_routing
        COMMAND vqec_vision_ai_multi_model_result_router_test)
    add_executable(vqec_vision_ai_source_perception_factory_test
        tests/contract/vqec_vision_source_perception_factory_test.cpp)
    target_link_libraries(vqec_vision_ai_source_perception_factory_test
        PRIVATE vqec_vision_ai_multi_model_result_router)
    set_target_properties(vqec_vision_ai_source_perception_factory_test
        PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME source_perception_factory_contract
        COMMAND vqec_vision_ai_source_perception_factory_test)
    add_executable(vqec_vision_ai_runtime_composition_factory_test
        tests/contract/vqec_vision_runtime_composition_factory_test.cpp)
    target_link_libraries(vqec_vision_ai_runtime_composition_factory_test
        PRIVATE vqec_vision_ai_runtime_composition_factory)
    set_target_properties(vqec_vision_ai_runtime_composition_factory_test
        PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME runtime_composition_factory_contract
        COMMAND vqec_vision_ai_runtime_composition_factory_test)
    add_executable(vqec_vision_ai_feature_fanout_test
        tests/contract/vqec_vision_feature_fanout_test.cpp)
    target_link_libraries(vqec_vision_ai_feature_fanout_test
        PRIVATE vqec_vision_ai_feature_fanout)
    set_target_properties(vqec_vision_ai_feature_fanout_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME feature_fanout_isolation COMMAND vqec_vision_ai_feature_fanout_test)
    add_executable(vqec_vision_ai_perception_result_stage_test
        tests/contract/vqec_vision_perception_result_stage_test.cpp)
    target_link_libraries(vqec_vision_ai_perception_result_stage_test
        PRIVATE vqec_vision_ai_perception_result_stage)
    set_target_properties(vqec_vision_ai_perception_result_stage_test
        PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME perception_result_correlation
        COMMAND vqec_vision_ai_perception_result_stage_test)
    add_executable(vqec_vision_ai_perception_stage_factory_test
        tests/contract/vqec_vision_perception_stage_factory_test.cpp)
    target_link_libraries(vqec_vision_ai_perception_stage_factory_test
        PRIVATE vqec_vision_ai_perception_result_stage)
    set_target_properties(vqec_vision_ai_perception_stage_factory_test
        PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME perception_stage_factory_contract
        COMMAND vqec_vision_ai_perception_stage_factory_test)
    add_executable(vqec_vision_ai_feature_event_dispatch_test
        tests/contract/vqec_vision_feature_event_dispatch_test.cpp)
    target_link_libraries(vqec_vision_ai_feature_event_dispatch_test
        PRIVATE vqec_vision_ai_feature_event_dispatch)
    target_include_directories(vqec_vision_ai_feature_event_dispatch_test PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/outputs>)
    set_target_properties(vqec_vision_ai_feature_event_dispatch_test
        PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME feature_event_dispatch_authorization
        COMMAND vqec_vision_ai_feature_event_dispatch_test)
    add_executable(vqec_vision_ai_feature_stage_test
        tests/contract/vqec_vision_feature_stage_test.cpp)
    target_link_libraries(vqec_vision_ai_feature_stage_test
        PRIVATE vqec_vision_ai_feature_runtime)
    target_include_directories(vqec_vision_ai_feature_stage_test PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/feature_manager>)
    set_target_properties(vqec_vision_ai_feature_stage_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME feature_stage_contract COMMAND vqec_vision_ai_feature_stage_test)
    add_executable(vqec_vision_ai_feature_processor_test
        tests/contract/vqec_vision_feature_processor_test.cpp)
    target_link_libraries(vqec_vision_ai_feature_processor_test PRIVATE vqec_vision_ai_core)
    set_target_properties(vqec_vision_ai_feature_processor_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME feature_processor_contract COMMAND vqec_vision_ai_feature_processor_test)
    add_executable(vqec_vision_ai_feature_event_test
        tests/unit/vqec_vision_feature_event_test.cpp)
    target_link_libraries(vqec_vision_ai_feature_event_test PRIVATE vqec_vision_ai_core)
    set_target_properties(vqec_vision_ai_feature_event_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME feature_event_contract COMMAND vqec_vision_ai_feature_event_test)
    add_executable(vqec_vision_ai_attribute_reader_test
        tests/contract/vqec_vision_attribute_reader_test.cpp)
    target_link_libraries(vqec_vision_ai_attribute_reader_test
        PRIVATE vqec_vision_ai_attributes)
    target_include_directories(vqec_vision_ai_attribute_reader_test PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/perception/attributes>)
    set_target_properties(vqec_vision_ai_attribute_reader_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME attribute_reader_contract COMMAND vqec_vision_ai_attribute_reader_test)
    add_executable(vqec_vision_ai_tracking_stage_test
        tests/contract/vqec_vision_tracking_stage_test.cpp)
    target_link_libraries(vqec_vision_ai_tracking_stage_test PRIVATE vqec_vision_ai_tracking)
    target_include_directories(vqec_vision_ai_tracking_stage_test PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/perception/tracking>)
    set_target_properties(vqec_vision_ai_tracking_stage_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME tracking_stage_contract COMMAND vqec_vision_ai_tracking_stage_test)
    add_executable(vqec_vision_ai_tracker_port_test
        tests/contract/vqec_vision_tracker_port_test.cpp)
    target_link_libraries(vqec_vision_ai_tracker_port_test PRIVATE vqec_vision_ai_core)
    set_target_properties(vqec_vision_ai_tracker_port_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME tracker_port_contract COMMAND vqec_vision_ai_tracker_port_test)
    add_executable(vqec_vision_ai_tensor_reader_test
        tests/contract/vqec_vision_tensor_reader_test.cpp)
    target_link_libraries(vqec_vision_ai_tensor_reader_test PRIVATE vqec_vision_ai_detection)
    target_include_directories(vqec_vision_ai_tensor_reader_test PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/perception/detection>)
    set_target_properties(vqec_vision_ai_tensor_reader_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME tensor_reader_contract COMMAND vqec_vision_ai_tensor_reader_test)
    add_executable(vqec_vision_ai_overlay_preparation_test
        tests/contract/vqec_vision_overlay_preparation_test.cpp)
    target_link_libraries(vqec_vision_ai_overlay_preparation_test
        PRIVATE vqec_vision_ai_overlay_preparation)
    target_include_directories(vqec_vision_ai_overlay_preparation_test PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/outputs>)
    set_target_properties(vqec_vision_ai_overlay_preparation_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME overlay_preparation_authorization
        COMMAND vqec_vision_ai_overlay_preparation_test)
    add_executable(vqec_vision_ai_model_decoder_registry_test
        tests/contract/vqec_vision_model_decoder_registry_test.cpp)
    target_link_libraries(vqec_vision_ai_model_decoder_registry_test
        PRIVATE vqec_vision_ai_detection)
    target_include_directories(vqec_vision_ai_model_decoder_registry_test PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/perception/detection>)
    set_target_properties(vqec_vision_ai_model_decoder_registry_test
        PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME model_decoder_registry_contract
        COMMAND vqec_vision_ai_model_decoder_registry_test)
    add_executable(vqec_vision_ai_model_decode_stage_test
        tests/contract/vqec_vision_model_decode_stage_test.cpp)
    target_link_libraries(vqec_vision_ai_model_decode_stage_test
        PRIVATE vqec_vision_ai_detection)
    target_include_directories(vqec_vision_ai_model_decode_stage_test PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/perception/detection>)
    set_target_properties(vqec_vision_ai_model_decode_stage_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME model_decode_stage_contract COMMAND vqec_vision_ai_model_decode_stage_test)
    add_executable(vqec_vision_ai_model_decoder_test
        tests/contract/vqec_vision_model_decoder_test.cpp)
    target_link_libraries(vqec_vision_ai_model_decoder_test PRIVATE vqec_vision_ai_core)
    set_target_properties(vqec_vision_ai_model_decoder_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME model_decoder_contract COMMAND vqec_vision_ai_model_decoder_test)
    add_executable(vqec_vision_ai_application_composition_test
        tests/contract/vqec_vision_application_composition_test.cpp)
    target_link_libraries(vqec_vision_ai_application_composition_test
        PRIVATE vqec_vision_ai_application_composition)
    target_include_directories(vqec_vision_ai_application_composition_test PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/app>)
    set_target_properties(vqec_vision_ai_application_composition_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME application_composition_lifecycle
        COMMAND vqec_vision_ai_application_composition_test)
    add_executable(vqec_vision_ai_model_cadence_test
        tests/unit/vqec_vision_model_cadence_test.cpp)
    target_link_libraries(vqec_vision_ai_model_cadence_test
        PRIVATE vqec_vision_ai_scheduler)
    set_target_properties(vqec_vision_ai_model_cadence_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME model_cadence_selection COMMAND vqec_vision_ai_model_cadence_test)
    add_executable(vqec_vision_ai_multi_model_pump_test
        tests/unit/vqec_vision_multi_model_pump_test.cpp)
    target_link_libraries(vqec_vision_ai_multi_model_pump_test
        PRIVATE vqec_vision_ai_multi_model_pump)
    set_target_properties(vqec_vision_ai_multi_model_pump_test
        PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME multi_model_frame_fanout COMMAND vqec_vision_ai_multi_model_pump_test)
    add_executable(vqec_vision_ai_multi_model_session_test
        tests/unit/vqec_vision_multi_model_session_test.cpp)
    target_link_libraries(vqec_vision_ai_multi_model_session_test
        PRIVATE vqec_vision_ai_multi_model_session)
    set_target_properties(vqec_vision_ai_multi_model_session_test
        PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME multi_model_source_lifecycle
        COMMAND vqec_vision_ai_multi_model_session_test)
    add_executable(vqec_vision_ai_multi_source_supervisor_test
        tests/unit/vqec_vision_multi_source_supervisor_test.cpp)
    target_link_libraries(vqec_vision_ai_multi_source_supervisor_test
        PRIVATE vqec_vision_ai_source_supervisor)
    set_target_properties(vqec_vision_ai_multi_source_supervisor_test
        PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME multi_source_supervision
        COMMAND vqec_vision_ai_multi_source_supervisor_test)
    add_executable(vqec_vision_ai_activation_snapshot_test
        tests/unit/vqec_vision_activation_snapshot_test.cpp)
    target_link_libraries(vqec_vision_ai_activation_snapshot_test
        PRIVATE vqec_vision_ai_admission)
    set_target_properties(vqec_vision_ai_activation_snapshot_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME activation_snapshot_validation COMMAND vqec_vision_ai_activation_snapshot_test)
    add_executable(vqec_vision_ai_model_catalog_test
        tests/unit/vqec_vision_model_catalog_test.cpp)
    target_link_libraries(vqec_vision_ai_model_catalog_test PRIVATE vqec_vision_ai_core)
    set_target_properties(vqec_vision_ai_model_catalog_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME model_catalog_validation COMMAND vqec_vision_ai_model_catalog_test)
    if(TARGET vqec_vision_ai_model_catalog_config)
        add_executable(vqec_vision_ai_model_catalog_loader_test
            tests/unit/vqec_vision_model_catalog_loader_test.cpp)
        target_link_libraries(vqec_vision_ai_model_catalog_loader_test
            PRIVATE vqec_vision_ai_model_catalog_config)
        set_target_properties(vqec_vision_ai_model_catalog_loader_test
            PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME model_catalog_loading COMMAND vqec_vision_ai_model_catalog_loader_test)
    endif()
    add_executable(vqec_vision_ai_deployment_config_test
        tests/unit/vqec_vision_deployment_config_test.cpp)
    target_link_libraries(vqec_vision_ai_deployment_config_test PRIVATE vqec_vision_ai_core)
    set_target_properties(vqec_vision_ai_deployment_config_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME deployment_config_validation COMMAND vqec_vision_ai_deployment_config_test)
    if(TARGET vqec_vision_ai_deployment_config)
        add_executable(vqec_vision_ai_deployment_loader_test
            tests/unit/vqec_vision_deployment_loader_test.cpp)
        target_link_libraries(vqec_vision_ai_deployment_loader_test
            PRIVATE vqec_vision_ai_deployment_config)
        set_target_properties(vqec_vision_ai_deployment_loader_test PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME deployment_config_loading COMMAND vqec_vision_ai_deployment_loader_test)
    endif()
    add_executable(vqec_vision_ai_observation_test tests/unit/vqec_vision_observation_test.cpp)
    target_link_libraries(vqec_vision_ai_observation_test PRIVATE vqec_vision_ai_core)
    set_target_properties(vqec_vision_ai_observation_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME observation_contract COMMAND vqec_vision_ai_observation_test)
    add_executable(vqec_vision_ai_encoder_contract_test
        tests/unit/vqec_vision_encoder_contract_test.cpp)
    target_link_libraries(vqec_vision_ai_encoder_contract_test PRIVATE vqec_vision_ai_core)
    add_test(NAME encoder_input_validation COMMAND vqec_vision_ai_encoder_contract_test)
    add_executable(vqec_vision_ai_output_generation_test
        tests/unit/vqec_vision_output_generation_test.cpp)
    target_link_libraries(vqec_vision_ai_output_generation_test PRIVATE vqec_vision_ai_core)
    set_target_properties(vqec_vision_ai_output_generation_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME output_generation_identity COMMAND vqec_vision_ai_output_generation_test)
    if(TARGET vqec_vision_ai_fw_ring)
        add_executable(vqec_vision_ai_ring_sink_test tests/contract/vqec_vision_ring_sink_test.cpp)
        target_link_libraries(vqec_vision_ai_ring_sink_test PRIVATE vqec_vision_ai_fw_ring
            ${VQEC_VISION_AI_FW_RING_TARGET})
        set_target_properties(vqec_vision_ai_ring_sink_test PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME fw_ring_sink_guards COMMAND vqec_vision_ai_ring_sink_test)
    endif()
    add_executable(vqec_vision_ai_encoded_dispatch_test tests/contract/vqec_vision_encoded_dispatch_test.cpp)
    target_link_libraries(vqec_vision_ai_encoded_dispatch_test PRIVATE vqec_vision_ai_encoded_dispatch)
    set_target_properties(vqec_vision_ai_encoded_dispatch_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME encoded_dispatch_authorization COMMAND vqec_vision_ai_encoded_dispatch_test)
    add_executable(vqec_vision_ai_encoded_output_test tests/unit/vqec_vision_encoded_output_test.cpp)
    target_link_libraries(vqec_vision_ai_encoded_output_test PRIVATE vqec_vision_ai_core)
    set_target_properties(vqec_vision_ai_encoded_output_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME encoded_output_ownership COMMAND vqec_vision_ai_encoded_output_test)
    add_executable(vqec_vision_ai_encoder_preparation_test
        tests/contract/vqec_vision_encoder_preparation_test.cpp)
    target_link_libraries(vqec_vision_ai_encoder_preparation_test PRIVATE vqec_vision_ai_encoder_preparation)
    set_target_properties(vqec_vision_ai_encoder_preparation_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME encoder_preparation_contract COMMAND vqec_vision_ai_encoder_preparation_test)
    add_executable(vqec_vision_ai_preview_pool_test tests/unit/vqec_vision_preview_pool_test.cpp)
    target_link_libraries(vqec_vision_ai_preview_pool_test PRIVATE vqec_vision_ai_core)
    set_target_properties(vqec_vision_ai_preview_pool_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME preview_pool_ownership COMMAND vqec_vision_ai_preview_pool_test)
    add_executable(vqec_vision_ai_encoder_window_test tests/unit/vqec_vision_encoder_window_test.cpp)
    target_link_libraries(vqec_vision_ai_encoder_window_test PRIVATE vqec_vision_ai_core)
    set_target_properties(vqec_vision_ai_encoder_window_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME encoder_window_lifecycle COMMAND vqec_vision_ai_encoder_window_test)
    add_executable(vqec_vision_ai_preview_surface_test tests/unit/vqec_vision_preview_surface_test.cpp)
    target_link_libraries(vqec_vision_ai_preview_surface_test PRIVATE vqec_vision_ai_core)
    set_target_properties(vqec_vision_ai_preview_surface_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME preview_surface_ownership COMMAND vqec_vision_ai_preview_surface_test)
    add_executable(vqec_vision_ai_preview_contract_test tests/unit/vqec_vision_preview_contract_test.cpp)
    target_link_libraries(vqec_vision_ai_preview_contract_test PRIVATE vqec_vision_ai_core)
    set_target_properties(vqec_vision_ai_preview_contract_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME preview_contract_validation COMMAND vqec_vision_ai_preview_contract_test)
    add_executable(vqec_vision_ai_output_gate_test tests/unit/vqec_vision_output_gate_test.cpp)
    target_link_libraries(vqec_vision_ai_output_gate_test PRIVATE vqec_vision_ai_core)
    set_target_properties(vqec_vision_ai_output_gate_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME output_gate_authorization COMMAND vqec_vision_ai_output_gate_test)
    if(TARGET vqec_vision_ai_artifact_digest)
        add_executable(vqec_vision_ai_artifact_digest_test tests/unit/vqec_vision_artifact_digest_test.cpp)
        target_link_libraries(vqec_vision_ai_artifact_digest_test
            PRIVATE vqec_vision_ai_artifact_digest)
        set_target_properties(vqec_vision_ai_artifact_digest_test PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME artifact_digest_validation COMMAND vqec_vision_ai_artifact_digest_test)
    endif()
    if(TARGET vqec_vision_ai_model_manifest)
        add_executable(vqec_vision_ai_output_manifest_test tests/unit/vqec_vision_output_manifest_test.cpp)
        target_link_libraries(vqec_vision_ai_output_manifest_test PRIVATE vqec_vision_ai_model_manifest)
        set_target_properties(vqec_vision_ai_output_manifest_test PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME output_manifest_validation COMMAND vqec_vision_ai_output_manifest_test)
        if(TARGET vqec_vision_ai_manifest_check)
            add_test(NAME output_manifest_example COMMAND vqec_vision_ai_manifest_check
                ${CMAKE_CURRENT_SOURCE_DIR}/manifests/models/output_fixture.json)
        endif()
    endif()
    add_executable(vqec_vision_ai_tensor_contract_test tests/unit/vqec_vision_tensor_contract_test.cpp)
    target_link_libraries(vqec_vision_ai_tensor_contract_test PRIVATE vqec_vision_ai_core)
    set_target_properties(vqec_vision_ai_tensor_contract_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME tensor_contract_validation COMMAND vqec_vision_ai_tensor_contract_test)
    if(VQEC_VISION_AI_ENABLE_CAMERA AND VQEC_VISION_AI_ENABLE_QUALCOMM)
        add_executable(vqec_vision_ai_camera_session_test tests/contract/vqec_vision_camera_session_test.cpp)
        target_link_libraries(vqec_vision_ai_camera_session_test
            PRIVATE vqec_vision_ai_camera_graph_pump vqec_vision_ai_camera
                    vqec_vision_ai_qualcomm)
        set_target_properties(vqec_vision_ai_camera_session_test PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME camera_session_coordination COMMAND vqec_vision_ai_camera_session_test)
        set_tests_properties(camera_session_coordination PROPERTIES TIMEOUT 5)
        add_executable(vqec_vision_ai_camera_graph_pump_test
            tests/contract/vqec_vision_camera_graph_pump_test.cpp)
        target_link_libraries(vqec_vision_ai_camera_graph_pump_test
            PRIVATE vqec_vision_ai_camera_graph_pump vqec_vision_ai_camera
                    vqec_vision_ai_qualcomm)
        set_target_properties(vqec_vision_ai_camera_graph_pump_test PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME camera_graph_pump_guards COMMAND vqec_vision_ai_camera_graph_pump_test)
    endif()
    add_executable(vqec_vision_ai_source_binding_test tests/unit/vqec_vision_source_binding_test.cpp)
    target_link_libraries(vqec_vision_ai_source_binding_test PRIVATE vqec_vision_ai_core)
    set_target_properties(vqec_vision_ai_source_binding_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME source_binding_validation COMMAND vqec_vision_ai_source_binding_test)
    if(VQEC_VISION_AI_ENABLE_QUALCOMM)
        add_executable(vqec_vision_ai_plugin_graph_test tests/contract/vqec_vision_plugin_graph_test.cpp)
        target_link_libraries(vqec_vision_ai_plugin_graph_test PRIVATE vqec_vision_ai_qualcomm)
        set_target_properties(vqec_vision_ai_plugin_graph_test PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME plugin_graph_state_guards COMMAND vqec_vision_ai_plugin_graph_test)
    endif()
    add_executable(vqec_vision_ai_inference_plan_test tests/unit/vqec_vision_inference_plan_test.cpp)
    target_link_libraries(vqec_vision_ai_inference_plan_test PRIVATE vqec_vision_ai_core)
    set_target_properties(vqec_vision_ai_inference_plan_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME inference_plan_validation COMMAND vqec_vision_ai_inference_plan_test)
    add_executable(vqec_vision_ai_submission_window_test tests/unit/vqec_vision_submission_window_test.cpp)
    target_link_libraries(vqec_vision_ai_submission_window_test PRIVATE vqec_vision_ai_core)
    set_target_properties(vqec_vision_ai_submission_window_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME submission_window COMMAND vqec_vision_ai_submission_window_test)
    add_executable(vqec_vision_ai_legacy_wire_test tests/unit/vqec_vision_legacy_wire_test.cpp)
    target_link_libraries(vqec_vision_ai_legacy_wire_test PRIVATE vqec_vision_ai_camera_wire)
    set_target_properties(vqec_vision_ai_legacy_wire_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME legacy_camera_wire COMMAND vqec_vision_ai_legacy_wire_test)
    add_executable(vqec_vision_ai_camera_control_test tests/unit/vqec_vision_camera_control_test.cpp)
    target_link_libraries(vqec_vision_ai_camera_control_test PRIVATE vqec_vision_ai_camera_control)
    set_target_properties(vqec_vision_ai_camera_control_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME camera_control_state COMMAND vqec_vision_ai_camera_control_test)
    if(TARGET vqec_vision_ai_gst_frame_bridge)
        find_package(Threads REQUIRED)
        # Separate binary: fixture entrypoint cannot be linked into the production backend.
        add_library(vqec_vision_ai_graph_fixture STATIC src/adapters/qualcomm/vqec_vision_plugin_graph.cpp)
        target_compile_definitions(vqec_vision_ai_graph_fixture
            PUBLIC VQEC_VISION_AI_GRAPH_TEST_FIXTURE)
        target_link_libraries(vqec_vision_ai_graph_fixture PUBLIC vqec_vision_ai_gst_frame_bridge)
        set_target_properties(vqec_vision_ai_graph_fixture PROPERTIES CXX_EXTENSIONS OFF)
        add_executable(vqec_vision_ai_graph_lifecycle_test tests/contract/vqec_vision_graph_lifecycle_test.cpp)
        target_link_libraries(vqec_vision_ai_graph_lifecycle_test
            PRIVATE vqec_vision_ai_graph_fixture Threads::Threads)
        set_target_properties(vqec_vision_ai_graph_lifecycle_test PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME graph_submission_lifecycle COMMAND vqec_vision_ai_graph_lifecycle_test)
        set_tests_properties(graph_submission_lifecycle PROPERTIES TIMEOUT 15)
        add_executable(vqec_vision_ai_frame_submission_test
            tests/contract/vqec_vision_frame_submission_test.cpp)
        target_link_libraries(vqec_vision_ai_frame_submission_test
            PRIVATE vqec_vision_ai_gst_frame_bridge)
        set_target_properties(vqec_vision_ai_frame_submission_test PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME frame_submission_contract COMMAND vqec_vision_ai_frame_submission_test)
        set_tests_properties(frame_submission_contract PROPERTIES TIMEOUT 5)
        add_executable(vqec_vision_ai_tensor_output_test tests/contract/vqec_vision_tensor_output_test.cpp)
        target_link_libraries(vqec_vision_ai_tensor_output_test PRIVATE vqec_vision_ai_gst_frame_bridge)
        set_target_properties(vqec_vision_ai_tensor_output_test PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME tensor_output_contract COMMAND vqec_vision_ai_tensor_output_test)
        add_executable(vqec_vision_ai_dmabuf_bridge_test tests/contract/vqec_vision_dmabuf_bridge_test.cpp)
        target_link_libraries(vqec_vision_ai_dmabuf_bridge_test PRIVATE vqec_vision_ai_gst_frame_bridge)
        set_target_properties(vqec_vision_ai_dmabuf_bridge_test PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME dmabuf_memory_ownership COMMAND vqec_vision_ai_dmabuf_bridge_test)
        set_tests_properties(dmabuf_memory_ownership PROPERTIES TIMEOUT 5)
    endif()
    if(VQEC_VISION_AI_ENABLE_CAMERA)
        add_executable(vqec_vision_ai_raw_source_resolver_test
            tests/unit/vqec_vision_raw_source_resolver_test.cpp)
        target_link_libraries(vqec_vision_ai_raw_source_resolver_test
            PRIVATE vqec_vision_ai_camera)
        set_target_properties(vqec_vision_ai_raw_source_resolver_test
            PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME raw_source_resolution COMMAND vqec_vision_ai_raw_source_resolver_test)
        add_executable(vqec_vision_ai_camera_receiver_test tests/contract/vqec_vision_camera_receiver_test.cpp)
        target_link_libraries(vqec_vision_ai_camera_receiver_test PRIVATE vqec_vision_ai_camera)
        target_compile_definitions(vqec_vision_ai_camera_receiver_test PRIVATE _GNU_SOURCE)
        set_target_properties(vqec_vision_ai_camera_receiver_test PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME camera_receiver_contract COMMAND vqec_vision_ai_camera_receiver_test)
        set_tests_properties(camera_receiver_contract PROPERTIES TIMEOUT 5)
    endif()
    if(TARGET vqec_vision_ai_reference)
        add_executable(vqec_vision_ai_runtime_executor_test
            tests/contract/vqec_vision_runtime_executor_test.cpp)
        target_link_libraries(vqec_vision_ai_runtime_executor_test PRIVATE
            vqec_vision_ai_runtime_executor
            vqec_vision_ai_runtime_composition_factory
            vqec_vision_ai_reference
            vqec_vision_ai_feature_fanout
            vqec_vision_ai_feature_runtime)
        set_target_properties(vqec_vision_ai_runtime_executor_test
            PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME runtime_executor_e2e COMMAND vqec_vision_ai_runtime_executor_test)
        set_tests_properties(runtime_executor_e2e PROPERTIES TIMEOUT 10)
    endif()
    if(TARGET vqec_ai_vision_applications)
        add_test(NAME service_executable_smoke COMMAND vqec_ai_vision_applications
            --deployment ${CMAKE_CURRENT_SOURCE_DIR}/tests/golden/service_smoke_deployment.json
            --model-catalog ${CMAKE_CURRENT_SOURCE_DIR}/tests/golden/service_smoke_catalog.json
            --feature-catalog ${CMAKE_CURRENT_SOURCE_DIR}/tests/golden/service_smoke_features.json
            --steps 160 --require-sources 2)
        set_tests_properties(service_executable_smoke PROPERTIES TIMEOUT 20)
    endif()
endif()

# Apply project warning policy to every build-system target defined in this
# directory instead of a hand-maintained list, so a new target cannot silently
# miss -Wall/-Wextra/-Wpedantic or -Werror. Imported/vendor targets are excluded.
get_property(_vqec_vision_ai_directory_targets DIRECTORY
    PROPERTY BUILDSYSTEM_TARGETS)
foreach(project_target IN LISTS _vqec_vision_ai_directory_targets)
    get_target_property(_vqec_vision_ai_target_type ${project_target} TYPE)
    if(_vqec_vision_ai_target_type MATCHES
       "^(STATIC_LIBRARY|SHARED_LIBRARY|MODULE_LIBRARY|EXECUTABLE|OBJECT_LIBRARY)$")
        target_compile_options(${project_target} PRIVATE
            "$<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>"
            "$<$<CXX_COMPILER_ID:MSVC>:/W4>")
        if(VQEC_VISION_AI_WERROR)
            target_compile_options(${project_target} PRIVATE
                "$<$<CXX_COMPILER_ID:GNU,Clang>:-Werror>"
                "$<$<CXX_COMPILER_ID:MSVC>:/WX>")
        endif()
        set_target_properties(${project_target} PROPERTIES CXX_EXTENSIONS OFF)
    endif()
endforeach()
unset(_vqec_vision_ai_directory_targets)
unset(_vqec_vision_ai_target_type)

# No camera service executable, installation/IPK target or implicit board execution yet.
