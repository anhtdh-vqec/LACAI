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
option(VQEC_VISION_AI_BUILD_MANIFEST_CHECK "Build read-only model metadata checker" OFF)
option(VQEC_VISION_AI_ENABLE_ARTIFACT_DIGEST "Enable bounded artifact SHA-256 comparison" OFF)

if(VQEC_VISION_AI_BUILD_MANIFEST_CHECK AND NOT VQEC_VISION_AI_ENABLE_MODEL_MANIFEST)
    message(FATAL_ERROR "Manifest checker requires VQEC_VISION_AI_ENABLE_MODEL_MANIFEST")
endif()

if(VQEC_VISION_AI_ENABLE_MODEL_MANIFEST OR VQEC_VISION_AI_ENABLE_MODEL_CATALOG OR
   VQEC_VISION_AI_ENABLE_DEPLOYMENT_CONFIG)
    find_package(nlohmann_json 3.12.0 EXACT CONFIG REQUIRED)
endif()

add_library(vqec_vision_ai_core STATIC
    src/core/vqec_vision_inference_plan.cpp src/core/vqec_vision_submission_window.cpp src/core/vqec_vision_source_binding.cpp
    src/core/vqec_vision_tensor_contract.cpp src/core/vqec_vision_output_gate.cpp
    src/core/vqec_vision_preview_contract.cpp src/core/vqec_vision_preview_surface.cpp
    src/core/vqec_vision_encoder_window.cpp src/core/vqec_vision_preview_pool.cpp
    src/core/vqec_vision_encoded_output.cpp src/core/vqec_vision_output_generation.cpp
    src/core/vqec_vision_encoder_contract.cpp src/core/vqec_vision_observation.cpp
    src/core/vqec_vision_deployment_config.cpp src/core/vqec_vision_model_catalog.cpp)
target_compile_features(vqec_vision_ai_core PUBLIC cxx_std_17)
target_include_directories(vqec_vision_ai_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
set_target_properties(vqec_vision_ai_core PROPERTIES CXX_EXTENSIONS OFF)

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
    add_library(vqec_vision_ai_qualcomm STATIC src/adapters/qualcomm/vqec_vision_plugin_graph.cpp)
    target_compile_features(vqec_vision_ai_qualcomm PUBLIC cxx_std_17)
    target_include_directories(vqec_vision_ai_qualcomm PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/adapters/qualcomm>)
    target_link_libraries(vqec_vision_ai_qualcomm
        PUBLIC vqec_vision_ai_core PRIVATE PkgConfig::gstreamer vqec_vision_ai_gst_frame_bridge)
    set_target_properties(vqec_vision_ai_qualcomm PROPERTIES CXX_EXTENSIONS OFF)
endif()

if(VQEC_VISION_AI_ENABLE_CAMERA AND VQEC_VISION_AI_ENABLE_QUALCOMM)
    add_library(vqec_vision_ai_camera_graph_pump STATIC
        src/app/vqec_vision_camera_graph_pump.cpp src/app/vqec_vision_camera_session.cpp)
    target_include_directories(vqec_vision_ai_camera_graph_pump PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/app>)
    target_link_libraries(vqec_vision_ai_camera_graph_pump
        PUBLIC vqec_vision_ai_qualcomm)
    set_target_properties(vqec_vision_ai_camera_graph_pump PROPERTIES CXX_EXTENSIONS OFF)
endif()

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
if(BUILD_TESTING)
    add_executable(vqec_vision_ai_model_cadence_test
        tests/unit/vqec_vision_model_cadence_test.cpp)
    target_link_libraries(vqec_vision_ai_model_cadence_test
        PRIVATE vqec_vision_ai_scheduler)
    set_target_properties(vqec_vision_ai_model_cadence_test PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME model_cadence_selection COMMAND vqec_vision_ai_model_cadence_test)
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
    if(TARGET vqec_vision_ai_camera_graph_pump)
        add_executable(vqec_vision_ai_camera_session_test tests/contract/vqec_vision_camera_session_test.cpp)
        target_link_libraries(vqec_vision_ai_camera_session_test
            PRIVATE vqec_vision_ai_camera_graph_pump vqec_vision_ai_camera)
        set_target_properties(vqec_vision_ai_camera_session_test PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME camera_session_coordination COMMAND vqec_vision_ai_camera_session_test)
        set_tests_properties(camera_session_coordination PROPERTIES TIMEOUT 5)
        add_executable(vqec_vision_ai_camera_graph_pump_test
            tests/contract/vqec_vision_camera_graph_pump_test.cpp)
        target_link_libraries(vqec_vision_ai_camera_graph_pump_test
            PRIVATE vqec_vision_ai_camera_graph_pump vqec_vision_ai_camera)
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
endif()

foreach(project_target vqec_vision_ai_core vqec_vision_ai_qualcomm
        vqec_vision_ai_source_supervisor vqec_vision_ai_multi_source_supervisor_test
        vqec_vision_ai_scheduler vqec_vision_ai_model_cadence_test
        vqec_vision_ai_inference_plan_test vqec_vision_ai_camera_wire
        vqec_vision_ai_camera vqec_vision_ai_legacy_wire_test
        vqec_vision_ai_camera_receiver_test vqec_vision_ai_raw_source_resolver_test
        vqec_vision_ai_camera_control vqec_vision_ai_camera_dbus vqec_vision_ai_camera_control_test
        vqec_vision_ai_gst_frame_bridge vqec_vision_ai_dmabuf_bridge_test
        vqec_vision_ai_submission_window_test vqec_vision_ai_plugin_graph_test
        vqec_vision_ai_tensor_output_test vqec_vision_ai_source_binding_test
        vqec_vision_ai_frame_submission_test vqec_vision_ai_graph_fixture
        vqec_vision_ai_graph_lifecycle_test vqec_vision_ai_camera_graph_pump
        vqec_vision_ai_camera_graph_pump_test vqec_vision_ai_camera_session_test
        vqec_vision_ai_tensor_contract_test vqec_vision_ai_model_manifest
        vqec_vision_ai_output_manifest_test vqec_vision_ai_manifest_check
        vqec_vision_ai_artifact_digest vqec_vision_ai_artifact_digest_test
        vqec_vision_ai_output_gate_test vqec_vision_ai_preview_contract_test
        vqec_vision_ai_preview_surface_test vqec_vision_ai_encoder_window_test
        vqec_vision_ai_preview_pool_test vqec_vision_ai_encoder_preparation
        vqec_vision_ai_encoder_preparation_test vqec_vision_ai_encoded_output_test
        vqec_vision_ai_encoded_dispatch vqec_vision_ai_encoded_dispatch_test
        vqec_vision_ai_fw_ring vqec_vision_ai_ring_sink_test
        vqec_vision_ai_deployment_config vqec_vision_ai_deployment_config_test
        vqec_vision_ai_deployment_loader_test vqec_vision_ai_model_catalog_test
        vqec_vision_ai_model_catalog_config vqec_vision_ai_model_catalog_loader_test
        vqec_vision_ai_admission vqec_vision_ai_activation_snapshot_test)
    if(TARGET ${project_target})
        target_compile_options(${project_target} PRIVATE
            "$<$<CXX_COMPILER_ID:GNU,Clang>:-Wall;-Wextra;-Wpedantic>"
            "$<$<CXX_COMPILER_ID:MSVC>:/W4>")
    endif()
endforeach()

# No camera service executable, installation/IPK target or implicit board execution yet.
