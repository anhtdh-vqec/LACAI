find_package(OpenSSL 3.0 REQUIRED COMPONENTS Crypto)
find_package(SQLite3 REQUIRED)

add_library(vqec_vision_ai_encrypted_gallery_store STATIC
    vqec_vision_encrypted_face_gallery_store.cpp)
target_include_directories(vqec_vision_ai_encrypted_gallery_store PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_link_libraries(vqec_vision_ai_encrypted_gallery_store
    PUBLIC vqec_vision_ai_core PRIVATE OpenSSL::Crypto)
set_target_properties(vqec_vision_ai_encrypted_gallery_store
    PROPERTIES CXX_EXTENSIONS OFF)

add_library(vqec_vision_ai_sqlite_metadata_store STATIC
    vqec_vision_sqlite_metadata_store.cpp)
target_include_directories(vqec_vision_ai_sqlite_metadata_store PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_link_libraries(vqec_vision_ai_sqlite_metadata_store
    PUBLIC vqec_vision_ai_core PRIVATE SQLite::SQLite3)
set_target_properties(vqec_vision_ai_sqlite_metadata_store
    PROPERTIES CXX_EXTENSIONS OFF)

add_library(vqec_vision_ai_spatiotemporal_store STATIC
    vqec_vision_spatiotemporal_store.cpp
    vqec_vision_spatiotemporal_store_projection.cpp)
target_include_directories(vqec_vision_ai_spatiotemporal_store PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_link_libraries(vqec_vision_ai_spatiotemporal_store
    PUBLIC vqec_vision_ai_core PRIVATE SQLite::SQLite3)
set_target_properties(vqec_vision_ai_spatiotemporal_store
    PROPERTIES CXX_EXTENSIONS OFF)
