find_package(OpenSSL 3.0 REQUIRED COMPONENTS Crypto)

add_library(vqec_vision_ai_ed25519_app_package_verifier STATIC
    vqec_vision_ed25519_app_package_verifier.cpp)
target_include_directories(vqec_vision_ai_ed25519_app_package_verifier PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/runtime/lifecycle>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/runtime/model_registry>)
target_link_libraries(vqec_vision_ai_ed25519_app_package_verifier PUBLIC
    vqec_vision_ai_app_manifest
    vqec_vision_ai_artifact_digest
    OpenSSL::Crypto)
set_target_properties(vqec_vision_ai_ed25519_app_package_verifier
    PROPERTIES CXX_EXTENSIONS OFF)
