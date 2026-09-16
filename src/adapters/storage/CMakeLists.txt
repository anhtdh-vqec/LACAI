find_package(OpenSSL 3.0 REQUIRED COMPONENTS Crypto)

add_library(vqec_vision_ai_encrypted_gallery_store STATIC
    vqec_vision_encrypted_face_gallery_store.cpp)
target_include_directories(vqec_vision_ai_encrypted_gallery_store PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_link_libraries(vqec_vision_ai_encrypted_gallery_store
    PUBLIC vqec_vision_ai_core PRIVATE OpenSSL::Crypto)
set_target_properties(vqec_vision_ai_encrypted_gallery_store
    PROPERTIES CXX_EXTENSIONS OFF)
