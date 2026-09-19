# Application orchestration targets and the service executable. Target names are
# unchanged from the former root definitions.

add_library(vqec_vision_ai_app_configuration_registry STATIC
    management/vqec_vision_app_configuration_registry.cpp)
target_include_directories(vqec_vision_ai_app_configuration_registry PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/management>)
target_link_libraries(vqec_vision_ai_app_configuration_registry
    PUBLIC vqec_vision_ai_core)
set_target_properties(vqec_vision_ai_app_configuration_registry
    PROPERTIES CXX_EXTENSIONS OFF)

add_library(vqec_vision_ai_app_manager_options STATIC
    management/vqec_vision_app_manager_options.cpp)
target_include_directories(vqec_vision_ai_app_manager_options PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/management>)
target_link_libraries(vqec_vision_ai_app_manager_options PUBLIC vqec_vision_ai_core)
set_target_properties(vqec_vision_ai_app_manager_options PROPERTIES CXX_EXTENSIONS OFF)

if(TARGET vqec_vision_ai_artifact_digest AND TARGET vqec_vision_ai_app_manifest)
    add_library(vqec_vision_ai_app_manager STATIC
        management/vqec_vision_app_manager.cpp)
    target_include_directories(vqec_vision_ai_app_manager PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/management>)
    target_link_libraries(vqec_vision_ai_app_manager PUBLIC
        vqec_vision_ai_app_configuration_registry
        vqec_vision_ai_app_manifest
        vqec_vision_ai_artifact_digest)
    set_target_properties(vqec_vision_ai_app_manager PROPERTIES CXX_EXTENSIONS OFF)
endif()

if(TARGET vqec_vision_ai_app_manager AND
   TARGET vqec_vision_ai_app_manager_dbus AND
   TARGET vqec_vision_ai_ed25519_app_package_verifier AND
   TARGET vqec_vision_ai_ed25519_app_entitlement_verifier AND
   TARGET vqec_vision_ai_sqlite_app_inventory)
    add_executable(vqec_vision_app_manager
        management/vqec_vision_app_manager_main.cpp)
    target_include_directories(vqec_vision_app_manager PRIVATE
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/adapters/fw_control>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/adapters/security>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/adapters/storage>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/features/fire_smoke>)
    target_link_libraries(vqec_vision_app_manager PRIVATE
        vqec_vision_ai_app_manager_options
        vqec_vision_ai_app_manager
        vqec_vision_ai_app_manager_dbus
        vqec_vision_ai_ed25519_app_package_verifier
        vqec_vision_ai_ed25519_app_entitlement_verifier
        vqec_vision_ai_sqlite_app_inventory
        vqec_vision_ai_fire_smoke_feature
        Threads::Threads)
    set_target_properties(vqec_vision_app_manager PROPERTIES CXX_EXTENSIONS OFF)
endif()

if(TARGET vqec_vision_ai_app_manager_dbus)
    add_executable(vqec_vision_app_manager_control
        ${CMAKE_SOURCE_DIR}/tools/diagnostics/vqec_vision_app_manager_control.cpp)
    target_include_directories(vqec_vision_app_manager_control PRIVATE
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/adapters/fw_control>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/runtime/lifecycle>)
    target_link_libraries(vqec_vision_app_manager_control PRIVATE
        vqec_vision_ai_app_manager_dbus)
    set_target_properties(vqec_vision_app_manager_control PROPERTIES CXX_EXTENSIONS OFF)
endif()

# Cold-path startup option parsing, split out of the service main translation unit so it
# can be unit-tested without building the full executable.
add_library(vqec_vision_ai_service_options STATIC
    service/bootstrap/vqec_vision_service_options.cpp)
target_include_directories(vqec_vision_ai_service_options PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/service/bootstrap>)
target_link_libraries(vqec_vision_ai_service_options PUBLIC vqec_vision_ai_core)
set_target_properties(vqec_vision_ai_service_options PROPERTIES CXX_EXTENSIONS OFF)

add_library(vqec_vision_ai_service_startup STATIC
    service/bootstrap/vqec_vision_service_startup.cpp)
target_include_directories(vqec_vision_ai_service_startup PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/service/bootstrap>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/runtime/feature_manager>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/runtime/model_registry>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/runtime/lifecycle>)
target_link_libraries(vqec_vision_ai_service_startup PUBLIC
    vqec_vision_ai_service_options
    vqec_vision_ai_model_catalog_config
    vqec_vision_ai_feature_catalog_config
    vqec_vision_ai_deployment_config
    vqec_vision_ai_usecase_config
    vqec_vision_ai_usecase_control)
set_target_properties(vqec_vision_ai_service_startup PROPERTIES CXX_EXTENSIONS OFF)

add_library(vqec_vision_ai_metadata_service STATIC
    service/output/vqec_vision_metadata_service.cpp)
target_include_directories(vqec_vision_ai_metadata_service PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/service/output>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/adapters/storage>)
target_link_libraries(vqec_vision_ai_metadata_service PUBLIC
    vqec_vision_ai_spatiotemporal_store Threads::Threads)
set_target_properties(vqec_vision_ai_metadata_service PROPERTIES CXX_EXTENSIONS OFF)

add_library(vqec_vision_ai_metadata_runtime STATIC
    service/output/vqec_vision_metadata_runtime.cpp)
target_include_directories(vqec_vision_ai_metadata_runtime PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/service/output>)
target_link_libraries(vqec_vision_ai_metadata_runtime PUBLIC
    vqec_vision_ai_metadata_service nlohmann_json::nlohmann_json)
set_target_properties(vqec_vision_ai_metadata_runtime PROPERTIES CXX_EXTENSIONS OFF)

add_library(vqec_vision_ai_evidence_service STATIC
    service/output/vqec_vision_evidence_service.cpp)
target_include_directories(vqec_vision_ai_evidence_service PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/service/output>)
target_link_libraries(vqec_vision_ai_evidence_service PUBLIC
    vqec_vision_ai_core Threads::Threads)
set_target_properties(vqec_vision_ai_evidence_service PROPERTIES CXX_EXTENSIONS OFF)

add_library(vqec_vision_ai_service_output_runtime STATIC
    service/output/vqec_vision_service_output_runtime.cpp)
target_include_directories(vqec_vision_ai_service_output_runtime PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/service/bootstrap>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/service/output>
    PRIVATE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/adapters/fw_output>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/adapters/storage>)
target_link_libraries(vqec_vision_ai_service_output_runtime PUBLIC
    vqec_vision_ai_metadata_runtime
    vqec_vision_ai_evidence_service
    vqec_vision_ai_sqlite_evidence_outbox
    vqec_vision_ai_evidence_uds_client)
set_target_properties(vqec_vision_ai_service_output_runtime
    PROPERTIES CXX_EXTENSIONS OFF)

add_executable(vqec_vision_evidence_probe
    ${CMAKE_SOURCE_DIR}/tools/diagnostics/vqec_vision_evidence_probe.cpp)
target_include_directories(vqec_vision_evidence_probe PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/service/output>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/adapters/fw_output>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/adapters/storage>)
target_link_libraries(vqec_vision_evidence_probe PRIVATE
    vqec_vision_ai_evidence_service
    vqec_vision_ai_sqlite_evidence_outbox
    vqec_vision_ai_evidence_uds_client)
set_target_properties(vqec_vision_evidence_probe PROPERTIES CXX_EXTENSIONS OFF)

add_library(vqec_vision_ai_camera_graph_pump STATIC
    pipeline/vqec_vision_camera_graph_pump.cpp
    session/vqec_vision_camera_session.cpp)
target_include_directories(vqec_vision_ai_camera_graph_pump PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/pipeline>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/session>)
target_link_libraries(vqec_vision_ai_camera_graph_pump PUBLIC vqec_vision_ai_core)
set_target_properties(vqec_vision_ai_camera_graph_pump PROPERTIES CXX_EXTENSIONS OFF)

add_library(vqec_vision_ai_source_supervisor STATIC
    supervision/vqec_vision_multi_source_supervisor.cpp)
target_include_directories(vqec_vision_ai_source_supervisor PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/supervision>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/session>)
target_link_libraries(vqec_vision_ai_source_supervisor
    PUBLIC vqec_vision_ai_core vqec_vision_ai_source_session_worker)
set_target_properties(vqec_vision_ai_source_supervisor PROPERTIES CXX_EXTENSIONS OFF)

add_library(vqec_vision_ai_source_session_worker STATIC
    supervision/vqec_vision_source_session_worker.cpp)
target_include_directories(vqec_vision_ai_source_session_worker PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/supervision>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/session>)
target_link_libraries(vqec_vision_ai_source_session_worker
    PUBLIC vqec_vision_ai_core Threads::Threads)
set_target_properties(vqec_vision_ai_source_session_worker PROPERTIES CXX_EXTENSIONS OFF)

add_library(vqec_vision_ai_multi_model_pump STATIC
    pipeline/vqec_vision_multi_model_pump.cpp)
target_include_directories(vqec_vision_ai_multi_model_pump PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/pipeline>)
target_link_libraries(vqec_vision_ai_multi_model_pump
    PUBLIC vqec_vision_ai_core vqec_vision_ai_scheduler)
set_target_properties(vqec_vision_ai_multi_model_pump PROPERTIES CXX_EXTENSIONS OFF)

add_library(vqec_vision_ai_multi_model_session STATIC
    session/vqec_vision_multi_model_session.cpp)
target_include_directories(vqec_vision_ai_multi_model_session PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/session>)
target_link_libraries(vqec_vision_ai_multi_model_session
    PUBLIC vqec_vision_ai_multi_model_pump)
set_target_properties(vqec_vision_ai_multi_model_session PROPERTIES CXX_EXTENSIONS OFF)

add_library(vqec_vision_ai_cascade_coordinator STATIC
    cascade/vqec_vision_cascade_coordinator.cpp
    cascade/vqec_vision_cascade_execution_worker.cpp)
target_include_directories(vqec_vision_ai_cascade_coordinator PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/cascade>)
target_link_libraries(vqec_vision_ai_cascade_coordinator
    PUBLIC vqec_vision_ai_core Threads::Threads)
set_target_properties(vqec_vision_ai_cascade_coordinator PROPERTIES CXX_EXTENSIONS OFF)

add_library(vqec_vision_ai_single_image_inference STATIC
    cascade/vqec_vision_single_image_inference.cpp)
target_include_directories(vqec_vision_ai_single_image_inference PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/cascade>)
target_link_libraries(vqec_vision_ai_single_image_inference PUBLIC vqec_vision_ai_core)
set_target_properties(vqec_vision_ai_single_image_inference PROPERTIES CXX_EXTENSIONS OFF)

add_library(vqec_vision_ai_face_enrollment_image_pipeline STATIC
    cascade/vqec_vision_face_enrollment_image_pipeline.cpp)
target_include_directories(vqec_vision_ai_face_enrollment_image_pipeline PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/cascade>)
target_link_libraries(vqec_vision_ai_face_enrollment_image_pipeline PUBLIC
    vqec_vision_ai_core vqec_vision_ai_single_image_inference
    vqec_vision_ai_cascade_coordinator)
set_target_properties(vqec_vision_ai_face_enrollment_image_pipeline
    PROPERTIES CXX_EXTENSIONS OFF)

add_library(vqec_vision_ai_cascade_graph_session STATIC
    session/vqec_vision_cascade_graph_session.cpp)
target_include_directories(vqec_vision_ai_cascade_graph_session PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/session>)
target_link_libraries(vqec_vision_ai_cascade_graph_session PUBLIC vqec_vision_ai_core)
set_target_properties(vqec_vision_ai_cascade_graph_session PROPERTIES CXX_EXTENSIONS OFF)

add_library(vqec_vision_ai_encoder_preparation STATIC
    pipeline/vqec_vision_encoder_preparation.cpp)
target_include_directories(vqec_vision_ai_encoder_preparation PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/pipeline>)
target_link_libraries(vqec_vision_ai_encoder_preparation PUBLIC vqec_vision_ai_core)
set_target_properties(vqec_vision_ai_encoder_preparation PROPERTIES CXX_EXTENSIONS OFF)
add_library(vqec_vision_ai_application_composition STATIC
    composition/vqec_vision_application_composition.cpp)
target_include_directories(vqec_vision_ai_application_composition PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/composition>)
target_link_libraries(vqec_vision_ai_application_composition PUBLIC vqec_vision_ai_source_supervisor)
set_target_properties(vqec_vision_ai_application_composition PROPERTIES CXX_EXTENSIONS OFF)
add_library(vqec_vision_ai_service_feature_registry STATIC
    composition/vqec_vision_service_feature_registry.cpp)
target_include_directories(vqec_vision_ai_service_feature_registry PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/composition>)
target_link_libraries(vqec_vision_ai_service_feature_registry PUBLIC
    vqec_vision_ai_feature_runtime vqec_vision_ai_fire_smoke_feature)
set_target_properties(vqec_vision_ai_service_feature_registry
    PROPERTIES CXX_EXTENSIONS OFF)
add_library(vqec_vision_ai_perception_result_stage STATIC
    pipeline/vqec_vision_perception_result_stage.cpp
    composition/vqec_vision_perception_stage_factory.cpp)
target_include_directories(vqec_vision_ai_perception_result_stage PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/pipeline>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/composition>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/perception/detection>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/perception/tracking>)
target_link_libraries(vqec_vision_ai_perception_result_stage
    PUBLIC vqec_vision_ai_detection vqec_vision_ai_tracking)
set_target_properties(vqec_vision_ai_perception_result_stage PROPERTIES CXX_EXTENSIONS OFF)
add_library(vqec_vision_ai_multi_model_result_router STATIC
    pipeline/vqec_vision_multi_model_result_router.cpp
    composition/vqec_vision_source_perception_factory.cpp)
target_include_directories(vqec_vision_ai_multi_model_result_router PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/pipeline>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/composition>)
target_link_libraries(vqec_vision_ai_multi_model_result_router
    PUBLIC vqec_vision_ai_multi_model_pump vqec_vision_ai_perception_result_stage)
set_target_properties(vqec_vision_ai_multi_model_result_router PROPERTIES CXX_EXTENSIONS OFF)
add_library(vqec_vision_ai_runtime_composition_factory STATIC
    composition/vqec_vision_runtime_composition_factory.cpp)
target_include_directories(vqec_vision_ai_runtime_composition_factory PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/composition>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/runtime/admission>)
target_link_libraries(vqec_vision_ai_runtime_composition_factory PUBLIC
    vqec_vision_ai_admission vqec_vision_ai_application_composition
    vqec_vision_ai_multi_model_session vqec_vision_ai_multi_model_result_router
    vqec_vision_ai_multi_model_feature_pipeline vqec_vision_ai_runtime_executor)
set_target_properties(vqec_vision_ai_runtime_composition_factory
    PROPERTIES CXX_EXTENSIONS OFF)
add_library(vqec_vision_ai_feature_fanout STATIC
    pipeline/vqec_vision_feature_fanout.cpp)
target_include_directories(vqec_vision_ai_feature_fanout PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/pipeline>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/runtime/feature_manager>)
target_link_libraries(vqec_vision_ai_feature_fanout PUBLIC vqec_vision_ai_feature_runtime)
set_target_properties(vqec_vision_ai_feature_fanout PROPERTIES CXX_EXTENSIONS OFF)
add_library(vqec_vision_ai_multi_model_feature_pipeline STATIC
    pipeline/vqec_vision_multi_model_feature_pipeline.cpp)
target_include_directories(vqec_vision_ai_multi_model_feature_pipeline PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/pipeline>)
target_link_libraries(vqec_vision_ai_multi_model_feature_pipeline
    PUBLIC vqec_vision_ai_multi_model_result_router vqec_vision_ai_feature_fanout)
set_target_properties(vqec_vision_ai_multi_model_feature_pipeline PROPERTIES CXX_EXTENSIONS OFF)
add_library(vqec_vision_ai_runtime_executor STATIC
    supervision/vqec_vision_runtime_executor.cpp)
target_include_directories(vqec_vision_ai_runtime_executor PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/supervision>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/outputs/events>)
target_link_libraries(vqec_vision_ai_runtime_executor
    PUBLIC vqec_vision_ai_application_composition
    vqec_vision_ai_cascade_coordinator
    vqec_vision_ai_multi_model_feature_pipeline
    vqec_vision_ai_feature_event_dispatch)
set_target_properties(vqec_vision_ai_runtime_executor PROPERTIES CXX_EXTENSIONS OFF)

if(TARGET vqec_vision_ai_qnn_engine AND TARGET vqec_vision_ai_reference AND
   TARGET vqec_vision_ai_detection)
    add_executable(vqec_vision_ai_model_runner
        ${CMAKE_SOURCE_DIR}/tools/diagnostics/vqec_vision_model_runner.cpp)
    target_link_libraries(vqec_vision_ai_model_runner PRIVATE
        vqec_vision_ai_qnn_engine vqec_vision_ai_reference vqec_vision_ai_detection
        nlohmann_json::nlohmann_json)
    set_target_properties(vqec_vision_ai_model_runner PROPERTIES CXX_EXTENSIONS OFF)
endif()

# Production platform owner for the Qualcomm target: concrete RAW source, QNN graph,
# decoder, tracker and feature owners behind neutral ports.
if(TARGET vqec_vision_ai_qnn_engine AND TARGET vqec_vision_ai_qualcomm AND
   TARGET vqec_vision_ai_camera AND TARGET vqec_vision_ai_camera_dbus AND
   TARGET vqec_vision_ai_detection)
    add_library(vqec_vision_ai_production_platform STATIC
        platform/vqec_vision_production_platform.cpp)
    target_include_directories(vqec_vision_ai_production_platform PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/platform>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/runtime/feature_manager>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/perception/tracking>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/perception/detection>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/perception/embedding>)
    target_link_libraries(vqec_vision_ai_production_platform PUBLIC
        vqec_vision_ai_core vqec_vision_ai_qnn_engine vqec_vision_ai_camera
        vqec_vision_ai_camera_dbus vqec_vision_ai_detection
        vqec_vision_ai_tracking vqec_vision_ai_embedding_decoder
        vqec_vision_ai_feature_runtime vqec_vision_ai_qualcomm
        nlohmann_json::nlohmann_json)
    if(TARGET vqec_vision_ai_fastcv_aligner)
        target_link_libraries(vqec_vision_ai_production_platform PUBLIC
            vqec_vision_ai_fastcv_aligner)
        target_compile_definitions(vqec_vision_ai_production_platform PRIVATE
            VQEC_VISION_AI_HAS_FASTCV_ALIGNER=1)
    endif()
    if(TARGET vqec_vision_ai_qtiv_renderer)
        target_link_libraries(vqec_vision_ai_production_platform PUBLIC
            vqec_vision_ai_qtiv_renderer)
    endif()
    if(TARGET vqec_vision_ai_decoder_package)
        target_link_libraries(vqec_vision_ai_production_platform PUBLIC
            vqec_vision_ai_decoder_package)
    endif()
    if(TARGET vqec_vision_ai_artifact_digest)
        target_link_libraries(vqec_vision_ai_production_platform PUBLIC
            vqec_vision_ai_artifact_digest)
        target_include_directories(vqec_vision_ai_production_platform PUBLIC
            $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/runtime/model_registry>)
        target_compile_definitions(vqec_vision_ai_production_platform PRIVATE
            VQEC_VISION_AI_HAS_ARTIFACT_RESOLVER=1)
    endif()
    set_target_properties(vqec_vision_ai_production_platform PROPERTIES CXX_EXTENSIONS OFF)
endif()

# Service-owned cascade graph/session lifecycle, separated from the generation controller.
if(TARGET vqec_vision_ai_production_platform)
    add_library(vqec_vision_ai_service_cascade_runtime STATIC
        cascade/vqec_vision_service_cascade_runtime.cpp)
    target_include_directories(vqec_vision_ai_service_cascade_runtime PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/cascade>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/platform>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/service/bootstrap>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/service/generation>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/session>)
    target_link_libraries(vqec_vision_ai_service_cascade_runtime PUBLIC
        vqec_vision_ai_production_platform
        vqec_vision_ai_cascade_graph_session
        vqec_vision_ai_cascade_coordinator
        vqec_vision_ai_service_options)
    set_target_properties(vqec_vision_ai_service_cascade_runtime
        PROPERTIES CXX_EXTENSIONS OFF)
endif()

# Required external service executable. Built only when the JSON loaders and the
# device-free reference backend are available; otherwise there is no runnable harness.
if(TARGET vqec_vision_ai_production_platform AND TARGET vqec_vision_ai_reference AND
   TARGET vqec_vision_ai_deployment_config AND
   TARGET vqec_vision_ai_model_catalog_config AND TARGET vqec_vision_ai_feature_catalog_config AND
   TARGET vqec_vision_ai_usecase_config)
    add_executable(vqec_ai_vision_applications
        service/bootstrap/vqec_vision_service_main.cpp
        service/bootstrap/vqec_vision_service_runtime.cpp
        service/generation/vqec_vision_service_generation.cpp
        platform/vqec_vision_fake_platform.cpp
        platform/vqec_vision_reference_platform.cpp
        platform/vqec_vision_fixture_detector.cpp)
    target_link_libraries(vqec_ai_vision_applications PRIVATE
        vqec_vision_ai_service_options
        vqec_vision_ai_service_startup
        vqec_vision_ai_service_cascade_runtime
        vqec_vision_ai_metadata_runtime
        vqec_vision_ai_evidence_service
        vqec_vision_ai_service_output_runtime
        vqec_vision_ai_sqlite_evidence_outbox
        vqec_vision_ai_evidence_uds_client
        vqec_vision_ai_admission
        vqec_vision_ai_production_platform
        vqec_vision_ai_cascade_graph_session
        vqec_vision_ai_face_enrollment_image_pipeline
        vqec_vision_ai_image_path_authorizer
        vqec_vision_ai_runtime_composition_factory
        vqec_vision_ai_reference
        vqec_vision_ai_feature_fanout
        vqec_vision_ai_feature_runtime
        vqec_vision_ai_service_feature_registry
        vqec_vision_ai_embedding
        vqec_vision_ai_deployment_config
        vqec_vision_ai_model_catalog_config
        vqec_vision_ai_feature_catalog_config
        vqec_vision_ai_usecase_config
        vqec_vision_ai_usecase_control
        vqec_vision_ai_encrypted_gallery_store
        vqec_vision_ai_overlay_preparation
        vqec_vision_ai_event_delivery_seam)
    target_include_directories(vqec_ai_vision_applications PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/service/bootstrap>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/service/generation>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/service/output>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/platform>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/outputs/events>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/outputs/media>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/adapters/storage>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/adapters/fw_output>)
    if(TARGET vqec_vision_ai_usecase_control_dbus)
        target_link_libraries(vqec_ai_vision_applications PRIVATE
            vqec_vision_ai_usecase_control_dbus)
        target_include_directories(vqec_ai_vision_applications PRIVATE
            $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/adapters/fw_control>)
        target_compile_definitions(vqec_ai_vision_applications PRIVATE
            VQEC_VISION_AI_HAS_USECASE_CONTROL_DBUS=1)
    endif()
    if(TARGET vqec_vision_ai_app_manager_dbus)
        target_link_libraries(vqec_ai_vision_applications PRIVATE
            vqec_vision_ai_app_manager_dbus)
        target_include_directories(vqec_ai_vision_applications PRIVATE
            $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/adapters/fw_control>)
        target_compile_definitions(vqec_ai_vision_applications PRIVATE
            VQEC_VISION_AI_HAS_APP_MANAGER_DBUS=1)
    endif()
    if(TARGET vqec_vision_ai_face_enrollment_dbus)
        target_link_libraries(vqec_ai_vision_applications PRIVATE
            vqec_vision_ai_face_enrollment_dbus)
        target_include_directories(vqec_ai_vision_applications PRIVATE
            $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/adapters/fw_control>)
        target_compile_definitions(vqec_ai_vision_applications PRIVATE
            VQEC_VISION_AI_HAS_FACE_ENROLLMENT_DBUS=1)
    endif()
    if(TARGET vqec_vision_ai_zvec_embedding)
        target_link_libraries(vqec_ai_vision_applications PRIVATE
            vqec_vision_ai_zvec_embedding)
        target_compile_definitions(vqec_ai_vision_applications PRIVATE
            VQEC_VISION_AI_HAS_ZVEC=1)
    endif()
    set_target_properties(vqec_ai_vision_applications PROPERTIES CXX_EXTENSIONS OFF)
endif()
