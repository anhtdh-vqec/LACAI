# Runtime targets (feature manager, loaders, model registry) and the scheduler submodule.
# Target names are unchanged from the former root definitions.

add_library(vqec_vision_ai_feature_runtime STATIC
    feature_manager/vqec_vision_feature_stage.cpp
    feature_manager/vqec_vision_feature_processor_registry.cpp
    feature_manager/vqec_vision_feature_activation_manager.cpp)
target_include_directories(vqec_vision_ai_feature_runtime PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/feature_manager>)
target_link_libraries(vqec_vision_ai_feature_runtime PUBLIC vqec_vision_ai_core)
set_target_properties(vqec_vision_ai_feature_runtime PROPERTIES CXX_EXTENSIONS OFF)

if(VQEC_VISION_AI_ENABLE_MODEL_CATALOG)
    add_library(vqec_vision_ai_model_catalog_config STATIC
        model_registry/vqec_vision_model_catalog.cpp
        model_registry/vqec_vision_model_package_registry.cpp)
    target_include_directories(vqec_vision_ai_model_catalog_config PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/model_registry>)
    target_link_libraries(vqec_vision_ai_model_catalog_config
        PUBLIC vqec_vision_ai_core PRIVATE nlohmann_json::nlohmann_json)
    set_target_properties(vqec_vision_ai_model_catalog_config PROPERTIES CXX_EXTENSIONS OFF)
endif()

if(VQEC_VISION_AI_ENABLE_ARTIFACT_DIGEST)
    find_package(OpenSSL 3.0 REQUIRED COMPONENTS Crypto)
    set(_vqec_vision_ai_digest_sources
        model_registry/vqec_vision_artifact_digest.cpp)
    if(UNIX)
        # The trusted resolver enforces containment with POSIX fd/stat/realpath semantics.
        list(APPEND _vqec_vision_ai_digest_sources
            model_registry/vqec_vision_artifact_resolver.cpp)
    endif()
    add_library(vqec_vision_ai_artifact_digest STATIC ${_vqec_vision_ai_digest_sources})
    target_include_directories(vqec_vision_ai_artifact_digest PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/model_registry>)
    target_link_libraries(vqec_vision_ai_artifact_digest
        PUBLIC vqec_vision_ai_core PRIVATE OpenSSL::Crypto)
    set_target_properties(vqec_vision_ai_artifact_digest PROPERTIES CXX_EXTENSIONS OFF)
endif()

if(VQEC_VISION_AI_ENABLE_MODEL_MANIFEST)
    add_library(vqec_vision_ai_model_manifest STATIC
        model_registry/vqec_vision_output_manifest.cpp)
    target_include_directories(vqec_vision_ai_model_manifest PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/model_registry>)
    target_link_libraries(vqec_vision_ai_model_manifest
        PUBLIC vqec_vision_ai_core PRIVATE nlohmann_json::nlohmann_json)
    set_target_properties(vqec_vision_ai_model_manifest PROPERTIES CXX_EXTENSIONS OFF)
    if(VQEC_VISION_AI_BUILD_MANIFEST_CHECK)
        add_executable(vqec_vision_ai_manifest_check
            ${CMAKE_SOURCE_DIR}/tools/diagnostics/vqec_vision_manifest_check.cpp)
        target_link_libraries(vqec_vision_ai_manifest_check PRIVATE vqec_vision_ai_model_manifest)
        set_target_properties(vqec_vision_ai_manifest_check PROPERTIES CXX_EXTENSIONS OFF)
    endif()
endif()

# Strict decoder package loader. Shared by the production platform and available whenever a
# JSON library is present; it has no vendor or perception dependency.
if(TARGET nlohmann_json::nlohmann_json)
    add_library(vqec_vision_ai_decoder_package STATIC
        model_registry/vqec_vision_decoder_package.cpp)
    target_include_directories(vqec_vision_ai_decoder_package PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/model_registry>)
    target_link_libraries(vqec_vision_ai_decoder_package
        PUBLIC vqec_vision_ai_core PRIVATE nlohmann_json::nlohmann_json)
    set_target_properties(vqec_vision_ai_decoder_package PROPERTIES CXX_EXTENSIONS OFF)
endif()

if(VQEC_VISION_AI_ENABLE_DEPLOYMENT_CONFIG)
    add_library(vqec_vision_ai_deployment_config STATIC
        lifecycle/vqec_vision_deployment_config.cpp)
    target_include_directories(vqec_vision_ai_deployment_config PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lifecycle>)
    target_link_libraries(vqec_vision_ai_deployment_config
        PUBLIC vqec_vision_ai_core PRIVATE nlohmann_json::nlohmann_json)
    set_target_properties(vqec_vision_ai_deployment_config PROPERTIES CXX_EXTENSIONS OFF)
endif()

if(VQEC_VISION_AI_ENABLE_FEATURE_CATALOG)
    add_library(vqec_vision_ai_feature_catalog_config STATIC
        feature_manager/vqec_vision_feature_catalog.cpp)
    target_include_directories(vqec_vision_ai_feature_catalog_config PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/feature_manager>)
    target_link_libraries(vqec_vision_ai_feature_catalog_config
        PUBLIC vqec_vision_ai_feature_runtime PRIVATE nlohmann_json::nlohmann_json)
    set_target_properties(vqec_vision_ai_feature_catalog_config PROPERTIES CXX_EXTENSIONS OFF)
endif()

if(TARGET nlohmann_json::nlohmann_json)
    add_library(vqec_vision_ai_app_manifest STATIC
        lifecycle/vqec_vision_app_catalog.cpp
        lifecycle/vqec_vision_app_manifest.cpp
        lifecycle/vqec_vision_app_entitlement.cpp
        lifecycle/vqec_vision_runtime_control_snapshot.cpp)
    target_include_directories(vqec_vision_ai_app_manifest PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lifecycle>)
    target_link_libraries(vqec_vision_ai_app_manifest
        PUBLIC vqec_vision_ai_core PRIVATE nlohmann_json::nlohmann_json)
    set_target_properties(vqec_vision_ai_app_manifest PROPERTIES CXX_EXTENSIONS OFF)

    add_library(vqec_vision_ai_usecase_config STATIC
        feature_manager/vqec_vision_usecase_config.cpp)
    target_include_directories(vqec_vision_ai_usecase_config PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/feature_manager>)
    target_link_libraries(vqec_vision_ai_usecase_config
        PUBLIC vqec_vision_ai_core PRIVATE nlohmann_json::nlohmann_json)
    set_target_properties(vqec_vision_ai_usecase_config PROPERTIES CXX_EXTENSIONS OFF)

    add_library(vqec_vision_ai_usecase_control STATIC
        feature_manager/vqec_vision_usecase_control_manager.cpp)
    target_include_directories(vqec_vision_ai_usecase_control PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/feature_manager>)
    target_link_libraries(vqec_vision_ai_usecase_control
        PUBLIC vqec_vision_ai_usecase_config vqec_vision_ai_core)
    set_target_properties(vqec_vision_ai_usecase_control PROPERTIES CXX_EXTENSIONS OFF)
endif()

add_library(vqec_vision_ai_admission STATIC
    admission/vqec_vision_activation_snapshot.cpp
    admission/vqec_vision_hardware_admission_profile.cpp)
target_include_directories(vqec_vision_ai_admission PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/admission>)
target_link_libraries(vqec_vision_ai_admission
    PUBLIC vqec_vision_ai_core PRIVATE nlohmann_json::nlohmann_json)
set_target_properties(vqec_vision_ai_admission PROPERTIES CXX_EXTENSIONS OFF)

add_library(vqec_vision_ai_recovery_controller STATIC
    lifecycle/vqec_vision_recovery_controller.cpp)
target_include_directories(vqec_vision_ai_recovery_controller PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lifecycle>)
target_link_libraries(vqec_vision_ai_recovery_controller PUBLIC vqec_vision_ai_core)
set_target_properties(vqec_vision_ai_recovery_controller PROPERTIES CXX_EXTENSIONS OFF)

add_subdirectory(scheduler)
